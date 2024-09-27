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
#define MOVE_UP_KEY XK_Up
#define MOVE_DOWN_KEY XK_Down
#define CHANGE_LAYOUT_KEY XK_space
#define RESIZE_DECREASE_KEY XK_h
#define RESIZE_INCREASE_KEY XK_l

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

// ewmh atoms
Atom net_supported, net_client_list, net_number_of_desktops, net_current_desktop, net_active_window;

void tile(int screen_w, int screen_h);
void fibonacci(int screen_w, int screen_h);
void euler(int screen_w, int screen_h);
void cleanup(void);
void init_ewmh(void);
void update_client_list(void);
void update_net_current_desktop(void);
void update_net_active_window(Window w);

static const Layout layouts[] = {
    {"tile", tile},
    {"fibonacci", fibonacci},
    {"euler", euler},
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
                
                // set initial size and position off-screen
                XMoveResizeWindow(dpy, w, -1, -1, 1, 1);

                // if this is the second window, adjust main_window_ratio
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
            clients[workspace][i].window = None;
            client_count[workspace]--;
            update_client_list();
            
            // reset main_window_ratio to full-screen if all windows are closed
            if (client_count[workspace] == 0) {
                main_window_ratio[workspace] = 1.0; // Reset to full-screen
            } else if (client_count[workspace] == 1) {
                // if only one window remains, make it full-screen
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

    // if there's only one window or main_window_ratio is very close to 1, make it full-screen
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

    // if there are multiple windows, use the tiling layout
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

void fibonacci(int screen_w, int screen_h) {
    int n = client_count[current_workspace];
    if (n == 0) return;

    XWindowChanges wc;
    unsigned int value_mask = CWX | CWY | CWWidth | CWHeight;

    int x = GAP_SIZE, y = GAP_SIZE;
    int w = screen_w - 2 * GAP_SIZE;
    int h = screen_h - 2 * GAP_SIZE;
    int next_x = x, next_y = y, next_w = w, next_h = h;

    for (int i = 0, active = 0; i < MAX_CLIENTS && active < n; i++) {
        if (clients[current_workspace][i].window != None) {
            Client *c = &clients[current_workspace][i];

            if (active == 0) {
                c->x = x;
                c->y = y;
                c->w = (int)(w * main_window_ratio[current_workspace]);
                c->h = h;
                next_x = x + c->w + GAP_SIZE;
                next_w = w - c->w - GAP_SIZE;
            } else if (active == 1) {
                c->x = next_x;
                c->y = y;
                c->w = next_w;
                c->h = (int)(h * main_window_ratio[current_workspace]);
                next_y = y + c->h + GAP_SIZE;
                next_h = h - c->h - GAP_SIZE;
            } else {
                if (active % 2 == 0) {
                    c->x = next_x;
                    c->y = next_y;
                    c->w = (int)(next_w * main_window_ratio[current_workspace]);
                    c->h = next_h;
                    next_x = c->x + c->w + GAP_SIZE;
                    next_w = next_w - c->w - GAP_SIZE;
                } else {
                    c->x = next_x;
                    c->y = next_y;
                    c->w = next_w;
                    c->h = (int)(next_h * main_window_ratio[current_workspace]);
                    next_y = c->y + c->h + GAP_SIZE;
                    next_h = next_h - c->h - GAP_SIZE;
                }
            }

            wc.x = c->x; wc.y = c->y; wc.width = c->w; wc.height = c->h;
            XConfigureWindow(dpy, c->window, value_mask, &wc);
            active++;
        }
    }
}

void euler(int screen_w, int screen_h) {
    int n = client_count[current_workspace];
    if (n == 0) return;

    int center_x = screen_w / 2;
    int center_y = screen_h / 2;
    int central_size = (int)(fmin(screen_w, screen_h) * main_window_ratio[current_workspace]);
    int gap = GAP_SIZE;

    int initial_radius = central_size / 2 + gap * 3;

    XWindowChanges wc;
    unsigned int value_mask = CWX | CWY | CWWidth | CWHeight;

    // configure the central window
    if (n > 0 && clients[current_workspace][0].window != None) {
        wc.x = center_x - central_size / 2;
        wc.y = center_y - central_size / 2;
        wc.width = central_size;
        wc.height = central_size;
        XConfigureWindow(dpy, clients[current_workspace][0].window, value_mask, &wc);
    }

    // configure surrounding windows
    for (int i = 1; i < n && i < MAX_CLIENTS; i++) {
        if (clients[current_workspace][i].window != None) {
            int radius = initial_radius + (i - 1) * gap;
            double angle = 2 * M_PI * (i - 1) / (n - 1);
            int win_size = central_size / 2;
            int win_x = center_x + (int)(radius * cos(angle)) - win_size / 2;
            int win_y = center_y + (int)(radius * sin(angle)) - win_size / 2;

            wc.x = win_x;
            wc.y = win_y;
            wc.width = win_size;
            wc.height = win_size;
            XConfigureWindow(dpy, clients[current_workspace][i].window, value_mask, &wc);
        }
    }
}

void arrange(void) {
    int screen_w = DisplayWidth(dpy, screen);
    int screen_h = DisplayHeight(dpy, screen);
    layouts[current_layout].arrange(screen_w, screen_h);
    XSync(dpy, False);
}

void switch_layout(void) {
    current_layout = (current_layout + 1) % LENGTH(layouts);
    arrange();
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

void move_focused_window(int direction) {
    Window focused;
    int revert_to;
    XGetInputFocus(dpy, &focused, &revert_to);
    if (focused == None || focused == root) return;

    int focused_index = -1, new_index, n = client_count[current_workspace];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[current_workspace][i].window == focused) {
            focused_index = i;
            break;
        }
    }
    if (focused_index == -1) return;

    switch (direction) {
        case MOVE_LEFT_KEY: new_index = (focused_index == 0) ? n - 1 : (focused_index == 1) ? 0 : focused_index - 1; break;
        case MOVE_RIGHT_KEY: new_index = (focused_index == 0) ? 1 : (focused_index == n - 1) ? 0 : focused_index + 1; break;
        case MOVE_UP_KEY: new_index = (focused_index > 1) ? 1 : (focused_index == 1) ? 0 : focused_index; break;
        case MOVE_DOWN_KEY: new_index = (focused_index == 0) ? n - 1 : (focused_index < n - 1) ? n - 1 : focused_index; break;
        default: return;
    }

    if (new_index != focused_index) {
        Client temp = clients[current_workspace][focused_index];
        if (new_index < focused_index) {
            for (int i = focused_index; i > new_index; i--) clients[current_workspace][i] = clients[current_workspace][i-1];
        } else {
            for (int i = focused_index; i < new_index; i++) clients[current_workspace][i] = clients[current_workspace][i+1];
        }
        clients[current_workspace][new_index] = temp;
        arrange();
        focus_window(focused);
    }
}

void resize_main_window(int direction) {
    float resize_step = 0.05; // 5% step
    if (main_window_ratio[current_workspace] > 0.99 && direction < 0) {
        // transitioning from full-screen to split
        main_window_ratio[current_workspace] = 0.6;  // start with a 60/40 split when coming out of full-screen
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
    XGrabKey(dpy, XKeysymToKeycode(dpy, MOVE_UP_KEY), MOD_KEY, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, MOVE_DOWN_KEY), MOD_KEY, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, CHANGE_LAYOUT_KEY), MOD_KEY, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, RESIZE_DECREASE_KEY), MOD_KEY, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, RESIZE_INCREASE_KEY), MOD_KEY, root, True, GrabModeAsync, GrabModeAsync);

    init_ewmh();

    // initialize main_window_ratio for each workspace
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
            if (keysym >= WORKSPACE_SWITCH_KEY && keysym < WORKSPACE_SWITCH_KEY + MAX_WORKSPACES) {
                switch_workspace(keysym - WORKSPACE_SWITCH_KEY);
            } else if (keysym == KILL_WINDOW_KEY) {
                kill_focused_window();
            } else if (keysym == MOVE_LEFT_KEY || keysym == MOVE_RIGHT_KEY ||
                       keysym == MOVE_UP_KEY || keysym == MOVE_DOWN_KEY) {
                move_focused_window(keysym);
            } else if (keysym == CHANGE_LAYOUT_KEY) {
                switch_layout();
            } else if (keysym == RESIZE_DECREASE_KEY) {
                resize_main_window(-1);
            } else if (keysym == RESIZE_INCREASE_KEY) {
                resize_main_window(1);
            }
        }
    }

    return 0;
}
