CC = gcc
CFLAGS = -Wall -O2 -g
LDFLAGS = -lX11
PREFIX = /usr/local

all: rude

rude: rude.c rude.h
	$(CC) $(CFLAGS) -o rude rude.c $(LDFLAGS)

install: rude
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp rude $(DESTDIR)$(PREFIX)/bin/
	chmod +x $(DESTDIR)$(PREFIX)/bin/rude

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/rude

clean:
	rm -f rude
