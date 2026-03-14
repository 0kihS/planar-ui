#ifndef PANEL_NOTIFICATION_H
#define PANEL_NOTIFICATION_H

#include <gtk/gtk.h>

#define APP_TYPE_PANEL_NOTIFICATION app_panel_notification_get_type()
#define APP_PANEL_NOTIFICATION(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, APP_TYPE_PANEL_NOTIFICATION, PanelNotification)

typedef struct _PanelNotification PanelNotification;
typedef struct _PanelNotificationClass PanelNotificationClass;

struct _PanelNotification {
    GtkWindow parent;
    GtkWidget *webview;
    guint dismiss_source;
};

struct _PanelNotificationClass {
    GtkWindowClass parent_class;
};

GType app_panel_notification_get_type(void);

GtkWidget *panel_notification_new(void);
void panel_notification_show(GtkWidget *widget, const char *text, double duration_seconds);

#endif
