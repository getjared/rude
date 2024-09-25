## main components

### data structures

```c
typedef struct { Window window; int x, y, w, h; } Client;
typedef struct { const char *name; void (*arrange)(int, int); } Layout;
```

- `client`: represents a managed window with its position and size.
- `layout`: represents a tiling layout with a name and an arrangement function.

### global variables

```c
Display *dpy;
Window root;
int screen, current_workspace = 0;
Client clients[MAX_WORKSPACES][MAX_CLIENTS];
int client_count[MAX_WORKSPACES] = {0};

// EWMH atoms
Atom net_supported, net_client_list, net_number_of_desktops, net_current_desktop, net_active_window;
```

- `dpy`: pointer to the x11 display.
- `root`: the root window.
- `screen`: the default screen.
- `current_workspace`: index of the current workspace.
- `clients`: 2d array of managed windows for each workspace.
- `client_count`: number of clients in each workspace.
- `ewmh atoms`: used for ewmh support and communication with other x11 clients.

### key functions

#### window management

1. `void manage_window(Window w, int workspace, int is_new)`
   - adds a window to the specified workspace.
   - updates the client list for ewmh compliance.
   - initially positions new windows off-screen to prevent flashing.
   - parameters:
     - `w`: window to manage
     - `workspace`: workspace index
     - `is_new`: flag indicating if it's a new window

2. `void unmanage_window(Window w, int workspace)`
   - removes a window from the specified workspace.
   - updates the client list for ewmh compliance.
   - parameters:
     - `w`: window to unmanage
     - `workspace`: workspace index

3. `void focus_window(Window w)`
   - sets input focus to the specified window and raises it.
   - updates the active window for ewmh compliance.
   - parameters:
     - `w`: window to focus

#### layouts

1. `void tile(int screen_w, int screen_h)`
   - arranges windows in a tile layout (main window on left, stack on right).
   - parameters:
     - `screen_w`: screen width
     - `screen_h`: screen height

2. `void fibonacci(int screen_w, int screen_h)`
   - arranges windows in a fibonacci spiral.
   - parameters:
     - `screen_w`: screen width
     - `screen_h`: screen height

3. `void euler(int screen_w, int screen_h)`
   - arranges windows in an euler layout (central window with surrounding windows).
   - parameters:
     - `screen_w`: screen width
     - `screen_h`: screen height

4. `void arrange(void)`
   - applies the current layout to the current workspace.

5. `void switch_layout(void)`
   - cycles to the next available layout.

#### workspace management

1. `void switch_workspace(int new_workspace)`
   - switches to the specified workspace, unmapping windows from the current workspace and mapping windows in the new workspace.
   - updates the current desktop for ewmh compliance.
   - parameters:
     - `new_workspace`: index of the workspace to switch to

#### window operations

1. `void kill_focused_window(void)`
   - attempts to close the currently focused window, first by sending a delete message, then by force if necessary.

2. `void move_focused_window(int direction)`
   - moves the focused window in the specified direction within the current layout.
   - parameters:
     - `direction`: direction to move (left, right, up, down)

### ewmh support functions

1. `void init_ewmh(void)`
   - initializes ewmh atoms and sets up initial ewmh properties.

2. `void update_client_list(void)`
   - updates the list of managed windows for ewmh compliance.

3. `void update_net_current_desktop(void)`
   - updates the current desktop (workspace) information for ewmh compliance.

4. `void update_net_active_window(Window w)`
   - updates the currently active window for ewmh compliance.
   - parameters:
     - `w`: window to set as active

### main loop

the `main()` function sets up the x11 environment, grabs keys for shortcuts, initializes ewmh support, and enters an event loop to handle x11 events. key events handled include:

- workspace switching
- window killing
- window movement
- layout switching

```c
int main(void) {
    // ... [initialization code] ...

    init_ewmh();

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
            // ... [key event handling] ...
        }
    }

    return 0;
}
```

## customization

key bindings and other parameters are defined as preprocessor macros at the top of the file. modify these to customize the behavior of rudewm:

```c
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
```

## compilation and installation

rudewm uses a simple makefile for compilation:

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -O2 
LDFLAGS = -lX11 -lm
rudewm: rude.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
.PHONY: clean install
clean:
	rm -f rudewm
install: rudewm
	install -D -m 755 rudewm $(DESTDIR)/usr/local/bin/rudewm
```

to compile and install:
1. run `make` to compile
2. run `sudo make install` to install to `/usr/local/bin/rudewm`

## error handling

rudewm uses a custom x error handler (`xerror`) to ignore `badwindow` errors and log other x11 errors.

## cleanup

the `cleanup` function is registered with `atexit` to ensure proper x11 resource cleanup on exit.

## ewmh compliance

rudewm supports basic ewmh (extended window manager hints) compliance, which allows it to interact better with status bars and other x11 clients. the following ewmh properties are supported:

- `_NET_SUPPORTED`: list of supported ewmh atoms
- `_NET_CLIENT_LIST`: list of managed windows
- `_NET_NUMBER_OF_DESKTOPS`: number of workspaces
- `_NET_CURRENT_DESKTOP`: current active workspace
- `_NET_ACTIVE_WINDOW`: currently focused window

these properties are initialized in `init_ewmh()` and updated as necessary throughout the window manager's operation.

## window flashing prevention

to prevent brief window flash in the top-left corner when creating new windows:

1. in the `manage_window` function, new windows are initially positioned off-screen with a size of 1x1 pixels.
2. the `arrange` function is called before mapping the window, ensuring proper positioning and sizing.
3. the window is then mapped and focused, appearing in its correct position without flashing.