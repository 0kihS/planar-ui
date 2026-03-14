#include <gtk/gtk.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>
#include "panel.h"

static void app_panel_class_init(AppPanelClass *class);
static void app_panel_init(AppPanel *panel);

G_DEFINE_TYPE(AppPanel, app_panel, GTK_TYPE_WINDOW)

void app_panel_class_init(AppPanelClass *class) {
    (void)class;
}

void app_panel_init(AppPanel *panel) {
    gtk_window_set_decorated(GTK_WINDOW(panel), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(panel), TRUE);

    /* Make the GTK window itself transparent so WebKit content controls the bg */
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css, "window { background: transparent; }");
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    panel->content = NULL;
    panel->layer = GTK_LAYER_SHELL_LAYER_OVERLAY;
    panel->anchor_top = FALSE;
    panel->anchor_bottom = FALSE;
    panel->anchor_left = FALSE;
    panel->anchor_right = FALSE;
    panel->margin_top = 0;
    panel->margin_bottom = 0;
    panel->margin_left = 0;
    panel->margin_right = 0;
    panel->exclusive_zone = 0;
    panel->keyboard_interactive = FALSE;
}

GtkWidget *app_panel_new(void) {
    AppPanel *panel = g_object_new(APP_TYPE_PANEL, NULL);
    return GTK_WIDGET(panel);
}

void app_panel_configure(AppPanel *panel,
    GtkLayerShellLayer layer,
    gboolean anchor_top, gboolean anchor_bottom,
    gboolean anchor_left, gboolean anchor_right,
    int margin_top, int margin_bottom, int margin_left, int margin_right,
    int exclusive_zone) {
    
    panel->layer = layer;
    panel->anchor_top = anchor_top;
    panel->anchor_bottom = anchor_bottom;
    panel->anchor_left = anchor_left;
    panel->anchor_right = anchor_right;
    panel->margin_top = margin_top;
    panel->margin_bottom = margin_bottom;
    panel->margin_left = margin_left;
    panel->margin_right = margin_right;
    panel->exclusive_zone = exclusive_zone;
}

void app_panel_set_content(AppPanel *panel, GtkWidget *content) {
    if (panel->content) {
        gtk_window_set_child(GTK_WINDOW(panel), NULL);
    }
    panel->content = content;
    if (content) {
        gtk_window_set_child(GTK_WINDOW(panel), content);
    }
}

void app_panel_show(AppPanel *panel) {
    gtk_layer_init_for_window(GTK_WINDOW(panel));
    gtk_layer_set_layer(GTK_WINDOW(panel), panel->layer);
    
    gtk_layer_set_anchor(GTK_WINDOW(panel), GTK_LAYER_SHELL_EDGE_TOP, panel->anchor_top);
    gtk_layer_set_anchor(GTK_WINDOW(panel), GTK_LAYER_SHELL_EDGE_BOTTOM, panel->anchor_bottom);
    gtk_layer_set_anchor(GTK_WINDOW(panel), GTK_LAYER_SHELL_EDGE_LEFT, panel->anchor_left);
    gtk_layer_set_anchor(GTK_WINDOW(panel), GTK_LAYER_SHELL_EDGE_RIGHT, panel->anchor_right);
    
    if (panel->margin_top > 0)
        gtk_layer_set_margin(GTK_WINDOW(panel), GTK_LAYER_SHELL_EDGE_TOP, panel->margin_top);
    if (panel->margin_bottom > 0)
        gtk_layer_set_margin(GTK_WINDOW(panel), GTK_LAYER_SHELL_EDGE_BOTTOM, panel->margin_bottom);
    if (panel->margin_left > 0)
        gtk_layer_set_margin(GTK_WINDOW(panel), GTK_LAYER_SHELL_EDGE_LEFT, panel->margin_left);
    if (panel->margin_right > 0)
        gtk_layer_set_margin(GTK_WINDOW(panel), GTK_LAYER_SHELL_EDGE_RIGHT, panel->margin_right);
    
    if (panel->exclusive_zone != 0)
        gtk_layer_set_exclusive_zone(GTK_WINDOW(panel), panel->exclusive_zone);
    
    if (panel->keyboard_interactive)
        gtk_layer_set_keyboard_mode(GTK_WINDOW(panel), TRUE);
    
    gtk_widget_set_visible(GTK_WIDGET(panel), TRUE);
}
