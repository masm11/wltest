#include <stdio.h>
#include <gtk/gtk.h>

static gboolean draw(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 40.0);
    
    cairo_move_to(cr, 10.0, 50.0);
    cairo_show_text(cr, "test");
    
    return FALSE;
}

int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);
    
    GtkWidget *toplevel = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_show(toplevel);
    
    GtkWidget *drawable = gtk_drawing_area_new();
    gtk_widget_show(drawable);
    gtk_widget_set_size_request(drawable, 500 ,500);
    gtk_container_add(GTK_CONTAINER(toplevel), drawable);
    
    g_signal_connect(drawable, "draw", G_CALLBACK(draw), NULL);
    
    gtk_main();
    
    return 0;
}
