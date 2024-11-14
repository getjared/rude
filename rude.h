#ifndef RUDE_H
#define RUDE_H

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MODKEY Mod4Mask
#define GAP 10
#define NUM_DESKTOPS 5
#define MAX(A, B) ((A) > (B) ? (A) : (B))

extern Display *dpy;
extern Window root;
extern int screen;
extern Window *clients;
extern int nclients;
extern int current_client;
extern unsigned int current_desktop;
extern Window **desktop_clients;
extern int *desktop_nclients;
extern Bool is_floating_mode;
extern XWindowAttributes wa;
extern XButtonEvent start;

void setup(void);
void run(void);
void handle_keypress(XEvent *e);
void handle_maprequest(XEvent *e);
void handle_unmapnotify(XEvent *e);
void handle_destroynotify(XEvent *e);
void handle_enternotify(XEvent *e);
void handle_clientmessage(XEvent *e);
void handle_buttonpress(XEvent *e);
void handle_buttonrelease(XEvent *e);
void handle_motionnotify(XEvent *e);
void tile_windows(void);
void add_client(Window w);
void remove_client(Window w);
void kill_client(Window w);
void focus_client(int index);
void setup_ewmh(void);
void update_client_list(Window *clients, int nclients);
void update_active_window(Window w);
void switch_desktop(unsigned int desktop);
void update_net_current_desktop(void);
void update_net_number_of_desktops(void);
void update_net_desktop_for_window(Window w);
int xerror(Display *dpy, XErrorEvent *ee);
void get_strut_partial(Window w, unsigned long *strut);

typedef struct {
    int x;
    int y;
    int width;
    int height;
} WindowGeometry;

extern WindowGeometry *window_geometries;

#endif // RUDE_H