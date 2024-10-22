#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/time.h>

#define SUPER Mod4Mask
#define MIN_WINDOW_SIZE 100
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define RESIZE_THROTTLE_MS 50

typedef struct {
    int x, y;
} Point;

typedef struct WindowInfo {
    Window window;
    int is_utility;
    struct WindowInfo *next;
} WindowInfo;

static Display *display;
static Window root;
static int screen;
static int screen_width;
static int screen_height;

static Atom net_supported;
static Atom net_wm_name;
static Atom net_active_window;
static Atom net_wm_window_type;
static Atom net_supporting_wm_check;
static Atom net_wm_pid;
static Atom wm_protocols;
static Atom wm_delete_window;
static Atom net_wm_window_type_utility;
static Atom net_wm_window_type_tooltip;

static GC xor_gc;
static WindowInfo *window_list = NULL;

static void setup_ewmh(void);
static void set_active_window(Window w);
static void handle_map_request(XMapRequestEvent *ev);
static void kill_window(Window w);
static void focus_next_window(void);
static void kill_focused_window(void);
static int x_error_handler(Display *display, XErrorEvent *e);
static void sigchld_handler(int sig);
static void initialize(void);
static void grab_keys_and_buttons(void);
static void raise_window(Window w);
static void add_window(Window w, int is_utility);
static void remove_window(Window w);
static void constrain_to_screen(int *x, int *y, int width, int height);

static void constrain_to_screen(int *x, int *y, int width, int height) {
    if (*x + width > screen_width) {
        *x = screen_width - width;
    }
    if (*x < 0) {
        *x = 0;
    }
    if (*y + height > screen_height) {
        *y = screen_height - height;
    }
    if (*y < 0) {
        *y = 0;
    }
}

static void initialize(void) {
    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }
    XSetErrorHandler(x_error_handler);
    screen = DefaultScreen(display);
    root = RootWindow(display, screen);
    screen_width = DisplayWidth(display, screen);
    screen_height = DisplayHeight(display, screen);
    XGCValues values = {
        .function = GXxor,
        .foreground = WhitePixel(display, screen) ^ BlackPixel(display, screen),
        .background = 0,
        .subwindow_mode = IncludeInferiors
    };
    xor_gc = XCreateGC(display, root, GCFunction | GCForeground | GCBackground | GCSubwindowMode, &values);
    setup_ewmh();
    XSelectInput(display, root, SubstructureRedirectMask | SubstructureNotifyMask | KeyPressMask);
}

static void grab_keys_and_buttons(void) {
    unsigned int modifiers[] = {SUPER, SUPER|LockMask, SUPER|Mod2Mask, SUPER|LockMask|Mod2Mask};
    
    for (size_t i = 0; i < sizeof(modifiers)/sizeof(modifiers[0]); i++) {
        XGrabButton(display, AnyButton, modifiers[i], root, True,
                    ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                    GrabModeAsync, GrabModeAsync, None, None);
        XGrabKey(display, XKeysymToKeycode(display, XK_q), modifiers[i], 
                 root, True, GrabModeAsync, GrabModeAsync);
    }
}

static void setup_ewmh(void) {
    net_supported = XInternAtom(display, "_NET_SUPPORTED", False);
    net_wm_name = XInternAtom(display, "_NET_WM_NAME", False);
    net_active_window = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
    net_wm_window_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    net_supporting_wm_check = XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", False);
    net_wm_pid = XInternAtom(display, "_NET_WM_PID", False);
    wm_protocols = XInternAtom(display, "WM_PROTOCOLS", False);
    wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
    net_wm_window_type_utility = XInternAtom(display, "_NET_WM_WINDOW_TYPE_UTILITY", False);
    net_wm_window_type_tooltip = XInternAtom(display, "_NET_WM_WINDOW_TYPE_TOOLTIP", False);
    Atom atoms[] = {
        net_supported,
        net_wm_name,
        net_active_window,
        net_wm_window_type,
        net_supporting_wm_check,
        net_wm_pid
    };
    XChangeProperty(display, root, net_supported, XA_ATOM, 32, PropModeReplace, (unsigned char *)atoms, sizeof(atoms)/sizeof(Atom));
    Window check_window = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, 0, 0);
    XChangeProperty(display, root, net_supporting_wm_check, XA_WINDOW, 32, PropModeReplace, (unsigned char *)&check_window, 1);
    XChangeProperty(display, check_window, net_supporting_wm_check, XA_WINDOW, 32, PropModeReplace, (unsigned char *)&check_window, 1);
    XChangeProperty(display, check_window, net_wm_name, XInternAtom(display, "UTF8_STRING", False), 8, PropModeReplace, (unsigned char *)"rude", strlen("rude"));
    pid_t pid = getpid();
    XChangeProperty(display, check_window, net_wm_pid, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&pid, 1);
}

static void add_window(Window w, int is_utility) {
    WindowInfo *win_info = malloc(sizeof(WindowInfo));
    if (!win_info) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    win_info->window = w;
    win_info->is_utility = is_utility;
    win_info->next = window_list;
    window_list = win_info;
}

static void remove_window(Window w) {
    WindowInfo **prev = &window_list;
    WindowInfo *curr = window_list;
    while (curr) {
        if (curr->window == w) {
            *prev = curr->next;
            free(curr);
            return;
        }
        prev = &curr->next;
        curr = curr->next;
    }
}

static void set_active_window(Window w) {
    XChangeProperty(display, root, net_active_window, XA_WINDOW, 32, PropModeReplace, (unsigned char *)&w, 1);
}

static void handle_map_request(XMapRequestEvent *ev) {
    XWindowAttributes attr;
    XGetWindowAttributes(display, ev->window, &attr);
    XSizeHints hints;
    long supplied;
    if (!XGetWMNormalHints(display, ev->window, &hints, &supplied)) {
        hints.flags = 0;
    }
    int width = (hints.flags & PSize) ? hints.width : MAX(attr.width, MIN_WINDOW_SIZE);
    int height = (hints.flags & PSize) ? hints.height : MAX(attr.height, MIN_WINDOW_SIZE);
    if (hints.flags & PMinSize) {
        width = MAX(width, hints.min_width);
        height = MAX(height, hints.min_height);
    }
    int x = (screen_width - width) / 2;
    int y = (screen_height - height) / 2;
    constrain_to_screen(&x, &y, width, height);
    XMoveResizeWindow(display, ev->window, x, y, width, height);
    XSetWindowBorderWidth(display, ev->window, 1);
    XSelectInput(display, ev->window, EnterWindowMask | StructureNotifyMask | SubstructureNotifyMask);
    XMapWindow(display, ev->window);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    Atom *wm_window_type = NULL;
    int is_utility = 0;
    if (XGetWindowProperty(display, ev->window, net_wm_window_type, 0, 1, False, XA_ATOM,
                           &actual_type, &actual_format, &nitems, &bytes_after,
                           (unsigned char **)&wm_window_type) == Success && wm_window_type != NULL) {
        if (*wm_window_type == net_wm_window_type_utility ||
            *wm_window_type == net_wm_window_type_tooltip) {
            is_utility = 1;
        }
        XFree(wm_window_type);
    }
    add_window(ev->window, is_utility);
    raise_window(ev->window);
    XSetInputFocus(display, ev->window, RevertToPointerRoot, CurrentTime);
    set_active_window(ev->window);
}

static void raise_window(Window w) {
    WindowInfo *win_info;
    int is_utility = 0;
    for (win_info = window_list; win_info != NULL; win_info = win_info->next) {
        if (win_info->window == w) {
            is_utility = win_info->is_utility;
            break;
        }
    }
    if (is_utility) {
        XRaiseWindow(display, w);
    } else {
        Window root_return, parent_return, *children = NULL;
        unsigned int nchildren = 0;
        if (XQueryTree(display, root, &root_return, &parent_return, &children, &nchildren)) {
            Window sibling = None;
            for (int i = nchildren - 1; i >= 0; i--) {
                for (win_info = window_list; win_info != NULL; win_info = win_info->next) {
                    if (win_info->window == children[i] && win_info->is_utility) {
                        sibling = children[i];
                        break;
                    }
                }
                if (sibling != None) {
                    break;
                }
            }
            XWindowChanges changes;
            changes.stack_mode = Below;
            if (sibling != None) {
                changes.sibling = sibling;
                XConfigureWindow(display, w, CWSibling | CWStackMode, &changes);
            } else {
                XRaiseWindow(display, w);
            }
            XFree(children);
        } else {
            XRaiseWindow(display, w);
        }
    }
}

static void kill_window(Window w) {
    Atom *protocols = NULL;
    int n = 0;
    if (XGetWMProtocols(display, w, &protocols, &n)) {
        for (int i = 0; i < n; i++) {
            if (protocols[i] == wm_delete_window) {
                XEvent ev = { .type = ClientMessage };
                ev.xclient.window = w;
                ev.xclient.message_type = wm_protocols;
                ev.xclient.format = 32;
                ev.xclient.data.l[0] = wm_delete_window;
                ev.xclient.data.l[1] = CurrentTime;
                XSendEvent(display, w, False, NoEventMask, &ev);
                XFlush(display);
                XFree(protocols);
                return;
            }
        }
        XFree(protocols);
    }
    XKillClient(display, w);
}

static void focus_next_window(void) {
    Window root_return, parent_return, *children = NULL;
    unsigned int nchildren = 0;
    if (XQueryTree(display, root, &root_return, &parent_return, &children, &nchildren)) {
        for (int i = nchildren - 1; i >= 0; i--) {
            XWindowAttributes attr;
            if (XGetWindowAttributes(display, children[i], &attr) && attr.map_state == IsViewable) {
                XSetInputFocus(display, children[i], RevertToPointerRoot, CurrentTime);
                raise_window(children[i]);
                set_active_window(children[i]);
                break;
            }
        }
        if (children) XFree(children);
    }
}

static void kill_focused_window(void) {
    Window focused;
    int revert_to;
    XGetInputFocus(display, &focused, &revert_to);
    if (focused != None && focused != root) {
        kill_window(focused);
    }
    focus_next_window();
}

static int x_error_handler(Display *display, XErrorEvent *e) {
    char error_text[1024];
    XGetErrorText(display, e->error_code, error_text, sizeof(error_text));
    fprintf(stderr, "X Error: %s\n", error_text);
    return 0;
}

static void sigchld_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main(void) {
    XEvent ev;
    Point start = {0, 0};
    Window drag_window = None;
    int dragging = 0;
    int resizing = 0;
    struct timeval last_resize_time = {0};
    int last_rect_x = 0, last_rect_y = 0, last_rect_width = 0, last_rect_height = 0;

    signal(SIGCHLD, sigchld_handler);
    initialize();
    grab_keys_and_buttons();

    while (1) {
        XNextEvent(display, &ev);
        if (XFilterEvent(&ev, None))
            continue;

        switch (ev.type) {
            case MapRequest:
                handle_map_request(&ev.xmaprequest);
                break;

            case EnterNotify:
                if (ev.xcrossing.mode == NotifyNormal) {
                    XSetInputFocus(display, ev.xcrossing.window, RevertToPointerRoot, CurrentTime);
                    raise_window(ev.xcrossing.window);
                    set_active_window(ev.xcrossing.window);
                }
                break;

            case ButtonPress:
                if (ev.xbutton.subwindow != None && (ev.xbutton.state & SUPER)) {
                    XWindowAttributes attr;
                    XGetWindowAttributes(display, ev.xbutton.subwindow, &attr);
                    start.x = ev.xbutton.x_root - attr.x;
                    start.y = ev.xbutton.y_root - attr.y;
                    drag_window = ev.xbutton.subwindow;

                    if (ev.xbutton.button == Button1) {
                        dragging = 1;
                    } else if (ev.xbutton.button == Button3) {
                        resizing = 1;
                        last_rect_width = last_rect_height = 0;
                    }
                }
                break;

            case MotionNotify:
                while (XCheckTypedEvent(display, MotionNotify, &ev));

                if (dragging) {
                    XWindowAttributes attr;
                    XGetWindowAttributes(display, drag_window, &attr);
                    
                    int x = ev.xmotion.x_root - start.x;
                    int y = ev.xmotion.y_root - start.y;
                    
                    constrain_to_screen(&x, &y, attr.width, attr.height);
                    XMoveWindow(display, drag_window, x, y);
                } else if (resizing) {
                    struct timeval now;
                    gettimeofday(&now, NULL);
                    long elapsed_ms = (now.tv_sec - last_resize_time.tv_sec) * 1000 + 
                                   (now.tv_usec - last_resize_time.tv_usec) / 1000;
                    if (elapsed_ms < RESIZE_THROTTLE_MS) {
                        break;
                    }
                    last_resize_time = now;

                    XWindowAttributes attr;
                    XGetWindowAttributes(display, drag_window, &attr);

                    int new_width = MAX(ev.xmotion.x_root - attr.x, MIN_WINDOW_SIZE);
                    int new_height = MAX(ev.xmotion.y_root - attr.y, MIN_WINDOW_SIZE);
                    
                    if (attr.x + new_width > screen_width) {
                        new_width = screen_width - attr.x;
                    }
                    if (attr.y + new_height > screen_height) {
                        new_height = screen_height - attr.y;
                    }

                    new_width = MAX(new_width, MIN_WINDOW_SIZE);
                    new_height = MAX(new_height, MIN_WINDOW_SIZE);

                    if (last_rect_width != 0 && last_rect_height != 0) {
                        XDrawRectangle(display, root, xor_gc, last_rect_x, last_rect_y,
                                     last_rect_width - 1, last_rect_height - 1);
                    }

                    last_rect_x = attr.x;
                    last_rect_y = attr.y;
                    last_rect_width = new_width;
                    last_rect_height = new_height;

                    XDrawRectangle(display, root, xor_gc, last_rect_x, last_rect_y,
                                 last_rect_width - 1, last_rect_height - 1);
                }
                XFlush(display);
                break;

            case ButtonRelease:
                if (resizing) {
                    if (last_rect_width != 0 && last_rect_height != 0) {
                        XDrawRectangle(display, root, xor_gc, last_rect_x, last_rect_y,
                                     last_rect_width - 1, last_rect_height - 1);
                    }
                    XResizeWindow(display, drag_window, last_rect_width, last_rect_height);
                    last_rect_width = last_rect_height = 0;
                }
                dragging = resizing = 0;
                XFlush(display);
                break;

            case KeyPress:
                if (ev.xkey.keycode == XKeysymToKeycode(display, XK_q) && 
                    (ev.xkey.state & SUPER)) {
                    kill_focused_window();
                }
                break;

            case DestroyNotify:
                remove_window(ev.xdestroywindow.window);
                if (ev.xdestroywindow.window == drag_window) {
                    drag_window = None;
                    dragging = resizing = 0;
                }
                focus_next_window();
                break;

            case ClientMessage:
                if (ev.xclient.message_type == wm_protocols &&
                    (Atom)ev.xclient.data.l[0] == wm_delete_window) {
                    kill_window(ev.xclient.window);
                    focus_next_window();
                }
                break;
        }
    }

    XFreeGC(display, xor_gc);
    XCloseDisplay(display);
    return 0;
}
