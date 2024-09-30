
## installation

### requirements

- arch linux
- development tools: gcc, make
- xlib libraries

### building from source

1. clone the repository

    ```sh
    git clone https://github.com/getjared/rude.git
    cd rude
    ```

2. build the window manager

    ```sh
    make
    ```

### installing

to install rude to `/usr/local/bin`, run:

```sh
sudo make install
```

## configuration

rude is configured using a plain text file located at `~/.config/rude/config`. you can customize various settings such as workspaces, keybindings, and application rules.

### general settings

- `max_workspaces`: number of workspaces (default: 9, max: 16)
- `max_clients`: maximum number of windows per workspace (default: 100, max: 200)
- `gaps`: spacing between windows in pixels (default: 35)

### column settings

- `initial_column_size`: starting width for window columns (default: 800)
- `step_size`: increment for resizing columns (default: 50)
- `min_column_size`: minimum column width (default: 300)
- `max_column_size`: maximum column width (default: 2000)

### hotkeys

configure keybindings using x11 key symbols.

- `mod_key`: modifier key (e.g., `Mod4` for the super key)
- `workspace_switch_keys`: keys to switch workspaces (e.g., `1,2,3,4,5,6,7,8,9`)
- `kill_window_key`: key to close the focused window (default: `q`)
- `move_left_key`: key to move windows left (default: `Left`)
- `move_right_key`: key to move windows right (default: `Right`)
- `change_layout_key`: key to change the window layout (default: `space`)
- `scroll_left_key`: key to scroll left (default: `h`)
- `scroll_right_key`: key to scroll right (default: `l`)
- `resize_left_key`: key to resize windows left (default: `Left`)
- `resize_right_key`: key to resize windows right (default: `Right`)

### application-specific rules

define custom column sizes for specific applications under the `[rules]` section.

```ini
[rules]
firefox=1800
term=500
```

## main components

### data structures

```c
typedef enum {
    NORMAL,
    FLOATING
} WindowType;

typedef struct { 
    Window window; 
    int x, y, w, h; 
    WindowType type; // categorize window type
    int custom_width; // per-window width
} Client;

typedef struct { 
    const char *name; 
    void (*arrange)(int, int); 
} Layout;

typedef struct {
    // general settings
    int max_workspaces;
    int max_clients;
    int gaps;

    // column settings
    int initial_column_size;
    int step_size;
    int min_column_size;
    int max_column_size;

    // hotkeys
    unsigned int mod_key;
    KeySym workspace_switch_keys[MAX_WORKSPACES_MAX];
    KeySym kill_window_key;
    KeySym move_left_key;
    KeySym move_right_key;
    KeySym change_layout_key;
    KeySym scroll_left_key;
    KeySym scroll_right_key;
    KeySym resize_left_key;
    KeySym resize_right_key;

    // application-specific rules
    int rule_count;
    struct {
        char app_name[MAX_APP_NAME_LEN];
        int column_size;
    } rules[MAX_RULES];
} Config;
```

- `windowtype`: distinguishes between normal and floating windows.
- `client`: represents a managed window with its position, size, type, and custom width.
- `layout`: represents a tiling layout with a name and an arrangement function.
- `config`: holds all configuration settings including general settings, column settings, hotkeys, and application-specific rules.

### global variables

```c
Display *dpy;
Window root;
int screen, current_workspace = 0;
Client clients[MAX_WORKSPACES_MAX][MAX_CLIENTS_MAX];
int client_count[MAX_WORKSPACES_MAX] = {0};
int floating_client_count[MAX_WORKSPACES_MAX] = {0}; // tracks floating windows per workspace
float main_window_ratio[MAX_WORKSPACES_MAX];
int last_moved_window_index = 0;

// view offset for each workspace to handle scrolling
int view_offset[MAX_WORKSPACES_MAX] = {0};

// dynamic column width and scroll step
int COLUMN_WIDTH[MAX_WORKSPACES_MAX];
int SCROLL_STEP[MAX_WORKSPACES_MAX];

// ewmh atoms
Atom net_supported, net_client_list, net_number_of_desktops, net_current_desktop, net_active_window;

// current configuration
Config config;
```

- `dpy`: pointer to the x11 display.
- `root`: the root window.
- `screen`: the default screen.
- `current_workspace`: index of the current workspace.
- `clients`: 2d array of managed windows for each workspace.
- `client_count`: number of normal clients in each workspace.
- `floating_client_count`: number of floating clients in each workspace.
- `main_window_ratio`: array of ratios for the main window size in each workspace.
- `last_moved_window_index`: keeps track of the last moved window for consistent movement.
- `view_offset`: handles scrolling view for each workspace.
- `COLUMN_WIDTH`: dynamic column widths for each workspace.
- `SCROLL_STEP`: scroll steps for each workspace.
- `ewmh atoms`: used for ewmh support and communication with other x11 clients.
- `config`: holds the current configuration settings.

### key functions

#### window management

1. `void manage_window(Window w, int workspace, int is_new)`
   - adds a window to the specified workspace.
   - categorizes the window type (normal or floating).
   - applies application-specific rules for column sizes.
   - updates client counts and positions new windows appropriately.
   - updates the client list for ewmh compliance.

2. `void unmanage_window(Window w, int workspace)`
   - removes a window from the specified workspace.
   - shifts remaining windows in the client array.
   - updates client counts and resets layout ratios if necessary.
   - updates the client list for ewmh compliance.

3. `void focus_window(Window w)`
   - sets input focus to the specified window and raises it.
   - updates the active window for ewmh compliance.
   - ensures floating windows are raised above normal windows.

#### layouts

1. `void horizontal_grid(int screen_w, int screen_h)`
   - arranges windows in a horizontal grid layout.
   - positions normal windows in their own columns with custom widths.
   - centers floating windows without altering their size.
   - raises floating windows above normal windows.

2. `void arrange(void)`
   - applies the current layout to the current workspace.
   - retrieves screen dimensions and calls the layout's arrange function.

#### workspace management

1. `void switch_workspace(int new_workspace)`
   - switches to the specified workspace.
   - unmaps windows from the current workspace and maps windows in the new workspace.
   - updates the current desktop for ewmh compliance.
   - arranges windows and focuses the first window in the new workspace.

#### window operations

1. `void kill_focused_window(void)`
   - attempts to close the currently focused window.
   - sends a delete message if supported, otherwise forcefully kills the client.

2. `void move_window(int direction)`
   - moves the focused window left or right within the current layout.
   - swaps the focused window with the adjacent window based on direction.
   - rearranges windows and focuses the moved window.

3. `void resize_focused_window(int direction)`
   - resizes the focused window.
   - for normal windows, adjusts the custom column width.
   - for floating windows, adjusts the window size and repositions it to remain centered.
   - enforces minimum and maximum size constraints.
   - rearranges windows after resizing.

#### ewmh support functions

1. `void init_ewmh(void)`
   - initializes ewmh atoms.
   - sets up initial ewmh properties such as supported atoms and number of desktops.

2. `void update_client_list(void)`
   - updates the `_NET_CLIENT_LIST` property with the list of managed windows.

3. `void update_net_current_desktop(void)`
   - updates the `_NET_CURRENT_DESKTOP` property with the current workspace index.

4. `void update_net_active_window(Window w)`
   - updates the `_NET_ACTIVE_WINDOW` property with the currently focused window.

#### utility functions

1. `char *trim(char *str)`
   - trims leading and trailing whitespace from a string.

2. `unsigned int parse_mod_key(const char *key_str)`
   - parses a modifier key string into the corresponding modifier mask.

3. `int load_config(const char *path, Config *config)`
   - loads configuration settings from the specified file.
   - sets default values if the configuration file cannot be opened.
   - parses general settings, column settings, hotkeys, and application-specific rules.

4. `int get_window_class(Window w, char *class_name, size_t len)`
   - retrieves the class name of a window.

5. `WindowType get_window_type(Window w)`
   - determines the type of a window (normal or floating) based on its _NET_WM_WINDOW_TYPE property.

### compilation and installation

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

## usage

once installed and configured, you can start using rude as your window manager.

### starting rude

add `exec rude` to your `.xinitrc` or appropriate startup script.

```sh
exec rude
```

then start your x session:

```sh
startx
```

### keybindings

- `mod + 1-9`: switch to workspace 1-9
- `mod + q`: kill focused window
- `mod + left/right`: move focused window left/right
- `mod + h/l`: scroll left/right through windows
- `mod + shift + left/right`: resize focused window left/right

## customization

key bindings and other parameters are defined in the configuration file located at `~/.config/rude/config`. modify these to customize the behavior of rude.

### example configuration

```ini
# general settings
max_workspaces=9
max_clients=100
gaps=35

# column settings
initial_column_size=800
step_size=50
min_column_size=300
max_column_size=2000

# hotkeys (use x11 key symbols)
mod_key=Mod4
workspace_switch_keys=1,2,3,4,5,6,7,8,9
kill_window_key=q
move_left_key=Left
move_right_key=Right
change_layout_key=space
scroll_left_key=h
scroll_right_key=l
resize_left_key=Left
resize_right_key=Right

# application-specific rules
[rules]
firefox=1800
term=500
```

## error handling

rude uses a custom x error handler (`xerror`) to ignore `BadWindow` errors and log other x11 errors.

```c
int xerror(Display *dpy, XErrorEvent *ee) {
    (void)dpy;
    if (ee->error_code == BadWindow) return 0;
    fprintf(stderr, "rude: x error: request code=%d, error code=%d\n", ee->request_code, ee->error_code);
    return 0;
}
```

## cleanup

the `cleanup` function is registered with `atexit` to ensure proper x11 resource cleanup on exit.

```c
void cleanup(void) {
    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    XCloseDisplay(dpy);
}
```

## ewmh compliance

rude supports basic ewmh (extended window manager hints) compliance, which allows it to interact better with status bars and other x11 clients. the following ewmh properties are supported:

- `_net_supported`: list of supported ewmh atoms
- `_net_client_list`: list of managed windows
- `_net_number_of_desktops`: number of workspaces
- `_net_current_desktop`: current active workspace
- `_net_active_window`: currently focused window

these properties are initialized in `init_ewmh()` and updated as necessary throughout the window manager's operation.

## window flashing prevention

to prevent brief window flash in the top-left corner when creating new windows:

1. in the `manage_window` function, new windows are initially positioned off-screen with a size of 1x1 pixels.
2. the `arrange` function is called before mapping the window, ensuring proper positioning and sizing.
3. the window is then mapped and focused, appearing in its correct position without flashing.

## window behavior

- the first window in each workspace opens in full-screen mode (minus gaps).
- when a second window is added, the layout transitions to a split view with default custom widths based on configuration or application-specific rules.
- normal windows can be resized using `mod + shift + left/right`.
- floating windows maintain their size and are centered on the screen.
- each workspace maintains its own `main_window_ratio` and `view_offset`, preserving layouts when switching workspaces.
- closing all windows in a workspace resets its layout to full-screen for the next window.

## key bindings

- `mod + 1-9`: switch to workspace 1-9
- `mod + q`: kill focused window
- `mod + left/right`: move focused window left/right
- `mod + h/l`: scroll left/right through windows
- `mod + shift + left`: resize focused window left
- `mod + shift + right`: resize focused window right

## layouts

1. **horizontal_grid**: arranges windows in a horizontal grid layout.
   - normal windows are placed in their own columns with customizable widths.
   - floating windows are centered without altering their size.
   - supports smooth scrolling through windows with adjustable view offsets.
