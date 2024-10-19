CC = gcc
CFLAGS = -Wall -Wextra -O2 
LDFLAGS = -lX11 -lm -lXext

rude: rude.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

.PHONY: clean install

clean:
	rm -f rude

install: rude
	install -D -m 755 rude $(DESTDIR)/usr/local/bin/rude
