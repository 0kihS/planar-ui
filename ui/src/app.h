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

    /* Token / message tracking (local approximation) */
    guint64 tokens_in;
    guint64 tokens_out;
    guint   msg_count;
    guint   tool_count;

    /* Agent session state (from gateway status frames) */
    char   *agent_model;
    char   *agent_provider;
    char   *session_id;
    gint64  context_length;
    gint64  tokens_prompt;
    gint64  tokens_completion;
    gint    api_calls;
    gint    compression_count;

    /* Current tool progress iteration */
    gint    current_iteration;
    gint    iteration_tool_count;

    /* Todo items (raw JSON string for panel injection) */
    char   *todo_json;

    /* Clarify state */
    char   *clarify_question;
    char   *clarify_choices_json;
    gint    clarify_timeout;
    gboolean clarify_active;
};

struct _PlanarAppClass {
    GtkApplicationClass parent_class;
};

GType planar_app_get_type(void);

PlanarApp *planar_app_new(void);
void planar_app_free(PlanarApp *app);

void planar_app_on_compositor_event(PlanarApp *app, const char *event, const char *data);
void planar_app_on_agent_message(PlanarApp *app, const char *role, const char *message);
void planar_app_on_agent_structured(PlanarApp *app, const char *type, JsonObject *data);
void planar_app_send_to_agent(PlanarApp *app, const char *message);
void planar_app_send_clarify_response(PlanarApp *app, const char *answer);

G_END_DECLS

#endif
