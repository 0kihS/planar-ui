#ifndef PANEL_TOPBAR_H
#define PANEL_TOPBAR_H

#include "panel.h"
#include "../src/app.h"

#define APP_TYPE_PANEL_TOPBAR panel_topbar_get_type()
#define APP_PANEL_TOPBAR(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, APP_TYPE_PANEL_TOPBAR, PanelTopbar)

typedef struct _PanelTopbar PanelTopbar;
typedef struct { AppPanelClass parent_class; } PanelTopbarClass;

struct _PanelTopbar {
    AppPanel panel;
    PlanarApp *app;
    GtkWidget *webview;
    guint refresh_timer;
};

GType panel_topbar_get_type(void);

GtkWidget *panel_topbar_new(PlanarApp *app);
void panel_topbar_refresh(GtkWidget *widget);

#endif
