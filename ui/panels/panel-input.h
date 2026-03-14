#ifndef PANEL_INPUT_H
#define PANEL_INPUT_H

#include "panel.h"
#include "../src/app.h"

#define APP_TYPE_PANEL_INPUT panel_input_get_type()
#define APP_PANEL_INPUT(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, APP_TYPE_PANEL_INPUT, PanelInput)

typedef struct _PanelInput PanelInput;
typedef struct { AppPanelClass parent_class; } PanelInputClass;

struct _PanelInput {
    AppPanel panel;
    PlanarApp *app;
    GtkWidget *webview;
};

GType panel_input_get_type(void);

GtkWidget *panel_input_new(PlanarApp *app);

#endif
