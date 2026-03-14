#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <json-glib/json-glib.h>
#include <time.h>
#include <string.h>
#include "panel-topbar.h"
#include "panel-css.h"
#include "../ipc/compositor-ipc.h"

G_DEFINE_TYPE(PanelTopbar, panel_topbar, APP_TYPE_PANEL)
static void panel_topbar_class_init(PanelTopbarClass *class) { (void)class; }
static void panel_topbar_init(PanelTopbar *p) {
    p->app = NULL;
    p->webview = NULL;
    p->refresh_timer = 0;
}

static gboolean on_topbar_decide_policy(GtkWidget *webview,
    WebKitPolicyDecision *decision, WebKitPolicyDecisionType type, gpointer data)
{
    PanelTopbar *p = (PanelTopbar *)data;
    if (type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) return FALSE;

    WebKitNavigationAction *action = webkit_navigation_policy_decision_get_navigation_action(
        WEBKIT_NAVIGATION_POLICY_DECISION(decision));
    WebKitURIRequest *request = webkit_navigation_action_get_request(action);
    const char *uri = webkit_uri_request_get_uri(request);

    if (!g_str_has_prefix(uri, "planar://")) return FALSE;

    if (strstr(uri, "toggle_windows") && p->app->windows_panel) {
        gboolean vis = gtk_widget_get_visible(p->app->windows_panel);
        gtk_widget_set_visible(p->app->windows_panel, !vis);
    } else if (strstr(uri, "toggle_chat") && p->app->chat_panel) {
        gboolean vis = gtk_widget_get_visible(p->app->chat_panel);
        gtk_widget_set_visible(p->app->chat_panel, !vis);
    } else if (strstr(uri, "toggle_logs") && p->app->log_panel) {
        gboolean vis = gtk_widget_get_visible(p->app->log_panel);
        gtk_widget_set_visible(p->app->log_panel, !vis);
    }

    webkit_policy_decision_ignore(decision);
    panel_topbar_refresh(GTK_WIDGET(p));
    return TRUE;
}

void panel_topbar_refresh(GtkWidget *widget) {
    PanelTopbar *p = APP_PANEL_TOPBAR(widget);
    if (!p->webview) return;

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char time_str[16];
    strftime(time_str, sizeof(time_str), "%H:%M", tm);

    gboolean comp_online = (p->app && p->app->compositor);
    gboolean agent_online = (p->app && p->app->agent);
    const char *sys_status = comp_online ? "SYSTEM ACTIVE" : "OFFLINE";
    const char *dot_class = comp_online ? "" : " offline";

    /* Query active workspace */
    int active_ws = 1;
    if (comp_online) {
        char *ws_json = NULL;
        if (compositor_ipc_get_workspaces(p->app->compositor, &ws_json) && ws_json) {
            JsonParser *parser = json_parser_new();
            if (json_parser_load_from_data(parser, ws_json, -1, NULL)) {
                JsonNode *root = json_parser_get_root(parser);
                if (root && JSON_NODE_HOLDS_OBJECT(root)) {
                    JsonObject *obj = json_node_get_object(root);
                    if (json_object_has_member(obj, "active"))
                        active_ws = (int)json_object_get_int_member(obj, "active") + 1;
                    else if (json_object_has_member(obj, "data")) {
                        JsonNode *data_node = json_object_get_member(obj, "data");
                        if (JSON_NODE_HOLDS_OBJECT(data_node)) {
                            JsonObject *data_obj = json_node_get_object(data_node);
                            if (json_object_has_member(data_obj, "active"))
                                active_ws = (int)json_object_get_int_member(data_obj, "active") + 1;
                        } else if (JSON_NODE_HOLDS_VALUE(data_node)) {
                            active_ws = (int)json_node_get_int(data_node) + 1;
                        }
                    }
                }
            }
            g_object_unref(parser);
            g_free(ws_json);
        }
    }

    gboolean win_vis = p->app->windows_panel && gtk_widget_get_visible(p->app->windows_panel);
    gboolean chat_vis = p->app->chat_panel && gtk_widget_get_visible(p->app->chat_panel);
    gboolean log_vis = p->app->log_panel && gtk_widget_get_visible(p->app->log_panel);

    char *html = g_strdup_printf(
        "<html><head><style>%s body{background:transparent;}</style></head><body>"
        "<div class='topbar'>"
        "  <div class='topbar-caution'></div>"
        "  <span class='topbar-title'>PLANAR</span>"
        "  <div class='topbar-sep'></div>"
        "  <div class='topbar-status'>"
        "    <div class='status-dot%s'></div>"
        "    <span class='topbar-sub'>%s</span>"
        "  </div>"
        "  <div class='topbar-sep'></div>"
        "  <span class='topbar-sub'>WS <span class='val'>%d</span></span>"
        "  <div class='topbar-sep'></div>"
        "  <span class='topbar-sub'><span class='val'>%s</span></span>"
        "  <div style='flex:1;'></div>"
        "  <button class='toggle-btn %s' onclick=\"window.location.href='planar://toggle_windows'\">WIN</button>"
        "  <button class='toggle-btn %s' onclick=\"window.location.href='planar://toggle_chat'\">CHAT</button>"
        "  <button class='toggle-btn %s' onclick=\"window.location.href='planar://toggle_logs'\">LOG</button>"
        "  <div class='topbar-sep'></div>"
        "  <span class='topbar-sub'>IPC <span class='val'>%s</span></span>"
        "  <div class='topbar-sep'></div>"
        "  <span class='topbar-sub'>AGENT <span class='val'>%s</span></span>"
        "</div>"
        "</body></html>",
        PANEL_CSS,
        dot_class,
        sys_status,
        active_ws,
        time_str,
        win_vis ? "active" : "inactive",
        chat_vis ? "active" : "inactive",
        log_vis ? "active" : "inactive",
        comp_online ? "BOUND" : "NONE",
        agent_online ? "LINK" : "DOWN");

    webkit_web_view_load_html(WEBKIT_WEB_VIEW(p->webview), html, NULL);
    g_free(html);
}

static gboolean topbar_tick(gpointer data) {
    panel_topbar_refresh(GTK_WIDGET(data));
    return G_SOURCE_CONTINUE;
}

GtkWidget *panel_topbar_new(PlanarApp *app) {
    PanelTopbar *p = g_object_new(APP_TYPE_PANEL_TOPBAR, NULL);
    p->app = app;

    app_panel_configure(&p->panel,
        GTK_LAYER_SHELL_LAYER_OVERLAY,
        TRUE, FALSE, TRUE, TRUE,
        0, 0, 0, 0, 28);

    p->webview = webkit_web_view_new();
    GdkRGBA transparent = {0, 0, 0, 0};
    webkit_web_view_set_background_color(WEBKIT_WEB_VIEW(p->webview), &transparent);
    g_signal_connect(p->webview, "decide-policy", G_CALLBACK(on_topbar_decide_policy), p);

    app_panel_set_content(&p->panel, p->webview);
    gtk_window_set_default_size(GTK_WINDOW(p), 800, 28);
    app_panel_show(&p->panel);

    panel_topbar_refresh(GTK_WIDGET(p));

    /* Refresh clock every 60 seconds */
    p->refresh_timer = g_timeout_add(60000, topbar_tick, p);

    return GTK_WIDGET(p);
}
