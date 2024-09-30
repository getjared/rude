rude.c (rude little window manager)
-----------------------------

hey, you. yeah, you. welcome to rude.
it's a 1 file window manager. it manages windows. shocking, right?

what it does:
-------------
- infinite horizontal grid (*because who doesn't love niri?*)
- gives you 9 workspaces (*because 10 would be too mainstream*)
- lets you have gaps between windows (*for those who fear commitment*)
- controlled by keyboard (*mouse users, we don't judge... much*)
- basic ewmh support (*we play nice with others, sometimes*)

what you need:
--------------
- x11 libs
- gcc or any c compiler
- make

how to make it yours:
---------------------
```
1. clone it:
   git clone https://github.com/getjared/rude.git
   cd rude

2. build it:
   make
   sudo make install

3. use it:
   add this to your .xinitrc:
   exec rude

   then cross your fingers and startx
   if you change anything in the config, just cd into the folder, rm -f rude, and make, sudo make install again.
```

*pro tip: rude doesn't launch apps for you. use sxhkd or something.*

how to boss it around:
----------------------
```
mOD + 1-9                : jump to workspace, like a boss
mOD + q                  : kill window, no mercy
mOD + LEFT/RIGHT         : move windows left/right, it's like tetris but less fun
mOD + SHIFT + LEFT/RIGHT : resize main window, for when size matters
mod + h/l                : scroll through the grid. .
```

final words:
------------
this wm is my own little project and my main driver. feel free to use it or laugh at it.

please check the docs.md for more details (it's still a work in progress but it's getting there. .)

credits for inspiration to tinywm & niri.
