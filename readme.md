# § rude.c

rude is a minimalist, lightweight floating window manager for x11.

## § features

• floating only window management

• super lightweight and fast

• simple and hackable codebase

• perfect for minimalists and tinkerers

## § dependencies

• c compiler (gcc or clang)

• make

• x11 libraries and headers (xlib)

## § installation

1. clone the repository:
   ```
   git clone https://github.com/getjared/rude.git
   ```
2. change to the rude directory:
   ```
   cd rude
   ```
3. compile the source:
   ```
   make
   ```
4. install (requires root privileges):
   ```
   sudo make install
   ```

## § usage

key bindings:
• super + q: kill focused window

• super + left mouse button: move window

• super + right mouse button: resize window

starting rude:
add the following line to your ~/.xinitrc file:
```
exec rude
```
then start x with:
```
startx
```

## § configuration

rude is designed to be simple and minimalist. configuration is done by modifying the source code and recompiling.

use something like sxhkd.


after making changes, recompile and reinstall:
```
make
sudo make install
```

## § acknowledgements

rude draws inspiration from other minimal window managers like sowm and dwm.

---

remember: rude is intentionally minimalist and lightweight. it's designed as a personal daily driver for those who appreciate simplicity.
