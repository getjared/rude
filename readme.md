rude.c (rude little window manager)
-----------------------------

hey, you. yeah, you. welcome to rude.
it's a 1 file window manager. it manages windows. shocking, right?

what it does:
-------------
- master stack tiling (*because who doesn't love a good stack?*)
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
   if you change anything in the source, just cd into the folder, rm -f rude, and make, sudo make install again.
```

*pro tip: rude doesn't launch apps for you. use sxhkd or something.*

how to boss it around:
----------------------
```
MOD + 1-9                : jump to workspace, like a boss
MOD + q                  : kill window, no mercy
MOD + LEFT/RIGHT         : move windows left/right, it's like tetris but less fun
MOD + SHIFT + LEFT/RIGHT : resize main window, for when size matters
```

final words:
------------
this wm is my own little project and my main driver. feel free to use it or laugh at it.

please check the docs.md for more details.
