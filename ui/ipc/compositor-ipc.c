#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <json-glib/json-glib.h>
#include "compositor-ipc.h"

#define BUFFER_SIZE 8192

struct compositor_ipc {
    int cmd_fd;
    int event_fd;
    GIOChannel *event_channel;
    compositor_event_cb callback;
    gpointer user_data;
    guint event_source;
};

static char *get_socket_path(const char *env_var, const char *default_pattern) {
    const char *path = getenv(env_var);
    if (path) return g_strdup(path);
    
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    const char *wayland_display = getenv("WAYLAND_DISPLAY");
    if (!runtime_dir || !wayland_display) return NULL;
    
    return g_strdup_printf(default_pattern, runtime_dir, wayland_display);
}

static int connect_socket(const char *path) {
    if (!path) return -1;
    
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static gboolean event_callback(GIOChannel *source, GIOCondition condition, gpointer data) {
    (void)condition;
    compositor_ipc_t *ipc = data;
    
    char buffer[BUFFER_SIZE];
    ssize_t n = read(ipc->event_fd, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        buffer[n] = '\0';
        
        char *line = buffer;
        char *next;
        while ((next = strchr(line, '\n')) != NULL) {
            *next = '\0';
            
            JsonParser *parser = json_parser_new();
            if (json_parser_load_from_data(parser, line, -1, NULL)) {
                JsonNode *root = json_parser_get_root(parser);
                if (JSON_NODE_TYPE(root) == JSON_NODE_OBJECT) {
                    JsonObject *obj = json_node_get_object(root);

                    const char *event = json_object_get_string_member(obj, "event");
                    const char *data = json_object_get_string_member(obj, "data");

                    if (event && ipc->callback) {
                        ipc->callback(event, data ? data : "", ipc->user_data);
                    }
                }
            } else {
                /* Compositor sends plain text: "event_type json_data" */
                char *space = strchr(line, ' ');
                if (space) {
                    *space = '\0';
                    const char *event = line;
                    const char *data = space + 1;
                    if (event[0] && ipc->callback) {
                        ipc->callback(event, data, ipc->user_data);
                    }
                } else if (line[0] && ipc->callback) {
                    /* Event with no data */
                    ipc->callback(line, "", ipc->user_data);
                }
            }
            g_object_unref(parser);
            
            line = next + 1;
        }
    }
    return TRUE;
}

compositor_ipc_t *compositor_ipc_new(void) {
    compositor_ipc_t *ipc = g_new0(compositor_ipc_t, 1);
    ipc->cmd_fd = -1;
    ipc->event_fd = -1;
    return ipc;
}

void compositor_ipc_free(compositor_ipc_t *ipc) {
    if (ipc->event_source > 0) {
        g_source_remove(ipc->event_source);
    }
    if (ipc->event_channel) {
        g_io_channel_unref(ipc->event_channel);
    }
    if (ipc->cmd_fd >= 0) close(ipc->cmd_fd);
    if (ipc->event_fd >= 0) close(ipc->event_fd);
    g_free(ipc);
}

gboolean compositor_ipc_connect(compositor_ipc_t *ipc, compositor_event_cb callback, gpointer user_data) {
    ipc->callback = callback;
    ipc->user_data = user_data;
    
    char *cmd_path = get_socket_path("PLANAR_SOCKET", "%s/planar.%s.sock");
    char *event_path = get_socket_path("PLANAR_EVENT_SOCKET", "%s/planar.%s.event.sock");
    
    ipc->cmd_fd = connect_socket(cmd_path);
    g_free(cmd_path);
    
    if (ipc->cmd_fd < 0) {
        g_printerr("Failed to connect to compositor command socket\n");
        g_free(event_path);
        return FALSE;
    }
    
    ipc->event_fd = connect_socket(event_path);
    g_free(event_path);
    
    if (ipc->event_fd >= 0) {
        ipc->event_channel = g_io_channel_unix_new(ipc->event_fd);
        g_io_channel_set_flags(ipc->event_channel, G_IO_FLAG_NONBLOCK, NULL);
        ipc->event_source = g_io_add_watch(ipc->event_channel, G_IO_IN | G_IO_HUP, event_callback, ipc);
    }
    
    return TRUE;
}

gboolean compositor_ipc_send_command(compositor_ipc_t *ipc, const char *command) {
    if (ipc->cmd_fd < 0) return FALSE;

    char cmd[BUFFER_SIZE];
    snprintf(cmd, sizeof(cmd), "%s\n", command);

    ssize_t written = write(ipc->cmd_fd, cmd, strlen(cmd));
    if (written <= 0) return FALSE;

    // Drain the response so it doesn't contaminate subsequent reads
    char discard[BUFFER_SIZE];
    read(ipc->cmd_fd, discard, sizeof(discard));

    return TRUE;
}

static gboolean ipc_query(compositor_ipc_t *ipc, const char *command, char **json_out, size_t buf_size) {
    if (ipc->cmd_fd < 0) return FALSE;

    char cmd[BUFFER_SIZE];
    snprintf(cmd, sizeof(cmd), "%s\n", command);
    if (write(ipc->cmd_fd, cmd, strlen(cmd)) <= 0) return FALSE;

    char *buffer = g_malloc(buf_size);
    ssize_t n = read(ipc->cmd_fd, buffer, buf_size - 1);
    if (n > 0) {
        buffer[n] = '\0';
        *json_out = buffer;
        return TRUE;
    }
    g_free(buffer);
    return FALSE;
}

gboolean compositor_ipc_get_windows(compositor_ipc_t *ipc, char **json_out) {
    return ipc_query(ipc, "get windows", json_out, BUFFER_SIZE * 4);
}

gboolean compositor_ipc_get_workspaces(compositor_ipc_t *ipc, char **json_out) {
    return ipc_query(ipc, "get workspaces", json_out, BUFFER_SIZE);
}

gboolean compositor_ipc_get_focused(compositor_ipc_t *ipc, char **json_out) {
    return ipc_query(ipc, "get focused", json_out, BUFFER_SIZE);
}
