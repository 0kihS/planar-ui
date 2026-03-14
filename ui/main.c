#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <json-glib/json-glib.h>
#include <time.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef struct {
    GtkApplication *app;
    int compositor_fd;
    int event_fd;
    int cmd_fd;
    GtkWidget *topbar;
    GtkWidget *topbar_webview;
    GtkWidget *windows_panel;
    GtkWidget *windows_webview;
    GtkWidget *chat_panel;
    GtkWidget *chat_webview;
    GtkWidget *log_panel;
    GtkWidget *status_panel;
    GtkWidget *input_panel;
    gboolean show_windows;
    gboolean show_chat;
    gboolean show_logs;
    gboolean btn_state_windows;
    gboolean btn_state_chat;
    gboolean btn_state_logs;
    guint refresh_timer;
    char *hermes_session;
    char *last_hermes_response;
    char *last_user_message;
} PlanarUI;

static void refresh_windows_panel(PlanarUI *ui);
static gboolean refresh_topbar_timer(gpointer data);
static void refresh_chat_panel(PlanarUI *ui);

static void toggle_panel(PlanarUI *ui, const char *panel, gboolean show) {
    if (strcmp(panel, "windows") == 0) {
        ui->show_windows = show;
        ui->btn_state_windows = show;
        gtk_widget_set_visible(ui->windows_panel, show);
        if (show) refresh_windows_panel(ui);
    } else if (strcmp(panel, "chat") == 0) {
        ui->show_chat = show;
        ui->btn_state_chat = show;
        gtk_widget_set_visible(ui->chat_panel, show);
        if (show) refresh_chat_panel(ui);
    } else if (strcmp(panel, "logs") == 0) {
        ui->show_logs = show;
        ui->btn_state_logs = show;
        gtk_widget_set_visible(ui->log_panel, show);
    }
    if (ui->topbar_webview) {
        char *script = g_strdup_printf(
            "var w=document.getElementById('btn-windows'),c=document.getElementById('btn-chat'),l=document.getElementById('btn-logs');"
            "if(w){w.classList.toggle('active', %s);w.classList.toggle('inactive', %s);}"
            "if(c){c.classList.toggle('active', %s);c.classList.toggle('inactive', %s);}"
            "if(l){l.classList.toggle('active', %s);l.classList.toggle('inactive', %s);}",
            ui->btn_state_windows ? "true" : "false",
            ui->btn_state_windows ? "false" : "true",
            ui->btn_state_chat ? "true" : "false", 
            ui->btn_state_chat ? "false" : "true",
            ui->btn_state_logs ? "true" : "false",
            ui->btn_state_logs ? "false" : "true");
        // Can't easily call JS from C, but the button visual update will happen on next click
        g_free(script);
    }
}

static const char *PANEL_CSS = ""
    "@import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Orbitron:wght@400;700;900&family=Rajdhani:wght@300;400;600;700&display=swap');"
    ":root {"
    "  --red: #e8001a; --orange: #ff6a00; --amber: #ffb300;"
    "  --bg: #070707; --bg2: #0d0d0d; --bg3: #111;"
    "  --text: #ff9040; --text-dim: #ff6a0077;"
    "  --glow: 0 0 8px #ff6a0088; --glow-red: 0 0 8px #e8001a88;"
    "  --mono: 'Share Tech Mono', monospace; --display: 'Orbitron', monospace;"
    "}"
    "* { box-sizing: border-box; margin: 0; padding: 0; }"
    "body { background: var(--bg); color: var(--text); font-family: var(--mono); font-size: 13px; overflow: hidden; }"
    ".panel { background: var(--bg2); border: 1px solid var(--red); box-shadow: inset 0 0 30px rgba(232,0,26,0.04), var(--glow-red); position: relative; }"
    ".header { position: relative; height: 28px; overflow: hidden; margin-bottom: 1px; }"
    ".header-fill { position: absolute; inset: 0; background: var(--red); transform: skewX(-12deg) translateX(-4px); box-shadow: var(--glow-red); }"
    ".header-fill.amber { background: var(--orange); box-shadow: var(--glow); }"
    ".header-text { position: relative; z-index: 1; font-family: var(--display); font-size: 10px; font-weight: 700; letter-spacing: 0.15em; color: #000; line-height: 28px; padding: 0 12px; text-transform: uppercase; }"
    ".content { flex: 1; overflow-y: auto; padding: 8px; }"
    ".content::-webkit-scrollbar { width: 6px; background: var(--bg3); }"
    ".content::-webkit-scrollbar-thumb { background: var(--red); border-radius: 3px; }"
    ".window-item { display: flex; align-items: center; gap: 8px; padding: 6px 10px; border-bottom: 1px solid rgba(232,0,26,0.2); cursor: pointer; transition: background 0.15s; font-size: 11px; }"
    ".window-item:hover { background: rgba(232,0,26,0.08); }"
    ".window-item.focused { background: rgba(232,0,26,0.15); border-left: 2px solid var(--orange); }"
    ".win-id { font-family: var(--display); font-size: 9px; color: var(--text-dim); min-width: 32px; }"
    ".win-name { flex: 1; color: var(--text); text-shadow: 0 0 8px #ff6a0088; }"
    ".win-pos { font-size: 9px; color: var(--text-dim); }"
    ".msg { margin-bottom: 8px; animation: fadeUp 0.3s ease; }"
    "@keyframes fadeUp { from { opacity: 0; transform: translateY(8px); } to { opacity: 1; transform: translateY(0); } }"
    ".msg-meta { font-size: 9px; color: var(--text-dim); letter-spacing: 0.08em; margin-bottom: 3px; }"
    ".msg-bubble { padding: 8px 12px; border-left: 2px solid var(--red); background: rgba(232,0,26,0.06); font-size: 12px; line-height: 1.6; }"
    ".msg-bubble.user { border-left-color: var(--orange); background: rgba(255,106,0,0.06); color: var(--amber); }"
    ".msg-bubble.system { border-left-color: #444; background: rgba(255,255,255,0.02); color: #666; font-size: 10px; }"
    ".msg-bubble.tool { border-left-color: #2a6; background: rgba(40,180,100,0.05); color: #4dbb80; font-size: 11px; font-family: var(--mono); }"
    ".log-item { padding: 6px 10px; border-bottom: 1px solid rgba(232,0,26,0.12); font-size: 10px; animation: fadeUp 0.3s ease; }"
    ".log-time { color: var(--text-dim); font-size: 9px; margin-bottom: 2px; }"
    ".log-event { color: var(--orange); letter-spacing: 0.05em; text-shadow: 0 0 8px #ff6a0088; }"
    ".log-detail { color: #666; font-size: 9px; margin-top: 2px; }"
    ".stat-row { display: flex; justify-content: space-between; align-items: center; font-size: 10px; padding: 3px 0; border-bottom: 1px solid rgba(232,0,26,0.1); }"
    ".stat-label { color: var(--text-dim); letter-spacing: 0.05em; }"
    ".stat-val { color: var(--amber); font-family: var(--display); font-size: 9px; }"
    ".input-area { background: var(--bg3); border: 1px solid var(--red); color: var(--text); font-family: var(--mono); font-size: 13px; padding: 8px 12px; width: 100%; resize: none; outline: none; box-shadow: inset 0 0 10px rgba(232,0,26,0.05); }"
    ".input-area:focus { border-color: var(--orange); box-shadow: inset 0 0 10px rgba(255,106,0,0.08), var(--glow); }"
    ".send-btn { background: var(--red); border: none; color: #000; font-family: var(--display); font-size: 10px; font-weight: 700; letter-spacing: 0.15em; padding: 8px 16px; cursor: pointer; text-transform: uppercase; box-shadow: var(--glow-red); }"
    ".send-btn:hover { background: var(--orange); }"
    ".qcmd { font-family: var(--mono); font-size: 9px; padding: 2px 8px; background: transparent; border: 1px solid rgba(232,0,26,0.4); color: var(--text-dim); cursor: pointer; letter-spacing: 0.05em; margin-right: 4px; }"
    ".qcmd:hover { border-color: var(--orange); color: var(--orange); background: rgba(255,106,0,0.08); }"
    ".quick-cmds { display: flex; gap: 4px; flex-wrap: wrap; margin-bottom: 8px; }"
    ".topbar { background: var(--bg2); border-bottom: 1px solid var(--red); display: flex; align-items: center; padding: 0 16px; height: 100%; box-shadow: 0 2px 12px rgba(232,0,26,0.2); }"
    ".topbar-title { font-family: var(--display); font-size: 14px; font-weight: 900; letter-spacing: 0.2em; text-transform: uppercase; color: var(--red); text-shadow: var(--glow-red); margin-left: 8px; }"
    ".topbar-sub { font-size: 10px; color: var(--text-dim); letter-spacing: 0.1em; margin-left: auto; }"
    ".status-dot { width: 7px; height: 7px; border-radius: 50%; background: var(--orange); box-shadow: var(--glow); animation: pulse 2s ease-in-out infinite; }"
    "@keyframes pulse { 0%,100% { opacity: 1; transform: scale(1); } 50% { opacity: 0.5; transform: scale(0.8); } }"
    ".toggle-btn { background: transparent; border: 1px solid var(--red); color: var(--text-dim); font-family: var(--display); font-size: 9px; padding: 4px 8px; cursor: pointer; letter-spacing: 0.1em; margin-left: 8px; }"
    ".toggle-btn:hover { border-color: var(--orange); color: var(--orange); }"
    ".toggle-btn.active { background: var(--red); color: #000; border-color: var(--red); }"
    ".toggle-btn.inactive { opacity: 0.5; }";

static char *get_socket_path(const char *env_var, const char *pattern) {
    const char *path = getenv(env_var);
    if (path) return g_strdup(path);
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    const char *wayland_display = getenv("WAYLAND_DISPLAY");
    if (!runtime_dir || !wayland_display) return NULL;
    return g_strdup_printf(pattern, runtime_dir, wayland_display);
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

static char *send_ipc_command(int fd, const char *cmd) {
    if (fd < 0) return NULL;
    char buf[8192];
    size_t cmd_len = strlen(cmd);
    if (write(fd, cmd, cmd_len) != (ssize_t)cmd_len) return NULL;
    if (write(fd, "\n", 1) != 1) return NULL;
    
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0) return NULL;
    buf[n] = '\0';
    return g_strdup(buf);
}

static gboolean cmd_socket_callback(GIOChannel *source, GIOCondition condition, gpointer data) {
    PlanarUI *ui = data;
    char buf[256];
    ssize_t n = read(g_io_channel_unix_get_fd(source), buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        if (n > 0 && buf[n-1] == '\n') buf[n-1] = '\0';
        
        if (strcmp(buf, "show_windows") == 0) toggle_panel(ui, "windows", TRUE);
        else if (strcmp(buf, "hide_windows") == 0) toggle_panel(ui, "windows", FALSE);
        else if (strcmp(buf, "show_chat") == 0) toggle_panel(ui, "chat", TRUE);
        else if (strcmp(buf, "hide_chat") == 0) toggle_panel(ui, "chat", FALSE);
        else if (strcmp(buf, "show_logs") == 0) toggle_panel(ui, "logs", TRUE);
        else if (strcmp(buf, "hide_logs") == 0) toggle_panel(ui, "logs", FALSE);
        else if (strcmp(buf, "toggle_windows") == 0) toggle_panel(ui, "windows", !ui->show_windows);
        else if (strcmp(buf, "toggle_chat") == 0) toggle_panel(ui, "chat", !ui->show_chat);
        else if (strcmp(buf, "toggle_logs") == 0) toggle_panel(ui, "logs", !ui->show_logs);
        
        g_print("UI received command: %s\n", buf);
    }
    return TRUE;
}

static gboolean socket_accept_callback(GIOChannel *source, GIOCondition condition, gpointer data) {
    PlanarUI *ui = data;
    int fd = g_io_channel_unix_get_fd(source);
    if (condition & G_IO_IN) {
        int client_fd = accept(fd, NULL, NULL);
        if (client_fd >= 0) {
            GIOChannel *ch = g_io_channel_unix_new(client_fd);
            g_io_add_watch(ch, G_IO_IN | G_IO_HUP, cmd_socket_callback, ui);
        }
    }
    return TRUE;
}

static void create_cmd_socket(PlanarUI *ui) {
    char *path = get_socket_path("PLANAR_AGENT_UI_SOCK", "%s/planar-agent-ui.%s.sock");
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
    
    listen(fd, 5);
    
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    
    GIOChannel *channel = g_io_channel_unix_new(fd);
    g_io_channel_set_flags(channel, G_IO_FLAG_NONBLOCK, NULL);
    g_io_add_watch(channel, G_IO_IN | G_IO_HUP, socket_accept_callback, ui);
    
    g_print("UI command socket: %s\n", path);
    g_free(path);
}

static void handle_compositor_event(PlanarUI *ui, const char *event) {
    g_print("Compositor event: %s\n", event);
    
    if (strncmp(event, "workspace ", 10) == 0 ||
        strncmp(event, "window_close", 12) == 0 ||
        strncmp(event, "window_focus", 12) == 0 ||
        strncmp(event, "window_open", 11) == 0) {
        g_print("Event triggering topbar refresh\n");
        refresh_topbar_timer(ui);
    }
    
    if (strncmp(event, "window_close", 12) == 0 ||
        strncmp(event, "window_open", 11) == 0 ||
        strncmp(event, "window_focus", 12) == 0) {
        g_print("Event triggering window list refresh\n");
        refresh_windows_panel(ui);
    }
}

static gboolean event_socket_callback(GIOChannel *source, GIOCondition condition, gpointer data) {
    PlanarUI *ui = data;
    int fd = g_io_channel_unix_get_fd(source);
    
    if (condition & G_IO_IN) {
        char buf[512];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            char *event = buf;
            char *newline = strchr(event, '\n');
            if (newline) *newline = '\0';
            handle_compositor_event(ui, event);
        }
    }
    return TRUE;
}

static void create_event_socket(PlanarUI *ui) {
    char *path = get_socket_path("PLANAR_EVENT_SOCKET", "%s/planar.%s.event.sock");
    if (!path) return;
    
    ui->event_fd = connect_socket(path);
    if (ui->event_fd >= 0) {
        GIOChannel *channel = g_io_channel_unix_new(ui->event_fd);
        g_io_channel_set_flags(channel, G_IO_FLAG_NONBLOCK, NULL);
        g_io_add_watch(channel, G_IO_IN | G_IO_HUP, event_socket_callback, ui);
    }
    g_print("UI event socket connected: %s\n", path);
    g_free(path);
}

static char *hermes_send_message(const char *message, char **session_id) {
    g_print("hermes_send_message: connecting to 89.167.84.8:5000\n");
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return NULL;
    
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("89.167.84.8");
    addr.sin_port = htons(5000);
    
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        g_print("hermes_send_message: connect failed\n");
        close(fd);
        return NULL;
    }
    
    char *post_data = g_strdup_printf(
        "{\"message\": \"%s\", \"session_id\": %s%s%s}",
        message,
        session_id && *session_id ? "\"" : "",
        session_id && *session_id ? *session_id : "null",
        session_id && *session_id ? "\"" : "");
    
    char *req = g_strdup_printf(
        "POST /chat HTTP/1.1\r\n"
        "Host: 89.167.84.8:5000\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        "%s",
        strlen(post_data), post_data);
    
    write(fd, req, strlen(req));
    g_free(req);
    g_free(post_data);
    
    // Read response with timeout
    struct timeval tv = {5, 0};  // 5 second timeout
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    char buf[16384];
    char *buf_ptr = buf;
    int total = 0;
    int buf_size = sizeof(buf) - 1;
    
    while (total < buf_size) {
        ssize_t n = read(fd, buf_ptr, buf_size - total);
        if (n <= 0) break;
        total += n;
        buf_ptr += n;
        
        // Check if we have a complete response (has \n\n and Content-Length matches)
        buf[total] = '\0';
        char *cl = strstr(buf, "Content-Length: ");
        if (cl) {
            int content_len = atoi(cl + 16);
            char *body_start = strstr(buf, "\n\n");
            if (body_start) {
                int body_start_pos = (body_start + 2) - buf;
                if (total >= body_start_pos + content_len) break;  // Got all data
            }
        }
        // Also break if response is small (like [])
        if (total > 10 && strstr(buf, "[]")) break;
    }
    close(fd);
    
    if (total <= 0) {
        g_print("hermes_send_message: read failed, total=%d\n", total);
        return NULL;
    }
    buf[total] = '\0';
    g_print("hermes_send_message: got %d bytes\n", total);
    
    char *body = buf;
    char *header_end = strstr(buf, "\n\n");
    if (header_end) {
        body = header_end + 2;
    } else {
        header_end = strstr(buf, "\r\n\r\n");
        if (header_end) body = header_end + 4;
    }
    
    g_print("hermes_send_message: body (raw): [%s]\n", body);
    
    g_print("hermes_send_message: body = %.100s\n", body);
    
    if (session_id) {
        char *sid = strstr(body, "\"session_id\":\"");
        if (sid) {
            sid += strlen("\"session_id\":\"");
            char *sid_end = strchr(sid, '\"');
            if (sid_end) {
                *sid_end = '\0';
                g_free(*session_id);
                *session_id = g_strdup(sid);
                g_print("hermes_send_message: session_id = %s\n", *session_id);
            }
        }
    }
    
    return g_strdup(body);
}

static void refresh_chat_panel(PlanarUI *ui) {
    if (!ui->chat_webview) return;
    
    GString *html = g_string_new("<html><head><style>");
    g_string_append(html, PANEL_CSS);
    g_string_append(html,
        "</style></head><body>"
        "<div class='panel' style='height:100%;display:flex;flex-direction:column;'>"
        "  <div class='header'><div class='header-fill amber'></div><span class='header-text'>AGENT CHANNEL</span></div>"
        "  <div class='content' style='flex:1;'>");
    
    if (ui->hermes_session) {
        g_string_append_printf(html,
            "    <div class='msg'><div class='msg-meta'>SESSION</div><div class='msg-bubble system'>Connected: %s</div></div>",
            ui->hermes_session);
    } else {
        g_string_append(html,
            "    <div class='msg'><div class='msg-meta'>SYSTEM</div><div class='msg-bubble system'>Click CHAT to connect</div></div>");
    }
    
    // Show user's message
    if (ui->last_user_message) {
        // Replace \n with <br> for display
        char *display_msg = g_strdup(ui->last_user_message);
        char *p = display_msg;
        while (*p) {
            if (*p == '\\' && *(p+1) == 'n') {
                *p = ' ';
                *(p+1) = ' ';
            }
            p++;
        }
        g_string_append_printf(html,
            "    <div class='msg'><div class='msg-meta'>YOU</div><div class='msg-bubble user'>%s</div></div>",
            display_msg);
        g_free(display_msg);
    }
    
    // Show agent's response
    if (ui->last_hermes_response) {
        // Replace \n with <br> for display
        char *display_resp = g_strdup(ui->last_hermes_response);
        char *p = display_resp;
        while (*p) {
            if (*p == '\\' && *(p+1) == 'n') {
                *p = '<';
                *(p+1) = 'b';
                *(p+2) = 'r';
                *(p+3) = '>';
                *(p+4) = ' ';
                p += 4;
            } else if (*p == '\\' && *(p+1) == 't') {
                *p = ' ';
                *(p+1) = ' ';
                p++;
            } else if (*p == '\\' && *(p+1) == '"') {
                *p = '"';
                *(p+1) = ' ';
                p++;
            }
            p++;
        }
        g_string_append_printf(html,
            "    <div class='msg'><div class='msg-meta'>AGENT</div><div class='msg-bubble assistant'>%s</div></div>",
            display_resp);
        g_free(display_resp);
    }
    
    g_string_append(html, "  </div></div></body></html>");
    
    GBytes *bytes = g_bytes_new_take(g_strdup(html->str), strlen(html->str));
    webkit_web_view_load_bytes(WEBKIT_WEB_VIEW(ui->chat_webview), bytes, "text/html", "UTF-8", NULL);
    g_bytes_unref(bytes);
    g_string_free(html, TRUE);
}

static gboolean http_client_callback(GIOChannel *source, GIOCondition condition, gpointer data) {
    PlanarUI *ui = data;
    int fd = g_io_channel_unix_get_fd(source);
    
    if (condition & G_IO_IN) {
        char buf[256];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            g_print("HTTP received: %s\n", buf);
            char *query = strstr(buf, "GET /?");
            if (query) {
                query += 6;
                char *end = strchr(query, ' ');
                if (end) *end = '\0';
                g_print("HTTP command: %s\n", query);
                
                if (strcmp(query, "toggle_windows") == 0) toggle_panel(ui, "windows", !ui->show_windows);
                else if (strcmp(query, "toggle_chat") == 0) toggle_panel(ui, "chat", !ui->show_chat);
                else if (strcmp(query, "toggle_logs") == 0) toggle_panel(ui, "logs", !ui->show_logs);
                else if (strncmp(query, "goto_window=", 12) == 0) {
                    char *window_id = query + 12;
                    char *cmd = g_strdup_printf("goto_window %s", window_id);
                    g_print("Sending to compositor: %s\n", cmd);
                    char *resp = send_ipc_command(ui->compositor_fd, cmd);
                    g_print("Compositor response: %s\n", resp ? resp : "NULL");
                    g_free(resp);
                    g_free(cmd);
                    refresh_windows_panel(ui);
                }
                else if (strncmp(query, "hermes=", 7) == 0) {
                    char *msg = query + 7;
                    g_print("Hermes message: %s\n", msg);
                    
                    // URL decode the message
                    char *decoded = g_uri_unescape_string(msg, NULL);
                    g_free(ui->last_user_message);
                    ui->last_user_message = g_strdup(decoded ? decoded : msg);
                    g_free(decoded);
                    
                    char *resp = hermes_send_message(msg, &ui->hermes_session);
                    g_print("Hermes response: %s\n", resp ? resp : "NULL");
                    
                    // Parse JSON response to get "response" field - use string search instead
                    if (resp) {
                        // Find "response":"..." - need to find the second quote after response:
                        char *p = strstr(resp, "\"response\":\"");
                        if (p) {
                            p += strlen("\"response\":\"");
                            // Find the closing quote - it's the one before ,"session_id"
                            char *end = strstr(p, "\",\"session_id\"");
                            if (!end) end = strstr(p, "\",\"messages\"");
                            if (end) {
                                *end = '\0';
                                g_free(ui->last_hermes_response);
                                ui->last_hermes_response = g_strdup(p);
                                g_print("Extracted response: %.100s\n", p);
                            }
                        }
                        g_free(resp);
                    }
                    refresh_chat_panel(ui);
                }
            }
            const char *resp = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
            write(fd, resp, strlen(resp));
        }
        close(fd);
    }
    return FALSE;
}

static gboolean http_accept_callback(GIOChannel *source, GIOCondition condition, gpointer data) {
    PlanarUI *ui = data;
    int fd = g_io_channel_unix_get_fd(source);
    if (condition & G_IO_IN) {
        int client_fd = accept(fd, NULL, NULL);
        if (client_fd >= 0) {
            GIOChannel *ch = g_io_channel_unix_new(client_fd);
            g_io_channel_set_flags(ch, G_IO_FLAG_NONBLOCK, NULL);
            g_io_add_watch(ch, G_IO_IN | G_IO_HUP, http_client_callback, ui);
        }
    }
    return TRUE;
}

static void create_http_server(PlanarUI *ui) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return;
    
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(18425);
    
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return;
    }
    
    listen(fd, 5);
    
    GIOChannel *channel = g_io_channel_unix_new(fd);
    g_io_channel_set_flags(channel, G_IO_FLAG_NONBLOCK, NULL);
    g_io_add_watch(channel, G_IO_IN | G_IO_HUP, http_accept_callback, ui);
    
    g_print("UI HTTP server: http://127.0.0.1:18425/\n");
}

static gboolean refresh_topbar_timer(gpointer data) {
    PlanarUI *ui = data;
    if (ui->compositor_fd < 0 || !ui->topbar_webview) return TRUE;
    
    char *resp_ws = send_ipc_command(ui->compositor_fd, "get workspaces");
    char *resp_focused = send_ipc_command(ui->compositor_fd, "get focused");
    
    int ws_num = 1;
    char focused_name_buf[256] = "none";
    char *focused_name = focused_name_buf;
    
    if (resp_ws) {
        g_print("Workspaces response: %s\n", resp_ws);
        JsonParser *parser = json_parser_new();
        GError *err = NULL;
        if (json_parser_load_from_data(parser, resp_ws, -1, &err)) {
            JsonNode *root = json_parser_get_root(parser);
            if (root && JSON_NODE_TYPE(root) == JSON_NODE_OBJECT) {
                JsonArray *arr = json_object_get_array_member(json_node_get_object(root), "data");
                if (arr) {
                    guint len = json_array_get_length(arr);
                    for (guint i = 0; i < len; i++) {
                        JsonObject *ws = json_array_get_object_element(arr, i);
                        if (json_object_get_boolean_member(ws, "active")) {
                            ws_num = json_object_get_int_member(ws, "index");
                            break;
                        }
                    }
                }
            }
        }
        g_object_unref(parser);
        g_free(resp_ws);
    }
    
    if (resp_focused && strstr(resp_focused, "\"ok\":true") && strstr(resp_focused, "\"id\":")) {
        g_print("Focused response: %s\n", resp_focused);
        JsonParser *parser = json_parser_new();
        GError *err = NULL;
        if (json_parser_load_from_data(parser, resp_focused, -1, &err)) {
            JsonNode *root = json_parser_get_root(parser);
            if (root && JSON_NODE_TYPE(root) == JSON_NODE_OBJECT) {
                JsonObject *obj = json_node_get_object(root);
                JsonNode *data_node = json_object_get_member(obj, "data");
                if (data_node && JSON_NODE_TYPE(data_node) != JSON_NODE_NULL) {
                    JsonObject *win = json_node_get_object(data_node);
                    const char *title = json_object_get_string_member(win, "title");
                    const char *app_id = json_object_get_string_member(win, "app_id");
                    const char *src = title && strlen(title) > 0 ? title : (app_id ? app_id : "none");
                    strncpy(focused_name_buf, src, sizeof(focused_name_buf) - 1);
                    focused_name_buf[sizeof(focused_name_buf) - 1] = '\0';
                }
            }
        }
        g_object_unref(parser);
        g_free(resp_focused);
    }
    
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char time_str[16];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm);
    
    GString *html = g_string_new("<html><head><style>");
    g_string_append(html, PANEL_CSS);
    g_string_append_printf(html,
        "</style></head><body>"
        "<div class='topbar'>"
        "  <div class='status-dot'></div>"
        "  <span class='topbar-title'>PLANAR // AGENT</span>"
        "  <span class='topbar-sub' style='margin-left: 20px;'>WS: %d</span>"
        "  <span class='topbar-sub' style='margin-left: 12px;'>%s</span>"
        "  <button class='toggle-btn %s' id='btn-windows'>WINDOWS</button>"
        "  <button class='toggle-btn %s' id='btn-chat'>CHAT</button>"
        "  <button class='toggle-btn %s' id='btn-logs'>LOGS</button>"
        "  <span class='topbar-sub' style='margin-left: auto;'>%s | IPC: CONNECTED</span>"
        "</div>"
        "<script>"
        "  var states = {windows: %s, chat: %s, logs: %s};"
        "  function sendCmd(cmd) {"
        "    var xhr = new XMLHttpRequest();"
        "    xhr.open('GET', 'http://127.0.0.1:18425/?' + cmd, true);"
        "    xhr.send();"
        "  }"
        "  function handleToggle(panel) {"
        "    states[panel] = !states[panel];"
        "    var btn = document.getElementById('btn-' + panel);"
        "    if (states[panel]) { btn.classList.add('active'); btn.classList.remove('inactive'); }"
        "    else { btn.classList.remove('active'); btn.classList.add('inactive'); }"
        "    sendCmd('toggle_' + panel);"
        "  }"
        "  document.getElementById('btn-windows').onclick = function() { handleToggle('windows'); };"
        "  document.getElementById('btn-chat').onclick = function() { handleToggle('chat'); };"
        "  document.getElementById('btn-logs').onclick = function() { handleToggle('logs'); };"
        "</script>"
        "</body></html>",
        ws_num, focused_name,
        ui->btn_state_windows ? "active" : "inactive",
        ui->btn_state_chat ? "active" : "inactive",
        ui->btn_state_logs ? "active" : "inactive",
        time_str,
        ui->btn_state_windows ? "true" : "false",
        ui->btn_state_chat ? "true" : "false",
        ui->btn_state_logs ? "true" : "false");
    
    GBytes *bytes = g_bytes_new_take(g_strdup(html->str), strlen(html->str));
    webkit_web_view_load_bytes(WEBKIT_WEB_VIEW(ui->topbar_webview), bytes, "text/html", "UTF-8", NULL);
    g_bytes_unref(bytes);
    g_string_free(html, TRUE);
    
    return TRUE;
}

static void create_topbar(PlanarUI *ui) {
    GtkWidget *win = gtk_application_window_new(ui->app);
    gtk_layer_init_for_window(GTK_WINDOW(win));
    gtk_layer_set_layer(GTK_WINDOW(win), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_exclusive_zone(GTK_WINDOW(win), 40);
    gtk_window_set_default_size(GTK_WINDOW(win), 800, 40);
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
    
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char time_str[16];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm);
    
    ui->btn_state_windows = TRUE;
    ui->btn_state_chat = FALSE;
    ui->btn_state_logs = FALSE;
    
    GtkWidget *wv = webkit_web_view_new();
    ui->topbar_webview = wv;
    
    char *html = g_strdup_printf(
        "<html><head><style>%s</style></head><body>"
        "<div class='topbar'>"
        "  <div class='status-dot'></div>"
        "  <span class='topbar-title'>PLANAR // AGENT</span>"
        "  <span class='topbar-sub' style='margin-left: 20px;'>WS: 1</span>"
        "  <span class='topbar-sub' style='margin-left: 12px;'>kitty</span>"
        "  <button class='toggle-btn active' id='btn-windows'>WINDOWS</button>"
        "  <button class='toggle-btn' id='btn-chat'>CHAT</button>"
        "  <button class='toggle-btn' id='btn-logs'>LOGS</button>"
        "  <span class='topbar-sub' style='margin-left: auto;'>%s | IPC: CONNECTED</span>"
        "</div>"
    "<script>"
    "  var states = {windows: true, chat: false, logs: false};"
    "  function sendCmd(cmd) {"
    "    var xhr = new XMLHttpRequest();"
    "    xhr.open('GET', 'http://127.0.0.1:18425/?' + cmd, true);"
    "    xhr.send();"
    "  }"
    "  function handleToggle(panel) {"
    "    states[panel] = !states[panel];"
    "    var btn = document.getElementById('btn-' + panel);"
    "    if (states[panel]) { btn.classList.add('active'); btn.classList.remove('inactive'); }"
    "    else { btn.classList.remove('active'); btn.classList.add('inactive'); }"
    "    sendCmd('toggle_' + panel);"
    "  }"
    "  document.getElementById('btn-windows').onclick = function() { handleToggle('windows'); };"
    "  document.getElementById('btn-chat').onclick = function() { handleToggle('chat'); };"
    "  document.getElementById('btn-logs').onclick = function() { handleToggle('logs'); };"
    "</script>"
        "</body></html>", PANEL_CSS, time_str);
    webkit_web_view_load_html(WEBKIT_WEB_VIEW(wv), html, NULL);
    g_free(html);
    gtk_window_set_child(GTK_WINDOW(win), wv);
    gtk_widget_set_visible(win, TRUE);
    ui->topbar = win;
}

static void refresh_windows_panel(PlanarUI *ui) {
    if (!ui->windows_webview || ui->compositor_fd < 0) {
        g_print("refresh_windows_panel: no webview or no compositor_fd\n");
        return;
    }
    
    char *resp = send_ipc_command(ui->compositor_fd, "get windows");
    if (!resp) {
        g_print("refresh_windows_panel: no response from compositor\n");
        return;
    }
    
    g_print("Windows response: %s\n", resp);
    
    JsonParser *parser = json_parser_new();
    GError *err = NULL;
    if (json_parser_load_from_data(parser, resp, -1, &err)) {
        JsonNode *root = json_parser_get_root(parser);
        if (root && JSON_NODE_TYPE(root) == JSON_NODE_OBJECT) {
            JsonObject *obj = json_node_get_object(root);
            JsonArray *arr = json_object_get_array_member(obj, "data");
            if (arr) {
            GString *html = g_string_new("<html><head><style>");
            html = g_string_append(html, PANEL_CSS);
            g_string_append(html,
                "</style></head><body>"
                "<div class='panel' style='height:100%;display:flex;flex-direction:column;'>"
                "  <div class='header'><div class='header-fill'></div><span class='header-text'>CANVAS WINDOWS</span></div>"
                "  <div class='content' style='flex:1;'>");
            
            guint len = json_array_get_length(arr);
            for (guint i = 0; i < len; i++) {
                JsonNode *node = json_array_get_element(arr, i);
                if (JSON_NODE_TYPE(node) == JSON_NODE_OBJECT) {
                    JsonObject *winobj = json_node_get_object(node);
                    const char *id = json_object_get_string_member(winobj, "id");
                    const char *app_id = json_object_get_string_member(winobj, "app_id");
                    const char *title = json_object_get_string_member(winobj, "title");
                    gboolean focused = json_object_get_boolean_member(winobj, "focused");
                    
                    const char *display_name = title && strlen(title) > 0 ? title : app_id;
                    
                    g_string_append_printf(html,
                        "<div class='window-item%s' onclick=\"window.location.href='http://127.0.0.1:18425/?goto_window=%s'\">"
                        "<span class='win-id'>%s</span>"
                        "<span class='win-name'>%s</span></div>",
                        focused ? " focused" : "",
                        id ? id : "",
                        id ? id : "?",
                        display_name ? display_name : "?");
                }
            }
            
            g_string_append(html, "  </div></div></body></html>");
            g_print("Loading HTML: %s\n", html->str);
            GBytes *bytes = g_bytes_new_take(g_strdup(html->str), strlen(html->str));
            webkit_web_view_load_bytes(WEBKIT_WEB_VIEW(ui->windows_webview), bytes, "text/html", "UTF-8", NULL);
            g_bytes_unref(bytes);
            g_string_free(html, TRUE);
            }
        }
    } else {
        g_print("JSON parse error: %s\n", err ? err->message : "unknown");
    }
    g_object_unref(parser);
    g_free(resp);
}

static gboolean on_decide_policy(GtkWidget *webview, WebKitPolicyDecision *decision, WebKitPolicyDecisionType type, gpointer data) {
    PlanarUI *ui = data;
    if (type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) {
        WebKitNavigationAction *action = webkit_navigation_policy_decision_get_navigation_action(WEBKIT_NAVIGATION_POLICY_DECISION(decision));
        WebKitURIRequest *request = webkit_navigation_action_get_request(action);
        const char *uri = webkit_uri_request_get_uri(request);
        g_print("Navigation to: %s\n", uri);
        if (g_str_has_prefix(uri, "http://127.0.0.1:18425/?goto_window=")) {
            const char *window_id = uri + strlen("http://127.0.0.1:18425/?goto_window=");
            char *cmd = g_strdup_printf("goto_window %s", window_id);
            g_print("Sending to compositor: %s\n", cmd);
            char *resp = send_ipc_command(ui->compositor_fd, cmd);
            g_print("Compositor response: %s\n", resp ? resp : "NULL");
            g_free(resp);
            g_free(cmd);
            webkit_policy_decision_ignore(decision);
            refresh_windows_panel(ui);
            return TRUE;
        }
    }
    return FALSE;
}

static void create_windows_panel(PlanarUI *ui) {
    GtkWidget *win = gtk_application_window_new(ui->app);
    gtk_layer_init_for_window(GTK_WINDOW(win));
    gtk_layer_set_layer(GTK_WINDOW(win), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_exclusive_zone(GTK_WINDOW(win), -1);
    gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_TOP, 50);
    gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_LEFT, 6);
    gtk_window_set_default_size(GTK_WINDOW(win), 260, 400);
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
    
    GtkWidget *wv = webkit_web_view_new();
    ui->windows_webview = wv;
    
    g_signal_connect(wv, "decide-policy", G_CALLBACK(on_decide_policy), ui);
    
    WebKitSettings *settings = webkit_settings_new_with_settings(
        "allow-file-access-from-file-urls", TRUE,
        "allow-universal-access-from-file-urls", TRUE,
        NULL);
    webkit_web_view_set_settings(WEBKIT_WEB_VIEW(wv), settings);
    
    char *html = g_strdup_printf(
        "<html><head><style>%s</style></head><body>"
        "<div class='panel' style='height:100%%;display:flex;flex-direction:column;'>"
        "  <div class='header'><div class='header-fill'></div><span class='header-text'>CANVAS WINDOWS</span></div>"
        "  <div class='content' style='flex:1;'>"
        "    <div class='window-item focused'><span class='win-id'>0x001</span><span class='win-name'>kitty</span><span class='win-pos'>120, 80</span></div>"
        "    <div class='window-item'><span class='win-id'>0x002</span><span class='win-name'>firefox</span><span class='win-pos'>800, 200</span></div>"
        "    <div class='window-item'><span class='win-id'>0x003</span><span class='win-name'>nvim</span><span class='win-pos'>400, 500</span></div>"
        "  </div>"
        "</div></body></html>", PANEL_CSS);
    webkit_web_view_load_html(WEBKIT_WEB_VIEW(wv), html, NULL);
    g_free(html);
    gtk_window_set_child(GTK_WINDOW(win), wv);
    gtk_widget_set_visible(win, TRUE);
    ui->windows_panel = win;
}

static void create_chat_panel(PlanarUI *ui) {
    GtkWidget *win = gtk_application_window_new(ui->app);
    gtk_layer_init_for_window(GTK_WINDOW(win));
    gtk_layer_set_layer(GTK_WINDOW(win), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_exclusive_zone(GTK_WINDOW(win), -1);
    gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_LEFT, 270);
    gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_RIGHT, 230);
    gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_TOP, 50);
    gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_BOTTOM, 170);
    gtk_window_set_default_size(GTK_WINDOW(win), 600, 400);
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
    
    GtkWidget *wv = webkit_web_view_new();
    ui->chat_webview = wv;
    
    char *html = g_strdup_printf(
        "<html><head><style>%s</style></head><body>"
        "<div class='panel' style='height:100%%;display:flex;flex-direction:column;'>"
        "  <div class='header'><div class='header-fill amber'></div><span class='header-text'>AGENT CHANNEL</span></div>"
        "  <div class='content' style='flex:1;'>"
        "    <div class='msg'><div class='msg-meta'>00:00:00 &nbsp; SYS</div><div class='msg-bubble system'>PLANAR COMPOSITOR AGENT ONLINE — READY</div></div>"
        "    <div class='msg'><div class='msg-meta'>00:00:00 &nbsp; SYS</div><div class='msg-bubble system'>Click CHAT to start</div></div>"
        "  </div>"
        "</div></body></html>", PANEL_CSS);
    webkit_web_view_load_html(WEBKIT_WEB_VIEW(wv), html, NULL);
    g_free(html);
    gtk_window_set_child(GTK_WINDOW(win), wv);
    gtk_widget_set_visible(win, FALSE);
    ui->chat_panel = win;
}

static void create_event_log(PlanarUI *ui) {
    GtkWidget *win = gtk_application_window_new(ui->app);
    gtk_layer_init_for_window(GTK_WINDOW(win));
    gtk_layer_set_layer(GTK_WINDOW(win), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_exclusive_zone(GTK_WINDOW(win), -1);
    gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_TOP, 50);
    gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_RIGHT, 6);
    gtk_window_set_default_size(GTK_WINDOW(win), 220, 400);
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
    
    GtkWidget *wv = webkit_web_view_new();
    char *html = g_strdup_printf(
        "<html><head><style>%s</style></head><body>"
        "<div class='panel' style='height:100%%;display:flex;flex-direction:column;'>"
        "  <div class='header'><div class='header-fill'></div><span class='header-text'>EVENT LOG</span></div>"
        "  <div class='content' style='flex:1;'>"
        "    <div class='log-item'><div class='log-time'>00:00:01</div><div class='log-event'>COMPOSITOR_READY</div><div class='log-detail'>planar v0.1.0</div></div>"
        "    <div class='log-item'><div class='log-time'>00:00:02</div><div class='log-event'>UI_STARTED</div><div class='log-detail'>agent interface loaded</div></div>"
        "  </div>"
        "</div></body></html>", PANEL_CSS);
    webkit_web_view_load_html(WEBKIT_WEB_VIEW(wv), html, NULL);
    g_free(html);
    gtk_window_set_child(GTK_WINDOW(win), wv);
    gtk_widget_set_visible(win, FALSE);
    ui->log_panel = win;
}

static void create_status_panel(PlanarUI *ui) {
    GtkWidget *win = gtk_application_window_new(ui->app);
    gtk_layer_init_for_window(GTK_WINDOW(win));
    gtk_layer_set_layer(GTK_WINDOW(win), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_exclusive_zone(GTK_WINDOW(win), 160);
    gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_LEFT, 6);
    gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_BOTTOM, 6);
    gtk_window_set_default_size(GTK_WINDOW(win), 260, 160);
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
    
    GtkWidget *wv = webkit_web_view_new();
    char *html = g_strdup_printf(
        "<html><head><style>%s</style></head><body>"
        "<div class='panel' style='height:100%%;display:flex;flex-direction:column;'>"
        "  <div class='header'><div class='header-fill'></div><span class='header-text'>COMPOSITOR STATUS</span></div>"
        "  <div class='content'>"
        "    <div class='stat-row'><span class='stat-label'>SOCKET</span><span class='stat-val'>BOUND</span></div>"
        "    <div class='stat-row'><span class='stat-label'>WINDOWS</span><span class='stat-val'>3</span></div>"
        "    <div class='stat-row'><span class='stat-label'>ZOOM</span><span class='stat-val'>1.00x</span></div>"
        "    <div class='stat-row'><span class='stat-label'>WORKSPACE</span><span class='stat-val'>1</span></div>"
        "    <div class='stat-row'><span class='stat-label'>XWAYLAND</span><span class='stat-val'>ACTIVE</span></div>"
        "    <div class='stat-row'><span class='stat-label'>SCALING</span><span class='stat-val'>1.5x</span></div>"
        "  </div>"
        "</div></body></html>", PANEL_CSS);
    webkit_web_view_load_html(WEBKIT_WEB_VIEW(wv), html, NULL);
    g_free(html);
    gtk_window_set_child(GTK_WINDOW(win), wv);
    gtk_widget_set_visible(win, TRUE);
    ui->status_panel = win;
}

static void create_input_panel(PlanarUI *ui) {
    GtkWidget *win = gtk_application_window_new(ui->app);
    gtk_layer_init_for_window(GTK_WINDOW(win));
    gtk_layer_set_layer(GTK_WINDOW(win), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_exclusive_zone(GTK_WINDOW(win), 110);
    gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_LEFT, 270);
    gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_RIGHT, 230);
    gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_BOTTOM, 6);
    gtk_window_set_default_size(GTK_WINDOW(win), 600, 110);
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
    
    GtkWidget *wv = webkit_web_view_new();
    char *html = g_strdup_printf(
        "<html><head><style>%s</style></head><body>"
        "<div class='panel' style='height:100%%;display:flex;flex-direction:column;padding:8px;'>"
        "  <div class='quick-cmds'>"
        "    <button class='qcmd' onclick='sendQuick(\"list windows\")'>list windows</button>"
        "    <button class='qcmd' onclick='sendQuick(\"focus terminal\")'>focus terminal</button>"
        "    <button class='qcmd' onclick='sendQuick(\"spawn kitty\")'>spawn kitty</button>"
        "    <button class='qcmd' onclick='sendQuick(\"workspace 2\")'>workspace 2</button>"
        "  </div>"
        "  <div style='display:flex;gap:6px;'>"
        "    <textarea id='hermes-input' class='input-area' rows='2' placeholder='Chat with Hermes Agent…'></textarea>"
        "    <button class='send-btn' onclick='sendHermes()'>SEND</button>"
        "  </div>"
        "</div>"
        "<script>"
        "  function sendQuick(cmd) {"
        "    var x=new XMLHttpRequest();"
        "    x.open('GET','http://127.0.0.1:18425/?hermes='+encodeURIComponent(cmd));"
        "    x.send();"
        "  }"
        "  function sendHermes() {"
        "    var input=document.getElementById('hermes-input');"
        "    var msg=input.value.trim();"
        "    if(!msg) return;"
        "    var x=new XMLHttpRequest();"
        "    x.open('GET','http://127.0.0.1:18425/?hermes='+encodeURIComponent(msg));"
        "    x.send();"
        "    input.value='';"
        "  }"
        "  document.getElementById('hermes-input').addEventListener('keydown',function(e){"
        "    if(e.key==='Enter'&&!e.shiftKey){e.preventDefault();sendHermes();}"
        "  });"
        "</script>"
        "</body></html>", PANEL_CSS);
    webkit_web_view_load_html(WEBKIT_WEB_VIEW(wv), html, NULL);
    g_free(html);
    gtk_window_set_child(GTK_WINDOW(win), wv);
    gtk_widget_set_visible(win, TRUE);
    ui->input_panel = win;
}

static void activate(GtkApplication *app, gpointer data) {
    PlanarUI *ui = g_new0(PlanarUI, 1);
    ui->app = app;
    ui->compositor_fd = -1;
    ui->event_fd = -1;
    ui->show_windows = TRUE;
    ui->show_chat = FALSE;
    ui->show_logs = FALSE;
    ui->btn_state_windows = TRUE;
    ui->btn_state_chat = FALSE;
    ui->btn_state_logs = FALSE;
    
    char *cmd_path = get_socket_path("PLANAR_SOCKET", "%s/planar.%s.sock");
    ui->compositor_fd = connect_socket(cmd_path);
    g_free(cmd_path);
    
    create_topbar(ui);
    create_windows_panel(ui);
    create_chat_panel(ui);
    create_event_log(ui);
    create_status_panel(ui);
    create_input_panel(ui);
    
    create_cmd_socket(ui);
    create_http_server(ui);
    create_event_socket(ui);
    
    ui->refresh_timer = g_timeout_add(5000, refresh_topbar_timer, ui);
    refresh_topbar_timer(ui);
    
    g_object_set_data_full(G_OBJECT(app), "ui", ui, g_free);
}

int main(int argc, char *argv[]) {
    GtkApplication *app = gtk_application_new("com.planar.agent-ui", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int ret = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return ret;
}
