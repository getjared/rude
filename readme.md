rude.c (rude little window manager)
-----------------------------

hey, you. yeah, you. welcome to rude. .
it's a 1 file window manager. it manages windows. shocking, right?

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
   git clone https://github.com/getjared/rude.git
   cd rude

2. build it:
   make
   sudo make install

3. use it:
   add this to your .xinitrc:
   exec rude

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

<a href="[https://user-images.githubusercontent.com/6799467/66687576-9747c200-ec72-11e9-947d-5b96753eab03.jpg](https://i.imgur.com/vZhbUZ8.png)"><img src="[https://user-images.githubusercontent.com/6799467/66687576-9747c200-ec72-11e9-947d-5b96753eab03.jpg](https://i.imgur.com/vZhbUZ8.png)" width="43%"></a>

final words:
------------
this wm is my own little project and my main driver, feel free to use it or laugh at it. .

please check the docs.md
