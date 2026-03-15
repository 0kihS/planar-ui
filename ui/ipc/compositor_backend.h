#ifndef COMPOSITOR_BACKEND_H
#define COMPOSITOR_BACKEND_H

#include <glib.h>
#include "compositor-ipc.h"

typedef struct compositor_backend compositor_backend_t;

typedef enum {
    COMPOSITOR_PLANAR,
    COMPOSITOR_HYPRLAND,
} compositor_type_t;

typedef void (*compositor_event_cb)(const char *event, const char *data, gpointer user_data);

/* Create a backend. type=AUTO picks the right one based on env vars.
 * Calls callback with compositor events (window_open/close/focus, workspace). */
/* Pass existing_planar_ipc to reuse an existing connection (NULL to create new for hyprland). */
compositor_backend_t *compositor_backend_create(compositor_event_cb callback, gpointer user_data, compositor_ipc_t *existing_planar_ipc);

/* Auto-detect which compositor is running */
compositor_type_t compositor_backend_detect(void);

void compositor_backend_free(compositor_backend_t *backend);
compositor_type_t compositor_backend_get_type(compositor_backend_t *backend);

/* Whether the windows panel is meaningful for this backend */
gboolean compositor_backend_has_windows_panel(compositor_backend_t *backend);

/* Query functions — return JSON strings, caller must g_free() */
gboolean compositor_backend_get_workspaces(compositor_backend_t *backend, char **json_out);
gboolean compositor_backend_get_windows(compositor_backend_t *backend, char **json_out);
gboolean compositor_backend_get_focused(compositor_backend_t *backend, char **json_out);

/* Send a command to the compositor */
gboolean compositor_backend_send_command(compositor_backend_t *backend, const char *command);

#endif /* COMPOSITOR_BACKEND_H */
