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
    "box-shadow: 0 0 6px var(--teal); animation: pulse 1s ease-in-out infinite; }"

    /* Clarify card */
    ".clarify-card { background: rgba(255,179,0,0.08); border: 1px solid rgba(255,179,0,0.3); "
    "border-radius: 4px; padding: 12px; margin: 8px 0; animation: fadeUp 0.3s ease; }"
    ".clarify-question { font-family: var(--display); font-size: 10px; color: var(--amber); "
    "letter-spacing: 0.08em; margin-bottom: 10px; }"
    ".clarify-choices { display: flex; flex-direction: column; gap: 6px; }"
    ".clarify-btn { display: block; background: rgba(0,229,255,0.06); "
    "border: 1px solid var(--teal-dim); color: var(--teal); font-family: var(--mono); "
    "font-size: 11px; padding: 8px 12px; cursor: pointer; text-align: left; text-decoration: none; "
    "border-radius: 2px; transition: all 0.2s ease; }"
    ".clarify-btn:hover { background: rgba(0,229,255,0.12); border-color: var(--teal); "
    "box-shadow: 0 0 8px rgba(0,229,255,0.3); }"

    /* Reasoning bubble */
    ".msg-bubble.reasoning { border-left-color: #556; background: rgba(85,85,102,0.05); "
    "color: #667; font-size: 10px; font-style: italic; cursor: pointer; "
    "max-height: 60px; overflow: hidden; transition: max-height 0.3s ease; }"
    ".msg-bubble.reasoning.expanded { max-height: 2000px; }"
    ;

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

/* Escape for JS single-quoted string only (no HTML escaping).
   Used when the JS side handles its own HTML escaping. */
static char *escape_for_js(const char *text) {
    GString *out = g_string_sized_new(strlen(text) + 32);
    for (const char *p = text; *p; p++) {
        switch (*p) {
            case '\\': g_string_append(out, "\\\\"); break;
            case '\'': g_string_append(out, "\\'"); break;
            case '\n': g_string_append(out, "\\n"); break;
            case '\r': break;
            default:   g_string_append_c(out, *p); break;
        }
    }
    return g_string_free(out, FALSE);
}

static void on_clarify_script_message(WebKitUserContentManager *manager,
    JSCValue *value, gpointer data)
{
    (void)manager;
    PanelChat *p = (PanelChat *)data;
    char *answer = jsc_value_to_string(value);
    if (answer && p->app) {
        planar_app_send_clarify_response(p->app, answer);
    }
    g_free(answer);
}

/* JS helpers injected into the chat page */
static const char *CHAT_JS =
    "<script>"
    "function showClarify(question, choices) {"
    "  var c = document.getElementById('chat-content');"
    "  if (!c) return;"
    "  var safe = function(s) { return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); };"
    "  var card = document.createElement('div');"
    "  card.className = 'clarify-card';"
    "  card.innerHTML = '<div class=\"clarify-question\">' + safe(question) + '</div><div class=\"clarify-choices\"></div>';"
    "  var box = card.querySelector('.clarify-choices');"
    "  for (var i = 0; i < choices.length; i++) {"
    "    var btn = document.createElement('button');"
    "    btn.className = 'clarify-btn';"
    "    btn.textContent = choices[i];"
    "    btn.dataset.answer = choices[i];"
    "    btn.addEventListener('click', function() {"
    "      var answer = this.dataset.answer;"
    "      var btns = card.querySelectorAll('.clarify-btn');"
    "      for (var j = 0; j < btns.length; j++) btns[j].disabled = true;"
    "      this.style.borderColor = 'var(--amber)';"
    "      this.style.color = 'var(--amber)';"
    "      window.webkit.messageHandlers.clarifyResponse.postMessage(answer);"
    "    });"
    "    box.appendChild(btn);"
    "  }"
    "  c.appendChild(card);"
    "  window.scrollTo(0, document.body.scrollHeight);"
    "}"
    "function toggleReasoning(el) {"
    "  el.classList.toggle('expanded');"
    "}"
    "</script>";

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
        "</div>"
        "%s"
        "</body></html>",
        PANEL_CSS, CHAT_EXTRA_CSS, html, CHAT_JS);

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

    /* Reasoning messages: collapsible thinking block */
    if (strcmp(role, "reasoning") == 0) {
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        char ts[16];
        strftime(ts, sizeof(ts), "%H:%M:%S", tm);

        char *escaped = escape_for_js_html(content);
        char *script = g_strdup_printf(
            "var pz = document.getElementById('progress-zone'); if(pz) pz.innerHTML = '';"
            "var c = document.getElementById('chat-content');"
            "if(c) {"
            "  c.innerHTML += '<div class=\"msg\"><div class=\"msg-meta\">%s &nbsp; REASONING</div>"
            "<div class=\"msg-bubble reasoning\" onclick=\"toggleReasoning(this)\">%s</div></div>';"
            "  window.scrollTo(0, document.body.scrollHeight);"
            "}",
            ts, escaped);

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

void panel_chat_show_clarify(GtkWidget *widget, const char *question, const char *choices_json) {
    PanelChat *p = APP_PANEL_CHAT(widget);
    if (!p->webview) return;

    char *esc_q = escape_for_js(question ? question : "Agent is asking...");

    /* choices_json is a valid JSON array from json_generator_to_data — pass directly as JS literal */
    char *script = g_strdup_printf(
        "showClarify('%s', %s);",
        esc_q,
        choices_json ? choices_json : "[]");

    webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(p->webview),
        script, -1, NULL, NULL, NULL, NULL, NULL);

    g_free(script);
    g_free(esc_q);
}

GtkWidget *panel_chat_new(PlanarApp *app) {
    PanelChat *p = g_object_new(APP_TYPE_PANEL_CHAT, NULL);
    p->app = app;

    app_panel_configure(&p->panel,
        GTK_LAYER_SHELL_LAYER_OVERLAY,
        TRUE, TRUE, TRUE, TRUE,
        28, 70, 270, 275, -1);

    p->webview = webkit_web_view_new();
    WebKitUserContentManager *ucm = webkit_web_view_get_user_content_manager(
        WEBKIT_WEB_VIEW(p->webview));
    g_signal_connect(ucm, "script-message-received::clarifyResponse",
        G_CALLBACK(on_clarify_script_message), p);
    webkit_user_content_manager_register_script_message_handler(ucm, "clarifyResponse", NULL);

    app_panel_set_content(&p->panel, p->webview);
    gtk_window_set_default_size(GTK_WINDOW(p), 600, 400);
    app_panel_show(&p->panel);

    const char *init_html =
        "<div class='msg'><div class='msg-meta'>00:00:00 &nbsp; SYS</div>"
        "<div class='msg-bubble system'>MAGI INTERFACE ONLINE // AWAITING INSTRUCTIONS</div></div>";
    panel_chat_load_html(GTK_WIDGET(p), init_html);

    return GTK_WIDGET(p);
}
