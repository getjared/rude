# rude

a minimal x11 window manager

## features

- no workspaces, just endless space
- zoom out to see all your windows at once
- minimal and rude
- perfect for tiling window layouts by hand
- swmh support for compatibility with status bars and scripts
- written in less than 300 lines of c

## keybindings

- `super + left click`: move window
- `super + right click`: resize window
- `super + shift + left/right arrow`: scroll the infinite canvas
- `super + spacebar`: toggle zoom (see all windows / return to normal view)
- `super + q`: close window

## how to build

make sure you have `libx11-dev` installed, then:

```
gcc -O3 -o rude rude.c -lX11 -lm
```

## how to use

add this to your `.xinitrc`:

```
exec ./rude
```

or

```
clone it
git clone https://github.com/getjared/rude.git

cd rude

build it with makefile
make
sudo make install
```

then start x with `startx`



## why. .

this is my own little project, not really meant for other people to actually use it, lol..

💀
