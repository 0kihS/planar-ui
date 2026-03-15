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
    app->compositor_backend = NULL;
    app->has_windows_panel = TRUE;
    app->agent = NULL;
    app->connected = FALSE;
    app->notification_panel = NULL;
    app->cmd_socket_fd = -1;
    app->cmd_socket_source = 0;
    app->agent_model = NULL;
    app->agent_provider = NULL;
    app->session_id = NULL;
    app->context_length = 0;
    app->tokens_prompt = 0;
    app->tokens_completion = 0;
    app->api_calls = 0;
    app->compression_count = 0;
    app->current_iteration = 0;
    app->iteration_tool_count = 0;
    app->todo_json = NULL;
    app->clarify_question = NULL;
    app->clarify_choices_json = NULL;
    app->clarify_timeout = 0;
    app->clarify_active = FALSE;
}

static void planar_app_class_init(PlanarAppClass *class) {
    G_APPLICATION_CLASS(class)->activate = planar_app_activate;
}

static void backend_event_callback(const char *event, const char *data, gpointer user_data) {
    PlanarApp *app = PLANAR_APP(user_data);
    planar_app_on_compositor_event(app, event, data);
}

static void agent_message_callback(const char *role, const char *message, gpointer user_data) {
    PlanarApp *app = PLANAR_APP(user_data);
    planar_app_on_agent_message(app, role, message);
}

static void agent_structured_callback(const char *type, JsonObject *data, gpointer user_data) {
    PlanarApp *app = PLANAR_APP(user_data);
    planar_app_on_agent_structured(app, type, data);
}

static void planar_app_activate(GApplication *application) {
    PlanarApp *app = PLANAR_APP(application);
    
    app->compositor = compositor_ipc_new();
    if (app->compositor) {
        compositor_ipc_connect(app->compositor, backend_event_callback, app);
        app->connected = TRUE;
    }
    
    app->agent = agent_ipc_new();
    if (app->agent) {
        agent_ipc_connect(app->agent, agent_message_callback, agent_structured_callback, app);
    }
    
    /* Create compositor backend abstraction — pass existing IPC to avoid duplicate connection */
    app->compositor_backend = compositor_backend_create(backend_event_callback, app, app->compositor);
    if (app->compositor_backend)
        app->has_windows_panel = compositor_backend_has_windows_panel(app->compositor_backend);

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
    if (app->compositor_backend) {
        compositor_backend_free(app->compositor_backend);
    }
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

void planar_app_on_agent_structured(PlanarApp *app, const char *type, JsonObject *data) {
    if (strcmp(type, "status") == 0) {
        /* Update agent session state */
        if (json_object_has_member(data, "model")) {
            g_free(app->agent_model);
            app->agent_model = g_strdup(json_object_get_string_member(data, "model"));
        }
        if (json_object_has_member(data, "provider")) {
            g_free(app->agent_provider);
            app->agent_provider = g_strdup(json_object_get_string_member(data, "provider"));
        }
        if (json_object_has_member(data, "session_id")) {
            g_free(app->session_id);
            app->session_id = g_strdup(json_object_get_string_member(data, "session_id"));
        }
        if (json_object_has_member(data, "context_length"))
            app->context_length = json_object_get_int_member(data, "context_length");
        if (json_object_has_member(data, "tokens_prompt"))
            app->tokens_prompt = json_object_get_int_member(data, "tokens_prompt");
        if (json_object_has_member(data, "tokens_completion"))
            app->tokens_completion = json_object_get_int_member(data, "tokens_completion");
        if (json_object_has_member(data, "api_calls"))
            app->api_calls = (gint)json_object_get_int_member(data, "api_calls");
        if (json_object_has_member(data, "compression_count"))
            app->compression_count = (gint)json_object_get_int_member(data, "compression_count");

        /* Refresh panels that show agent state */
        if (app->topbar_panel) panel_topbar_refresh(app->topbar_panel);
        if (app->status_panel) panel_status_refresh(app->status_panel);

    } else if (strcmp(type, "progress") == 0) {
        /* Update iteration counter from structured progress */
        if (json_object_has_member(data, "iteration"))
            app->current_iteration = (gint)json_object_get_int_member(data, "iteration");
        if (json_object_has_member(data, "total_tools"))
            app->iteration_tool_count = (gint)json_object_get_int_member(data, "total_tools");
        if (app->log_panel)
            panel_log_set_iteration(app->log_panel, app->current_iteration, app->iteration_tool_count);

    } else if (strcmp(type, "todo") == 0) {
        /* Store todo items as raw JSON for panel injection */
        if (json_object_has_member(data, "items")) {
            JsonNode *items_node = json_object_get_member(data, "items");
            JsonGenerator *gen = json_generator_new();
            json_generator_set_root(gen, items_node);
            g_free(app->todo_json);
            app->todo_json = json_generator_to_data(gen, NULL);
            g_object_unref(gen);
        }
        /* Refresh status panel which may show todo */
        if (app->status_panel) panel_status_refresh(app->status_panel);

    } else if (strcmp(type, "clarify") == 0) {
        /* Store clarify question state */
        g_free(app->clarify_question);
        app->clarify_question = NULL;
        g_free(app->clarify_choices_json);
        app->clarify_choices_json = NULL;

        if (json_object_has_member(data, "question"))
            app->clarify_question = g_strdup(json_object_get_string_member(data, "question"));
        if (json_object_has_member(data, "choices")) {
            JsonNode *choices_node = json_object_get_member(data, "choices");
            JsonGenerator *gen = json_generator_new();
            json_generator_set_root(gen, choices_node);
            app->clarify_choices_json = json_generator_to_data(gen, NULL);
            g_object_unref(gen);
        }
        if (json_object_has_member(data, "timeout"))
            app->clarify_timeout = (gint)json_object_get_int_member(data, "timeout");
        app->clarify_active = TRUE;

        /* Render clarify card in chat panel */
        if (app->chat_panel) {
            panel_chat_show_clarify(app->chat_panel,
                                    app->clarify_question,
                                    app->clarify_choices_json);
        }

    } else if (strcmp(type, "reasoning") == 0) {
        /* Show reasoning in chat panel */
        if (json_object_has_member(data, "text")) {
            const char *text = json_object_get_string_member(data, "text");
            if (text && app->chat_panel) {
                panel_chat_add_message(app->chat_panel, "reasoning", text);
            }
        }
    }
}

void planar_app_send_clarify_response(PlanarApp *app, const char *answer) {
    if (!app->agent || !app->clarify_active) return;
    agent_ipc_send_json(app->agent, "clarify_response", "answer", answer);
    app->clarify_active = FALSE;
    g_free(app->clarify_question);
    app->clarify_question = NULL;
    g_free(app->clarify_choices_json);
    app->clarify_choices_json = NULL;
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
        } else if (strcmp(buf, "toggle_status") == 0) {
            if (app->status_panel) {
                gboolean vis = gtk_widget_get_visible(app->status_panel);
                gtk_widget_set_visible(app->status_panel, !vis);
            }
            write(fd, "{\"ok\":true}\n", 12);
        } else if (strcmp(buf, "show_status") == 0) {
            if (app->status_panel) gtk_widget_set_visible(app->status_panel, TRUE);
            write(fd, "{\"ok\":true}\n", 12);
        } else if (strcmp(buf, "hide_status") == 0) {
            if (app->status_panel) gtk_widget_set_visible(app->status_panel, FALSE);
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
