// rude.c - a rude little window manager. .

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <ctype.h>

// constants
#define MAX_WORKSPACES_DEFAULT 9
#define MAX_WORKSPACES_MAX 16
#define MAX_CLIENTS_DEFAULT 100
#define MAX_CLIENTS_MAX 200
#define GAP_SIZE_DEFAULT 45
#define MAX_RULES 100
#define MAX_APP_NAME_LEN 256
#define CONFIG_PATH ".config/rude/config"

#define SCROLL_ANIMATION_STEPS 5
#define SCROLL_ANIMATION_DELAY 1000 // 10ms


// macro definitions
#define LENGTH(X) (sizeof(X) / sizeof(*X))
#define CLAMP(V, MIN, MAX) ((V) < (MIN) ? (MIN) : ((V) > (MAX) ? (MAX) : (V)))

// enumeration for window types
typedef enum {
    NORMAL,
    FLOATING
} WindowType;

// client structure
typedef struct { 
    Window window; 
    int x, y, w, h; 
    WindowType type; // categorize window type
    int custom_width; // per-window width
} Client;

// layout structure
typedef struct { 
    const char *name; 
    void (*arrange)(int, int); 
} Layout;

// configuration structure
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

// global variables
Display *dpy;
Window root;
int screen, current_workspace = 0;
Client clients[MAX_WORKSPACES_MAX][MAX_CLIENTS_MAX];
int client_count[MAX_WORKSPACES_MAX] = {0};
int floating_client_count[MAX_WORKSPACES_MAX] = {0}; // tracks floating windows per workspace
float main_window_ratio[MAX_WORKSPACES_MAX];
int last_moved_window_index = 0;  // tracks the last moved window index

// view offset for each workspace to handle scrolling
int view_offset[MAX_WORKSPACES_MAX] = {0};

// dynamic column width and scroll step
int COLUMN_WIDTH[MAX_WORKSPACES_MAX];
int SCROLL_STEP[MAX_WORKSPACES_MAX];

// EWMH atoms
Atom net_supported, net_client_list, net_number_of_desktops, net_current_desktop, net_active_window;

// current configuration
Config config;

// declarations
void horizontal_grid(int, int);
void scroll_left(int);
void scroll_right(int);
void cleanup(void);
void init_ewmh(void);
void update_client_list(void);
void update_net_current_desktop(void);
void update_net_active_window(Window w);
void arrange(void);
void switch_workspace(int new_workspace);
void kill_focused_window(void);
void move_window(int direction);
void resize_focused_window(int direction);
int xerror(Display *dpy, XErrorEvent *ee);
void manage_window(Window w, int workspace, int is_new);
void unmanage_window(Window w, int workspace);
void focus_window(Window w);
WindowType get_window_type(Window w);

// layout definitions
static const Layout layouts[] = {
    {"horizontal_grid", horizontal_grid},
};
static int current_layout = 0;

// configuration parsing function declarations
char *trim(char *str);
unsigned int parse_mod_key(const char *key_str);
int load_config(const char *path, Config *config);
int get_window_class(Window w, char *class_name, size_t len);


// trim leading and trailing whitespace
char *trim(char *str) {
    char *end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    *(end + 1) = 0;
    return str;
}

// parse modifier keys
unsigned int parse_mod_key(const char *key_str) {
    if (strcasecmp(key_str, "Mod1") == 0) return Mod1Mask;
    if (strcasecmp(key_str, "Mod2") == 0) return Mod2Mask;
    if (strcasecmp(key_str, "Mod3") == 0) return Mod3Mask;
    if (strcasecmp(key_str, "Mod4") == 0) return Mod4Mask;
    if (strcasecmp(key_str, "Mod5") == 0) return Mod5Mask;
    fprintf(stderr, "rude: unknown mod_key %s, defaulting to Mod4\n", key_str);
    return Mod4Mask; // default modifier
}

// load configuration
int load_config(const char *path, Config *config) {
    FILE *file = fopen(path, "r");
    if (!file) {
        fprintf(stderr, "rude: cannot open config file at %s, using default settings\n", path);
        // set default values
        config->max_workspaces = MAX_WORKSPACES_DEFAULT;
        config->max_clients = MAX_CLIENTS_DEFAULT;
        config->gaps = GAP_SIZE_DEFAULT;
        config->initial_column_size = 800;
        config->step_size = 50;
        config->min_column_size = 300;
        config->max_column_size = 2000;
        config->mod_key = Mod4Mask;
        memset(config->workspace_switch_keys, NoSymbol, sizeof(config->workspace_switch_keys));
        for (int i = 0; i < config->max_workspaces; i++) {
            config->workspace_switch_keys[i] = XStringToKeysym("1") + i;
        }
        config->kill_window_key = XStringToKeysym("q");
        config->move_left_key = XStringToKeysym("Left");
        config->move_right_key = XStringToKeysym("Right");
        config->change_layout_key = XStringToKeysym("space");
        config->scroll_left_key = XStringToKeysym("h");
        config->scroll_right_key = XStringToKeysym("l");
        config->resize_left_key = XStringToKeysym("Left");
        config->resize_right_key = XStringToKeysym("Right");
        config->rule_count = 0;
        return -1;
    }

    char line[512];
    int in_rules = 0;

    // initialize config with default values
    config->max_workspaces = MAX_WORKSPACES_DEFAULT;
    config->max_clients = MAX_CLIENTS_DEFAULT;
    config->gaps = GAP_SIZE_DEFAULT;
    config->initial_column_size = 800;
    config->step_size = 50;
    config->min_column_size = 300;
    config->max_column_size = 2000;
    config->mod_key = Mod4Mask;
    memset(config->workspace_switch_keys, NoSymbol, sizeof(config->workspace_switch_keys));
    for (int i = 0; i < config->max_workspaces; i++) {
        config->workspace_switch_keys[i] = XStringToKeysym("1") + i;
    }
    config->kill_window_key = XStringToKeysym("q");
    config->move_left_key = XStringToKeysym("Left");
    config->move_right_key = XStringToKeysym("Right");
    config->change_layout_key = XStringToKeysym("space");
    config->scroll_left_key = XStringToKeysym("h");
    config->scroll_right_key = XStringToKeysym("l");
    config->resize_left_key = XStringToKeysym("Left");
    config->resize_right_key = XStringToKeysym("Right");
    config->rule_count = 0;

    while (fgets(line, sizeof(line), file)) {
        char *trimmed = trim(line);

        // ignore comments and empty lines
        if (*trimmed == '#' || *trimmed == '\0') continue;

        // check for section headers
        if (*trimmed == '[') {
            if (strncmp(trimmed, "[rules]", 7) == 0) {
                in_rules = 1;
            } else {
                in_rules = 0;
            }
            continue;
        }

        if (in_rules) {
            // parse application-specific rules
            char *eq = strchr(trimmed, '=');
            if (!eq) continue;
            *eq = '\0';
            char *app = trim(trimmed);
            char *size_str = trim(eq + 1);
            if (config->rule_count < MAX_RULES) {
                strncpy(config->rules[config->rule_count].app_name, app, MAX_APP_NAME_LEN - 1);
                config->rules[config->rule_count].app_name[MAX_APP_NAME_LEN - 1] = '\0';
                config->rules[config->rule_count].column_size = atoi(size_str);
                config->rule_count++;
            }
        } else {
            // parse key=value pairs
            char *eq = strchr(trimmed, '=');
            if (!eq) continue;
            *eq = '\0';
            char *key = trim(trimmed);
            char *value = trim(eq + 1);

            if (strcmp(key, "max_workspaces") == 0) {
                int mw = atoi(value);
                config->max_workspaces = (mw > MAX_WORKSPACES_MAX) ? MAX_WORKSPACES_MAX : mw;
            } else if (strcmp(key, "max_clients") == 0) {
                int mc = atoi(value);
                config->max_clients = (mc > MAX_CLIENTS_MAX) ? MAX_CLIENTS_MAX : mc;
            } else if (strcmp(key, "gaps") == 0) {
                config->gaps = atoi(value);
            } else if (strcmp(key, "initial_column_size") == 0) {
                config->initial_column_size = atoi(value);
            } else if (strcmp(key, "step_size") == 0) {
                config->step_size = atoi(value);
            } else if (strcmp(key, "min_column_size") == 0) {
                config->min_column_size = atoi(value);
            } else if (strcmp(key, "max_column_size") == 0) {
                config->max_column_size = atoi(value);
            } else if (strcmp(key, "mod_key") == 0) {
                config->mod_key = parse_mod_key(value);
            } else if (strcmp(key, "workspace_switch_keys") == 0) {
                // assume keys are comma-separated
                char *token = strtok(value, ",");
                int idx = 0;
                while (token && idx < config->max_workspaces) {
                    config->workspace_switch_keys[idx++] = XStringToKeysym(trim(token));
                    token = strtok(NULL, ",");
                }
                // fill remaining with NoSymbol
                for (; idx < config->max_workspaces; idx++) {
                    config->workspace_switch_keys[idx] = NoSymbol;
                }
            } else if (strcmp(key, "kill_window_key") == 0) {
                config->kill_window_key = XStringToKeysym(value);
            } else if (strcmp(key, "move_left_key") == 0) {
                config->move_left_key = XStringToKeysym(value);
            } else if (strcmp(key, "move_right_key") == 0) {
                config->move_right_key = XStringToKeysym(value);
            } else if (strcmp(key, "change_layout_key") == 0) {
                config->change_layout_key = XStringToKeysym(value);
            } else if (strcmp(key, "scroll_left_key") == 0) {
                config->scroll_left_key = XStringToKeysym(value);
            } else if (strcmp(key, "scroll_right_key") == 0) {
                config->scroll_right_key = XStringToKeysym(value);
            } else if (strcmp(key, "resize_left_key") == 0) {
                config->resize_left_key = XStringToKeysym(value);
            } else if (strcmp(key, "resize_right_key") == 0) {
                config->resize_right_key = XStringToKeysym(value);
            }
        }
    }

    fclose(file);
    return 0;
}

// get window class name
int get_window_class(Window w, char *class_name, size_t len) {
    XClassHint class_hint;
    if (XGetClassHint(dpy, w, &class_hint)) {
        if (class_hint.res_class) {
            strncpy(class_name, class_hint.res_class, len - 1);
            class_name[len - 1] = '\0';
            XFree(class_hint.res_class);
            if (class_hint.res_name) XFree(class_hint.res_name);
            return 1;
        }
        if (class_hint.res_name) XFree(class_hint.res_name);
    }
    return 0;
}

// retrieve window type
WindowType get_window_type(Window w) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;
    WindowType type = NORMAL; // default type

    Atom wm_window_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom net_wm_window_type_normal = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_NORMAL", False);
    Atom net_wm_window_type_dialog = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    Atom net_wm_window_type_tooltip = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_TOOLTIP", False);
    Atom net_wm_window_type_notification = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);

    if (XGetWindowProperty(dpy, w, wm_window_type, 0, (~0L), False, XA_ATOM,
                           &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success) {
        if (prop) {
            Atom window_type = *(Atom *)prop;
            if (window_type == net_wm_window_type_normal) {
                type = NORMAL;
            } else if (window_type == net_wm_window_type_dialog ||
                       window_type == net_wm_window_type_tooltip ||
                       window_type == net_wm_window_type_notification) {
                type = FLOATING;
            }
            XFree(prop);
        }
    }

    return type;
}

// grid layout
void horizontal_grid(int screen_w, int screen_h) {
    int n = client_count[current_workspace];
    if (n == 0) return;

    XWindowChanges wc;
    unsigned int value_mask = CWX | CWY | CWWidth | CWHeight;

    // check if only one normal window exists
    if (n == 1) {
        // find the single normal window
        for (int i = 0; i < config.max_clients; i++) {
            if (clients[current_workspace][i].window != None && clients[current_workspace][i].type == NORMAL) {
                Client *c = &clients[current_workspace][i];
                c->x = config.gaps;
                c->y = config.gaps;
                c->w = screen_w - 2 * config.gaps;
                c->h = screen_h - 2 * config.gaps;
                wc.x = c->x;
                wc.y = c->y;
                wc.width = c->w;
                wc.height = c->h;
                XConfigureWindow(dpy, c->window, value_mask, &wc);
                break;
            }
        }
    } else {
        // position each normal window in its own column using custom_width
        int current_x = config.gaps - view_offset[current_workspace];
        for (int i = 0; i < config.max_clients; i++) {
            if (clients[current_workspace][i].window != None && clients[current_workspace][i].type == NORMAL) {
                Client *c = &clients[current_workspace][i];
                // enforce min and max column sizes
                c->custom_width = CLAMP(c->custom_width, config.min_column_size, config.max_column_size);
                c->x = current_x;
                c->y = config.gaps;
                c->w = c->custom_width;
                c->h = screen_h - 2 * config.gaps;
                wc.x = c->x;
                wc.y = c->y;
                wc.width = c->w;
                wc.height = c->h;
                XConfigureWindow(dpy, c->window, value_mask, &wc);

                current_x += c->w + config.gaps; // increment x position for next window
            }
        }
    }

    // handle floating windows separately
    for (int i = 0; i < config.max_clients; i++) {
        if (clients[current_workspace][i].window != None && clients[current_workspace][i].type == FLOATING) {
            Client *c = &clients[current_workspace][i];
            // center floating windows without altering their size
            c->x = (screen_w - c->w) / 2; // center horizontally
            c->y = (screen_h - c->h) / 2; // center vertically
            XMoveResizeWindow(dpy, c->window, c->x, c->y, c->w, c->h);
        }
    }

    // ensure floating windows are raised above normal windows
    for (int i = 0; i < config.max_clients; i++) {
        if (clients[current_workspace][i].window != None && clients[current_workspace][i].type == FLOATING) {
            XRaiseWindow(dpy, clients[current_workspace][i].window);
        }
    }
}

// scroll left by decreasing the view offset
void scroll_left(int workspace) {
    int total_scroll = SCROLL_STEP[workspace];
    int steps = SCROLL_ANIMATION_STEPS;
    int step_size = total_scroll / steps;
    int remainder = total_scroll % steps;

    for (int i = 0; i < steps; i++) {
        view_offset[workspace] -= step_size;
        if (i == steps - 1) {
            // add any remaining pixels to ensure total_scroll is achieved
            view_offset[workspace] -= remainder;
        }
        arrange();
        usleep(SCROLL_ANIMATION_DELAY);
    }
}

// scroll right by increasing the view offset
void scroll_right(int workspace) {
    int total_scroll = SCROLL_STEP[workspace];
    int steps = SCROLL_ANIMATION_STEPS;
    int step_size = total_scroll / steps;
    int remainder = total_scroll % steps;

    for (int i = 0; i < steps; i++) {
        view_offset[workspace] += step_size;
        if (i == steps - 1) {
            // add any remaining pixels to ensure total_scroll is achieved
            view_offset[workspace] += remainder;
        }
        arrange();
        usleep(SCROLL_ANIMATION_DELAY);
    }
}

// error handler to ignore certain X errors
int xerror(Display *dpy, XErrorEvent *ee) {
    (void)dpy;
    if (ee->error_code == BadWindow) return 0;
    fprintf(stderr, "rude: X error: request code=%d, error code=%d\n", ee->request_code, ee->error_code);
    return 0;
}

// manage a new window
void manage_window(Window w, int workspace, int is_new) {
    if (workspace < 0 || workspace >= config.max_workspaces || 
        (client_count[workspace] + floating_client_count[workspace]) >= config.max_clients) return;
    for (int i = 0; i < config.max_clients; i++) {
        if (clients[workspace][i].window == None) {
            clients[workspace][i].window = w;
            clients[workspace][i].type = get_window_type(w); // categorize window

            // default column width
            clients[workspace][i].custom_width = config.initial_column_size;

            // apply application-specific rules
            char app_class[MAX_APP_NAME_LEN];
            if (get_window_class(w, app_class, sizeof(app_class))) {
                for (int r = 0; r < config.rule_count; r++) {
                    if (strcasecmp(app_class, config.rules[r].app_name) == 0) {
                        clients[workspace][i].custom_width = config.rules[r].column_size;
                        break;
                    }
                }
            }

            if (is_new) {
                if (clients[workspace][i].type == NORMAL) {
                    client_count[workspace]++;
                    // set initial size and position off-screen for normal windows
                    XMoveResizeWindow(dpy, w, -1, -1, 1, 1);
                } else {
                    floating_client_count[workspace]++;
                    // do NOT set size for floating windows; allow them to manage their own size
                }
                XSelectInput(dpy, w, EnterWindowMask | FocusChangeMask | StructureNotifyMask);
                
                // if this is the second normal window, adjust main_window_ratio
                if (client_count[workspace] == 2 && clients[workspace][i].type == NORMAL) {
                    main_window_ratio[workspace] = 0.5;
                }
            }
            update_client_list();
            return;
        }
    }
}

// unmanage (remove) a window
void unmanage_window(Window w, int workspace) {
    if (workspace < 0 || workspace >= config.max_workspaces) return;
    for (int i = 0; i < config.max_clients; i++) {
        if (clients[workspace][i].window == w) {
            // determine the type of the window
            WindowType type = clients[workspace][i].type;

            // remove the window
            clients[workspace][i].window = None;
            
            // shift all subsequent windows
            for (int j = i; j < config.max_clients - 1; j++) {
                clients[workspace][j] = clients[workspace][j + 1];
            }
            clients[workspace][config.max_clients - 1].window = None;
            clients[workspace][config.max_clients - 1].type = NORMAL; // reset type
            clients[workspace][config.max_clients - 1].custom_width = config.initial_column_size; // reset custom_width

            // decrement the appropriate client count
            if (type == NORMAL) {
                client_count[workspace]--;
                // reset main_window_ratio if necessary
                if (client_count[workspace] == 1) {
                    main_window_ratio[workspace] = 1.0;
                }
            } else {
                floating_client_count[workspace]--;
            }

            update_client_list();
            return;
        }
    }
}

// focus a specific window
void focus_window(Window w) {
    if (w != None && w != root) {
        XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);
        XRaiseWindow(dpy, w);
        update_net_active_window(w);

        // if the window is floating, ensure it's raised above others
        for (int ws = 0; ws < config.max_workspaces; ws++) {
            for (int i = 0; i < config.max_clients; i++) {
                if (clients[ws][i].window == w && clients[ws][i].type == FLOATING) {
                    XRaiseWindow(dpy, w);
                    break;
                }
            }
        }
    }
}

// arrange windows based on the current layout
void arrange(void) {
    int screen_w = DisplayWidth(dpy, screen);
    int screen_h = DisplayHeight(dpy, screen);
    
    layouts[current_layout].arrange(screen_w, screen_h);
    XSync(dpy, False);
}

// switch to a different workspace
void switch_workspace(int new_workspace) {
    if (new_workspace == current_workspace || new_workspace < 0 || new_workspace >= config.max_workspaces) return;
    for (int i = 0; i < config.max_clients; i++) {
        if (clients[current_workspace][i].window != None) XUnmapWindow(dpy, clients[current_workspace][i].window);
        if (clients[new_workspace][i].window != None) XMapWindow(dpy, clients[new_workspace][i].window);
    }
    current_workspace = new_workspace;
    update_net_current_desktop();
    arrange();
    for (int i = 0; i < config.max_clients; i++) {
        if (clients[current_workspace][i].window != None) {
            focus_window(clients[current_workspace][i].window);
            break;
        }
    }
}

// kill the currently focused window
void kill_focused_window(void) {
    Window focused;
    int revert_to;
    XGetInputFocus(dpy, &focused, &revert_to);
    if (focused == None || focused == root) return;

    Atom wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
    Atom wm_delete_window = XInternAtom(dpy, "WM_DELETE_WINDOW", False);

    if (wm_protocols != None && wm_delete_window != None) {
        XEvent ev = {0};
        ev.type = ClientMessage;
        ev.xclient.window = focused;
        ev.xclient.message_type = wm_protocols;
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = wm_delete_window;
        ev.xclient.data.l[1] = CurrentTime;
        XSendEvent(dpy, focused, False, NoEventMask, &ev);
    } else {
        XKillClient(dpy, focused);
    }
}

// move a window left or right in the current layout
void move_window(int direction) {
    int n = client_count[current_workspace];
    if (n <= 1) return;  // no need to move if there's only one window or none

    // calculate the new index
    int new_index = (last_moved_window_index + direction + n) % n;

    // find the actual array indices for the windows we're swapping
    int current_array_index = -1, new_array_index = -1;
    for (int i = 0, count = 0; i < config.max_clients; i++) {
        if (clients[current_workspace][i].window != None && clients[current_workspace][i].type == NORMAL) {
            if (count == last_moved_window_index) current_array_index = i;
            if (count == new_index) new_array_index = i;
            count++;
            if (current_array_index != -1 && new_array_index != -1) break;
        }
    }

    if (current_array_index != -1 && new_array_index != -1) {
        // swap the windows
        Client temp = clients[current_workspace][current_array_index];
        clients[current_workspace][current_array_index] = clients[current_workspace][new_array_index];
        clients[current_workspace][new_array_index] = temp;

        // update the last moved window index
        last_moved_window_index = new_index;

        // rearrange and focus the moved window
        arrange();
        focus_window(clients[current_workspace][new_array_index].window);
    }
}

// resize the focused window (column for normal, size for floating)
void resize_focused_window(int direction) {
    Window focused;
    int revert_to;
    XGetInputFocus(dpy, &focused, &revert_to);
    if (focused == None || focused == root) return;

    // find the focused window's client
    Client *c = NULL;
    for (int i = 0; i < config.max_clients; i++) {
        if (clients[current_workspace][i].window == focused) {
            c = &clients[current_workspace][i];
            break;
        }
    }
    if (c == NULL) return;

    if (c->type == NORMAL) {
        // adjust custom_width for the focused window
        c->custom_width += direction * config.step_size; // adjust by step_size from config
        c->custom_width = CLAMP(c->custom_width, config.min_column_size, config.max_column_size); // enforce bounds
    } else if (c->type == FLOATING) {
        // adjust floating window's width
        c->w += direction * config.step_size; // adjust by step_size from config
        c->w = CLAMP(c->w, 200, 1000); // enforce bounds (could also be configurable)
        
        // reposition floating window to keep it centered
        int screen_w = DisplayWidth(dpy, screen);
        int screen_h = DisplayHeight(dpy, screen);
        c->x = (screen_w - c->w) / 2;
        c->y = (screen_h - c->h) / 2;
        XMoveResizeWindow(dpy, c->window, c->x, c->y, c->w, c->h);
    }
    arrange();
}

// release resources
void cleanup(void) {
    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    XCloseDisplay(dpy);
}

// initialize EWMH properties
void init_ewmh(void) {
    net_supported = XInternAtom(dpy, "_NET_SUPPORTED", False);
    net_client_list = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    net_number_of_desktops = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
    net_current_desktop = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
    net_active_window = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);

    Atom supported[] = {
        net_supported,
        net_client_list,
        net_number_of_desktops,
        net_current_desktop,
        net_active_window
    };

    XChangeProperty(dpy, root, net_supported, XA_ATOM, 32, PropModeReplace, (unsigned char *)supported, LENGTH(supported));

    long number_of_desktops = config.max_workspaces;
    XChangeProperty(dpy, root, net_number_of_desktops, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&number_of_desktops, 1);

    update_net_current_desktop();
}

// update the EWMH _NET_CLIENT_LIST property
void update_client_list(void) {
    Window client_list[MAX_WORKSPACES_MAX * MAX_CLIENTS_MAX];
    int total_clients = 0;

    for (int i = 0; i < config.max_workspaces; i++) {
        for (int j = 0; j < config.max_clients; j++) {
            if (clients[i][j].window != None) {
                client_list[total_clients++] = clients[i][j].window;
            }
        }
    }

    XChangeProperty(dpy, root, net_client_list, XA_WINDOW, 32, PropModeReplace, (unsigned char *)client_list, total_clients);
}

// update the EWMH _NET_CURRENT_DESKTOP property
void update_net_current_desktop(void) {
    long desktop = current_workspace;
    XChangeProperty(dpy, root, net_current_desktop, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&desktop, 1);
}

// update the EWMH _NET_ACTIVE_WINDOW property
void update_net_active_window(Window w) {
    XChangeProperty(dpy, root, net_active_window, XA_WINDOW, 32, PropModeReplace, (unsigned char *)&w, 1);
}

// main function
int main(void) {
    // load configuration
    const char *home = getenv("HOME");
    if (!home) home = ".";
    char full_config_path[512];
    snprintf(full_config_path, sizeof(full_config_path), "%s/%s", home, CONFIG_PATH);

    load_config(full_config_path, &config);

    // initialize global settings based on config
    for (int i = 0; i < config.max_workspaces; i++) {
        COLUMN_WIDTH[i] = config.initial_column_size;
        SCROLL_STEP[i] = config.step_size;
    }

    if (!(dpy = XOpenDisplay(NULL))) {
        fprintf(stderr, "rude: cannot open display\n");
        return 1;
    }

    XSetErrorHandler(xerror);
    atexit(cleanup);
    signal(SIGTERM, exit);
    signal(SIGINT, exit);

    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask);

    // set WM_NAME
    Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
    Atom utf8_string = XInternAtom(dpy, "UTF8_STRING", False);
    const char *wm_name = "rude";
    XChangeProperty(dpy, root, net_wm_name, utf8_string, 8, PropModeReplace, (unsigned char *)wm_name, strlen(wm_name));

    // supporting WM Check
    Window check_window = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
    Atom net_supporting_wm_check = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
    XChangeProperty(dpy, root, net_supporting_wm_check, XA_WINDOW, 32, PropModeReplace, (unsigned char *)&check_window, 1);
    XChangeProperty(dpy, check_window, net_supporting_wm_check, XA_WINDOW, 32, PropModeReplace, (unsigned char *)&check_window, 1);
    XChangeProperty(dpy, check_window, net_wm_name, utf8_string, 8, PropModeReplace, (unsigned char *)wm_name, strlen(wm_name));

    // initialize EWMH
    init_ewmh();

    // grab workspace switch keys
    for (int i = 0; i < config.max_workspaces; i++) {
        if (config.workspace_switch_keys[i] != NoSymbol) {
            KeyCode kc = XKeysymToKeycode(dpy, config.workspace_switch_keys[i]);
            XGrabKey(dpy, kc, config.mod_key, root, True, GrabModeAsync, GrabModeAsync);
        }
    }

    // grab other keys
    if (config.kill_window_key != NoSymbol) {
        KeyCode kc = XKeysymToKeycode(dpy, config.kill_window_key);
        XGrabKey(dpy, kc, config.mod_key, root, True, GrabModeAsync, GrabModeAsync);
    }
    if (config.move_left_key != NoSymbol) {
        KeyCode kc = XKeysymToKeycode(dpy, config.move_left_key);
        XGrabKey(dpy, kc, config.mod_key, root, True, GrabModeAsync, GrabModeAsync);
    }
    if (config.move_right_key != NoSymbol) {
        KeyCode kc = XKeysymToKeycode(dpy, config.move_right_key);
        XGrabKey(dpy, kc, config.mod_key, root, True, GrabModeAsync, GrabModeAsync);
    }
    if (config.change_layout_key != NoSymbol) {
        KeyCode kc = XKeysymToKeycode(dpy, config.change_layout_key);
        XGrabKey(dpy, kc, config.mod_key, root, True, GrabModeAsync, GrabModeAsync);
    }
    if (config.scroll_left_key != NoSymbol) {
        KeyCode kc = XKeysymToKeycode(dpy, config.scroll_left_key);
        XGrabKey(dpy, kc, config.mod_key, root, True, GrabModeAsync, GrabModeAsync);
    }
    if (config.scroll_right_key != NoSymbol) {
        KeyCode kc = XKeysymToKeycode(dpy, config.scroll_right_key);
        XGrabKey(dpy, kc, config.mod_key, root, True, GrabModeAsync, GrabModeAsync);
    }

    // grab resize keys: Mod + Shift + Left/Right
    if (config.resize_left_key != NoSymbol) {
        KeyCode kc = XKeysymToKeycode(dpy, config.resize_left_key);
        XGrabKey(dpy, kc, config.mod_key | ShiftMask, root, True, GrabModeAsync, GrabModeAsync);
    }
    if (config.resize_right_key != NoSymbol) {
        KeyCode kc = XKeysymToKeycode(dpy, config.resize_right_key);
        XGrabKey(dpy, kc, config.mod_key | ShiftMask, root, True, GrabModeAsync, GrabModeAsync);
    }

    // initialize main_window_ratio and view_offset for each workspace
    for (int i = 0; i < config.max_workspaces; i++) {
        main_window_ratio[i] = 1.0;
        view_offset[i] = 0; // initialize view offset
    }

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
            KeySym keysym = XkbKeycodeToKeysym(dpy, ev.xkey.keycode, 0, 0);
            unsigned int modifiers = ev.xkey.state;

            // workspace switching
            for (int i = 0; i < config.max_workspaces; i++) {
                if (keysym == config.workspace_switch_keys[i] && (modifiers & config.mod_key)) {
                    switch_workspace(i);
                    break;
                }
            }

            // kill window
            if (keysym == config.kill_window_key && (modifiers & config.mod_key)) {
                kill_focused_window();
            }

            // move window Left/Right
            if ((keysym == config.move_left_key || keysym == config.move_right_key) && (modifiers & config.mod_key) && !(modifiers & ShiftMask)) {
                move_window(keysym == config.move_left_key ? -1 : 1);
            }

            // scroll Left/Right
            if (keysym == config.scroll_left_key && (modifiers & config.mod_key)) {
                scroll_left(current_workspace);
            }
            if (keysym == config.scroll_right_key && (modifiers & config.mod_key)) {
                scroll_right(current_workspace);
            }

            // resize window Left/Right
            if ((keysym == config.resize_left_key || keysym == config.resize_right_key) && 
                (modifiers & config.mod_key) && (modifiers & ShiftMask)) {
                resize_focused_window(keysym == config.resize_left_key ? -1 : 1);
            }

            // change Layout (placeholder for future layouts)
            if (keysym == config.change_layout_key && (modifiers & config.mod_key)) {
                // currently only one layout; implement switching if more layouts are added
            }
        }
    }

    return 0;
}
