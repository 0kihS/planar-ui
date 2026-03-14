#include <gtk/gtk.h>
#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "app.h"
#include "panels/panel.h"
#include "panels/panel-windows.h"
#include "panels/panel-chat.h"
#include "panels/panel-log.h"
#include "panels/panel-status.h"
#include "panels/panel-topbar.h"
#include "panels/panel-input.h"
#include "panels/panel-notification.h"


static void planar_app_activate(GApplication *application);
static void create_cmd_socket(PlanarApp *app);

G_DEFINE_TYPE(PlanarApp, planar_app, GTK_TYPE_APPLICATION)

PlanarApp *planar_app_new(void) {
    PlanarApp *app = g_object_new(PLANAR_TYPE_APP,
        "application-id", "com.planar.agent-ui",
        "flags", G_APPLICATION_NON_UNIQUE,
        NULL);
    return app;
}

static void planar_app_init(PlanarApp *app) {
    app->compositor = NULL;
    app->agent = NULL;
    app->connected = FALSE;
    app->notification_panel = NULL;
    app->cmd_socket_fd = -1;
    app->cmd_socket_source = 0;
}

static void planar_app_class_init(PlanarAppClass *class) {
    G_APPLICATION_CLASS(class)->activate = planar_app_activate;
}

static void compositor_event_callback(const char *event, const char *data, gpointer user_data) {
    PlanarApp *app = PLANAR_APP(user_data);
    planar_app_on_compositor_event(app, event, data);
}

static void agent_message_callback(const char *role, const char *message, gpointer user_data) {
    PlanarApp *app = PLANAR_APP(user_data);
    planar_app_on_agent_message(app, role, message);
}

static void planar_app_activate(GApplication *application) {
    PlanarApp *app = PLANAR_APP(application);
    
    app->compositor = compositor_ipc_new();
    if (app->compositor) {
        compositor_ipc_connect(app->compositor, compositor_event_callback, app);
        app->connected = TRUE;
    }
    
    app->agent = agent_ipc_new();
    if (app->agent) {
        agent_ipc_connect(app->agent, agent_message_callback, app);
    }
    
    app->topbar_panel = panel_topbar_new(app);
    app->windows_panel = panel_windows_new(app);
    app->chat_panel = panel_chat_new(app);
    app->log_panel = panel_log_new(app);
    app->status_panel = panel_status_new(app);
    app->input_panel = panel_input_new(app);
    app->notification_panel = panel_notification_new();

    GtkApplication *gtk_app = GTK_APPLICATION(application);
    gtk_application_add_window(gtk_app, GTK_WINDOW(app->topbar_panel));
    gtk_application_add_window(gtk_app, GTK_WINDOW(app->windows_panel));
    gtk_application_add_window(gtk_app, GTK_WINDOW(app->chat_panel));
    gtk_application_add_window(gtk_app, GTK_WINDOW(app->log_panel));
    gtk_application_add_window(gtk_app, GTK_WINDOW(app->status_panel));
    gtk_application_add_window(gtk_app, GTK_WINDOW(app->input_panel));
    gtk_application_add_window(gtk_app, GTK_WINDOW(app->notification_panel));

    create_cmd_socket(app);
}

void planar_app_free(PlanarApp *app) {
    if (app->compositor) {
        compositor_ipc_free(app->compositor);
    }
    if (app->agent) {
        agent_ipc_free(app->agent);
    }
}

void planar_app_on_compositor_event(PlanarApp *app, const char *event, const char *data) {
    if (app->log_panel) {
        panel_log_add_event(app->log_panel, event, data);
    }
    if (app->windows_panel) {
        if (strcmp(event, "window_open") == 0 || 
            strcmp(event, "window_close") == 0 ||
            strcmp(event, "window_focus") == 0) {
            panel_windows_refresh(app->windows_panel);
        }
    }
    if (app->status_panel) {
        panel_status_refresh(app->status_panel);
    }
    if (app->topbar_panel) {
        panel_topbar_refresh(app->topbar_panel);
    }
}

void planar_app_on_agent_message(PlanarApp *app, const char *role, const char *message) {
    if (app->chat_panel) {
        panel_chat_add_message(app->chat_panel, role, message);
    }
    if (strcmp(role, "tool") == 0) {
        app->tool_count++;
        if (app->log_panel)
            panel_log_add_tool(app->log_panel, message);
    }
    if (strcmp(role, "agent") == 0) {
        app->msg_count++;
        app->tokens_in += (strlen(message) + 3) / 4;
    }
}

void planar_app_send_to_agent(PlanarApp *app, const char *message) {
    if (app->chat_panel) {
        panel_chat_add_message(app->chat_panel, "user", message);
    }
    app->tokens_out += (strlen(message) + 3) / 4;
    if (app->agent) {
        agent_ipc_send(app->agent, message);
    }
}

/* ── Command Socket ───────────────────────────────────────────────────────── */

static char *get_cmd_socket_path(void) {
    const char *path = getenv("PLANAR_AGENT_UI_SOCK");
    if (path) return g_strdup(path);

    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    const char *wayland_display = getenv("WAYLAND_DISPLAY");
    if (!runtime_dir || !wayland_display) return NULL;
    return g_strdup_printf("%s/planar-agent-ui.%s.sock", runtime_dir, wayland_display);
}

static gboolean handle_notify_command(PlanarApp *app, const char *args) {
    if (!app->notification_panel) return FALSE;

    /* Parse: <duration_seconds> <text...> */
    double duration = 5.0;
    const char *text = args;

    /* Try to parse a leading duration number */
    char *endptr;
    double d = g_strtod(args, &endptr);
    if (endptr != args && (*endptr == ' ' || *endptr == '\t')) {
        duration = d;
        text = endptr;
        while (*text == ' ' || *text == '\t') text++;
    }

    if (!text[0]) return FALSE;

    panel_notification_show(app->notification_panel, text, duration);
    return TRUE;
}

static gboolean cmd_client_callback(GIOChannel *source, GIOCondition condition, gpointer data) {
    PlanarApp *app = data;

    if (condition & (G_IO_HUP | G_IO_ERR)) return G_SOURCE_REMOVE;

    if (condition & G_IO_IN) {
        int fd = g_io_channel_unix_get_fd(source);
        char buf[4096];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n <= 0) return G_SOURCE_REMOVE;

        buf[n] = '\0';
        /* Strip trailing newline */
        if (buf[n - 1] == '\n') buf[n - 1] = '\0';

        g_print("UI command: %s\n", buf);

        if (g_str_has_prefix(buf, "notify ")) {
            gboolean ok = handle_notify_command(app, buf + 7);
            const char *resp = ok ? "{\"ok\":true}\n" : "{\"ok\":false}\n";
            write(fd, resp, strlen(resp));
        } else if (strcmp(buf, "toggle_windows") == 0) {
            if (app->windows_panel) {
                gboolean vis = gtk_widget_get_visible(app->windows_panel);
                gtk_widget_set_visible(app->windows_panel, !vis);
            }
            write(fd, "{\"ok\":true}\n", 12);
        } else if (strcmp(buf, "toggle_chat") == 0) {
            if (app->chat_panel) {
                gboolean vis = gtk_widget_get_visible(app->chat_panel);
                gtk_widget_set_visible(app->chat_panel, !vis);
            }
            write(fd, "{\"ok\":true}\n", 12);
        } else if (strcmp(buf, "show_windows") == 0) {
            if (app->windows_panel) gtk_widget_set_visible(app->windows_panel, TRUE);
            write(fd, "{\"ok\":true}\n", 12);
        } else if (strcmp(buf, "hide_windows") == 0) {
            if (app->windows_panel) gtk_widget_set_visible(app->windows_panel, FALSE);
            write(fd, "{\"ok\":true}\n", 12);
        } else {
            write(fd, "{\"ok\":false,\"error\":\"unknown command\"}\n", 39);
        }
    }

    return G_SOURCE_REMOVE; /* One-shot per connection */
}

static gboolean cmd_socket_accept(GIOChannel *source, GIOCondition condition, gpointer data) {
    PlanarApp *app = data;
    int fd = g_io_channel_unix_get_fd(source);

    if (condition & G_IO_IN) {
        int client_fd = accept(fd, NULL, NULL);
        if (client_fd >= 0) {
            GIOChannel *ch = g_io_channel_unix_new(client_fd);
            g_io_channel_set_close_on_unref(ch, TRUE);
            g_io_add_watch(ch, G_IO_IN | G_IO_HUP, cmd_client_callback, app);
        }
    }
    return TRUE;
}

static void create_cmd_socket(PlanarApp *app) {
    char *path = get_cmd_socket_path();
    if (!path) return;

    unlink(path);

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { g_free(path); return; }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        g_free(path);
        return;
    }

    if (listen(fd, 5) < 0) {
        close(fd);
        g_free(path);
        return;
    }

    app->cmd_socket_fd = fd;
    GIOChannel *channel = g_io_channel_unix_new(fd);
    app->cmd_socket_source = g_io_add_watch(channel, G_IO_IN | G_IO_HUP, cmd_socket_accept, app);

    g_print("UI command socket: %s\n", path);
    g_free(path);
}
