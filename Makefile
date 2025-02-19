# See LICENSE file for copyright and license details.
.POSIX:

# swt version
VERSION = 0.1.0

# paths
PREFIX = ~/.local

PKG_CONFIG = pkg-config

# includes and libs
PKGS = xkbcommon wayland-client wayland-cursor fcft pixman-1
INCS = `$(PKG_CONFIG) --cflags $(PKGS)`
LIBS = `$(PKG_CONFIG) --libs $(PKGS)`

# flags
SWTCPPFLAGS = -DVERSION=\"$(VERSION)\" -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600 -D_GNU_SOURCE
SWTCFLAGS   = $(INCS) $(SWTCPPFLAGS) $(CPPFLAGS) $(CFLAGS)
SWTLDFLAGS  = $(LIBS) $(LDFLAGS)

# uncomment if you want ligatures
# SWTCPPFLAGS += -DLIGATURES

# compiler and linker
CC = c99

PROTO = xdg-shell-protocol.h
SRC = swt.c st.c util.c $(PROTO:.h=.c)
OBJ = $(SRC:.c=.o)

all: swt

.c.o:
	$(CC) $(SWTCFLAGS) -c $<

$(OBJ): $(PROTO)

util.o: util.h
st.o: st.h win.h util.h config.h arg.h
swt.o: win.h st.h util.h config.h bufpool.h

swt: $(OBJ)
	$(CC) -o $@ $(OBJ) $(SWTLDFLAGS)

WAYLAND_PROTOCOLS != pkg-config --variable=pkgdatadir wayland-protocols
WAYLAND_SCANNER   != pkg-config --variable=wayland_scanner wayland-scanner

xdg-shell-protocol.h:
	$(WAYLAND_SCANNER) client-header $(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@
xdg-shell-protocol.c:
	$(WAYLAND_SCANNER) private-code $(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

clean:
	rm -f swt $(OBJ) $(PROTO:.h=.c) $(PROTO)

install: swt
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f swt $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/swt

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/swt

.PHONY: all clean install uninstall
