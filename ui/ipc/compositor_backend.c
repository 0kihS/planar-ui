#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <json-glib/json-glib.h>
#include "compositor_backend.h"
#include "compositor-ipc.h"

#define BUFFER_SIZE 8192

struct compositor_backend {
    compositor_type_t type;
    compositor_event_cb callback;
    gpointer user_data;
    /* planar */
    compositor_ipc_t *planar_ipc;
    /* hyprland */
    int hypr_cmd_fd;
    int hypr_event_fd;
    GIOChannel *hypr_event_channel;
    guint hypr_event_source;
};

/* ── Detection ─────────────────────────────────────────────── */

compositor_type_t compositor_backend_detect(void) {
    if (getenv("HYPRLAND_INSTANCE_SIGNATURE"))
        return COMPOSITOR_HYPRLAND;
    return COMPOSITOR_PLANAR;
}

/* ── Hyprland helpers ─────────────────────────────────────── */

static char *hypr_get_socket_path(const char *sock_name) {
    const char *sig = getenv("HYPRLAND_INSTANCE_SIGNATURE");
    const char *rundir = getenv("XDG_RUNTIME_DIR");
    if (!sig || !rundir) return NULL;
    return g_strdup_printf("%s/hypr/%s/.%s.sock", rundir, sig, sock_name);
}

static int hypr_connect(const char *sock_name) {
    char *path = hypr_get_socket_path(sock_name);
    if (!path) return -1;

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { g_free(path); return -1; }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    g_free(path);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static gboolean hypr_event_callback(GIOChannel *source, GIOCondition condition, gpointer data) {
    (void)source; (void)condition;
    compositor_backend_t *b = data;

    char buffer[BUFFER_SIZE];
    ssize_t n = read(b->hypr_event_fd, buffer, sizeof(buffer) - 1);
    if (n <= 0) return TRUE;
    buffer[n] = '\0';

    /* Hyprland sends "event>>payload" format, one per line */
    char *line = buffer;
    char *next;
    while ((next = strchr(line, '\n')) != NULL) {
        *next = '\0';

        char *sep = strstr(line, ">>");
        if (sep) {
            *sep = '\0';
            const char *ev = line;
            const char *payload = sep + 2;

            /* Map hyprland events to planar event names */
            const char *mapped = ev;
            if (strcmp(ev, "openwindow") == 0)
                mapped = "window_open";
            else if (strcmp(ev, "closewindow") == 0)
                mapped = "window_close";
            else if (strcmp(ev, "activewindow") == 0)
                mapped = "window_focus";
            else if (strcmp(ev, "workspace") == 0 ||
                     strcmp(ev, "activespecial") == 0)
                mapped = "workspace";

            if (b->callback)
                b->callback(mapped, payload, b->user_data);
        }

        line = next + 1;
    }
    return TRUE;
}

/* ── Public API ───────────────────────────────────────────── */

compositor_backend_t *compositor_backend_create(compositor_event_cb callback, gpointer user_data, compositor_ipc_t *existing_planar_ipc) {
    compositor_backend_t *b = g_new0(compositor_backend_t, 1);
    b->type = compositor_backend_detect();
    b->callback = callback;
    b->user_data = user_data;
    b->hypr_cmd_fd = -1;
    b->hypr_event_fd = -1;

    if (b->type == COMPOSITOR_PLANAR) {
        /* Reuse the existing IPC connection — events go through the original callback */
        b->planar_ipc = existing_planar_ipc;
    } else {
        /* Connect to hyprland sockets */
        b->hypr_cmd_fd = hypr_connect("socket");
        b->hypr_event_fd = hypr_connect("socket2");

        if (b->hypr_event_fd >= 0) {
            b->hypr_event_channel = g_io_channel_unix_new(b->hypr_event_fd);
            g_io_channel_set_flags(b->hypr_event_channel, G_IO_FLAG_NONBLOCK, NULL);
            b->hypr_event_source = g_io_add_watch(
                b->hypr_event_channel, G_IO_IN | G_IO_HUP, hypr_event_callback, b);
        }
    }

    return b;
}

void compositor_backend_free(compositor_backend_t *b) {
    if (!b) return;
    /* planar_ipc is owned by app, not freed here */ else {
        if (b->hypr_event_source > 0) g_source_remove(b->hypr_event_source);
        if (b->hypr_event_channel) g_io_channel_unref(b->hypr_event_channel);
        if (b->hypr_cmd_fd >= 0) close(b->hypr_cmd_fd);
        if (b->hypr_event_fd >= 0) close(b->hypr_event_fd);
    }
    g_free(b);
}

compositor_type_t compositor_backend_get_type(compositor_backend_t *b) {
    return b ? b->type : COMPOSITOR_PLANAR;
}

gboolean compositor_backend_has_windows_panel(compositor_backend_t *b) {
    return b && b->type == COMPOSITOR_PLANAR;
}

/* ── Hyprland query via command socket ────────────────────── */

static gboolean hypr_query(compositor_backend_t *b, const char *cmd, char **json_out) {
    if (b->hypr_cmd_fd < 0) return FALSE;

    char buf[BUFFER_SIZE];
    snprintf(buf, sizeof(buf), "%s\n", cmd);
    if (write(b->hypr_cmd_fd, buf, strlen(buf)) <= 0) return FALSE;

    char *response = g_malloc(BUFFER_SIZE * 4);
    ssize_t n = read(b->hypr_cmd_fd, response, BUFFER_SIZE * 4 - 1);
    if (n > 0) {
        response[n] = '\0';
        *json_out = response;
        return TRUE;
    }
    g_free(response);
    return FALSE;
}

static gboolean hypr_send_cmd(compositor_backend_t *b, const char *cmd) {
    if (b->hypr_cmd_fd < 0) return FALSE;
    char buf[BUFFER_SIZE];
    snprintf(buf, sizeof(buf), "%s\n", cmd);
    ssize_t w = write(b->hypr_cmd_fd, buf, strlen(buf));
    char discard[256];
    read(b->hypr_cmd_fd, discard, sizeof(discard));
    return w > 0;
}

/* ── Dispatch ─────────────────────────────────────────────── */

gboolean compositor_backend_get_workspaces(compositor_backend_t *b, char **json_out) {
    if (!b) return FALSE;
    if (b->type == COMPOSITOR_PLANAR && b->planar_ipc)
        return compositor_ipc_get_workspaces(b->planar_ipc, json_out);
    return hypr_query(b, "j/workspaces", json_out);
}

gboolean compositor_backend_get_windows(compositor_backend_t *b, char **json_out) {
    if (!b) return FALSE;
    if (b->type == COMPOSITOR_PLANAR && b->planar_ipc)
        return compositor_ipc_get_windows(b->planar_ipc, json_out);
    return hypr_query(b, "j/clients", json_out);
}

gboolean compositor_backend_get_focused(compositor_backend_t *b, char **json_out) {
    if (!b) return FALSE;
    if (b->type == COMPOSITOR_PLANAR && b->planar_ipc)
        return compositor_ipc_get_focused(b->planar_ipc, json_out);
    return hypr_query(b, "j/activewindow", json_out);
}

gboolean compositor_backend_send_command(compositor_backend_t *b, const char *command) {
    if (!b) return FALSE;
    if (b->type == COMPOSITOR_PLANAR && b->planar_ipc)
        return compositor_ipc_send_command(b->planar_ipc, command);
    return hypr_send_cmd(b, command);
}
