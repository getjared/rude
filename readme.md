# rude

a minimal x11 window manager

## features

- infinite scrollable workspace
- no workspaces, just endless space
- zoom out to see all your windows at once
- minimal
- floating
- EWMH support for compatibility with status bars and scripts
- written in less than 300 lines of c

## keybindings

- `super + left click`: move window
- `super + right click`: resize window
- `super + shift + left/right arrow`: scroll the infinite canvas
- `super + spacebar`: toggle zoom (see all windows / return to normal view)
- `super + q`: close window

## how to build

*this does not have any external keybinds, so use something like sxhkd*

make sure you have `libx11-dev` installed, then:

```
git clone https://github.com/getjared/rude.git
cd rude
 -make
 -sudo make install
```

## how to use

add this to your `.xinitrc`:

i also suggest adding ```setxkbmap -option terminate:ctrl_alt_bksp``` to kill the wm.

```
exec rude
```

then start x with `startx`

## why ..

this is my own little wm project, not really meant to be used by anyone else..

💀
