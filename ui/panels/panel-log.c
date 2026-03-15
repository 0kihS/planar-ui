#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <time.h>
#include <string.h>
#include "panel-log.h"
#include "panel-css.h"

G_DEFINE_TYPE(PanelLog, panel_log, APP_TYPE_PANEL)

static void panel_log_class_init(PanelLogClass *class) { (void)class; }
static void panel_log_init(PanelLog *p) {
    p->app = NULL;
    p->webview = NULL;
}

static const char *LOG_EXTRA_CSS =
    ".log-section-label { font-family: var(--display); font-size: 7px; color: var(--teal-dim); "
    "  letter-spacing: 0.18em; padding: 6px 8px 2px; text-transform: uppercase; }"
    ".step-tag { font-size: 7px; color: var(--amber); font-family: var(--display); "
    "  letter-spacing: 0.08em; margin-right: 4px; }";

/* JS that lives in the page: sparkline engine + counters */
static const char *LOG_JS =
    "<script>"
    "var sparkData = new Array(60).fill(0);"
    "var eventCount = 0, toolCount = 0;"
    "var currentIteration = 0, totalTools = 0;"
    "var startTime = Date.now();"

    "function setIteration(iter, total) {"
    "  currentIteration = iter; totalTools = total;"
    "}"

    "setInterval(function() {"
    "  sparkData.push(0); sparkData.shift();"
    "  renderSparkline();"
    "  updateCounters();"
    "}, 1000);"

    "function bumpSpark() { sparkData[59]++; renderSparkline(); }"

    "function renderSparkline() {"
    "  var el = document.getElementById('sparkline');"
    "  if (!el) return;"
    "  var max = Math.max(1, Math.max.apply(null, sparkData));"
    "  var html = '';"
    "  for (var i = 0; i < sparkData.length; i++) {"
    "    var h = Math.max(1, Math.round((sparkData[i]/max)*22));"
    "    var cls = (i === 59 && sparkData[i] > 0) ? 'spark-bar active' : 'spark-bar';"
    "    html += '<div class=\"' + cls + '\" style=\"height:' + h + 'px\"></div>';"
    "  }"
    "  el.innerHTML = html;"
    "}"

    "function updateCounters() {"
    "  var el = document.getElementById('counters');"
    "  if (!el) return;"
    "  var elapsed = Math.floor((Date.now() - startTime) / 1000);"
    "  var mm = String(Math.floor(elapsed/60)).padStart(2,'0');"
    "  var ss = String(elapsed % 60).padStart(2,'0');"
    "  el.innerHTML = 'EVENTS <span>' + eventCount + '</span>"
    "    &nbsp;&nbsp; TOOLS <span>' + toolCount + '</span>"
    "    &nbsp;&nbsp; UP <span>' + mm + ':' + ss + '</span>';"
    "}"

    "function addEvent(time, event, detail) {"
    "  eventCount++; bumpSpark();"
    "  var c = document.getElementById('log-content');"
    "  if (!c) return;"
    "  c.innerHTML += '<div class=\"log-item\">"
    "    <div class=\"log-time\">' + time + '</div>"
    "    <div class=\"log-event\">' + event + '</div>"
    "    <div class=\"log-detail\">' + detail + '</div></div>';"
    "  c.scrollTop = c.scrollHeight;"
    "}"

    "function addTool(time, name, preview) {"
    "  toolCount++; bumpSpark();"
    "  var c = document.getElementById('log-content');"
    "  if (!c) return;"
    "  var stepTag = currentIteration > 0"
    "    ? '<span class=\"step-tag\">STEP ' + currentIteration + (totalTools > 0 ? '/' + totalTools : '') + '</span> '"
    "    : '';"
    "  c.innerHTML += '<div class=\"log-item tool-entry\">"
    "    <div class=\"log-time\">' + time + '</div>"
    "    <div class=\"log-event tool-name\">' + stepTag + name + '</div>"
    "    <div class=\"log-detail\">' + preview + '</div></div>';"
    "  c.scrollTop = c.scrollHeight;"
    "}"

    "renderSparkline();"
    "updateCounters();"
    "</script>";

void panel_log_add_event(GtkWidget *widget, const char *event, const char *detail) {
    PanelLog *p = APP_PANEL_LOG(widget);
    if (!p->webview) return;

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char ts[16];
    strftime(ts, sizeof(ts), "%H:%M:%S", tm);

    char *escaped_event = g_markup_escape_text(event, -1);
    char *escaped_detail = detail ? g_markup_escape_text(detail, -1) : g_strdup("");

    char *script = g_strdup_printf(
        "addEvent('%s','%s','%s');", ts, escaped_event, escaped_detail);

    webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(p->webview),
        script, -1, NULL, NULL, NULL, NULL, NULL);

    g_free(script);
    g_free(escaped_event);
    g_free(escaped_detail);
}

void panel_log_add_tool(GtkWidget *widget, const char *tool_info) {
    PanelLog *p = APP_PANEL_LOG(widget);
    if (!p->webview) return;

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char ts[16];
    strftime(ts, sizeof(ts), "%H:%M:%S", tm);

    char *escaped = g_markup_escape_text(tool_info, -1);

    /* Split "tool_name: preview" */
    char *colon = strchr(escaped, ':');
    char *script;
    if (colon) {
        *colon = '\0';
        const char *preview = colon + 1;
        while (*preview == ' ') preview++;
        script = g_strdup_printf("addTool('%s','%s','%s');", ts, escaped, preview);
    } else {
        script = g_strdup_printf("addTool('%s','%s','');", ts, escaped);
    }

    webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(p->webview),
        script, -1, NULL, NULL, NULL, NULL, NULL);

    g_free(script);
    g_free(escaped);
}

void panel_log_set_iteration(GtkWidget *widget, int iteration, int total_tools) {
    PanelLog *p = APP_PANEL_LOG(widget);
    if (!p->webview) return;

    char *script = g_strdup_printf("setIteration(%d,%d);", iteration, total_tools);
    webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(p->webview),
        script, -1, NULL, NULL, NULL, NULL, NULL);
    g_free(script);
}

GtkWidget *panel_log_new(PlanarApp *app) {
    PanelLog *p = g_object_new(APP_TYPE_PANEL_LOG, NULL);
    p->app = app;

    app_panel_configure(&p->panel,
        GTK_LAYER_SHELL_LAYER_OVERLAY,
        TRUE, FALSE, FALSE, TRUE,
        28, 0, 0, 6, -1);

    p->webview = webkit_web_view_new();

    char *html = g_strdup_printf(
        "<html><head><style>%s %s</style></head><body>"
        "<div class='panel' style='height:100%%;display:flex;flex-direction:column;'>"
        "  <div class='header'><div class='header-fill teal-fill'></div>"
        "    <span class='header-text'>MAGI SYSTEM LOG</span></div>"
        "  <div class='caution-bar'></div>"
        "  <div class='spark-container'>"
        "    <div class='spark-label'>Activity</div>"
        "    <div class='sparkline' id='sparkline'></div>"
        "  </div>"
        "  <div class='spark-counters' id='counters'>EVENTS <span>0</span> &nbsp;&nbsp; TOOLS <span>0</span> &nbsp;&nbsp; UP <span>00:00</span></div>"
        "  <div class='content' id='log-content' style='flex:1;overflow-y:auto;'>"
        "    <div class='log-item'><div class='log-time'>00:00:00</div>"
        "    <div class='log-event'>UI_INITIALIZED</div>"
        "    <div class='log-detail'>magi system online</div></div>"
        "  </div>"
        "</div>"
        "%s"
        "</body></html>", PANEL_CSS, LOG_EXTRA_CSS, LOG_JS);

    webkit_web_view_load_html(WEBKIT_WEB_VIEW(p->webview), html, NULL);
    g_free(html);

    app_panel_set_content(&p->panel, p->webview);
    gtk_window_set_default_size(GTK_WINDOW(p), 260, 400);
    app_panel_show(&p->panel);

    return GTK_WIDGET(p);
}
