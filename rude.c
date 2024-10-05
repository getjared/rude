// rude.c
// https://github.com/getjared

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define MAX_WORKSPACES 9
#define MAX_CLIENTS 100
#define GAP_SIZE 45
#define MOD_KEY Mod4Mask
#define WORKSPACE_SWITCH_KEY XK_1
#define KILL_WINDOW_KEY XK_q
#define MOVE_LEFT_KEY XK_Left
#define MOVE_RIGHT_KEY XK_Right
#define RESIZE_DECREASE_KEY XK_Left
#define RESIZE_INCREASE_KEY XK_Right
#define SHIFT_MOD ShiftMask

#define CLAMP(V, MIN, MAX) ((V) < (MIN) ? (MIN) : (V) > (MAX) ? (MAX) : (V))

typedef struct { Window window; int x, y, w, h; int is_floating; } Client;
typedef struct { Client *clients; int client_count; float main_window_ratio; int is_initialized; } Workspace;

Display *dpy;
Window root;
int screen, current_workspace = 0;
Workspace *workspaces;
int last_moved_window_index = 0;
int should_warp_pointer = 0;

// ewmh atoms
Atom net_supported, net_client_list, net_number_of_desktops, net_current_desktop, net_active_window;

void arrange(void);
void cleanup(void);
void init_ewmh(void);
void update_client_list(void);
void update_net_current_desktop(void);
void update_net_active_window(Window w);
void init_workspace(int workspace);
void warp_pointer_to_window(Window w);
void move_floating_window(XButtonEvent *ev);

int xerror(Display *dpy, XErrorEvent *ee) {
    (void)dpy;
    if (ee->error_code == BadWindow) return 0;
    fprintf(stderr, "rude: X error: request code=%d, error code=%d\n", ee->request_code, ee->error_code);
    return 0;
}

void init_workspace(int workspace) {
    if (workspace < 0 || workspace >= MAX_WORKSPACES) return;
    if (workspaces[workspace].is_initialized) return;

    workspaces[workspace].clients = calloc(MAX_CLIENTS, sizeof(Client));
    workspaces[workspace].client_count = 0;
    workspaces[workspace].main_window_ratio = 1.0;
    workspaces[workspace].is_initialized = 1;
}

void manage_window(Window w, int workspace, int is_new) {
    if (workspace < 0 || workspace >= MAX_WORKSPACES) return;
    init_workspace(workspace);
    
    Workspace *ws = &workspaces[workspace];
    if (ws->client_count >= MAX_CLIENTS) return;
    
    int slot = ws->client_count;
    ws->clients[slot].window = w;
    ws->clients[slot].is_floating = 0;  // initialize as non-floating

    // check if the window should be floating
    XWindowAttributes wa;
    if (XGetWindowAttributes(dpy, w, &wa)) {
        // check for override_redirect flag (typically used for tooltips and popups)
        if (wa.override_redirect) {
            ws->clients[slot].is_floating = 1;
        } else {
            // check window type
            Atom actual_type;
            int actual_format;
            unsigned long nitems, bytes_after;
            unsigned char *data = NULL;
            Atom wm_window_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
            
            if (XGetWindowProperty(dpy, w, wm_window_type, 0, 1, False, XA_ATOM, &actual_type, &actual_format, &nitems, &bytes_after, &data) == Success && data) {
                Atom window_type = *(Atom*)data;
                Atom dialog_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
                Atom utility_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_UTILITY", False);
                
                if (window_type == dialog_type || window_type == utility_type) {
                    ws->clients[slot].is_floating = 1;
                }
                XFree(data);
            }
        }
    }

    if (is_new) {
        ws->client_count++;
        XSelectInput(dpy, w, EnterWindowMask | FocusChangeMask | StructureNotifyMask);
        if (!ws->clients[slot].is_floating) {
            XMoveResizeWindow(dpy, w, -1, -1, 1, 1);
        } else {
            // for floating windows, keep their original size and center them
            XMoveWindow(dpy, w, (DisplayWidth(dpy, screen) - wa.width) / 2, 
                               (DisplayHeight(dpy, screen) - wa.height) / 2);
        }
        if (ws->client_count == 2) {
            ws->main_window_ratio = 0.5;
        }
        should_warp_pointer = 1;
    }
    update_client_list();
}

void unmanage_window(Window w, int workspace) {
    if (workspace < 0 || workspace >= MAX_WORKSPACES) return;
    if (!workspaces[workspace].is_initialized) return;

    Workspace *ws = &workspaces[workspace];
    for (int i = 0; i < ws->client_count; i++) {
        if (ws->clients[i].window == w) {
            for (int j = i; j < ws->client_count - 1; j++) {
                ws->clients[j] = ws->clients[j + 1];
            }
            ws->client_count--;
            update_client_list();
            if (ws->client_count <= 1) {
                ws->main_window_ratio = 1.0;
            }
            return;
        }
    }
}

void focus_window(Window w) {
    if (w != None && w != root) {
        XWindowChanges wc = {.stack_mode = Above};
        XConfigureWindow(dpy, w, CWStackMode, &wc);
        XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);
        update_net_active_window(w);
        if (should_warp_pointer) {
            warp_pointer_to_window(w);
            should_warp_pointer = 0;
        }
    }
}

void warp_pointer_to_window(Window w) {
    if (w == None || w == root) return;

    Window dummy_root;
    int win_x, win_y;
    unsigned int win_width, win_height, dummy_border, dummy_depth;

    if (XGetGeometry(dpy, w, &dummy_root, &win_x, &win_y, &win_width, &win_height, &dummy_border, &dummy_depth)) {
        int center_x = win_x + (win_width / 2);
        int center_y = win_y + (win_height / 2);
        XWarpPointer(dpy, None, root, 0, 0, 0, 0, center_x, center_y);
    }
}

void tile(int screen_w, int screen_h) {
    Workspace *ws = &workspaces[current_workspace];
    int n = 0;
    for (int i = 0; i < ws->client_count; i++) {
        if (!ws->clients[i].is_floating) n++;
    }
    if (n == 0) return;

    XWindowChanges wc;
    unsigned int value_mask = CWX | CWY | CWWidth | CWHeight;

    if (n == 1 || ws->main_window_ratio > 0.99) {
        for (int i = 0; i < ws->client_count; i++) {
            if (!ws->clients[i].is_floating) {
                Client *c = &ws->clients[i];
                c->x = GAP_SIZE;
                c->y = GAP_SIZE;
                c->w = screen_w - 2 * GAP_SIZE;
                c->h = screen_h - 2 * GAP_SIZE;
                wc.x = c->x; wc.y = c->y; wc.width = c->w; wc.height = c->h;
                XConfigureWindow(dpy, c->window, value_mask, &wc);
                break;
            }
        }
        return;
    }

    int main_w = (int)((screen_w - 3 * GAP_SIZE) * ws->main_window_ratio);
    int tiled_index = 0;

    for (int i = 0; i < ws->client_count; i++) {
        Client *c = &ws->clients[i];
        if (c->is_floating) continue;

        if (tiled_index == 0) {
            c->x = GAP_SIZE;
            c->y = GAP_SIZE;
            c->w = main_w;
            c->h = screen_h - 2 * GAP_SIZE;
        } else {
            int slave_h = (screen_h - (n * GAP_SIZE)) / (n - 1);
            c->x = main_w + 2 * GAP_SIZE;
            c->y = GAP_SIZE + (tiled_index - 1) * (slave_h + GAP_SIZE);
            c->w = screen_w - main_w - 3 * GAP_SIZE;
            c->h = slave_h;
        }
        wc.x = c->x; wc.y = c->y; wc.width = c->w; wc.height = c->h;
        XConfigureWindow(dpy, c->window, value_mask, &wc);
        tiled_index++;
    }
}

void arrange(void) {
    int screen_w = DisplayWidth(dpy, screen);
    int screen_h = DisplayHeight(dpy, screen);
    tile(screen_w, screen_h);
}

void switch_workspace(int new_workspace) {
    if (new_workspace == current_workspace || new_workspace < 0 || new_workspace >= MAX_WORKSPACES) return;
    
    // unmap windows in the current workspace
    if (workspaces[current_workspace].is_initialized) {
        for (int i = 0; i < workspaces[current_workspace].client_count; i++) {
            XUnmapWindow(dpy, workspaces[current_workspace].clients[i].window);
        }
    }

    // initialize the new workspace if it hasn't been used yet
    init_workspace(new_workspace);

    // map windows in the new workspace
    for (int i = 0; i < workspaces[new_workspace].client_count; i++) {
        XMapWindow(dpy, workspaces[new_workspace].clients[i].window);
    }

    current_workspace = new_workspace;
    update_net_current_desktop();
    arrange();
    if (workspaces[current_workspace].client_count > 0) {
        should_warp_pointer = 1;
        focus_window(workspaces[current_workspace].clients[0].window);
    }
}

void kill_focused_window(void) {
    Window focused;
    int revert_to;
    XGetInputFocus(dpy, &focused, &revert_to);
    if (focused == None || focused == root) return;

    Atom wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
    Atom wm_delete_window = XInternAtom(dpy, "WM_DELETE_WINDOW", False);

    if (wm_protocols != None && wm_delete_window != None) {
        XEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = ClientMessage;
        ev.xclient.window = focused;
        ev.xclient.message_type = wm_protocols;
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = wm_delete_window;
        ev.xclient.data.l[1] = CurrentTime;
        XSendEvent(dpy, focused, False, NoEventMask, &ev);
    } else {
        XKillClient(dpy, focused);
    }
}

void move_window(int direction) {
    Workspace *ws = &workspaces[current_workspace];
    int n = ws->client_count;
    if (n <= 1) return;

    int new_index = (last_moved_window_index + direction + n) % n;
    Client temp = ws->clients[last_moved_window_index];
    ws->clients[last_moved_window_index] = ws->clients[new_index];
    ws->clients[new_index] = temp;

    last_moved_window_index = new_index;
    arrange();
    focus_window(ws->clients[new_index].window);
}

void resize_main_window(int direction) {
    Workspace *ws = &workspaces[current_workspace];
    float resize_step = 0.05;
    if (ws->main_window_ratio > 0.99 && direction < 0) {
        ws->main_window_ratio = 0.6;
    } else {
        ws->main_window_ratio = CLAMP(ws->main_window_ratio + (direction > 0 ? resize_step : -resize_step), 0.1, 1.0);
    }
    arrange();
}

void move_floating_window(XButtonEvent *ev) {
    Window focused;
    int revert_to;
    XGetInputFocus(dpy, &focused, &revert_to);
    
    Workspace *ws = &workspaces[current_workspace];
    Client *c = NULL;
    for (int i = 0; i < ws->client_count; i++) {
        if (ws->clients[i].window == focused) {
            c = &ws->clients[i];
            break;
        }
    }
    
    if (c && c->is_floating) {
        int old_x = ev->x_root;
        int old_y = ev->y_root;
        
        XGrabPointer(dpy, root, True, 
                     PointerMotionMask | ButtonReleaseMask, 
                     GrabModeAsync, GrabModeAsync, 
                     None, None, CurrentTime);
        
        XEvent ev;
        while (1) {
            XMaskEvent(dpy, PointerMotionMask | ButtonReleaseMask, &ev);
            if (ev.type == MotionNotify) {
                int xdiff = ev.xmotion.x_root - old_x;
                int ydiff = ev.xmotion.y_root - old_y;
                c->x += xdiff;
                c->y += ydiff;
                XMoveWindow(dpy, c->window, c->x, c->y);
                old_x = ev.xmotion.x_root;
                old_y = ev.xmotion.y_root;
            } else if (ev.type == ButtonRelease) {
                break;
            }
        }
        XUngrabPointer(dpy, CurrentTime);
    }
}

void cleanup(void) {
    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    for (int i = 0; i < MAX_WORKSPACES; i++) {
        if (workspaces[i].is_initialized) {
            free(workspaces[i].clients);
        }
    }
    free(workspaces);
    XCloseDisplay(dpy);
}

void init_ewmh(void) {
    net_supported = XInternAtom(dpy, "_NET_SUPPORTED", False);
    net_client_list = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    net_number_of_desktops = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
    net_current_desktop = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
    net_active_window = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    Atom net_supporting_wm_check = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
    Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
    Atom utf8_string = XInternAtom(dpy, "UTF8_STRING", False);

    Atom supported[] = {
        net_supported,
        net_client_list,
        net_number_of_desktops,
        net_current_desktop,
        net_active_window,
        net_supporting_wm_check,
        net_wm_name
    };

    XChangeProperty(dpy, root, net_supported, XA_ATOM, 32, PropModeReplace, (unsigned char *)supported, sizeof(supported) / sizeof(Atom));

    long number_of_desktops = MAX_WORKSPACES;
    XChangeProperty(dpy, root, net_number_of_desktops, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&number_of_desktops, 1);

    Window dummy = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
    XChangeProperty(dpy, root, net_supporting_wm_check, XA_WINDOW, 32, PropModeReplace, (unsigned char *)&dummy, 1);
    XChangeProperty(dpy, dummy, net_supporting_wm_check, XA_WINDOW, 32, PropModeReplace, (unsigned char *)&dummy, 1);

    const char *wm_name = "rude";
    XChangeProperty(dpy, dummy, net_wm_name, utf8_string, 8, PropModeReplace, (unsigned char *)wm_name, strlen(wm_name));

    update_net_current_desktop();
}

void update_client_list(void) {
    Window client_list[MAX_WORKSPACES * MAX_CLIENTS];
    int total_clients = 0;
    for (int i = 0; i < MAX_WORKSPACES; i++) {
        if (workspaces[i].is_initialized) {
            for (int j = 0; j < workspaces[i].client_count; j++) {
                client_list[total_clients++] = workspaces[i].clients[j].window;
            }
        }
    }
    XChangeProperty(dpy, root, net_client_list, XA_WINDOW, 32, PropModeReplace, (unsigned char *)client_list, total_clients);
}

void update_net_current_desktop(void) {
    long desktop = current_workspace;
    XChangeProperty(dpy, root, net_current_desktop, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&desktop, 1);
}

void update_net_active_window(Window w) {
    XChangeProperty(dpy, root, net_active_window, XA_WINDOW, 32, PropModeReplace, (unsigned char *)&w, 1);
}

int main(void) {
    if (!(dpy = XOpenDisplay(NULL))) {
        fprintf(stderr, "rude: cannot open display\n");
        return 1;
    }

    XSetErrorHandler(xerror);
    atexit(cleanup);
    signal(SIGTERM, exit);
    signal(SIGINT, exit);

    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask);

    for (int i = 0; i < MAX_WORKSPACES; i++) {
        XGrabKey(dpy, XKeysymToKeycode(dpy, WORKSPACE_SWITCH_KEY + i), MOD_KEY, root, True, GrabModeAsync, GrabModeAsync);
    }

    XGrabKey(dpy, XKeysymToKeycode(dpy, KILL_WINDOW_KEY), MOD_KEY, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, MOVE_LEFT_KEY), MOD_KEY, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, MOVE_RIGHT_KEY), MOD_KEY, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Left), MOD_KEY | ShiftMask, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Right), MOD_KEY | ShiftMask, root, True, GrabModeAsync, GrabModeAsync);

    // grab left mouse button with MOD_KEY for moving floating windows
    XGrabButton(dpy, Button1, MOD_KEY, root, True, ButtonPressMask, 
                GrabModeAsync, GrabModeAsync, None, None);

    workspaces = calloc(MAX_WORKSPACES, sizeof(Workspace));
    init_workspace(0);  // initialize the first workspace

    init_ewmh();

    XEvent ev;
    while (1) {
        XNextEvent(dpy, &ev);
        switch (ev.type) {
            case MapRequest:
                manage_window(ev.xmaprequest.window, current_workspace, 1);
                arrange();
                XMapWindow(dpy, ev.xmaprequest.window);
                focus_window(ev.xmaprequest.window);
                break;
            case UnmapNotify:
            case DestroyNotify:
                unmanage_window(ev.type == UnmapNotify ? ev.xunmap.window : ev.xdestroywindow.window, current_workspace);
                arrange();
                break;
            case EnterNotify:
                should_warp_pointer = 0;
                focus_window(ev.xcrossing.window);
                break;
            case KeyPress: {
                KeySym keysym = XkbKeycodeToKeysym(dpy, ev.xkey.keycode, 0, 0);
                unsigned int modifiers = ev.xkey.state;
                
                if (keysym >= WORKSPACE_SWITCH_KEY && keysym < WORKSPACE_SWITCH_KEY + MAX_WORKSPACES) {
                    switch_workspace(keysym - WORKSPACE_SWITCH_KEY);
                } else if (keysym == KILL_WINDOW_KEY && modifiers == MOD_KEY) {
                    kill_focused_window();
                } else if ((keysym == MOVE_LEFT_KEY || keysym == MOVE_RIGHT_KEY) && modifiers == MOD_KEY) {
                    move_window(keysym == MOVE_LEFT_KEY ? -1 : 1);
                } else if (keysym == XK_Left && modifiers == (MOD_KEY | ShiftMask)) {
                    resize_main_window(-1);
                } else if (keysym == XK_Right && modifiers == (MOD_KEY | ShiftMask)) {
                    resize_main_window(1);
                }
                break;
            }
            case ButtonPress:
                if (ev.xbutton.button == Button1 && ev.xbutton.state == MOD_KEY) {
                    move_floating_window(&ev.xbutton);
                }
                break;
        }
    }

    return 0;
}
