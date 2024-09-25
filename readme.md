rudewm (rude window manager)
-----------------------------

hey, you. yeah, you. welcome to rudewm.
it's a window manager. it manages windows. shocking, right?

what it does:
-------------
- tiles your windows (fibonacci & euler style, fancy huh?)
- gives you 9 workspaces (because 10 would be too mainstream)
- lets you have gaps between windows (for those who fear commitment)
- controlled by keyboard (mouse users, we don't judge... much)
- basic ewmh support (we play nice with others, sometimes)

what you need:
--------------
- x11 libs (because we're not savages)
- gcc or any c compiler (preferably one that understands your cursing)
- make (as in 'make it work', but also the program)

how to make it yours:
---------------------
1. clone it:
   git clone https://github.com/getjared/rudewm.git
   cd rudewm

2. build it:
   make
   sudo make install   # sudo because we like to live dangerously

3. configure it:
   edit config.h. go wild. break things. fix them. repeat.

4. use it:
   add this to your .xinitrc:
   exec rudewm

   then cross your fingers and startx

   pro tip: rudewm doesn't launch apps for you. use sxhkd or something.
   we're a window manager, not your personal assistant.

how to boss it around:
----------------------
MOD + 1-9        : jump to workspace, like a boss
MOD + SPACE      : change layout, because change is good
MOD + q          : kill window, no mercy
MOD + arrow keys : move windows, it's like tetris but less fun

final words:
------------
this wm is like a cat. it does what it wants, when it wants.
but it's also kind of cool, so you keep it around.
and by responsibility, we mean the potential to royally mess up your desktop.

enjoy your rudewm experience. or don't. we're a readme, not a cop.
