#ifndef PANEL_WINDOWS_H
#define PANEL_WINDOWS_H

#include "panel.h"
#include "../src/app.h"

#define APP_TYPE_PANEL_WINDOWS panel_windows_get_type()
#define APP_PANEL_WINDOWS(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, APP_TYPE_PANEL_WINDOWS, PanelWindows)

typedef struct _PanelWindows PanelWindows;
typedef struct { AppPanelClass parent_class; } PanelWindowsClass;

struct _PanelWindows {
    AppPanel panel;
    PlanarApp *app;
    GtkWidget *webview;
};

GType panel_windows_get_type(void);

GtkWidget *panel_windows_new(PlanarApp *app);
void panel_windows_refresh(GtkWidget *widget);

#endif
