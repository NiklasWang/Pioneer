WAYLAND_FLAGS = $(shell pkg-config wayland-client --cflags --libs)
WAYLAND_PROTOCOLS_DIR = $(shell pkg-config wayland-protocols --variable=pkgdatadir)
WAYLAND_SCANNER = $(shell pkg-config --variable=wayland_scanner wayland-scanner)

XDG_SHELL_PROTOCOL = $(WAYLAND_PROTOCOLS_DIR)/stable/xdg-shell/xdg-shell.xml

HEADERS=xdg-shell-client-protocol.h
SOURCES=xdg-shell-protocol.c

all: $(HEADERS) $(SOURCES)
	gcc -o seat seat.c $(SOURCES) -I. -lwayland-client

$(HEADERS):
	$(WAYLAND_SCANNER) client-header $(XDG_SHELL_PROTOCOL) $@

$(SOURCES):
	$(WAYLAND_SCANNER) private-code $(XDG_SHELL_PROTOCOL) $@

clean:
	rm -rf point point_egl $(HEADERS) $(SOURCES)
