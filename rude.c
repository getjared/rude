#include "rude.h"

Display *dpy;
Window root;
int screen;
Window *clients = NULL;
int nclients = 0;
int current_client = -1;
unsigned int current_desktop = 0;
Window **desktop_clients = NULL;
int *desktop_nclients = NULL;
Bool is_floating_mode = False;
XWindowAttributes wa;
XButtonEvent start;
WindowGeometry *window_geometries = NULL;

void get_strut_partial(Window w, unsigned long *strut) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned long *data = NULL;
    Atom net_wm_strut_partial = XInternAtom(dpy, "_NET_WM_STRUT_PARTIAL", False);

    if (XGetWindowProperty(dpy, w, net_wm_strut_partial, 0, 12, False, XA_CARDINAL,
                          &actual_type, &actual_format, &nitems, &bytes_after,
                          (unsigned char **)&data) == Success && data) {
        memcpy(strut, data, 12 * sizeof(unsigned long));
        XFree(data);
    }
}

int main() {
    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "rude: cannot open display\n");
        exit(1);
    }
    setup();
    run();
    XCloseDisplay(dpy);
    return 0;
}

void setup() {
    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    XSetErrorHandler(xerror);
    XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask | PointerMotionMask | EnterWindowMask);
    
    desktop_clients = calloc(NUM_DESKTOPS, sizeof(Window *));
    desktop_nclients = calloc(NUM_DESKTOPS, sizeof(int));
    if (!desktop_clients || !desktop_nclients) {
        fprintf(stderr, "rude: failed to allocate memory\n");
        exit(1);
    }

    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("q")), MODKEY, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("Tab")), MODKEY, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("space")), MODKEY, root, True, GrabModeAsync, GrabModeAsync);
    
    for (int i = 1; i <= NUM_DESKTOPS; i++) {
        char num[2] = { i + '0', '\0' };
        XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym(num)), MODKEY, root, True, GrabModeAsync, GrabModeAsync);
    }
    
    setup_ewmh();
    update_net_number_of_desktops();
    update_net_current_desktop();
}

void run() {
    XEvent ev;
    while (!XNextEvent(dpy, &ev)) {
        switch (ev.type) {
            case KeyPress:
                handle_keypress(&ev);
                break;
            case MapRequest:
                handle_maprequest(&ev);
                break;
            case UnmapNotify:
                handle_unmapnotify(&ev);
                break;
            case DestroyNotify:
                handle_destroynotify(&ev);
                break;
            case EnterNotify:
                handle_enternotify(&ev);
                break;
            case ClientMessage:
                handle_clientmessage(&ev);
                break;
            case ButtonPress:
                handle_buttonpress(&ev);
                break;
            case ButtonRelease:
                handle_buttonrelease(&ev);
                break;
            case MotionNotify:
                handle_motionnotify(&ev);
                break;
        }
    }
}

void handle_keypress(XEvent *e) {
    XKeyEvent *ev = &e->xkey;
    KeySym keysym = XLookupKeysym(ev, 0);
    if (ev->state & MODKEY) {
        if (keysym == XStringToKeysym("q")) {
            if (nclients > 0 && current_client >= 0 && current_client < nclients) {
                kill_client(clients[current_client]);
            }
        } else if (keysym == XStringToKeysym("Tab")) {
            if (nclients > 0) {
                current_client = (current_client + 1) % nclients;
                focus_client(current_client);
            }
        } else if (keysym == XStringToKeysym("space")) {
            is_floating_mode = !is_floating_mode;
            if (is_floating_mode) {
                XGrabButton(dpy, Button1, MODKEY, root, True,
                            ButtonPressMask|ButtonReleaseMask|PointerMotionMask,
                            GrabModeAsync, GrabModeAsync, None, None);
                XGrabButton(dpy, Button3, MODKEY, root, True,
                            ButtonPressMask|ButtonReleaseMask|PointerMotionMask,
                            GrabModeAsync, GrabModeAsync, None, None);

                int sw = DisplayWidth(dpy, screen);
                int sh = DisplayHeight(dpy, screen);
                
                for (int i = 0; i < nclients; i++) {
                    XWindowAttributes wa;
                    XGetWindowAttributes(dpy, clients[i], &wa);
                    
                    if (wa.x <= -9000) {  
                        int offset = i * 30;
                        window_geometries[i].x = (sw / 4) + offset;
                        window_geometries[i].y = (sh / 4) + offset;
                        window_geometries[i].width = (sw * 2) / 3;
                        window_geometries[i].height = (sh * 2) / 3;
                    }
                    
                    XMoveResizeWindow(dpy, clients[i],
                                    window_geometries[i].x,
                                    window_geometries[i].y,
                                    window_geometries[i].width,
                                    window_geometries[i].height);
                }
            } else {
                XUngrabButton(dpy, Button1, MODKEY, root);
                XUngrabButton(dpy, Button3, MODKEY, root);
                tile_windows();
            }
        }
        for (int i = 1; i <= NUM_DESKTOPS; i++) {
            char num[2] = { i + '0', '\0' };
            if (keysym == XStringToKeysym(num)) {
                switch_desktop(i - 1);
                break;
            }
        }
    }
}

void handle_maprequest(XEvent *e) {
    XMapRequestEvent *ev = &e->xmaprequest;
    XWindowAttributes wa;
    XSizeHints hints;
    long supplied;

    XGetWindowAttributes(dpy, ev->window, &wa);
    if (wa.override_redirect)
        return;
    
    if (!is_floating_mode) {
        XMoveWindow(dpy, ev->window, -10000, -10000);
    } else {
        int sw = DisplayWidth(dpy, screen);
        int sh = DisplayHeight(dpy, screen);
        
        if (XGetWMNormalHints(dpy, ev->window, &hints, &supplied)) {
            if (hints.flags & PSize) {
                wa.width = hints.width;
                wa.height = hints.height;
            }
            if (hints.flags & PBaseSize) {
                wa.width = hints.base_width;
                wa.height = hints.base_height;
            }
        }
        
        if (wa.width < 100) wa.width = 800;
        if (wa.height < 100) wa.height = 600;
        
        wa.x = (sw - wa.width) / 2;
        wa.y = (sh - wa.height) / 2;
        
        XMoveResizeWindow(dpy, ev->window, wa.x, wa.y, wa.width, wa.height);
    }
    
    add_client(ev->window);
    XMapWindow(dpy, ev->window);
    
    if (!is_floating_mode) {
        tile_windows();
    }
    focus_client(nclients - 1);
}

void handle_unmapnotify(XEvent *e) {
    XUnmapEvent *ev = &e->xunmap;
    if (ev->event == root || ev->window == root)
        return;
        
    remove_client(ev->window);
    if (!is_floating_mode) {
        tile_windows();
    }
}

void handle_destroynotify(XEvent *e) {
    XDestroyWindowEvent *ev = &e->xdestroywindow;
    if (ev->event != root && ev->window != root) {
        remove_client(ev->window);
        tile_windows();
    }
}

void handle_enternotify(XEvent *e) {
    XCrossingEvent *ev = &e->xcrossing;
    for (int i = 0; i < nclients; i++) {
        if (clients[i] == ev->window) {
            focus_client(i);
            break;
        }
    }
}

void handle_clientmessage(XEvent *e) {
    XClientMessageEvent *ev = &e->xclient;
    if (ev->message_type == XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False)) {
        if (ev->data.l[0] >= 0 && ev->data.l[0] < NUM_DESKTOPS) {
            switch_desktop(ev->data.l[0]);
        }
    } else if (ev->message_type == XInternAtom(dpy, "WM_PROTOCOLS", True)) {
    } else if (ev->message_type == XInternAtom(dpy, "_NET_CLOSE_WINDOW", False)) {
        kill_client(ev->window);
    }
}

void handle_buttonpress(XEvent *e) {
    XButtonEvent *ev = &e->xbutton;
    if (!(ev->state & MODKEY))
        return;

    Window w = ev->subwindow;
    if (w == None)
        return;

    XGrabPointer(dpy, root, True,
                 ButtonPressMask|ButtonReleaseMask|PointerMotionMask,
                 GrabModeAsync, GrabModeAsync,
                 root, None, CurrentTime);

    XGetWindowAttributes(dpy, w, &wa);
    start = *ev;
}

void handle_buttonrelease(XEvent *e) {
    if (!is_floating_mode)
        return;
        
    XButtonEvent *ev = &e->xbutton;
    if (!(ev->state & MODKEY))
        return;

    XUngrabPointer(dpy, CurrentTime);
}

void handle_motionnotify(XEvent *e) {
    if (!is_floating_mode)
        return;

    if (!(e->xbutton.state & Button1Mask) && !(e->xbutton.state & Button3Mask))
        return;

    XMotionEvent *ev = &e->xmotion;
    int xdiff = ev->x_root - start.x_root;
    int ydiff = ev->y_root - start.y_root;

    for (int i = 0; i < nclients; i++) {
        if (clients[i] == start.subwindow) {
            if (start.button == Button1) {
                window_geometries[i].x = wa.x + xdiff;
                window_geometries[i].y = wa.y + ydiff;
                XMoveWindow(dpy, start.subwindow,
                           window_geometries[i].x,
                           window_geometries[i].y);
            } else if (start.button == Button3) {
                window_geometries[i].width = MAX(1, wa.width + xdiff);
                window_geometries[i].height = MAX(1, wa.height + ydiff);
                XResizeWindow(dpy, start.subwindow,
                            window_geometries[i].width,
                            window_geometries[i].height);
            }
            break;
        }
    }
}

void tile_windows() {
    if (nclients == 0 || is_floating_mode)
        return;

    unsigned long strut[12] = {0};
    int sw = DisplayWidth(dpy, screen);
    int sh = DisplayHeight(dpy, screen);
    int gap = GAP;
    int total_gap = gap * 2;
    
    Window *windows = NULL;
    unsigned int num_windows;
    Window dummy1, dummy2;
    
    if (XQueryTree(dpy, root, &dummy1, &dummy2, &windows, &num_windows)) {
        for (unsigned int i = 0; i < num_windows; i++) {
            unsigned long window_strut[12] = {0};
            get_strut_partial(windows[i], window_strut);
            for (int j = 0; j < 12; j++) {
                strut[j] = strut[j] > window_strut[j] ? strut[j] : window_strut[j];
            }
        }
        XFree(windows);
    }
    
    int x = strut[0];
    int y = strut[2];
    sw -= (strut[0] + strut[1]);
    sh -= (strut[2] + strut[3]);

    if (nclients == 1) {
        XMoveResizeWindow(dpy, clients[0],
                         x + gap, y + gap,
                         sw - total_gap - 2, sh - total_gap - 2);
    } else {
        int master_width = sw / 2;
        XMoveResizeWindow(dpy, clients[0],
                         x + gap, y + gap,
                         master_width - total_gap - 2, sh - total_gap - 2);
        int stack_count = nclients - 1;
        int stack_width = sw - master_width;
        int stack_height = (sh - total_gap - gap * (stack_count - 1)) / stack_count;
        for (int i = 1; i < nclients; i++) {
            XMoveResizeWindow(dpy, clients[i],
                            x + master_width + gap,
                            y + gap + (i - 1) * (stack_height + gap),
                            stack_width - total_gap - 2,
                            stack_height - 2);
        }
    }
}

void switch_desktop(unsigned int desktop) {
    if (desktop >= NUM_DESKTOPS || desktop == current_desktop)
        return;

    desktop_clients[current_desktop] = clients;
    desktop_nclients[current_desktop] = nclients;

    Window *old_clients = clients;
    int old_nclients = nclients;

    clients = desktop_clients[desktop];
    nclients = desktop_nclients[desktop];
    current_desktop = desktop;

    for (int i = 0; i < old_nclients; i++)
        XUnmapWindow(dpy, old_clients[i]);

    for (int i = 0; i < nclients; i++)
        XMapWindow(dpy, clients[i]);

    update_net_current_desktop();
    tile_windows();
    
    if (nclients > 0) {
        current_client = 0;
        focus_client(current_client);
    } else {
        current_client = -1;
        XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
    }
}

void add_client(Window w) {
    Window *new_clients = realloc(clients, sizeof(Window) * (nclients + 1));
    WindowGeometry *new_geometries = realloc(window_geometries, sizeof(WindowGeometry) * (nclients + 1));
    
    if (new_clients == NULL || new_geometries == NULL) {
        fprintf(stderr, "rude: failed to allocate memory\n");
        exit(1);
    }
    
    clients = new_clients;
    window_geometries = new_geometries;
    desktop_clients[current_desktop] = clients;
    
    XWindowAttributes wa;
    XGetWindowAttributes(dpy, w, &wa);
    window_geometries[nclients].x = wa.x;
    window_geometries[nclients].y = wa.y;
    window_geometries[nclients].width = wa.width;
    window_geometries[nclients].height = wa.height;
    
    clients[nclients++] = w;
    desktop_nclients[current_desktop] = nclients;
    
    XSelectInput(dpy, w, StructureNotifyMask | EnterWindowMask | FocusChangeMask);
    update_client_list(clients, nclients);
    update_net_desktop_for_window(w);
}

void remove_client(Window w) {
    for (int i = 0; i < nclients; i++) {
        if (clients[i] == w) {
            memmove(&clients[i], &clients[i + 1], sizeof(Window) * (nclients - i - 1));
            memmove(&window_geometries[i], &window_geometries[i + 1], 
                   sizeof(WindowGeometry) * (nclients - i - 1));
            nclients--;
            desktop_nclients[current_desktop] = nclients;
            
            if (nclients == 0) {
                free(clients);
                free(window_geometries);
                clients = NULL;
                window_geometries = NULL;
                desktop_clients[current_desktop] = NULL;
                current_client = -1;
            } else {
                Window *new_clients = realloc(clients, sizeof(Window) * nclients);
                WindowGeometry *new_geometries = realloc(window_geometries, 
                                                       sizeof(WindowGeometry) * nclients);
                if (new_clients == NULL || new_geometries == NULL) {
                    fprintf(stderr, "rude: failed to allocate memory\n");
                    exit(1);
                }
                clients = new_clients;
                window_geometries = new_geometries;
                desktop_clients[current_desktop] = clients;
                if (current_client >= nclients)
                    current_client = nclients - 1;
                focus_client(current_client);
            }
            break;
        }
    }
    update_client_list(clients, nclients);
}

void focus_client(int index) {
    if (index < 0 || index >= nclients)
        return;
    Window w = clients[index];
    XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);
    XRaiseWindow(dpy, w);
    current_client = index;
    update_active_window(w);
}

void kill_client(Window w) {
    Atom *protocols = NULL;
    int n = 0;
    Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);

    if (XGetWMProtocols(dpy, w, &protocols, &n)) {
        for (int i = 0; i < n; i++) {
            if (protocols[i] == wm_delete) {
                XEvent ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = ClientMessage;
                ev.xclient.window = w;
                ev.xclient.message_type = XInternAtom(dpy, "WM_PROTOCOLS", True);
                ev.xclient.format = 32;
                ev.xclient.data.l[0] = wm_delete;
                ev.xclient.data.l[1] = CurrentTime;
                XSendEvent(dpy, w, False, NoEventMask, &ev);
                XFree(protocols);
                return;
            }
        }
        XFree(protocols);
    }
    XKillClient(dpy, w);
}

void setup_ewmh() {
    Atom net_supported = XInternAtom(dpy, "_NET_SUPPORTED", False);
    Atom net_wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    Atom net_wm_state_fullscreen = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    Atom net_active_window = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
    Atom net_supporting_wm_check = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
    Atom net_client_list = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    Atom net_current_desktop = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
    Atom net_number_of_desktops = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
    Atom net_wm_desktop = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
    
    Atom supported_atoms[] = {
        net_supported,
        net_wm_state,
        net_wm_state_fullscreen,
        net_active_window,
        net_wm_name,
        net_supporting_wm_check,
        net_client_list,
        net_current_desktop,
        net_number_of_desktops,
        net_wm_desktop
    };
    
    XChangeProperty(dpy, root, net_supported, XA_ATOM, 32, PropModeReplace, 
                    (unsigned char *)supported_atoms, sizeof(supported_atoms)/sizeof(Atom));

    Window wm_window = XCreateSimpleWindow(dpy, root, -1, -1, 1, 1, 0, 0, 0);
    XChangeProperty(dpy, root, net_supporting_wm_check, XA_WINDOW, 32, PropModeReplace, 
                    (unsigned char *)&wm_window, 1);
    XChangeProperty(dpy, wm_window, net_supporting_wm_check, XA_WINDOW, 32, PropModeReplace, 
                    (unsigned char *)&wm_window, 1);

    const char *wm_name = "rude";
    Atom utf8_string = XInternAtom(dpy, "UTF8_STRING", False);
    XChangeProperty(dpy, wm_window, net_wm_name, utf8_string, 8, PropModeReplace, 
                    (unsigned char *)wm_name, strlen(wm_name));
    XChangeProperty(dpy, root, net_wm_name, utf8_string, 8, PropModeReplace, 
                    (unsigned char *)wm_name, strlen(wm_name));
}

void update_client_list(Window *clients, int nclients) {
    Atom net_client_list = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    if (nclients > 0) {
        XChangeProperty(dpy, root, net_client_list, XA_WINDOW, 32, PropModeReplace, (unsigned char *)clients, nclients);
    } else {
        XDeleteProperty(dpy, root, net_client_list);
    }
}

void update_active_window(Window w) {
    Atom net_active_window = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    XChangeProperty(dpy, root, net_active_window, XA_WINDOW, 32, PropModeReplace, (unsigned char *)&w, 1);
}

void update_net_current_desktop() {
    Atom net_current_desktop = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
    XChangeProperty(dpy, root, net_current_desktop, XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)&current_desktop, 1);
}

void update_net_number_of_desktops() {
    Atom net_number_of_desktops = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
    long num_desktops = NUM_DESKTOPS;
    XChangeProperty(dpy, root, net_number_of_desktops, XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)&num_desktops, 1);
}

void update_net_desktop_for_window(Window w) {
    Atom net_wm_desktop = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
    XChangeProperty(dpy, w, net_wm_desktop, XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)&current_desktop, 1);
}

int xerror(Display *dpy, XErrorEvent *ee) {
    if (ee->error_code == BadWindow ||
        ee->error_code == BadMatch ||
        ee->error_code == BadDrawable ||
        ee->error_code == BadAccess)
        return 0;
    fprintf(stderr, "rude: fatal error: request code=%d, error code=%d\n",
            ee->request_code, ee->error_code);
    return 0;
}
