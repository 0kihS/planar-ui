#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <json-glib/json-glib.h>
#include <stdio.h>
#include "panel-windows.h"
#include "panel-css.h"
#include "../ipc/compositor-ipc.h"

G_DEFINE_TYPE(PanelWindows, panel_windows, APP_TYPE_PANEL)

static void panel_windows_class_init(PanelWindowsClass *class) { (void)class; }
static void panel_windows_init(PanelWindows *p) {
    p->app = NULL;
    p->webview = NULL;
}

static gboolean panel_windows_refresh_idle(gpointer data);

static gboolean on_windows_decide_policy(GtkWidget *webview,
    WebKitPolicyDecision *decision, WebKitPolicyDecisionType type, gpointer data)
{
    PanelWindows *p = (PanelWindows *)data;
    if (type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) return FALSE;

    WebKitNavigationAction *action = webkit_navigation_policy_decision_get_navigation_action(
        WEBKIT_NAVIGATION_POLICY_DECISION(decision));
    WebKitURIRequest *request = webkit_navigation_action_get_request(action);
    const char *uri = webkit_uri_request_get_uri(request);

    if (!g_str_has_prefix(uri, "planar://goto_window?id=")) return FALSE;

    const char *window_id = uri + strlen("planar://goto_window?id=");
    if (p->app && p->app->compositor && window_id[0]) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "goto_window %s", window_id);
        compositor_ipc_send_command(p->app->compositor, cmd);
    }

    webkit_policy_decision_ignore(decision);
    g_idle_add(panel_windows_refresh_idle, p);
    return TRUE;
}

static gboolean panel_windows_refresh_idle(gpointer data) {
    panel_windows_refresh(GTK_WIDGET(data));
    return G_SOURCE_REMOVE;
}

void panel_windows_refresh(GtkWidget *widget) {
    PanelWindows *p = APP_PANEL_WINDOWS(widget);
    if (!p->app || !p->app->compositor || !p->webview) return;

    GString *html = g_string_new("<html><head><style>");
    g_string_append(html, PANEL_CSS);
    g_string_append(html,
        "</style></head><body>"
        "<div class='panel' style='height:100%;display:flex;flex-direction:column;'>"
        "  <div class='header'><div class='header-fill'></div><span class='header-text'>CANVAS WINDOWS</span></div>"
        "  <div class='caution-bar'></div>"
        "  <div class='content' style='flex:1;'>");

    char *json = NULL;
    if (compositor_ipc_get_windows(p->app->compositor, &json)) {
        JsonParser *parser = json_parser_new();
        if (json_parser_load_from_data(parser, json, -1, NULL)) {
            JsonNode *root = json_parser_get_root(parser);
            JsonArray *arr = NULL;
            if (root && JSON_NODE_HOLDS_OBJECT(root)) {
                JsonObject *root_obj = json_node_get_object(root);
                if (json_object_has_member(root_obj, "data"))
                    arr = json_object_get_array_member(root_obj, "data");
            } else if (root && JSON_NODE_HOLDS_ARRAY(root)) {
                arr = json_node_get_array(root);
            }
            if (arr) {
                guint len = json_array_get_length(arr);
                for (guint i = 0; i < len; i++) {
                    JsonObject *obj = json_array_get_object_element(arr, i);
                    const char *id = json_object_get_string_member(obj, "id");
                    const char *app_id = json_object_get_string_member(obj, "app_id");
                    const char *title = NULL;
                    if (json_object_has_member(obj, "title"))
                        title = json_object_get_string_member(obj, "title");
                    gboolean focused = json_object_get_boolean_member(obj, "focused");

                    const char *display_name = (title && strlen(title) > 0) ? title : app_id;

                    g_string_append_printf(html,
                        "<div class='window-item%s' onclick=\"window.location.href='planar://goto_window?id=%s'\">"
                        "<span class='win-hex'>&#x2B22;</span>"
                        "<span class='win-id'>%s</span>"
                        "<span class='win-name'>%s</span></div>",
                        focused ? " focused" : "",
                        id ? id : "",
                        id ? id : "?",
                        display_name ? display_name : "?");
                }
            }
        }
        g_object_unref(parser);
        g_free(json);
    }

    g_string_append(html, "  </div></div></body></html>");
    webkit_web_view_load_html(WEBKIT_WEB_VIEW(p->webview), html->str, NULL);
    g_string_free(html, TRUE);
}

GtkWidget *panel_windows_new(PlanarApp *app) {
    PanelWindows *p = g_object_new(APP_TYPE_PANEL_WINDOWS, NULL);
    p->app = app;

    app_panel_configure(&p->panel,
        GTK_LAYER_SHELL_LAYER_OVERLAY,
        TRUE, FALSE, TRUE, FALSE,
        28, 0, 6, 0, -1);

    p->webview = webkit_web_view_new();
    g_signal_connect(p->webview, "decide-policy", G_CALLBACK(on_windows_decide_policy), p);

    app_panel_set_content(&p->panel, p->webview);
    gtk_window_set_default_size(GTK_WINDOW(p), 260, 400);
    app_panel_show(&p->panel);

    panel_windows_refresh(GTK_WIDGET(p));

    return GTK_WIDGET(p);
}
