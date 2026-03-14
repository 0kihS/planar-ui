#include <gtk/gtk.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>
#include "panel-notification.h"

G_DEFINE_TYPE(PanelNotification, app_panel_notification, GTK_TYPE_WINDOW)

static const char *NOTIFY_CSS =
    "window { background: transparent; }"
    "box.notification {"
    "  background: rgba(13, 13, 13, 0.95);"
    "  border: 1px solid rgba(232, 0, 26, 0.4);"
    "  border-radius: 0;"
    "  padding: 10px 14px;"
    "  margin: 8px;"
    "  font-size: 11px;"
    "  font-family: 'Share Tech Mono', monospace;"
    "}"
    "label.notification-label {"
    "  color: #ff9040;"
    "}";

static void app_panel_notification_class_init(PanelNotificationClass *class) { (void)class; }

static void app_panel_notification_init(PanelNotification *n) {
    n->webview = NULL;
    n->dismiss_source = 0;

    gtk_window_set_decorated(GTK_WINDOW(n), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(n), FALSE);
}

GtkWidget *panel_notification_new(void) {
    PanelNotification *n = g_object_new(APP_TYPE_PANEL_NOTIFICATION, NULL);

    gtk_layer_init_for_window(GTK_WINDOW(n));
    gtk_layer_set_layer(GTK_WINDOW(n), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_anchor(GTK_WINDOW(n), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(n), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_exclusive_zone(GTK_WINDOW(n), -1);
    gtk_layer_set_margin(GTK_WINDOW(n), GTK_LAYER_SHELL_EDGE_BOTTOM, 60);
    gtk_layer_set_margin(GTK_WINDOW(n), GTK_LAYER_SHELL_EDGE_RIGHT, 60);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(n), FALSE);

    gtk_widget_set_visible(GTK_WIDGET(n), FALSE);

    return GTK_WIDGET(n);
}

static gboolean dismiss_notification(gpointer data) {
    PanelNotification *n = data;
    n->dismiss_source = 0;
    gtk_widget_set_visible(GTK_WIDGET(n), FALSE);
    return G_SOURCE_REMOVE;
}

void panel_notification_show(GtkWidget *widget, const char *text, double duration_seconds) {
    PanelNotification *n = APP_PANEL_NOTIFICATION(widget);

    /* Cancel any pending dismiss */
    if (n->dismiss_source > 0) {
        g_source_remove(n->dismiss_source);
        n->dismiss_source = 0;
    }

    /* Clear old content */
    gtk_window_set_child(GTK_WINDOW(n), NULL);

    /* Build the notification content */
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(box, "notification");
    gtk_widget_set_size_request(box, 250, -1);  /* minimum width */

    GtkWidget *label = gtk_label_new(text);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_max_width_chars(GTK_LABEL(label), 35);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_NONE);
    gtk_widget_add_css_class(label, "notification-label");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(label, TRUE);

    gtk_box_append(GTK_BOX(box), label);
    gtk_window_set_child(GTK_WINDOW(n), box);

    /* Set reasonable window bounds */
    gtk_window_set_default_size(GTK_WINDOW(n), 350, -1);

    /* Apply CSS */
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css, NOTIFY_CSS);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    /* Show */
    gtk_widget_set_visible(GTK_WIDGET(n), TRUE);

    /* Schedule dismiss */
    if (duration_seconds <= 0) duration_seconds = 5.0;
    n->dismiss_source = g_timeout_add((guint)(duration_seconds * 1000), dismiss_notification, n);
}
