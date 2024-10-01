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

typedef struct { Window window; int x, y, w, h; } Client;

Display *dpy;
Window root;
int screen, current_workspace = 0;
Client clients[MAX_WORKSPACES][MAX_CLIENTS];
int client_count[MAX_WORKSPACES] = {0};
float main_window_ratio[MAX_WORKSPACES];
int last_moved_window_index = 0;

// ewmh atoms
Atom net_supported, net_client_list, net_number_of_desktops, net_current_desktop, net_active_window;

void arrange(void);
void cleanup(void);
void init_ewmh(void);
void update_client_list(void);
void update_net_current_desktop(void);
void update_net_active_window(Window w);

int xerror(Display *dpy, XErrorEvent *ee) {
    (void)dpy;
    if (ee->error_code == BadWindow) return 0;
    fprintf(stderr, "rude: X error: request code=%d, error code=%d\n", ee->request_code, ee->error_code);
    return 0;
}

void manage_window(Window w, int workspace, int is_new) {
    if (workspace < 0 || workspace >= MAX_WORKSPACES || client_count[workspace] >= MAX_CLIENTS) return;
    int slot = client_count[workspace];
    clients[workspace][slot].window = w;
    if (is_new) {
        client_count[workspace]++;
        XSelectInput(dpy, w, EnterWindowMask | FocusChangeMask | StructureNotifyMask);
        XMoveResizeWindow(dpy, w, -1, -1, 1, 1);
        if (client_count[workspace] == 2) {
            main_window_ratio[workspace] = 0.5;
        }
    }
    update_client_list();
}

void unmanage_window(Window w, int workspace) {
    if (workspace < 0 || workspace >= MAX_WORKSPACES) return;
    for (int i = 0; i < client_count[workspace]; i++) {
        if (clients[workspace][i].window == w) {
            for (int j = i; j < client_count[workspace] - 1; j++) {
                clients[workspace][j] = clients[workspace][j + 1];
            }
            client_count[workspace]--;
            update_client_list();
            if (client_count[workspace] <= 1) {
                main_window_ratio[workspace] = 1.0;
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
    }
}

void tile(int screen_w, int screen_h) {
    int n = client_count[current_workspace];
    if (n == 0) return;

    XWindowChanges wc;
    unsigned int value_mask = CWX | CWY | CWWidth | CWHeight;

    if (n == 1 || main_window_ratio[current_workspace] > 0.99) {
        Client *c = &clients[current_workspace][0];
        c->x = GAP_SIZE;
        c->y = GAP_SIZE;
        c->w = screen_w - 2 * GAP_SIZE;
        c->h = screen_h - 2 * GAP_SIZE;
        wc.x = c->x; wc.y = c->y; wc.width = c->w; wc.height = c->h;
        XConfigureWindow(dpy, c->window, value_mask, &wc);
        return;
    }

    int main_w = (int)((screen_w - 3 * GAP_SIZE) * main_window_ratio[current_workspace]);

    for (int i = 0; i < n; i++) {
        Client *c = &clients[current_workspace][i];
        if (i == 0) {
            c->x = GAP_SIZE;
            c->y = GAP_SIZE;
            c->w = main_w;
            c->h = screen_h - 2 * GAP_SIZE;
        } else {
            int slave_h = (screen_h - (n * GAP_SIZE)) / (n - 1);
            c->x = main_w + 2 * GAP_SIZE;
            c->y = GAP_SIZE + (i - 1) * (slave_h + GAP_SIZE);
            c->w = screen_w - main_w - 3 * GAP_SIZE;
            c->h = slave_h;
        }
        wc.x = c->x; wc.y = c->y; wc.width = c->w; wc.height = c->h;
        XConfigureWindow(dpy, c->window, value_mask, &wc);
    }
}

void arrange(void) {
    int screen_w = DisplayWidth(dpy, screen);
    int screen_h = DisplayHeight(dpy, screen);
    tile(screen_w, screen_h);
}

void switch_workspace(int new_workspace) {
    if (new_workspace == current_workspace || new_workspace < 0 || new_workspace >= MAX_WORKSPACES) return;
    for (int i = 0; i < client_count[current_workspace]; i++) {
        XUnmapWindow(dpy, clients[current_workspace][i].window);
    }
    for (int i = 0; i < client_count[new_workspace]; i++) {
        XMapWindow(dpy, clients[new_workspace][i].window);
    }
    current_workspace = new_workspace;
    update_net_current_desktop();
    arrange();
    if (client_count[current_workspace] > 0) {
        focus_window(clients[current_workspace][0].window);
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
    int n = client_count[current_workspace];
    if (n <= 1) return;

    int new_index = (last_moved_window_index + direction + n) % n;
    Client temp = clients[current_workspace][last_moved_window_index];
    clients[current_workspace][last_moved_window_index] = clients[current_workspace][new_index];
    clients[current_workspace][new_index] = temp;

    last_moved_window_index = new_index;
    arrange();
    focus_window(clients[current_workspace][new_index].window);
}

void resize_main_window(int direction) {
    float resize_step = 0.05;
    if (main_window_ratio[current_workspace] > 0.99 && direction < 0) {
        main_window_ratio[current_workspace] = 0.6;
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

    // create a dummy window to hold WM_NAME
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
        for (int j = 0; j < client_count[i]; j++) {
            client_list[total_clients++] = clients[i][j].window;
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
        main_window_ratio[i] = 1.0;
    }

    XGrabKey(dpy, XKeysymToKeycode(dpy, KILL_WINDOW_KEY), MOD_KEY, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, MOVE_LEFT_KEY), MOD_KEY, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, MOVE_RIGHT_KEY), MOD_KEY, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Left), MOD_KEY | ShiftMask, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Right), MOD_KEY | ShiftMask, root, True, GrabModeAsync, GrabModeAsync);

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
        }
    }

    return 0;
}
