// need dependency wayland-client
// which download from https://gitlab.freedesktop.org/wayland/wayland.git
// test connect


#include <wayland-client.h>
#include <stdio.h>

int main()
{
    struct wl_display *display = wl_display_connect(0);

    if (!display)
    {
        fprintf(stderr, "Unable to connect to wayland compositor\n");
        return -1;
    }

    wl_display_disconnect(display);

    return 0;
}
