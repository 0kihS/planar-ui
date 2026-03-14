#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "panel-status.h"
#include "panel-css.h"
#include "../ipc/compositor-ipc.h"

G_DEFINE_TYPE(PanelStatus, panel_status, APP_TYPE_PANEL)
static void panel_status_class_init(PanelStatusClass *class) { (void)class; }
static void panel_status_init(PanelStatus *p) {
    p->app = NULL;
    p->webview = NULL;
    p->refresh_timer = 0;
    p->prev_total = 0;
    p->prev_idle = 0;
}

/* ── System info readers ──────────────────────────────────────────────── */

static int read_cpu_percent(PanelStatus *p) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return -1;

    char line[256];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return -1; }
    fclose(f);

    guint64 user, nice, system, idle, iowait, irq, softirq, steal;
    if (sscanf(line, "cpu %lu %lu %lu %lu %lu %lu %lu %lu",
               &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) < 4)
        return -1;

    guint64 total = user + nice + system + idle + iowait + irq + softirq + steal;
    guint64 idle_all = idle + iowait;

    int pct = -1;
    if (p->prev_total > 0) {
        guint64 dt = total - p->prev_total;
        guint64 di = idle_all - p->prev_idle;
        if (dt > 0)
            pct = (int)(100 * (dt - di) / dt);
    }

    p->prev_total = total;
    p->prev_idle = idle_all;
    return pct;
}

static void read_mem_info(int *used_pct, int *used_mb, int *total_mb) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) { *used_pct = -1; return; }

    long mem_total = 0, mem_available = 0;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "MemTotal:", 9) == 0)
            mem_total = atol(line + 9);
        else if (strncmp(line, "MemAvailable:", 13) == 0)
            mem_available = atol(line + 13);
    }
    fclose(f);

    if (mem_total > 0) {
        long used = mem_total - mem_available;
        *used_pct = (int)(100 * used / mem_total);
        *used_mb = (int)(used / 1024);
        *total_mb = (int)(mem_total / 1024);
    } else {
        *used_pct = -1;
    }
}

static int read_battery(void) {
    FILE *f = fopen("/sys/class/power_supply/BAT0/capacity", "r");
    if (!f) return -1;
    int cap = -1;
    if (fscanf(f, "%d", &cap) != 1) cap = -1;
    fclose(f);
    return cap;
}

static gboolean read_battery_charging(void) {
    FILE *f = fopen("/sys/class/power_supply/BAT0/status", "r");
    if (!f) return FALSE;
    char buf[32] = {0};
    if (fgets(buf, sizeof(buf), f)) { /* ok */ }
    fclose(f);
    return (strstr(buf, "Charging") != NULL || strstr(buf, "Full") != NULL);
}

/* ── Rendering ────────────────────────────────────────────────────────── */

static const char *STATUS_EXTRA_CSS =
    ".sys-bar { display: flex; align-items: center; gap: 6px; padding: 2px 0; }"
    ".bar-track { flex: 1; height: 4px; background: #111; border-radius: 2px; overflow: hidden; }"
    ".bar-fill { height: 100%; border-radius: 2px; transition: width 0.8s ease; }"
    ".bar-fill.teal { background: linear-gradient(90deg, #006a77, var(--teal)); }"
    ".bar-fill.amber { background: linear-gradient(90deg, #664400, var(--amber)); }"
    ".bar-fill.green { background: linear-gradient(90deg, #005500, #0f6); }"
    ".bar-fill.red { background: linear-gradient(90deg, #550000, var(--red)); }"
    ".stat-val-lg { color: var(--teal); font-family: var(--display); font-size: 14px; "
    "  font-weight: 900; letter-spacing: 0.05em; }"
    ".stat-unit { color: var(--teal-dim); font-family: var(--display); font-size: 8px; "
    "  letter-spacing: 0.1em; margin-left: 2px; }"
    ".token-row { display: flex; align-items: baseline; gap: 4px; padding: 3px 0; }"
    ".token-dir { font-family: var(--display); font-size: 7px; color: var(--text-dim); "
    "  letter-spacing: 0.1em; min-width: 20px; }"
    ".token-val { font-family: var(--display); font-size: 11px; color: var(--amber); }"
    ;

void panel_status_refresh(GtkWidget *widget) {
    PanelStatus *p = APP_PANEL_STATUS(widget);
    if (!p->webview) return;

    int cpu_pct = read_cpu_percent(p);
    int mem_pct, mem_used_mb, mem_total_mb;
    read_mem_info(&mem_pct, &mem_used_mb, &mem_total_mb);
    int battery = read_battery();
    gboolean charging = read_battery_charging();

    gboolean socket_ok = (p->app && p->app->compositor);
    gboolean agent_ok = (p->app && p->app->agent);

    guint   tools = p->app ? p->app->tool_count : 0;
    guint   msgs = p->app ? p->app->msg_count : 0;

    /* Battery color */
    const char *bat_bar_class = battery > 30 ? "green" : (battery > 10 ? "amber" : "red");
    const char *bat_icon = charging ? "&#x26A1;" : "";

    char *html = g_strdup_printf(
        "<html><head><style>%s %s</style></head><body>"
        "<div class='panel' style='height:100%%;display:flex;flex-direction:column;'>"
        "  <div class='header'><div class='header-fill teal-fill'></div>"
        "    <span class='header-text'>SYSTEM STATUS</span></div>"
        "  <div class='caution-bar'></div>"

        /* CPU */
        "  <div class='diag-section'>"
        "    <div class='stat-row'>"
        "      <span class='stat-label'>CPU</span>"
        "      <span class='stat-val'>%s</span>"
        "    </div>"
        "    <div class='sys-bar'>"
        "      <div class='bar-track'><div class='bar-fill teal' style='width:%d%%;'></div></div>"
        "    </div>"

        /* RAM */
        "    <div class='stat-row' style='margin-top:4px;'>"
        "      <span class='stat-label'>RAM</span>"
        "      <span class='stat-val'>%s</span>"
        "    </div>"
        "    <div class='sys-bar'>"
        "      <div class='bar-track'><div class='bar-fill teal' style='width:%d%%;'></div></div>"
        "    </div>"

        /* Battery */
        "    <div class='stat-row' style='margin-top:4px;'>"
        "      <span class='stat-label'>BAT %s</span>"
        "      <span class='stat-val'>%s</span>"
        "    </div>"
        "    <div class='sys-bar'>"
        "      <div class='bar-track'><div class='bar-fill %s' style='width:%d%%;'></div></div>"
        "    </div>"
        "  </div>"

        /* Agent & Connection */
        "  <div class='diag-section' style='border-top:1px solid rgba(0,229,255,0.06);'>"
        "    <div class='stat-row'>"
        "      <span class='stat-label'>MESSAGES</span>"
        "      <span class='stat-val'>%u</span>"
        "    </div>"
        "    <div class='stat-row'>"
        "      <span class='stat-label'>TOOL CALLS</span>"
        "      <span class='stat-val'>%u</span>"
        "    </div>"
        "    <div class='stat-row'>"
        "      <span class='stat-label'>IPC</span>"
        "      <span class='stat-val %s'>%s</span>"
        "    </div>"
        "    <div class='stat-row'>"
        "      <span class='stat-label'>AGENT</span>"
        "      <span class='stat-val %s'>%s</span>"
        "    </div>"
        "  </div>"

        "</div></body></html>",
        PANEL_CSS, STATUS_EXTRA_CSS,

        /* CPU */
        cpu_pct >= 0 ? g_strdup_printf("%d%%", cpu_pct) : g_strdup("--"),
        cpu_pct >= 0 ? cpu_pct : 0,

        /* RAM */
        mem_pct >= 0 ? g_strdup_printf("%d%% (%dM/%dM)", mem_pct, mem_used_mb, mem_total_mb)
                     : g_strdup("--"),
        mem_pct >= 0 ? mem_pct : 0,

        /* Battery */
        bat_icon,
        battery >= 0 ? g_strdup_printf("%d%%", battery) : g_strdup("N/A"),
        bat_bar_class,
        battery >= 0 ? battery : 0,

        /* Agent stats */
        msgs, tools,

        /* Connection */
        socket_ok ? "online" : "offline",
        socket_ok ? "BOUND" : "NONE",
        agent_ok ? "online" : "offline",
        agent_ok ? "CONNECTED" : "OFFLINE");

    webkit_web_view_load_html(WEBKIT_WEB_VIEW(p->webview), html, NULL);
    g_free(html);
}

static gboolean status_tick(gpointer data) {
    panel_status_refresh(GTK_WIDGET(data));
    return G_SOURCE_CONTINUE;
}

GtkWidget *panel_status_new(PlanarApp *app) {
    PanelStatus *p = g_object_new(APP_TYPE_PANEL_STATUS, NULL);
    p->app = app;

    app_panel_configure(&p->panel,
        GTK_LAYER_SHELL_LAYER_OVERLAY,
        FALSE, TRUE, TRUE, FALSE,
        0, 6, 6, 0, -1);

    p->webview = webkit_web_view_new();

    app_panel_set_content(&p->panel, p->webview);
    gtk_window_set_default_size(GTK_WINDOW(p), 260, 250);
    app_panel_show(&p->panel);

    /* Prime the CPU delta with a first reading */
    read_cpu_percent(p);

    panel_status_refresh(GTK_WIDGET(p));

    /* Refresh every 60 seconds */
    p->refresh_timer = g_timeout_add(60000, status_tick, p);

    return GTK_WIDGET(p);
}
