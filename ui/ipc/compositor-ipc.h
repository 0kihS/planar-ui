#ifndef COMPOSITOR_IPC_H
#define COMPOSITOR_IPC_H

#include <glib.h>

typedef struct compositor_ipc compositor_ipc_t;

typedef void (*compositor_event_cb)(const char *event, const char *data, gpointer user_data);

compositor_ipc_t *compositor_ipc_new(void);
void compositor_ipc_free(compositor_ipc_t *ipc);
gboolean compositor_ipc_connect(compositor_ipc_t *ipc, compositor_event_cb callback, gpointer user_data);

gboolean compositor_ipc_send_command(compositor_ipc_t *ipc, const char *command);
gboolean compositor_ipc_get_windows(compositor_ipc_t *ipc, char **json_out);
gboolean compositor_ipc_get_workspaces(compositor_ipc_t *ipc, char **json_out);
gboolean compositor_ipc_get_focused(compositor_ipc_t *ipc, char **json_out);

#endif
