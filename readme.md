
                              rude.c
                 <https://github.com/getjared/rude>

features:
• floating only
• lightweight and fast

see ./makefile for a easy installation


§ installation

git clone https://github.com/getjared/rude.git
cd rude
make
sudo make install

§ dependencies

• c compiler (gcc or clang)
• make
• x11 libraries and headers (xlib)


§ quick-start

key bindings:
mod4 + q                   : kill focus window
mod4 + mouse               : move windows around

configuration:
rude is designed to be simple and minimalist. configuration is done by modifying the source code and recompiling.
use sxhkd or something similar for basic keybinds, like opening a terminal.

launching rude:
add the following line to your ~/.xinitrc file:
    exec rude

then start x with:
    startx


§ extra

keep in mind that rude is intended to remain minimalist and lightweight, it's my personal daily driver
that i have made from scratch with of course inspiration from sowm & dwm.
