#include <glib.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "agent-ipc.h"

#define DEFAULT_WS_URL "ws://89.167.84.8:5050"
#define RECONNECT_INTERVAL_SEC 5

struct agent_ipc {
    SoupSession *session;
    SoupWebsocketConnection *ws;
    agent_message_cb callback;
    agent_structured_cb structured_callback;
    gpointer user_data;
    char *url;
    guint reconnect_source;
    gboolean connecting;
};

static void agent_ipc_attempt_connect(agent_ipc_t *ipc);

static void on_ws_message(SoupWebsocketConnection *ws, SoupWebsocketDataType type,
                          GBytes *message, gpointer data) {
    (void)ws;
    agent_ipc_t *ipc = data;

    if (type != SOUP_WEBSOCKET_DATA_TEXT) return;

    gsize len;
    const char *text = g_bytes_get_data(message, &len);
    if (!text || len == 0) return;

    JsonParser *parser = json_parser_new();
    if (!json_parser_load_from_data(parser, text, len, NULL)) {
        g_object_unref(parser);
        return;
    }

    JsonNode *root = json_parser_get_root(parser);
    if (!root || !JSON_NODE_HOLDS_OBJECT(root)) {
        g_object_unref(parser);
        return;
    }

    JsonObject *obj = json_node_get_object(root);
    const char *msg_type = json_object_get_string_member(obj, "type");
    if (!msg_type) {
        g_object_unref(parser);
        return;
    }

    /* Structured message types — dispatch to structured callback */
    if (strcmp(msg_type, "status") == 0 ||
        strcmp(msg_type, "todo") == 0 ||
        strcmp(msg_type, "clarify") == 0 ||
        strcmp(msg_type, "reasoning") == 0) {
        if (ipc->structured_callback) {
            ipc->structured_callback(msg_type, obj, ipc->user_data);
        }
        g_object_unref(parser);
        return;
    }

    /* Text-based message types — dispatch to message callback */
    const char *role = NULL;
    const char *content = NULL;

    if (strcmp(msg_type, "response") == 0) {
        role = "agent";
        content = json_object_get_string_member(obj, "text");
    } else if (strcmp(msg_type, "progress") == 0) {
        role = "tool";
        const char *tool_name = json_object_get_string_member(obj, "tool");
        const char *preview = json_object_get_string_member(obj, "preview");
        /* Build "tool_name: preview" into a static buffer for the callback */
        static char progress_buf[1024];
        if (tool_name && preview && preview[0]) {
            snprintf(progress_buf, sizeof(progress_buf), "%s: %s", tool_name, preview);
        } else if (tool_name) {
            snprintf(progress_buf, sizeof(progress_buf), "%s", tool_name);
        } else if (preview) {
            snprintf(progress_buf, sizeof(progress_buf), "%s", preview);
        } else {
            snprintf(progress_buf, sizeof(progress_buf), "working...");
        }
        content = progress_buf;

        /* Also forward structured progress data if available */
        if (ipc->structured_callback) {
            ipc->structured_callback(msg_type, obj, ipc->user_data);
        }
    } else if (strcmp(msg_type, "system") == 0) {
        role = "system";
        content = json_object_get_string_member(obj, "text");
    } else if (strcmp(msg_type, "error") == 0) {
        role = "system";
        content = json_object_get_string_member(obj, "text");
    }

    if (role && content && ipc->callback) {
        ipc->callback(role, content, ipc->user_data);
    }

    g_object_unref(parser);
}

static gboolean reconnect_timer(gpointer data) {
    agent_ipc_t *ipc = data;
    ipc->reconnect_source = 0;
    agent_ipc_attempt_connect(ipc);
    return G_SOURCE_REMOVE;
}

static void schedule_reconnect(agent_ipc_t *ipc) {
    if (ipc->reconnect_source > 0) return;
    g_printerr("agent-ipc: scheduling reconnect in %ds\n", RECONNECT_INTERVAL_SEC);
    ipc->reconnect_source = g_timeout_add_seconds(RECONNECT_INTERVAL_SEC, reconnect_timer, ipc);
}

static void on_ws_closed(SoupWebsocketConnection *ws, gpointer data) {
    (void)ws;
    agent_ipc_t *ipc = data;
    g_printerr("agent-ipc: WebSocket closed\n");

    if (ipc->callback) {
        ipc->callback("system", "Disconnected from agent", ipc->user_data);
    }

    g_clear_object(&ipc->ws);
    schedule_reconnect(ipc);
}

static void on_ws_error(SoupWebsocketConnection *ws, GError *error, gpointer data) {
    (void)ws;
    (void)data;
    g_printerr("agent-ipc: WebSocket error: %s\n", error->message);
}

static void on_websocket_connect(GObject *source, GAsyncResult *result, gpointer data) {
    (void)source;
    agent_ipc_t *ipc = data;
    GError *error = NULL;

    ipc->connecting = FALSE;
    ipc->ws = soup_session_websocket_connect_finish(ipc->session, result, &error);

    if (error) {
        g_printerr("agent-ipc: connect failed: %s\n", error->message);
        g_error_free(error);
        schedule_reconnect(ipc);
        return;
    }

    g_printerr("agent-ipc: connected to %s\n", ipc->url);

    g_signal_connect(ipc->ws, "message", G_CALLBACK(on_ws_message), ipc);
    g_signal_connect(ipc->ws, "closed", G_CALLBACK(on_ws_closed), ipc);
    g_signal_connect(ipc->ws, "error", G_CALLBACK(on_ws_error), ipc);

    if (ipc->callback) {
        ipc->callback("system", "Connected to Hermes gateway", ipc->user_data);
    }

    /* Request initial status from gateway */
    agent_ipc_send_json(ipc, "query", "what", "status");
}

static void agent_ipc_attempt_connect(agent_ipc_t *ipc) {
    if (ipc->connecting || ipc->ws) return;
    ipc->connecting = TRUE;

    SoupMessage *msg = soup_message_new(SOUP_METHOD_GET, ipc->url);
    if (!msg) {
        g_printerr("agent-ipc: invalid URL: %s\n", ipc->url);
        ipc->connecting = FALSE;
        return;
    }

    soup_session_websocket_connect_async(ipc->session, msg, NULL, NULL, G_PRIORITY_DEFAULT,
                                         NULL, on_websocket_connect, ipc);
    g_object_unref(msg);
}

agent_ipc_t *agent_ipc_new(void) {
    agent_ipc_t *ipc = g_new0(agent_ipc_t, 1);

    const char *env_url = getenv("HERMES_WS_URL");
    ipc->url = g_strdup(env_url ? env_url : DEFAULT_WS_URL);

    ipc->session = soup_session_new();

    return ipc;
}

void agent_ipc_free(agent_ipc_t *ipc) {
    if (!ipc) return;

    if (ipc->reconnect_source > 0) {
        g_source_remove(ipc->reconnect_source);
    }
    if (ipc->ws) {
        if (soup_websocket_connection_get_state(ipc->ws) == SOUP_WEBSOCKET_STATE_OPEN) {
            soup_websocket_connection_close(ipc->ws, SOUP_WEBSOCKET_CLOSE_NORMAL, NULL);
        }
        g_object_unref(ipc->ws);
    }
    g_clear_object(&ipc->session);
    g_free(ipc->url);
    g_free(ipc);
}

gboolean agent_ipc_connect(agent_ipc_t *ipc,
                           agent_message_cb callback,
                           agent_structured_cb structured_callback,
                           gpointer user_data) {
    ipc->callback = callback;
    ipc->structured_callback = structured_callback;
    ipc->user_data = user_data;
    agent_ipc_attempt_connect(ipc);
    return TRUE;
}

void agent_ipc_send(agent_ipc_t *ipc, const char *message) {
    if (!ipc || !ipc->ws) return;
    if (soup_websocket_connection_get_state(ipc->ws) != SOUP_WEBSOCKET_STATE_OPEN) return;

    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);

    if (message[0] == '/') {
        json_builder_set_member_name(builder, "type");
        json_builder_add_string_value(builder, "command");
        json_builder_set_member_name(builder, "cmd");
        json_builder_add_string_value(builder, message + 1);
    } else {
        json_builder_set_member_name(builder, "type");
        json_builder_add_string_value(builder, "message");
        json_builder_set_member_name(builder, "text");
        json_builder_add_string_value(builder, message);
    }

    json_builder_end_object(builder);

    JsonGenerator *gen = json_generator_new();
    JsonNode *root = json_builder_get_root(builder);
    json_generator_set_root(gen, root);

    gchar *json_str = json_generator_to_data(gen, NULL);

    soup_websocket_connection_send_text(ipc->ws, json_str);

    g_free(json_str);
    json_node_unref(root);
    g_object_unref(gen);
    g_object_unref(builder);
}

void agent_ipc_send_json(agent_ipc_t *ipc, const char *type, const char *key, const char *value) {
    if (!ipc || !ipc->ws) return;
    if (soup_websocket_connection_get_state(ipc->ws) != SOUP_WEBSOCKET_STATE_OPEN) return;

    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);

    json_builder_set_member_name(builder, "type");
    json_builder_add_string_value(builder, type);

    if (key && value) {
        json_builder_set_member_name(builder, key);
        json_builder_add_string_value(builder, value);
    }

    json_builder_end_object(builder);

    JsonGenerator *gen = json_generator_new();
    JsonNode *root = json_builder_get_root(builder);
    json_generator_set_root(gen, root);

    gchar *json_str = json_generator_to_data(gen, NULL);

    soup_websocket_connection_send_text(ipc->ws, json_str);

    g_free(json_str);
    json_node_unref(root);
    g_object_unref(gen);
    g_object_unref(builder);
}
