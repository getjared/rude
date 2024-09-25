CC = gcc
CFLAGS = -Wall -Wextra -O2 
LDFLAGS = -lX11 -lm

rudewm: rude.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

.PHONY: clean install

clean:
	rm -f rudewm

install: rudewm
	install -D -m 755 rudewm $(DESTDIR)/usr/local/bin/rudewm