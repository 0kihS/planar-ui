#ifndef PANEL_LOG_H
#define PANEL_LOG_H

#include "panel.h"
#include "../src/app.h"

#define APP_TYPE_PANEL_LOG panel_log_get_type()
#define APP_PANEL_LOG(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, APP_TYPE_PANEL_LOG, PanelLog)

typedef struct _PanelLog PanelLog;
typedef struct { AppPanelClass parent_class; } PanelLogClass;

struct _PanelLog {
    AppPanel panel;
    PlanarApp *app;
    GtkWidget *webview;
};

GType panel_log_get_type(void);

GtkWidget *panel_log_new(PlanarApp *app);
void panel_log_add_event(GtkWidget *widget, const char *event, const char *detail);
void panel_log_add_tool(GtkWidget *widget, const char *tool_info);
void panel_log_set_iteration(GtkWidget *widget, int iteration, int total_tools);

#endif
