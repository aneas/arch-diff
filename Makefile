PREFIX  = /usr/local
CFLAGS  = -Wall -O2 -std=gnu99 -pedantic
LDFLAGS =

NAME       = arch-diff
VERSION    = 0.1
PKG_CONFIG = libalpm zlib
SOURCES    = $(wildcard src/*.c)
HEADERS    = $(wildcard src/*.h)

$(NAME): $(SOURCES) $(HEADERS)
	$(CC) -o $@ $(SOURCES) -DNAME="\"$(NAME)\"" -DVERSION="\"$(VERSION)\"" $(CFLAGS) $(LDFLAGS) `pkg-config --cflags --libs $(PKG_CONFIG)`

.PHONY: clean
clean:
	rm -f $(NAME)

.PHONY: install
install: $(NAME)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp $< $(DESTDIR)$(PREFIX)/bin/$(NAME)

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(NAME)
