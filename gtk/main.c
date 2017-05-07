#define WL_EGL_PLATFORM
#include <stdio.h>
#include <gtk/gtk.h>
#include <EGL/egl.h>
#include <gdk/gdkwayland.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <cairo/cairo-gl.h>

static GtkWidget *drawable;

static EGLDisplay dpy = 0;
static EGLSurface sfc;
static EGLContext ctxt;

static void init_egl(void)
{
    dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    
    int major, minor;
    eglInitialize(dpy, &major, &minor);
    printf("%d.%d\n", major, minor);
    
    int attribs[] = {
	EGL_RED_SIZE,
	8,
	EGL_GREEN_SIZE,
	8,
	EGL_BLUE_SIZE,
	8,
	EGL_ALPHA_SIZE,
	8,
	EGL_DEPTH_SIZE,
	8,
	EGL_SURFACE_TYPE,
	EGL_WINDOW_BIT,
	EGL_NONE,
    };
    EGLConfig config;
    int num_configs;
    eglChooseConfig(dpy, attribs, &config, 1, &num_configs);
    
    struct wl_surface *native_window =
	    gdk_wayland_window_get_wl_surface(gtk_widget_get_window(drawable));
    sfc = eglCreateWindowSurface(dpy, config,
	    wl_egl_window_create(native_window, 500, 500),
	    NULL);
    
    ctxt = eglCreateContext(dpy, config, NULL, NULL);
}

static gboolean draw(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
/*
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 40.0);
    
    cairo_move_to(cr, 10.0, 50.0);
    cairo_show_text(cr, "test");
*/
    
    if (dpy == 0)
	init_egl();
    
    cairo_device_t *device = cairo_egl_device_create(dpy, ctxt);
    cairo_surface_t *surface = cairo_gl_surface_create_for_egl(device, sfc, 500, 500);
    cr = cairo_create(surface);
    
    
    
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    cairo_device_destroy(device);
    
    return FALSE;
}

int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);
    
    GtkWidget *toplevel = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_show(toplevel);
    
    drawable = gtk_drawing_area_new();
    gtk_widget_show(drawable);
    gtk_widget_set_size_request(drawable, 500 ,500);
    gtk_container_add(GTK_CONTAINER(toplevel), drawable);
    
    g_signal_connect(drawable, "draw", G_CALLBACK(draw), NULL);
    
    gtk_main();
    
    return 0;
}
