#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define MIN_WINDOW_WIDTH 200
#define MIN_WINDOW_HEIGHT 100
#define SCROLL_STEP 50
#define MAX_WINDOWS 100
#define ZOOM_FACTOR 0.1

typedef struct {
    Window win;
    int x, y, width, height;
} WindowInfo;

static Display *dpy;
static Window root;
static XWindowAttributes attr;
static XButtonEvent start;
static XEvent ev;
static int screen, sh, sw, viewport_x, viewport_y;
static WindowInfo windows[MAX_WINDOWS];
static int window_count;
static int is_zoomed = 0;
static double zoom_level = 1.0;
static int original_viewport_x, original_viewport_y;

static Atom net_supported, net_wm_name, net_supporting_wm_check;

static void setup_ewmh(void) {
    net_supported = XInternAtom(dpy, "_NET_SUPPORTED", False);
    net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
    net_supporting_wm_check = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);

    Window check_window = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);

    XChangeProperty(dpy, check_window, net_wm_name, XA_STRING, 8,
                    PropModeReplace, (unsigned char *) "rude", strlen("rude"));

    XChangeProperty(dpy, root, net_supporting_wm_check, XA_WINDOW, 32,
                    PropModeReplace, (unsigned char *) &check_window, 1);

    XChangeProperty(dpy, check_window, net_supporting_wm_check, XA_WINDOW, 32,
                    PropModeReplace, (unsigned char *) &check_window, 1);

    XChangeProperty(dpy, root, net_supported, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *) &net_supporting_wm_check, 1);
}

static void focus(Window w) {
    XSetInputFocus(dpy, w == None ? root : w, RevertToPointerRoot, CurrentTime);
}

static void add_window(Window win, int x, int y, int width, int height) {
    if (window_count < MAX_WINDOWS) {
        windows[window_count++] = (WindowInfo){win, x, y, width, height};
    }
}

static void remove_window(Window win) {
    for (int i = 0; i < window_count; i++) {
        if (windows[i].win == win) {
            windows[i] = windows[--window_count];
            return;
        }
    }
}

static void update_window_positions() {
    for (int i = 0; i < window_count; i++) {
        int x = (windows[i].x - viewport_x) * zoom_level;
        int y = (windows[i].y - viewport_y) * zoom_level;
        int width = windows[i].width * zoom_level;
        int height = windows[i].height * zoom_level;
        XMoveResizeWindow(dpy, windows[i].win, x, y, width, height);
    }
}

static void scroll_viewport(int dx, int dy) {
    viewport_x += dx;
    viewport_y += dy;
    update_window_positions();
}

static void move(Window win) {
    XRaiseWindow(dpy, win);
    XGrabPointer(dpy, win, True, PointerMotionMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
    start = ev.xbutton;
    WindowInfo *wi = NULL;
    for (int i = 0; i < window_count; i++) {
        if (windows[i].win == win) {
            wi = &windows[i];
            break;
        }
    }
    if (!wi) return;
    while (XMaskEvent(dpy, PointerMotionMask | ButtonReleaseMask, &ev) == 0) {
        if (ev.type == ButtonRelease) break;
        if (ev.type == MotionNotify) {
            int xdiff = (ev.xbutton.x_root - start.x_root) / zoom_level;
            int ydiff = (ev.xbutton.y_root - start.y_root) / zoom_level;
            wi->x += xdiff;
            wi->y += ydiff;
            XMoveWindow(dpy, win, (wi->x - viewport_x) * zoom_level, (wi->y - viewport_y) * zoom_level);
            start = ev.xbutton;
        }
    }
    XUngrabPointer(dpy, CurrentTime);
}

static void resize(Window win) {
    XRaiseWindow(dpy, win);
    XGrabPointer(dpy, win, True, PointerMotionMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
    XGetWindowAttributes(dpy, win, &attr);
    start = ev.xbutton;
    WindowInfo *wi = NULL;
    for (int i = 0; i < window_count; i++) {
        if (windows[i].win == win) {
            wi = &windows[i];
            break;
        }
    }
    if (!wi) return;
    int last_width = wi->width, last_height = wi->height;
    double resize_factor = 1.0 / zoom_level;
    while (XMaskEvent(dpy, PointerMotionMask | ButtonReleaseMask, &ev) == 0) {
        if (ev.type == ButtonRelease) break;
        if (ev.type == MotionNotify) {
            int xdiff = (ev.xbutton.x_root - start.x_root) * resize_factor;
            int ydiff = (ev.xbutton.y_root - start.y_root) * resize_factor;
            int new_width = MAX(MIN_WINDOW_WIDTH, wi->width + xdiff);
            int new_height = MAX(MIN_WINDOW_HEIGHT, wi->height + ydiff);
            if (abs(new_width - last_width) > 1 || abs(new_height - last_height) > 1) {
                wi->width = new_width;
                wi->height = new_height;
                XResizeWindow(dpy, win, new_width * zoom_level, new_height * zoom_level);
                XFlush(dpy);
                last_width = new_width;
                last_height = new_height;
            }
            start = ev.xbutton;
        }
    }
    XUngrabPointer(dpy, CurrentTime);
}

static void map_request(XMapRequestEvent *ev) {
    XWindowAttributes wa;
    if (!XGetWindowAttributes(dpy, ev->window, &wa) || wa.override_redirect) return;
    int width = MAX(wa.width, MIN_WINDOW_WIDTH);
    int height = MAX(wa.height, MIN_WINDOW_HEIGHT);
    int x = viewport_x + (sw - width) / 2;
    int y = viewport_y + (sh - height) / 2;
    XMoveResizeWindow(dpy, ev->window, (x - viewport_x) * zoom_level, (y - viewport_y) * zoom_level, width * zoom_level, height * zoom_level);
    XMapWindow(dpy, ev->window);
    add_window(ev->window, x, y, width, height);
    focus(ev->window);
}

static void toggle_zoom() {
    if (is_zoomed) {
        zoom_level = 1.0;
        viewport_x = original_viewport_x;
        viewport_y = original_viewport_y;
    } else {
        original_viewport_x = viewport_x;
        original_viewport_y = viewport_y;
        int min_x = viewport_x, max_x = viewport_x;
        int min_y = viewport_y, max_y = viewport_y;
        for (int i = 0; i < window_count; i++) {
            min_x = MIN(min_x, windows[i].x);
            max_x = MAX(max_x, windows[i].x + windows[i].width);
            min_y = MIN(min_y, windows[i].y);
            max_y = MAX(max_y, windows[i].y + windows[i].height);
        }
        int canvas_width = max_x - min_x;
        int canvas_height = max_y - min_y;
        zoom_level = MIN((double)sw / canvas_width, (double)sh / canvas_height) * ZOOM_FACTOR;
        viewport_x = min_x - (sw / zoom_level - canvas_width) / 2;
        viewport_y = min_y - (sh / zoom_level - canvas_height) / 2;
    }
    is_zoomed = !is_zoomed;
    update_window_positions();
}

static void key_press(XKeyEvent *ev) {
    if (ev->state & Mod4Mask) {
        KeySym ks = XkbKeycodeToKeysym(dpy, ev->keycode, 0, 0);
        if (ks == XK_q && ev->subwindow != None) {
            XKillClient(dpy, ev->subwindow);
            remove_window(ev->subwindow);
        } else if (ks == XK_space) {
            toggle_zoom();
        } else if (ev->state & ShiftMask) {
            if (ks == XK_Left) scroll_viewport(-SCROLL_STEP / zoom_level, 0);
            else if (ks == XK_Right) scroll_viewport(SCROLL_STEP / zoom_level, 0);
        }
    }
}

int main(void) {
    if (!(dpy = XOpenDisplay(NULL))) return 1;
    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    sw = DisplayWidth(dpy, screen);
    sh = DisplayHeight(dpy, screen);

    setup_ewmh();

    XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask | KeyPressMask);
    XGrabButton(dpy, 1, Mod4Mask, root, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(dpy, 3, Mod4Mask, root, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_q), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Left), Mod4Mask | ShiftMask, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Right), Mod4Mask | ShiftMask, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_space), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
    GC gc = XCreateGC(dpy, root, 0, NULL);
    XSetForeground(dpy, gc, BlackPixel(dpy, screen));

    printf("rude: window manager with infinite canvas started\n");

    for (;;) {
        XNextEvent(dpy, &ev);
        switch (ev.type) {
            case MapRequest: map_request(&ev.xmaprequest); break;
            case ButtonPress:
                if (ev.xbutton.subwindow != None) {
                    focus(ev.xbutton.subwindow);
                    if (ev.xbutton.button == 1) move(ev.xbutton.subwindow);
                    else if (ev.xbutton.button == 3) resize(ev.xbutton.subwindow);
                }
                break;
            case KeyPress: key_press(&ev.xkey); break;
            case EnterNotify: focus(ev.xcrossing.window); break;
            case DestroyNotify: remove_window(ev.xdestroywindow.window); break;
            case Expose: XFillRectangle(dpy, root, gc, 0, 0, sw, sh); break;
        }
    }
    XFreeGC(dpy, gc);
    XCloseDisplay(dpy);
    return 0;
}
