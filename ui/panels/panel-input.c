#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <string.h>
#include "panel-input.h"
#include "panel-css.h"

G_DEFINE_TYPE(PanelInput, panel_input, APP_TYPE_PANEL)
static void panel_input_class_init(PanelInputClass *class) { (void)class; }
static void panel_input_init(PanelInput *p) {
    p->app = NULL;
    p->webview = NULL;
}

static gboolean on_input_decide_policy(GtkWidget *webview,
    WebKitPolicyDecision *decision, WebKitPolicyDecisionType type, gpointer data)
{
    PanelInput *p = (PanelInput *)data;
    if (type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) return FALSE;

    WebKitNavigationAction *action = webkit_navigation_policy_decision_get_navigation_action(
        WEBKIT_NAVIGATION_POLICY_DECISION(decision));
    WebKitURIRequest *request = webkit_navigation_action_get_request(action);
    const char *uri = webkit_uri_request_get_uri(request);

    if (!g_str_has_prefix(uri, "planar://send?msg=")) return FALSE;

    const char *encoded_msg = uri + strlen("planar://send?msg=");
    char *msg = g_uri_unescape_string(encoded_msg, NULL);

    if (msg && msg[0] && p->app) {
        planar_app_send_to_agent(p->app, msg);
    }
    g_free(msg);

    webkit_policy_decision_ignore(decision);
    return TRUE;
}

GtkWidget *panel_input_new(PlanarApp *app) {
    PanelInput *p = g_object_new(APP_TYPE_PANEL_INPUT, NULL);
    p->app = app;

    app_panel_configure(&p->panel,
        GTK_LAYER_SHELL_LAYER_OVERLAY,
        FALSE, TRUE, TRUE, TRUE,
        0, 0, 270, 230, -1);

    p->webview = webkit_web_view_new();
    GdkRGBA transparent = {0, 0, 0, 0};
    webkit_web_view_set_background_color(WEBKIT_WEB_VIEW(p->webview), &transparent);
    g_signal_connect(p->webview, "decide-policy", G_CALLBACK(on_input_decide_policy), p);

    char *html = g_strdup_printf(
        "<html><head><style>%s body{background:transparent;}</style>"
        "<style>"
        "  .input-bar { height: 36px; transition: height 0.2s ease, padding 0.2s ease; "
        "    padding: 4px 8px; display: flex; gap: 6px; align-items: center; "
        "    background: var(--bg2); border: 1px solid rgba(232,0,26,0.3); border-top: none; "
        "    border-radius: 0 0 3px 3px; }"
        "  .input-bar.expanded { height: 64px; padding: 8px; align-items: flex-end; }"
        "  #hermes-input { flex: 1; background: var(--bg); border: 1px solid var(--teal-dim); "
        "    color: var(--teal); font-family: var(--mono); font-size: 11px; padding: 4px 8px; "
        "    resize: none; outline: none; transition: border-color 0.2s; }"
        "  #hermes-input:focus { border-color: var(--teal); }"
        "  .send-btn { background: var(--red); color: #fff; border: 1px solid var(--red); "
        "    font-family: var(--display); font-size: 9px; font-weight: 900; letter-spacing: 0.12em; "
        "    padding: 6px 14px; cursor: pointer; flex-shrink: 0; align-self: center; "
        "    border-radius: 2px; box-shadow: 0 0 8px rgba(232,0,26,0.4); transition: all 0.2s ease; "
        "    text-shadow: 0 0 4px rgba(255,255,255,0.3); }"
        "  .send-btn:hover { background: var(--teal); border-color: var(--teal); color: #000; "
        "    box-shadow: 0 0 10px rgba(0,229,255,0.5); text-shadow: none; }"
        "</style></head><body>"
        "<div style='height:100%%;display:flex;flex-direction:column;justify-content:flex-end;'>"
        "  <div class='caution-bar'></div>"
        "  <div id='input-bar' class='input-bar'>"
        "    <textarea id='hermes-input' rows='1' placeholder='// TRANSMIT...'></textarea>"
        "    <button class='send-btn' onclick='doSend()'>SEND</button>"
        "  </div>"
        "</div>"
        "<script>"
        "  var bar = document.getElementById('input-bar');"
        "  var input = document.getElementById('hermes-input');"
        "  input.addEventListener('focus', function() { bar.classList.add('expanded'); input.rows = 3; });"
        "  input.addEventListener('blur', function() { if (!input.value.trim()) { bar.classList.remove('expanded'); input.rows = 1; } });"
        "  function doSend() {"
        "    var msg = input.value.trim();"
        "    if (!msg) return;"
        "    input.value = ''; bar.classList.remove('expanded'); input.rows = 1;"
        "    window.location.href = 'planar://send?msg=' + encodeURIComponent(msg);"
        "  }"
        "  input.addEventListener('keydown', function(e) {"
        "    if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); doSend(); }"
        "  });"
        "</script>"
        "</body></html>", PANEL_CSS);

    webkit_web_view_load_html(WEBKIT_WEB_VIEW(p->webview), html, NULL);
    g_free(html);

    app_panel_set_content(&p->panel, p->webview);
    gtk_window_set_default_size(GTK_WINDOW(p), 600, 70);
    app_panel_show(&p->panel);

    gtk_layer_set_keyboard_mode(GTK_WINDOW(p), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);

    return GTK_WIDGET(p);
}
