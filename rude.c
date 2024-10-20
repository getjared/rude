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

static void setup_ewmh(void) {
    Atom atoms[] = {
        net_supported = XInternAtom(display, "_NET_SUPPORTED", False),
        net_wm_name = XInternAtom(display, "_NET_WM_NAME", False),
        net_active_window = XInternAtom(display, "_NET_ACTIVE_WINDOW", False),
        net_wm_window_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False),
        net_supporting_wm_check = XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", False),
        net_wm_pid = XInternAtom(display, "_NET_WM_PID", False)
    };
    wm_protocols = XInternAtom(display, "WM_PROTOCOLS", False);
    wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);

    XChangeProperty(display, root, atoms[0], XA_ATOM, 32, PropModeReplace, (unsigned char *)atoms, sizeof(atoms)/sizeof(Atom));

    Window check_window = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, 0, 0);
    XChangeProperty(display, root, net_supporting_wm_check, XA_WINDOW, 32, PropModeReplace, (unsigned char *)&check_window, 1);
    XChangeProperty(display, check_window, net_supporting_wm_check, XA_WINDOW, 32, PropModeReplace, (unsigned char *)&check_window, 1);
    XChangeProperty(display, check_window, net_wm_name, XInternAtom(display, "UTF8_STRING", False), 8, PropModeReplace, (unsigned char *)"rude", 4);

    pid_t pid = getpid();
    XChangeProperty(display, check_window, net_wm_pid, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&pid, 1);
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

    XMoveResizeWindow(display, ev->window, x, y, width, height);
    XMapWindow(display, ev->window);
    XSetWindowBorderWidth(display, ev->window, 1);
    XSelectInput(display, ev->window, EnterWindowMask | StructureNotifyMask | SubstructureNotifyMask);
    XRaiseWindow(display, ev->window);
    XSetInputFocus(display, ev->window, RevertToPointerRoot, CurrentTime);
    set_active_window(ev->window);
}

static void kill_window(Window w) {
    Atom *protocols;
    int n;
    if (XGetWMProtocols(display, w, &protocols, &n)) {
        for (int i = 0; i < n; i++) {
            if (protocols[i] == wm_delete_window) {
                XEvent ev = {.type = ClientMessage};
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
    Window root_return, parent_return, *children;
    unsigned int nchildren;
    if (XQueryTree(display, root, &root_return, &parent_return, &children, &nchildren)) {
        for (int i = nchildren - 1; i >= 0; i--) {
            XWindowAttributes attr;
            if (XGetWindowAttributes(display, children[i], &attr) && attr.map_state == IsViewable) {
                XSetInputFocus(display, children[i], RevertToPointerRoot, CurrentTime);
                XRaiseWindow(display, children[i]);
                break;
            }
        }
        XFree(children);
    }
}

static void kill_focused_window(void) {
    Window focused;
    int revert_to;
    XGetInputFocus(display, &focused, &revert_to);
    if (focused != None && focused != root) {
        kill_window(focused);
        focus_next_window();
    }
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

int main() {
    XEvent ev;
    Point start = {0};
    Window drag_window = None;
    int dragging = 0;
    int resizing = 0;

    struct timeval last_resize_time = {0};
    static int last_rect_x = 0, last_rect_y = 0, last_rect_width = 0, last_rect_height = 0;
    static GC xor_gc;

    signal(SIGCHLD, sigchld_handler);

    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }

    XSetErrorHandler(x_error_handler);

    screen = DefaultScreen(display);
    root = RootWindow(display, screen);
    screen_width = DisplayWidth(display, screen);
    screen_height = DisplayHeight(display, screen);

    XGCValues values;
    values.function = GXxor;
    values.foreground = WhitePixel(display, screen) ^ BlackPixel(display, screen);
    values.background = 0;
    values.subwindow_mode = IncludeInferiors;
    xor_gc = XCreateGC(display, root, GCFunction | GCForeground | GCBackground | GCSubwindowMode, &values);

    setup_ewmh();

    XSelectInput(display, root, SubstructureRedirectMask | SubstructureNotifyMask | KeyPressMask);
    XGrabButton(display, AnyButton, SUPER, root, True,
                ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                GrabModeAsync, GrabModeAsync, None, None);

    XGrabKey(display, XKeysymToKeycode(display, XK_q), SUPER, root, True, GrabModeAsync, GrabModeAsync);

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
                    XRaiseWindow(display, ev.xcrossing.window);
                    XSetInputFocus(display, ev.xcrossing.window, RevertToPointerRoot, CurrentTime);
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
                    int x = ev.xmotion.x_root - start.x;
                    int y = ev.xmotion.y_root - start.y;
                    XWindowAttributes attr;
                    XGetWindowAttributes(display, drag_window, &attr);
                    XMoveResizeWindow(display, drag_window, x, y, attr.width, attr.height);
                    XFlush(display);
                } else if (resizing) {
                    struct timeval now;
                    gettimeofday(&now, NULL);
                    long elapsed_ms = (now.tv_sec - last_resize_time.tv_sec) * 1000 + (now.tv_usec - last_resize_time.tv_usec) / 1000;
                    if (elapsed_ms < RESIZE_THROTTLE_MS) {
                        break;
                    }
                    last_resize_time = now;

                    XWindowAttributes attr;
                    XGetWindowAttributes(display, drag_window, &attr);

                    int new_width = MAX(ev.xmotion.x_root - attr.x, MIN_WINDOW_SIZE);
                    int new_height = MAX(ev.xmotion.y_root - attr.y, MIN_WINDOW_SIZE);

                    if (last_rect_width != 0 && last_rect_height != 0) {
                        XDrawRectangle(display, root, xor_gc, last_rect_x, last_rect_y, last_rect_width - 1, last_rect_height - 1);
                    }

                    last_rect_x = attr.x;
                    last_rect_y = attr.y;
                    last_rect_width = new_width;
                    last_rect_height = new_height;

                    XDrawRectangle(display, root, xor_gc, last_rect_x, last_rect_y, last_rect_width - 1, last_rect_height - 1);
                    XFlush(display);
                }
                break;
            case ButtonRelease:
                if (resizing) {
                    if (last_rect_width != 0 && last_rect_height != 0) {
                        XDrawRectangle(display, root, xor_gc, last_rect_x, last_rect_y, last_rect_width - 1, last_rect_height - 1);
                        XFlush(display);
                    }
                    XMoveResizeWindow(display, drag_window, last_rect_x, last_rect_y, last_rect_width, last_rect_height);
                    last_rect_width = last_rect_height = 0;
                    XFlush(display);
                }
                dragging = resizing = 0;
                break;
            case KeyPress:
                if (ev.xkey.keycode == XKeysymToKeycode(display, XK_q) && (ev.xkey.state & SUPER)) {
                    kill_focused_window();
                }
                break;
            case DestroyNotify:
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

    XCloseDisplay(display);
    return 0;
}
