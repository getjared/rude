
# rude.c (*rudewm, rude window mngr*)

<a href="https://i.imgur.com/Zzo21vU.png"><img src="https://i.imgur.com/Zzo21vU.png" width="43%" align="right"></a>
<br>
- master stack tiling *fibonacci* & *euler*
- workspaces (up to 9)
- window gaps
- keyboard focus
- move tiled windows around
- basic ewmh support
<br>
<a href="https://i.imgur.com/oWQE8vJ.png"><img src="https://i.imgur.com/oWQE8vJ.png" width="43%" align="right"></a>

**requirements**
- x11 libs
- gcc or c compiler
- make

*note : there is no built-in keybinds for a terminal or anything else really, so make sure you use something like sxhkd*

 clone the repository:
   ```
   git clone https://github.com/getjared/rudewm.git
   cd rudewm

   
   make
   sudo make install
   ```
**config**
```c
#define MAX_WORKSPACES 9 // Define how many workspaces you want
#define MAX_CLIENTS 100 // Max windows per workspace
#define MOD_KEY Mod4Mask // Default is the SUPER key
#define WORKSPACE_SWITCH_KEY XK_1 // Keys 1-9
#define KILL_WINDOW_KEY XK_q // Kill windows with Q
#define CHANGE_LAYOUT_KEY XK_space // Change the tile layout
#define MOVE_LEFT_KEY XK_Left
#define MOVE_RIGHT_KEY XK_Right
#define MOVE_UP_KEY XK_Up
#define MOVE_DOWN_KEY XK_Down
```
**start it up**
1. *i do everything from the .xinitrc file, here's mine. .*
   ```
   #!/bin/sh
   
   # kill wm session with Ctrl + Alt + Backspace
   setxkbmap -option terminate:ctrl_alt_bksp
   
   xrdb -merge $HOME/.Xresources
   xset b off
   xset r rate 250 25
   xsetroot -cursor_name left_ptr
   
   # startup programs
   #picom & # xrender comp
   sxhkd & # keyboard shortcut daemon
   
   # walls
   feh --bg-scale j/wallpapers/ruder.jpg &
   
   # start rude
   exec rudewm
   ```
   
3. Start X with:
   ```
   startx
   ```
**basic shortcuts**
- `MOD + 1-9`: Switch to workspace 1-9
- `MOD + SPACE`: Change tile layouts
- `MOD + q`: Kill focused window
- `MOD + Left/Right/Up/Down`: Move focused window within the tiling layout


final word, this is my own personal wm, it does what i need it to do. . that is all 🖖
also, i'm pretty basic, i don't really add commits to the code, i just remove the entire thing and reupload. :/

*ty dylan@sowm for the cool git-readme*
