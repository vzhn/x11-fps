#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

struct {
    int width;
    int height;

    Display * display;
    Visual * visual;
    int screen;
    Window root;
    Window window;
    GC gc;

    XImage* pixmap;
    char* pixmap_buffer;
    int depth;
    int pixmap_buffer_size;
} fps_app;

struct {
    int ticks;
    unsigned long long show_millis;
} fps_counter;

static const int REFRESH_PERIOD_MS = 500;

static long epoch_millis() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void x_connect () {
    fps_app.display = XOpenDisplay (NULL);
    if (!fps_app.display) {
        fprintf (stderr, "Could not open display.\n");
        exit (1);
    }
    fps_app.screen = DefaultScreen (fps_app.display);
    fps_app.root = RootWindow (fps_app.display, fps_app.screen);
}

static void create_window () {
    unsigned long xAttrMask = CWBackPixel;
    XSetWindowAttributes xAttr;

    fps_app.width = 400;
    fps_app.height = 300;

    fps_app.window =
            XCreateWindow(fps_app.display,
                          DefaultRootWindow(fps_app.display),
                          0, 0,
                          fps_app.width, fps_app.height, 0, CopyFromParent, CopyFromParent,
                          fps_app.visual,
                          xAttrMask, &xAttr);

    XStoreName(fps_app.display, fps_app.window, "x11_fps");


    XSelectInput (fps_app.display, fps_app.window, ExposureMask|StructureNotifyMask);
    XMapWindow (fps_app.display, fps_app.window);

    XWindowAttributes windowAttributes;
    if (!XGetWindowAttributes(fps_app.display, fps_app.window, &windowAttributes)) {
        fprintf (stderr, "Could not get window attributes.\n");
        exit (1);
    }
    fps_app.depth = windowAttributes.depth;

}

static void set_up_gc () {
    fps_app.screen = DefaultScreen (fps_app.display);
    fps_app.gc = XCreateGC (fps_app.display, fps_app.window, 0, 0);
}

static void set_up_counter() {
    fps_counter.ticks = 0;
    fps_counter.show_millis = epoch_millis();
}

static void set_up_pixmap() {
    const int bytes_per_pixel = 4;
    fps_app.pixmap_buffer_size = fps_app.width * fps_app.height * bytes_per_pixel;
    fps_app.pixmap_buffer = malloc(fps_app.pixmap_buffer_size);
    memset(fps_app.pixmap_buffer, 255, fps_app.pixmap_buffer_size);

    fps_app.pixmap = XCreateImage(
            fps_app.display,
            fps_app.visual,
            fps_app.depth, ZPixmap,
            0, (char*) fps_app.pixmap_buffer,
            fps_app.width, fps_app.height,
            8 * bytes_per_pixel, 0);
}

static void touch_fps_counter() {
    const int msecs_in_second = 1000;
    unsigned long now_ms = epoch_millis();
    unsigned long refresh_delta_ms = now_ms - fps_counter.show_millis;

    ++fps_counter.ticks;
    if (refresh_delta_ms >= REFRESH_PERIOD_MS) {
        char text[80];
        float fps = (float) msecs_in_second * fps_counter.ticks / refresh_delta_ms;
        float throughput_mbytes_per_second =
                (float) 1e-6 * msecs_in_second * fps_app.pixmap_buffer_size * fps_counter.ticks / refresh_delta_ms;

        sprintf(text,
               "[%dx%d] fps: %.2f throughput: %.2f megabytes/sec",
               fps_app.width,
               fps_app.height,
               fps,
               throughput_mbytes_per_second
        );

        XStoreName(fps_app.display, fps_app.window, text);
        fps_counter.ticks = 0;
        fps_counter.show_millis = now_ms;
    }
}

static void reset_fps_counter() {
    fps_counter.ticks = 0;
    fps_counter.show_millis = epoch_millis();
}

static void draw_screen() {
    if (XPutImage(fps_app.display,
              fps_app.window,
              fps_app.gc,
              fps_app.pixmap, 0,0,0,0,
              fps_app.pixmap->width,
              fps_app.pixmap->height)) {

        fprintf (stderr, "Could not draw image\n");
        exit (1);
    };
}

static void event_loop() {
    XEvent exposeEvent;
    memset(&exposeEvent, 0, sizeof(exposeEvent));
    exposeEvent.type = Expose;
    exposeEvent.xexpose.window = fps_app.window;

    while (1) {
        while (XPending(fps_app.display) > 0) {
            XEvent e;
            XNextEvent (fps_app.display, & e);
            if (e.type == Expose) {
                touch_fps_counter();
                draw_screen();
            } else
            if (e.type == ConfigureNotify) {
                XConfigureEvent xce = e.xconfigure;
                if (xce.width != fps_app.width ||
                    xce.height != fps_app.height) {
                    fps_app.width = xce.width;
                    fps_app.height = xce.height;

                    XDestroyImage(fps_app.pixmap);
                    set_up_pixmap();
                    reset_fps_counter();
                }
            }
        }

        XSendEvent(fps_app.display, fps_app.window, False, ExposureMask, &exposeEvent);
        XFlush(fps_app.display);
    }
}

int main (int argc, char ** argv) {
    x_connect();
    create_window();
    set_up_gc();
    set_up_pixmap();
    set_up_counter();
    event_loop();
    return 0;
}