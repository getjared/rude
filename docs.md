
## key components

### header files and definitions

```c
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
```

these headers provide necessary functions and definitions for x11 interaction, string manipulation, and mathematical operations.

### constants

```c
#define MIN_WINDOW_WIDTH 200
#define MIN_WINDOW_HEIGHT 100
#define SCROLL_STEP 50
#define MAX_WINDOWS 100
#define ZOOM_FACTOR 0.1
```

these define the minimum window dimensions, scroll step size, maximum number of windows, and zoom factor.

### data structures

```c
typedef struct {
    Window win;
    int x, y, width, height;
} WindowInfo;
```

`WindowInfo` structure holds information about each window, including its x11 `Window` identifier and position/size on the infinite canvas.

### global variables

```c
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
```

these variables manage the x11 display, root window, event handling, screen dimensions, viewport position, window list, zoom state, and original viewport position.

## key functions

### setup_ewmh

```c
static void setup_ewmh(void)
```

this function sets up Extended Window Manager Hints (EWMH) support, allowing other applications to recognize rude as the active window manager.

### focus

```c
static void focus(Window w)
```

sets input focus to the specified window or the root window if `None` is passed.

### add_window and remove_window

```c
static void add_window(Window win, int x, int y, int width, int height)
static void remove_window(Window win)
```

these functions manage the `windows` array, adding or removing windows as they are created or destroyed.

### update_window_positions

```c
static void update_window_positions()
```

updates the position of all windows based on the current viewport and zoom level.

### scroll_viewport

```c
static void scroll_viewport(int dx, int dy)
```

scrolls the viewport by the specified delta x and y, then updates window positions.

### move and resize

```c
static void move(Window win)
static void resize(Window win)
```

these functions handle window movement and resizing, taking into account the current zoom level.

### map_request

```c
static void map_request(XMapRequestEvent *ev)
```

handles requests to map (display) new windows, setting their initial position and size.

### toggle_zoom

```c
static void toggle_zoom()
```

toggles between zoomed-out view (showing all windows) and normal view.

### key_press

```c
static void key_press(XKeyEvent *ev)
```

handles keyboard events, including window closing, scrolling, and zoom toggling.

### main

```c
int main(void)
```

the main function initializes the display, sets up event handling, and enters the main event loop.

## event handling

the main event loop in the `main` function handles various x11 events:

- `MapRequest`: for new windows
- `ButtonPress`: for moving and resizing windows
- `KeyPress`: for keyboard shortcuts
- `EnterNotify`: for focus changes
- `DestroyNotify`: for window closures
- `Expose`: for redrawing the root window

## infinite canvas implementation

the infinite canvas is implemented using a viewport system. the `viewport_x` and `viewport_y` variables track the current view position in the infinite space. window positions are stored relative to this infinite space and are adjusted for display based on the viewport position and zoom level.

## zooming functionality

zooming is implemented by scaling window positions and sizes. when zoomed out, all windows are visible, scaled to fit within the screen. the `toggle_zoom` function calculates the appropriate zoom level and viewport position to display all windows.

## EWMH support

EWMH support is implemented in the `setup_ewmh` function. it sets up necessary x11 properties to advertise rude as the active window manager and provide its name to other applications.

## compilation

to compile rude, use:

```
gcc -O3 -o rude rude.c -lX11 -lm
```

this compiles the code with optimizations and links against the x11 and math libraries.
