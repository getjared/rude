                              rude.c
                 <https://github.com/getjared/rude>

features:
• minimalist floating window manager for x11
• written in c with minimal dependencies
• lightweight and fast
• completely endless canvas
• ewmh compliant

see ./makefile for a easy installation


§ installation

git clone https://github.com/getjared/rude.git
cd rude
make
sudo make install

dependencies:
• c compiler (gcc or clang)
• make
• x11 libraries and headers (xlib)


§ quick-start

key bindings:
• mod4 + left click: move window
• mod4 + right click: resize window
• mod4 + q: close focused window
• mod4 + space: toggle zoom mode
• mod4 + shift + left/right: scroll viewport

configuration:
rude is designed to be simple and minimalist. configuration is done by modifying the source code and recompiling.

launching rude:
add the following line to your ~/.xinitrc file:
    exec rude

then start x with:
    startx


§ contributing

feedback, bug reports, and pull requests are welcome.
please use the github issue tracker for bug reports and feature requests.

keep in mind that rude is intended to remain minimalist and lightweight.
