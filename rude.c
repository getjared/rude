#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define LENGTH(X) (sizeof(X) / sizeof(*X))

typedef struct { 
    Window win;
    int is_floating;
    int x, y, w, h;
} Client;

static Display *dpy;
static Window root;
static Client clients[16];
static unsigned int nc = 0, focus = 0;
static const unsigned int borderpx = 1, gappx = 5;
static const float master_size = 0.55;

static void focus_client(int i) {
    if (i >= 0 && i < (int)nc) {
        XSetInputFocus(dpy, clients[i].win, RevertToPointerRoot, CurrentTime);
        XSetWindowBorder(dpy, clients[i].win, 0xFFFFFF);
        if (focus != (unsigned int)i) 
            XSetWindowBorder(dpy, clients[focus].win, 0x000000);
        focus = i;
        XRaiseWindow(dpy, clients[focus].win);
    }
}

static void tile(void) {
    int sw = DisplayWidth(dpy, DefaultScreen(dpy));
    int sh = DisplayHeight(dpy, DefaultScreen(dpy));
    int mw = (sw * master_size) - gappx;
    int mx = gappx, my = gappx, sx = mw + 2*gappx, sy = gappx;
    int mh = sh - 2*gappx, sh_adj;
    unsigned int i, n = 0;

    for (i = 0; i < nc; i++)
        if (!clients[i].is_floating)
            n++;

    if (n == 1) {
        for (i = 0; i < nc; i++)
            if (!clients[i].is_floating) {
                XMoveResizeWindow(dpy, clients[i].win, 0, 0, sw, sh);
                XSetWindowBorderWidth(dpy, clients[i].win, 0);
                break;
            }
    } else if (n > 1) {
        int j = 0;
        for (i = 0; i < nc; i++) {
            if (clients[i].is_floating) continue;
            if (j == 0) {
                XMoveResizeWindow(dpy, clients[i].win, mx, my, mw - 2*borderpx, mh - 2*borderpx);
            } else {
                sh_adj = (sh - (n * gappx)) / (n - 1);
                XMoveResizeWindow(dpy, clients[i].win, sx, sy, 
                                  sw - sx - gappx - 2*borderpx, 
                                  sh_adj - 2*borderpx);
                sy += sh_adj + gappx;
            }
            XSetWindowBorderWidth(dpy, clients[i].win, borderpx);
            j++;
        }
    }

    for (i = 0; i < nc; i++) {
        if (clients[i].is_floating) {
            XMoveResizeWindow(dpy, clients[i].win, 
                              clients[i].x, clients[i].y, 
                              clients[i].w, clients[i].h);
            XSetWindowBorderWidth(dpy, clients[i].win, borderpx);
        }
    }

    XRaiseWindow(dpy, clients[focus].win);
}

static void toggle_float(void) {
    if (focus < nc) {
        clients[focus].is_floating = !clients[focus].is_floating;
        if (clients[focus].is_floating) {
            Window returned_root;
            int x, y;
            unsigned int width, height, border_width, depth;
            XGetGeometry(dpy, clients[focus].win, &returned_root, &x, &y,
                         &width, &height, &border_width, &depth);
            
            int sw = DisplayWidth(dpy, DefaultScreen(dpy));
            int sh = DisplayHeight(dpy, DefaultScreen(dpy));
            
            width = sw * 2 / 3;
            height = sh * 2 / 3;
            x = (sw - width) / 2;
            y = (sh - height) / 2;
            
            clients[focus].x = x;
            clients[focus].y = y;
            clients[focus].w = width;
            clients[focus].h = height;
        }
        tile();
    }
}

static void move_resize(XEvent *ev) {
    if (focus >= nc || !clients[focus].is_floating) return;

    XButtonEvent start = ev->xbutton;
    XWindowAttributes wa;
    XGetWindowAttributes(dpy, clients[focus].win, &wa);

    int mode = ev->xbutton.button == 1 ? 1 : 2;

    if (mode == 1) {
        int xdiff, ydiff;
        XGrabPointer(dpy, root, True, PointerMotionMask | ButtonReleaseMask,
                     GrabModeAsync, GrabModeAsync, None, XCreateFontCursor(dpy, XC_fleur), CurrentTime);
        do {
            XMaskEvent(dpy, PointerMotionMask | ButtonReleaseMask, ev);
            if (ev->type == MotionNotify) {
                xdiff = ev->xbutton.x_root - start.x_root;
                ydiff = ev->xbutton.y_root - start.y_root;
                clients[focus].x = wa.x + xdiff;
                clients[focus].y = wa.y + ydiff;
                XMoveWindow(dpy, clients[focus].win, clients[focus].x, clients[focus].y);
            }
        } while (ev->type != ButtonRelease);
        XUngrabPointer(dpy, CurrentTime);
    } else {
        int xdiff, ydiff;
        XGrabPointer(dpy, root, True, PointerMotionMask | ButtonReleaseMask,
                     GrabModeAsync, GrabModeAsync, None, XCreateFontCursor(dpy, XC_sizing), CurrentTime);
        do {
            XMaskEvent(dpy, PointerMotionMask | ButtonReleaseMask, ev);
            if (ev->type == MotionNotify) {
                xdiff = ev->xbutton.x_root - start.x_root;
                ydiff = ev->xbutton.y_root - start.y_root;
                clients[focus].w = MAX(1, wa.width + xdiff);
                clients[focus].h = MAX(1, wa.height + ydiff);
                XResizeWindow(dpy, clients[focus].win, clients[focus].w, clients[focus].h);
            }
        } while (ev->type != ButtonRelease);
        XUngrabPointer(dpy, CurrentTime);
    }
}

static void kill_client(void) {
    if (focus < nc) {
        XEvent ev = {.type = ClientMessage};
        ev.xclient.window = clients[focus].win;
        ev.xclient.message_type = XInternAtom(dpy, "WM_PROTOCOLS", False);
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
        ev.xclient.data.l[1] = CurrentTime;
        XSendEvent(dpy, clients[focus].win, False, NoEventMask, &ev);
    }
}

int main(void) {
    XEvent ev;
    XSetWindowAttributes wa = {
        .event_mask = SubstructureRedirectMask | SubstructureNotifyMask | 
                      ButtonPressMask | EnterWindowMask
    };

    if (!(dpy = XOpenDisplay(0))) return 1;
    root = DefaultRootWindow(dpy);
    XChangeWindowAttributes(dpy, root, CWEventMask, &wa);
    XSelectInput(dpy, root, wa.event_mask);

    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_q), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Tab), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Tab), Mod4Mask|ShiftMask, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_f), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
    XGrabButton(dpy, Button1, Mod4Mask, root, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(dpy, Button3, Mod4Mask, root, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);

    for (;;) {
        XNextEvent(dpy, &ev);
        if (ev.type == MapRequest) {
            if (nc < LENGTH(clients)) {
                clients[nc].win = ev.xmaprequest.window;
                clients[nc].is_floating = 0;
                XSetWindowBorderWidth(dpy, clients[nc].win, borderpx);
                XMoveResizeWindow(dpy, clients[nc].win, -1, -1, 1, 1);
                XMapWindow(dpy, clients[nc].win);
                focus_client(nc);
                nc++;
                tile();
            }
        } else if (ev.type == UnmapNotify) {
            for (unsigned int i = 0; i < nc; i++)
                if (clients[i].win == ev.xunmap.window) {
                    for (unsigned int j = i; j < nc - 1; j++)
                        clients[j] = clients[j + 1];
                    if (--nc > 0) {
                        focus = MIN(focus, nc - 1);
                        focus_client(focus);
                        tile();
                    }
                    break;
                }
        } else if (ev.type == KeyPress) {
            if (ev.xkey.keycode == XKeysymToKeycode(dpy, XK_q) && ev.xkey.state & Mod4Mask)
                kill_client();
            else if (ev.xkey.keycode == XKeysymToKeycode(dpy, XK_Tab) && ev.xkey.state & Mod4Mask)
                focus_client((ev.xkey.state & ShiftMask) ? (focus - 1 + nc) % nc : (focus + 1) % nc);
            else if (ev.xkey.keycode == XKeysymToKeycode(dpy, XK_f) && ev.xkey.state & Mod4Mask)
                toggle_float();
        } else if (ev.type == ButtonPress) {
            for (unsigned int i = 0; i < nc; i++)
                if (clients[i].win == ev.xbutton.subwindow) {
                    focus_client(i);
                    if (clients[i].is_floating && ev.xbutton.state & Mod4Mask)
                        move_resize(&ev);
                    break;
                }
        } else if (ev.type == EnterNotify) {
            for (unsigned int i = 0; i < nc; i++)
                if (clients[i].win == ev.xcrossing.window) {
                    focus_client(i);
                    break;
                }
        }
    }
}
