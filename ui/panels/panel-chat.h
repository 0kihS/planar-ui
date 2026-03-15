#ifndef PANEL_CHAT_H
#define PANEL_CHAT_H

#include "panel.h"
#include "../src/app.h"

#define APP_TYPE_PANEL_CHAT panel_chat_get_type()
#define APP_PANEL_CHAT(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, APP_TYPE_PANEL_CHAT, PanelChat)

typedef struct _PanelChat PanelChat;
typedef struct { AppPanelClass parent_class; } PanelChatClass;

struct _PanelChat {
    AppPanel panel;
    PlanarApp *app;
    GtkWidget *messages;
    GtkWidget *webview;
};

GType panel_chat_get_type(void);

GtkWidget *panel_chat_new(PlanarApp *app);
void panel_chat_add_message(GtkWidget *widget, const char *role, const char *content);
void panel_chat_load_html(GtkWidget *widget, const char *html);
void panel_chat_show_clarify(GtkWidget *widget, const char *question, const char *choices_json);

#endif
