#ifndef PANEL_H
#define PANEL_H

#include <gtk/gtk.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>

#define APP_TYPE_PANEL app_panel_get_type()
#define APP_PANEL(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, APP_TYPE_PANEL, AppPanel)

typedef struct _AppPanel AppPanel;
typedef struct _AppPanelClass AppPanelClass;

struct _AppPanel {
    GtkWindow window;
    GtkWidget *content;
    GtkLayerShellLayer layer;
    GtkLayerShellEdge anchor_top;
    GtkLayerShellEdge anchor_bottom;
    GtkLayerShellEdge anchor_left;
    GtkLayerShellEdge anchor_right;
    int margin_top;
    int margin_bottom;
    int margin_left;
    int margin_right;
    int exclusive_zone;
    gboolean keyboard_interactive;
};

struct _AppPanelClass {
    GtkWindowClass parent_class;
};

GType app_panel_get_type(void);

GtkWidget *app_panel_new(void);

void app_panel_configure(AppPanel *panel,
    GtkLayerShellLayer layer,
    gboolean anchor_top, gboolean anchor_bottom,
    gboolean anchor_left, gboolean anchor_right,
    int margin_top, int margin_bottom, int margin_left, int margin_right,
    int exclusive_zone);

void app_panel_set_content(AppPanel *panel, GtkWidget *content);

void app_panel_show(AppPanel *panel);

#endif
