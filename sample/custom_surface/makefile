WAYLAND_FLAGS = $(shell pkg-config wayland-client --cflags --libs)
WAYLAND_PROTOCOLS_DIR = $(shell pkg-config wayland-protocols --variable=pkgdatadir)
WAYLAND_SCANNER = $(shell pkg-config --variable=wayland_scanner wayland-scanner)

XDG_SHELL_PROTOCOL = $(WAYLAND_PROTOCOLS_DIR)/stable/xdg-shell/xdg-shell.xml

HEADERS=xdg-shell-client-protocol.h
SOURCES=xdg-shell-protocol.c
IMAGE=test.rgb

all: $(IMAGE) $(HEADERS) $(SOURCES)
	gcc -o surface surface.c $(SOURCES) -I. -lwayland-client

$(IMAGE):
	ffmpeg -i /usr/share/backgrounds/Clouds_by_Tibor_Mokanszki.jpg -vf  scale=1080:810 -pix_fmt rgba  $(CURDIR)/test.rgb

$(HEADERS):
	$(WAYLAND_SCANNER) client-header $(XDG_SHELL_PROTOCOL) $@

$(SOURCES):
	$(WAYLAND_SCANNER) private-code $(XDG_SHELL_PROTOCOL) $@

clean:
	rm -rf custom_surface $(HEADERS) $(SOURCES) test.rgb
