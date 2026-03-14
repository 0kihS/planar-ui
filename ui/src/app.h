#ifndef APP_H
#define APP_H

#include <gtk/gtk.h>
#include "ipc/compositor-ipc.h"
#include "ipc/agent-ipc.h"

G_BEGIN_DECLS

#define PLANAR_TYPE_APP (planar_app_get_type())
#define PLANAR_APP(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), PLANAR_TYPE_APP, PlanarApp))

typedef struct _PlanarApp PlanarApp;
typedef struct _PlanarAppClass PlanarAppClass;

struct _PlanarApp {
    GtkApplication gtk_app;
    struct compositor_ipc *compositor;
    struct agent_ipc *agent;

    GtkWidget *windows_panel;
    GtkWidget *chat_panel;
    GtkWidget *log_panel;
    GtkWidget *status_panel;
    GtkWidget *topbar_panel;
    GtkWidget *input_panel;
    GtkWidget *notification_panel;

    gboolean connected;
    int cmd_socket_fd;
    guint cmd_socket_source;

    /* Token / message tracking */
    guint64 tokens_in;
    guint64 tokens_out;
    guint   msg_count;
    guint   tool_count;
};

struct _PlanarAppClass {
    GtkApplicationClass parent_class;
};

GType planar_app_get_type(void);

PlanarApp *planar_app_new(void);
void planar_app_free(PlanarApp *app);

void planar_app_on_compositor_event(PlanarApp *app, const char *event, const char *data);
void planar_app_on_agent_message(PlanarApp *app, const char *role, const char *message);
void planar_app_send_to_agent(PlanarApp *app, const char *message);

G_END_DECLS

#endif
