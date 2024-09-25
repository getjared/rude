rudewm (rude window manager)
-----------------------------

hey, you. yeah, you. welcome to rudewm.
it's a window manager. it manages windows. shocking, right?

what it does:
-------------
- master stack tiling  (*fibonacci & euler style, fancy huh?*)
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
   git clone https://github.com/getjared/rudewm.git
   cd rudewm

2. build it:
   make
   sudo make install

3. use it:
   add this to your .xinitrc:
   exec rudewm

   then cross your fingers and startx
```
   
   
   *pro tip: rude doesn't launch apps for you. use sxhkd or something.*
   

how to boss it around:
----------------------
```
MOD + 1-9        : jump to workspace, like a boss
MOD + SPACE      : change layout, because change is good
MOD + q          : kill window, no mercy
MOD + arrow keys : move windows, it's like tetris but less fun
```

final words:
------------
this wm is my own little project and my main driver, feel free to use it or laugh at it. .

enjoy your rudewm experience. or don't. we're a readme, not a cop.
