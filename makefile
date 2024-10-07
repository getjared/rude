CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lX11

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

TARGET = rude
SOURCES = rude.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

install: $(TARGET)
	install -D -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all install uninstall clean
