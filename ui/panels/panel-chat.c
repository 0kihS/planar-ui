#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <time.h>
#include <string.h>
#include "panel-chat.h"
#include "panel-css.h"

G_DEFINE_TYPE(PanelChat, panel_chat, APP_TYPE_PANEL)

static void panel_chat_class_init(PanelChatClass *class) { (void)class; }
static void panel_chat_init(PanelChat *p) {
    p->app = NULL;
    p->messages = NULL;
    p->webview = NULL;
}

static const char *CHAT_EXTRA_CSS =
    ".progress-bar { padding: 6px 12px; border-left: 2px solid var(--teal); "
    "background: rgba(0,229,255,0.04); font-size: 11px; color: var(--teal); "
    "font-family: var(--mono); animation: fadeUp 0.2s ease; border-radius: 0 2px 2px 0; }"
    ".progress-bar .tool-name { color: var(--teal); font-weight: bold; "
    "font-family: var(--display); font-size: 8px; letter-spacing: 0.12em; }"
    ".progress-bar .tool-preview { color: #00e5ffaa; margin-top: 2px; "
    "white-space: nowrap; overflow: hidden; text-overflow: ellipsis; font-size: 10px; }"
    ".spinner { display: inline-block; width: 5px; height: 5px; "
    "border-radius: 50%; background: var(--teal); margin-right: 6px; "
    "box-shadow: 0 0 6px var(--teal); animation: pulse 1s ease-in-out infinite; }";

/* Escape text for safe embedding inside a JS single-quoted string that
   will be assigned to innerHTML.  Steps:
   1. HTML-escape (& < > ") via g_markup_escape_text
   2. Escape backslashes and single quotes for JS
   3. Convert newlines to <br> tags */
static char *escape_for_js_html(const char *text) {
    char *html = g_markup_escape_text(text, -1);
    GString *out = g_string_sized_new(strlen(html) + 64);
    for (const char *p = html; *p; p++) {
        switch (*p) {
            case '\\': g_string_append(out, "\\\\"); break;
            case '\'': g_string_append(out, "\\'"); break;
            case '\n': g_string_append(out, "<br>"); break;
            case '\r': break;
            default:   g_string_append_c(out, *p); break;
        }
    }
    g_free(html);
    return g_string_free(out, FALSE);
}

void panel_chat_load_html(GtkWidget *widget, const char *html) {
    PanelChat *p = APP_PANEL_CHAT(widget);
    if (!p->webview) return;

    char *full_html = g_strdup_printf(
        "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<style>%s %s body { overflow-y: auto; }</style></head><body>"
        "<div class='panel' style='min-height:100%%;display:flex;flex-direction:column;'>"
        "  <div class='header'><div class='header-fill amber'></div><span class='header-text'>AGENT CHANNEL</span></div>"
        "  <div class='caution-bar'></div>"
        "  <div class='content' id='chat-content' style='flex:1;'>%s</div>"
        "  <div id='progress-zone'></div>"
        "</div></body></html>",
        PANEL_CSS, CHAT_EXTRA_CSS, html);

    webkit_web_view_load_html(WEBKIT_WEB_VIEW(p->webview), full_html, NULL);
    g_free(full_html);
}

void panel_chat_add_message(GtkWidget *widget, const char *role, const char *content) {
    PanelChat *p = APP_PANEL_CHAT(widget);
    if (!p->webview) return;

    WebKitWebView *wv = WEBKIT_WEB_VIEW(p->webview);

    /* Tool/progress messages: update the progress zone in-place */
    if (strcmp(role, "tool") == 0) {
        char *escaped = escape_for_js_html(content);

        /* Split "tool_name: preview" into parts */
        char *colon = strchr(escaped, ':');
        char *script;
        if (colon) {
            *colon = '\0';
            const char *preview = colon + 1;
            while (*preview == ' ') preview++;
            script = g_strdup_printf(
                "var pz = document.getElementById('progress-zone');"
                "if(pz) { pz.innerHTML = '<div class=\"progress-bar\">"
                "<div><span class=\"spinner\"></span><span class=\"tool-name\">%s</span></div>"
                "<div class=\"tool-preview\">%s</div></div>'; }"
                "window.scrollTo(0, document.body.scrollHeight);",
                escaped, preview);
        } else {
            script = g_strdup_printf(
                "var pz = document.getElementById('progress-zone');"
                "if(pz) { pz.innerHTML = '<div class=\"progress-bar\">"
                "<span class=\"spinner\"></span><span class=\"tool-name\">%s</span></div>'; }"
                "window.scrollTo(0, document.body.scrollHeight);",
                escaped);
        }

        webkit_web_view_evaluate_javascript(wv, script, -1, NULL, NULL, NULL, NULL, NULL);
        g_free(script);
        g_free(escaped);
        return;
    }

    /* For non-tool messages, clear the progress zone first */
    webkit_web_view_evaluate_javascript(wv,
        "var pz = document.getElementById('progress-zone'); if(pz) pz.innerHTML = '';",
        -1, NULL, NULL, NULL, NULL, NULL);

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char ts[16];
    strftime(ts, sizeof(ts), "%H:%M:%S", tm);

    char *upper_role = g_ascii_strup(role, -1);
    char *escaped_content = escape_for_js_html(content);
    char *script = g_strdup_printf(
        "var c = document.getElementById('chat-content');"
        "if(c) {"
        "  c.innerHTML += '<div class=\"msg\"><div class=\"msg-meta\">%s &nbsp; %s</div>"
        "<div class=\"msg-bubble %s\">%s</div></div>';"
        "  window.scrollTo(0, document.body.scrollHeight);"
        "}",
        ts, upper_role, role, escaped_content);

    webkit_web_view_evaluate_javascript(wv, script, -1, NULL, NULL, NULL, NULL, NULL);
    g_free(script);
    g_free(escaped_content);
    g_free(upper_role);
}

GtkWidget *panel_chat_new(PlanarApp *app) {
    PanelChat *p = g_object_new(APP_TYPE_PANEL_CHAT, NULL);
    p->app = app;

    app_panel_configure(&p->panel,
        GTK_LAYER_SHELL_LAYER_OVERLAY,
        TRUE, TRUE, TRUE, TRUE,
        28, 70, 270, 275, -1);

    p->webview = webkit_web_view_new();

    app_panel_set_content(&p->panel, p->webview);
    gtk_window_set_default_size(GTK_WINDOW(p), 600, 400);
    app_panel_show(&p->panel);

    const char *init_html =
        "<div class='msg'><div class='msg-meta'>00:00:00 &nbsp; SYS</div>"
        "<div class='msg-bubble system'>MAGI INTERFACE ONLINE // AWAITING INSTRUCTIONS</div></div>";
    panel_chat_load_html(GTK_WIDGET(p), init_html);

    return GTK_WIDGET(p);
}
