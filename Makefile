LDFLAGS += -lutil -lm
CFLAGS += -Wall -Wextra -Wpedantic
PREFIX ?= /usr/local
DESTDIR ?=

all: msoak

.PHONY: install
install:
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 msoak $(DESTDIR)$(PREFIX)/bin

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/msoak

.PHONY: clean
clean:
	rm -f msoak
