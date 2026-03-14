#ifndef PANEL_STATUS_H
#define PANEL_STATUS_H

#include "panel.h"
#include "../src/app.h"

#define APP_TYPE_PANEL_STATUS panel_status_get_type()
#define APP_PANEL_STATUS(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, APP_TYPE_PANEL_STATUS, PanelStatus)

typedef struct _PanelStatus PanelStatus;
typedef struct { AppPanelClass parent_class; } PanelStatusClass;

struct _PanelStatus {
    AppPanel panel;
    PlanarApp *app;
    GtkWidget *webview;
    guint refresh_timer;
    /* CPU delta tracking */
    guint64 prev_total;
    guint64 prev_idle;
};

GType panel_status_get_type(void);

GtkWidget *panel_status_new(PlanarApp *app);
void panel_status_refresh(GtkWidget *widget);

#endif
