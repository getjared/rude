#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>

#define MAX_WORKSPACES 9
#define MAX_CLIENTS 100
#define GAP_SIZE 45
#define MOD_KEY Mod4Mask
#define WORKSPACE_SWITCH_KEY XK_1
#define KILL_WINDOW_KEY XK_q
#define MOVE_LEFT_KEY XK_Left
#define MOVE_RIGHT_KEY XK_Right
#define CHANGE_LAYOUT_KEY XK_space
#define RESIZE_DECREASE_KEY XK_Left
#define RESIZE_INCREASE_KEY XK_Right

#define SHIFT_MOD ShiftMask

#define LENGTH(X) (sizeof(X) / sizeof(*X))
#define CLAMP(V, MIN, MAX) ((V) < (MIN) ? (MIN) : (V) > (MAX) ? (MAX) : (V))

typedef struct { Window window; int x, y, w, h; } Client;
typedef struct { const char *name; void (*arrange)(int, int); } Layout;

Display *dpy;
Window root;
int screen, current_workspace = 0;
Client clients[MAX_WORKSPACES][MAX_CLIENTS];
int client_count[MAX_WORKSPACES] = {0};
float main_window_ratio[MAX_WORKSPACES];
int last_moved_window_index = 0;  

// EWMH atoms
Atom net_supported, net_client_list, net_number_of_desktops, net_current_desktop, net_active_window;

void tile(int screen_w, int screen_h);
void cleanup(void);
void init_ewmh(void);
void update_client_list(void);
void update_net_current_desktop(void);
void update_net_active_window(Window w);

static const Layout layouts[] = {
    {"tile", tile},
};
static int current_layout = 0;

int xerror(Display *dpy, XErrorEvent *ee) {
    (void)dpy;
    if (ee->error_code == BadWindow) return 0;
    fprintf(stderr, "rude: X error: request code=%d, error code=%d\n", ee->request_code, ee->error_code);
    return 0;
}

void manage_window(Window w, int workspace, int is_new) {
    if (workspace < 0 || workspace >= MAX_WORKSPACES || client_count[workspace] >= MAX_CLIENTS) return;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[workspace][i].window == None) {
            clients[workspace][i].window = w;
            if (is_new) {
                client_count[workspace]++;
                XSelectInput(dpy, w, EnterWindowMask | FocusChangeMask | StructureNotifyMask);
                
                // Set initial size and position off-screen
                XMoveResizeWindow(dpy, w, -1, -1, 1, 1);

                // If this is the second window, adjust main_window_ratio
                if (client_count[workspace] == 2) {
                    main_window_ratio[workspace] = 0.5;
                }
            }
            update_client_list();
            return;
        }
    }
}

void unmanage_window(Window w, int workspace) {
    if (workspace < 0 || workspace >= MAX_WORKSPACES) return;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[workspace][i].window == w) {
            // Remove the window
            clients[workspace][i].window = None;
            
            // Shift all subsequent windows
            for (int j = i; j < MAX_CLIENTS - 1; j++) {
                clients[workspace][j] = clients[workspace][j + 1];
            }
            clients[workspace][MAX_CLIENTS - 1].window = None;
            
            client_count[workspace]--;
            update_client_list();
            
            // Reset main_window_ratio to full-screen if all windows are closed
            if (client_count[workspace] == 0) {
                main_window_ratio[workspace] = 1.0; // Reset to full-screen
            } else if (client_count[workspace] == 1) {
                // If only one window remains, make it full-screen
                main_window_ratio[workspace] = 1.0;
            }
            
            return;
        }
    }
}

void focus_window(Window w) {
    if (w != None && w != root) {
        XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);
        XRaiseWindow(dpy, w);
        update_net_active_window(w);
    }
}

void tile(int screen_w, int screen_h) {
    int n = client_count[current_workspace];
    if (n == 0) return;

    XWindowChanges wc;
    unsigned int value_mask = CWX | CWY | CWWidth | CWHeight;

    // If there's only one window or main_window_ratio is very close to 1, make it full-screen
    if (n == 1 || main_window_ratio[current_workspace] > 0.99) {
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[current_workspace][i].window != None) {
                Client *c = &clients[current_workspace][i];
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

    // If there are multiple windows, use the tiling layout
    int main_w = (int)((screen_w - 3 * GAP_SIZE) * main_window_ratio[current_workspace]);

    for (int i = 0, active = 0; i < MAX_CLIENTS && active < n; i++) {
        if (clients[current_workspace][i].window != None) {
            Client *c = &clients[current_workspace][i];
            if (active == 0) {
                c->x = GAP_SIZE;
                c->y = GAP_SIZE;
                c->w = main_w;
                c->h = screen_h - 2 * GAP_SIZE;
            } else {
                int slave_h = (screen_h - (n * GAP_SIZE)) / (n - 1);
                c->x = main_w + 2 * GAP_SIZE;
                c->y = GAP_SIZE + (active - 1) * (slave_h + GAP_SIZE);
                c->w = screen_w - main_w - 3 * GAP_SIZE;
                c->h = slave_h;
            }
            wc.x = c->x; wc.y = c->y; wc.width = c->w; wc.height = c->h;
            XConfigureWindow(dpy, c->window, value_mask, &wc);
            active++;
        }
    }
}

void arrange(void) {
    int screen_w = DisplayWidth(dpy, screen);
    int screen_h = DisplayHeight(dpy, screen);
    
    layouts[current_layout].arrange(screen_w, screen_h);
    XSync(dpy, False);
}

void switch_workspace(int new_workspace) {
    if (new_workspace == current_workspace || new_workspace < 0 || new_workspace >= MAX_WORKSPACES) return;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[current_workspace][i].window != None) XUnmapWindow(dpy, clients[current_workspace][i].window);
        if (clients[new_workspace][i].window != None) XMapWindow(dpy, clients[new_workspace][i].window);
    }
    current_workspace = new_workspace;
    update_net_current_desktop();
    arrange();
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[current_workspace][i].window != None) {
            focus_window(clients[current_workspace][i].window);
            break;
        }
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
        XEvent ev = {0};
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
    int n = client_count[current_workspace];
    if (n <= 1) return;  // No need to move if there's only one window or none

    // Calculate the new index
    int new_index = (last_moved_window_index + direction + n) % n;

    // Find the actual array indices for the windows we're swapping
    int current_array_index = -1, new_array_index = -1;
    for (int i = 0, count = 0; i < MAX_CLIENTS; i++) {
        if (clients[current_workspace][i].window != None) {
            if (count == last_moved_window_index) current_array_index = i;
            if (count == new_index) new_array_index = i;
            count++;
            if (current_array_index != -1 && new_array_index != -1) break;
        }
    }

    if (current_array_index != -1 && new_array_index != -1) {
        // Swap the windows
        Client temp = clients[current_workspace][current_array_index];
        clients[current_workspace][current_array_index] = clients[current_workspace][new_array_index];
        clients[current_workspace][new_array_index] = temp;

        // Update the last moved window index
        last_moved_window_index = new_index;

        // Rearrange and focus the moved window
        arrange();
        focus_window(clients[current_workspace][new_array_index].window);
    }
}

void resize_main_window(int direction) {
    float resize_step = 0.05; // 5% step
    if (main_window_ratio[current_workspace] > 0.99 && direction < 0) {
        // Transitioning from full-screen to split
        main_window_ratio[current_workspace] = 0.6;  // Start with a 60/40 split when coming out of full-screen
    } else {
        main_window_ratio[current_workspace] = CLAMP(main_window_ratio[current_workspace] + (direction > 0 ? resize_step : -resize_step), 0.1, 1.0);
    }
    arrange();
}

void cleanup(void) {
    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    XCloseDisplay(dpy);
}

void init_ewmh(void) {
    net_supported = XInternAtom(dpy, "_NET_SUPPORTED", False);
    net_client_list = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    net_number_of_desktops = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
    net_current_desktop = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
    net_active_window = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);

    Atom supported[] = {
        net_supported,
        net_client_list,
        net_number_of_desktops,
        net_current_desktop,
        net_active_window
    };

    XChangeProperty(dpy, root, net_supported, XA_ATOM, 32, PropModeReplace, (unsigned char *)supported, sizeof(supported) / sizeof(Atom));

    long number_of_desktops = MAX_WORKSPACES;
    XChangeProperty(dpy, root, net_number_of_desktops, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&number_of_desktops, 1);

    update_net_current_desktop();
}

void update_client_list(void) {
    Window client_list[MAX_WORKSPACES * MAX_CLIENTS];
    int client_count = 0;

    for (int i = 0; i < MAX_WORKSPACES; i++) {
        for (int j = 0; j < MAX_CLIENTS; j++) {
            if (clients[i][j].window != None) {
                client_list[client_count++] = clients[i][j].window;
            }
        }
    }

    XChangeProperty(dpy, root, net_client_list, XA_WINDOW, 32, PropModeReplace, (unsigned char *)client_list, client_count);
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

    Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
    Atom utf8_string = XInternAtom(dpy, "UTF8_STRING", False);
    const char *wm_name = "rude";
    XChangeProperty(dpy, root, net_wm_name, utf8_string, 8, PropModeReplace, (unsigned char *)wm_name, strlen(wm_name));

    Window check_window = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
    Atom net_supporting_wm_check = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
    XChangeProperty(dpy, root, net_supporting_wm_check, XA_WINDOW, 32, PropModeReplace, (unsigned char *)&check_window, 1);
    XChangeProperty(dpy, check_window, net_supporting_wm_check, XA_WINDOW, 32, PropModeReplace, (unsigned char *)&check_window, 1);
    XChangeProperty(dpy, check_window, net_wm_name, utf8_string, 8, PropModeReplace, (unsigned char *)wm_name, strlen(wm_name));

    for (int i = 0; i < MAX_WORKSPACES; i++)
        XGrabKey(dpy, XKeysymToKeycode(dpy, WORKSPACE_SWITCH_KEY + i), MOD_KEY, root, True, GrabModeAsync, GrabModeAsync);

    XGrabKey(dpy, XKeysymToKeycode(dpy, KILL_WINDOW_KEY), MOD_KEY, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, MOVE_LEFT_KEY), MOD_KEY, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, MOVE_RIGHT_KEY), MOD_KEY, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, CHANGE_LAYOUT_KEY), MOD_KEY, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Left), MOD_KEY | ShiftMask, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Right), MOD_KEY | ShiftMask, root, True, GrabModeAsync, GrabModeAsync);

    init_ewmh();

    // Initialize main_window_ratio for each workspace
    for (int i = 0; i < MAX_WORKSPACES; i++) {
        main_window_ratio[i] = 1.0;
    }

    XEvent ev;
    while (1) {
        XNextEvent(dpy, &ev);
        if (ev.type == MapRequest) {
            manage_window(ev.xmaprequest.window, current_workspace, 1);
            arrange();
            XMapWindow(dpy, ev.xmaprequest.window);
            focus_window(ev.xmaprequest.window);
        } else if (ev.type == UnmapNotify || ev.type == DestroyNotify) {
            unmanage_window(ev.type == UnmapNotify ? ev.xunmap.window : ev.xdestroywindow.window, current_workspace);
            arrange();
        } else if (ev.type == EnterNotify) {
            focus_window(ev.xcrossing.window);
        } else if (ev.type == KeyPress) {
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
            } else if (keysym == CHANGE_LAYOUT_KEY && modifiers == MOD_KEY) {
                // This does nothing for now. .
            }
        }
    }

    return 0;
}
