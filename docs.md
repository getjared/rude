## main components

### data structures

```c
typedef struct { Window window; int x, y, w, h; } Client;
typedef struct { Client *clients; int client_count; float main_window_ratio; int is_initialized; } Workspace;
```

- `client`: represents a managed window with its position and size.
- `workspace`: represents a workspace with its clients and layout information.

### global variables

```c
Display *dpy;
Window root;
int screen, current_workspace = 0;
Workspace *workspaces;
int last_moved_window_index = 0, should_warp_pointer = 0;

// ewmh atoms
Atom net_supported, net_client_list, net_number_of_desktops, net_current_desktop, net_active_window;
```

- `dpy`: pointer to the x11 display.
- `root`: the root window.
- `screen`: the default screen.
- `current_workspace`: index of the current workspace.
- `workspaces`: array of workspace structures.
- `last_moved_window_index`: keeps track of the last moved window for consistent movement.
- `should_warp_pointer`: flag to control pointer warping behavior.
- `ewmh atoms`: used for ewmh support and communication with other x11 clients.

### key functions

#### workspace management

1. `void init_workspace(int workspace)`
   - initializes a workspace on-demand.
   - allocates memory for clients and sets initial values.

2. `void switch_workspace(int new_workspace)`
   - switches to the specified workspace, unmapping windows from the current workspace and mapping windows in the new workspace.
   - initializes the new workspace if it hasn't been used yet.
   - updates the current desktop for ewmh compliance.
   - arranges windows and focuses the first window in the new workspace.

#### window management

1. `void manage_window(Window w, int workspace, int is_new)`
   - adds a window to the specified workspace.
   - initializes the workspace if necessary.
   - updates the client list for ewmh compliance.
   - initially positions new windows off-screen to prevent flashing.
   - adjusts the main_window_ratio when adding a second window.
   - sets the should_warp_pointer flag for new windows.

2. `void unmanage_window(Window w, int workspace)`
   - removes a window from the specified workspace.
   - updates the client list for ewmh compliance.
   - resets main_window_ratio to full-screen if all windows are closed.

3. `void focus_window(Window w)`
   - sets input focus to the specified window and raises it.
   - updates the active window for ewmh compliance.
   - warps the pointer to the window's center if should_warp_pointer is set.

4. `void warp_pointer_to_window(Window w)`
   - moves the mouse pointer to the center of the specified window.

#### layouts

1. `void tile(int screen_w, int screen_h)`
   - arranges windows in a tile layout (main window on left, stack on right).
   - uses the workspace-specific main_window_ratio for sizing.
   - handles full-screen case when only one window or ratio is close to 1.

2. `void arrange(void)`
   - applies the current layout to the current workspace.

#### window operations

1. `void kill_focused_window(void)`
   - attempts to close the currently focused window, first by sending a delete message, then by force if necessary.

2. `void move_window(int direction)`
   - moves the focused window in the specified direction within the current layout.

3. `void resize_main_window(int direction)`
   - resizes the main window by adjusting the main_window_ratio.
   - handles transition from full-screen to split layout.

### ewmh support functions

1. `void init_ewmh(void)`
   - initializes ewmh atoms and sets up initial ewmh properties.

2. `void update_client_list(void)`
   - updates the list of managed windows for ewmh compliance.

3. `void update_net_current_desktop(void)`
   - updates the current desktop (workspace) information for ewmh compliance.

4. `void update_net_active_window(Window w)`
   - updates the currently active window for ewmh compliance.

### main loop

the `main()` function sets up the x11 environment, grabs keys for shortcuts, initializes ewmh support, initializes the first workspace, and enters an event loop to handle x11 events. key events handled include:

- workspace switching
- window killing
- window movement
- window resizing

## customization

key bindings and other parameters are defined as preprocessor macros at the top of the file. modify these to customize the behavior of rude:

```c
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
```

## compilation and installation

rude uses a simple makefile for compilation:

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -O2 
LDFLAGS = -lX11 -lm

rude: rude.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

.PHONY: clean install

clean:
	rm -f rude

install: rude
	install -D -m 755 rude $(DESTDIR)/usr/local/bin/rude
```

to compile and install:
1. run `make` to compile
2. run `sudo make install` to install to `/usr/local/bin/rude`

## error handling

rude uses a custom x error handler (`xerror`) to ignore `badwindow` errors and log other x11 errors.

## cleanup

the `cleanup` function is registered with `atexit` to ensure proper x11 resource cleanup on exit.

## ewmh compliance

rude supports basic ewmh (extended window manager hints) compliance, which allows it to interact better with status bars and other x11 clients. the following ewmh properties are supported:

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

## window behavior

- the first window in each workspace opens in full-screen mode (minus gaps).
- when a second window is added, the layout transitions to a split view with a default 50/50 ratio.
- the main window can be resized using mod+shift+left (decrease) and mod+shift+right (increase).
- each workspace maintains its own main_window_ratio, preserving layouts when switching workspaces.
- closing all windows in a workspace resets its layout to full-screen for the next window.

## key bindings

- `mod + 1-9`: switch to workspace 1-9
- `mod + q`: kill focused window
- `mod + left/right`: move focused window left/right
- `mod + shift + left`: decrease main window size
- `mod + shift + right`: increase main window size

## layouts

1. tile: main window on the left, stack on the right

the layout respects the workspace-specific main_window_ratio for sizing windows.

## lazy loading of workspaces

rude implements lazy loading of workspaces:

- workspaces are only initialized when they are first accessed.
- this reduces memory usage and startup time, especially for users who don't frequently use all available workspaces.
- the `init_workspace` function is called on-demand to set up a new workspace.

## pointer warping

rude includes a pointer warping feature:

- the mouse pointer is automatically centered on a window in two scenarios:
  1. when a new window is created
  2. when switching to a different workspace
- the `should_warp_pointer` flag controls when pointer warping occurs
