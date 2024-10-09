#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

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
    int prev_x, prev_y, prev_width, prev_height;
} WindowInfo;

static Display *dpy;
static Window root;
static XWindowAttributes attr;
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
        windows[window_count++] = (WindowInfo){win, x, y, width, height, -1, -1, -1, -1};
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

        if (x != windows[i].prev_x || y != windows[i].prev_y ||
            width != windows[i].prev_width || height != windows[i].prev_height) {
            XMoveResizeWindow(dpy, windows[i].win, x, y, width, height);
            windows[i].prev_x = x;
            windows[i].prev_y = y;
            windows[i].prev_width = width;
            windows[i].prev_height = height;
        }
    }
    XFlush(dpy);
}

static void scroll_viewport(int dx, int dy) {
    viewport_x += dx;
    viewport_y += dy;
    update_window_positions();
}

static void move(Window win) {
    if (XRaiseWindow(dpy, win) == BadWindow) return;
    if (XGrabPointer(dpy, win, True, PointerMotionMask | ButtonReleaseMask,
                     GrabModeAsync, GrabModeAsync, None, None, CurrentTime) != GrabSuccess)
        return;
    int start_x = ev.xbutton.x_root;
    int start_y = ev.xbutton.y_root;
    WindowInfo *wi = NULL;
    for (int i = 0; i < window_count; i++) {
        if (windows[i].win == win) {
            wi = &windows[i];
            break;
        }
    }
    if (!wi) {
        XUngrabPointer(dpy, CurrentTime);
        return;
    }
    while (1) {
        XEvent new_ev;
        if (XCheckMaskEvent(dpy, ButtonReleaseMask, &new_ev)) {
            break;
        }
        while (XCheckMaskEvent(dpy, PointerMotionMask, &new_ev)) {
            if (new_ev.type == MotionNotify) {
                double xdiff = (new_ev.xmotion.x_root - start_x) / zoom_level;
                double ydiff = (new_ev.xmotion.y_root - start_y) / zoom_level;
                wi->x += xdiff;
                wi->y += ydiff;
                XMoveWindow(dpy, win, (wi->x - viewport_x) * zoom_level, (wi->y - viewport_y) * zoom_level);
                start_x = new_ev.xmotion.x_root;
                start_y = new_ev.xmotion.y_root;
            }
        }
        usleep(1000);
    }
    XUngrabPointer(dpy, CurrentTime);
    XFlush(dpy);
}

static void resize(Window win) {
    if (XRaiseWindow(dpy, win) == BadWindow) return;
    if (XGrabPointer(dpy, win, True, PointerMotionMask | ButtonReleaseMask,
                     GrabModeAsync, GrabModeAsync, None, None, CurrentTime) != GrabSuccess)
        return;
    int start_x = ev.xbutton.x_root;
    int start_y = ev.xbutton.y_root;
    WindowInfo *wi = NULL;
    for (int i = 0; i < window_count; i++) {
        if (windows[i].win == win) {
            wi = &windows[i];
            break;
        }
    }
    if (!wi) {
        XUngrabPointer(dpy, CurrentTime);
        return;
    }
    int last_width = wi->width, last_height = wi->height;
    double resize_factor = 1.0 / zoom_level;
    while (1) {
        XEvent new_ev;
        if (XCheckMaskEvent(dpy, ButtonReleaseMask, &new_ev)) {
            break;
        }
        while (XCheckMaskEvent(dpy, PointerMotionMask, &new_ev)) {
            if (new_ev.type == MotionNotify) {
                double xdiff = (new_ev.xmotion.x_root - start_x) * resize_factor;
                double ydiff = (new_ev.xmotion.y_root - start_y) * resize_factor;
                int new_width = MAX(MIN_WINDOW_WIDTH, wi->width + xdiff);
                int new_height = MAX(MIN_WINDOW_HEIGHT, wi->height + ydiff);
                if (abs(new_width - last_width) > 1 || abs(new_height - last_height) > 1) {
                    wi->width = new_width;
                    wi->height = new_height;
                    XResizeWindow(dpy, win, new_width * zoom_level, new_height * zoom_level);
                    last_width = new_width;
                    last_height = new_height;
                }
                start_x = new_ev.xmotion.x_root;
                start_y = new_ev.xmotion.y_root;
            }
        }
        usleep(1000);
    }
    XUngrabPointer(dpy, CurrentTime);
    XFlush(dpy);
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
    XFlush(dpy);
}

static void toggle_zoom() {
    if (is_zoomed) {
        zoom_level = 1.0;
        viewport_x = original_viewport_x;
        viewport_y = original_viewport_y;
    } else {
        original_viewport_x = viewport_x;
        original_viewport_y = viewport_y;
        if (window_count == 0) return;
        int min_x = windows[0].x, max_x = windows[0].x + windows[0].width;
        int min_y = windows[0].y, max_y = windows[0].y + windows[0].height;
        for (int i = 1; i < window_count; i++) {
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
            Atom wmDeleteMessage = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
            XEvent msg;
            memset(&msg, 0, sizeof(msg));
            msg.xclient.type = ClientMessage;
            msg.xclient.message_type = XInternAtom(dpy, "WM_PROTOCOLS", True);
            msg.xclient.window = ev->subwindow;
            msg.xclient.format = 32;
            msg.xclient.data.l[0] = wmDeleteMessage;
            msg.xclient.data.l[1] = CurrentTime;
            XSendEvent(dpy, ev->subwindow, False, NoEventMask, &msg);
        } else if (ks == XK_space) {
            toggle_zoom();
        } else if (ev->state & ShiftMask) {
            if (ks == XK_Left) scroll_viewport(-SCROLL_STEP / zoom_level, 0);
            else if (ks == XK_Right) scroll_viewport(SCROLL_STEP / zoom_level, 0);
            else if (ks == XK_Up) scroll_viewport(0, -SCROLL_STEP / zoom_level);
            else if (ks == XK_Down) scroll_viewport(0, SCROLL_STEP / zoom_level);
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
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Up), Mod4Mask | ShiftMask, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Down), Mod4Mask | ShiftMask, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_space), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
    GC gc = XCreateGC(dpy, root, 0, NULL);
    XSetForeground(dpy, gc, BlackPixel(dpy, screen));

    printf("rude: oh, it worked\n");

    for (;;) {
        while (XPending(dpy)) {
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
        usleep(1000);
    }
    XFreeGC(dpy, gc);
    XCloseDisplay(dpy);
    return 0;
}
