#ifndef AGENT_IPC_H
#define AGENT_IPC_H

#include <glib.h>
#include <json-glib/json-glib.h>

typedef struct agent_ipc agent_ipc_t;

/* Callback for text-based messages (response, progress, system, error) */
typedef void (*agent_message_cb)(const char *role, const char *message, gpointer user_data);

/* Callback for structured JSON messages (status, todo, clarify, reasoning) */
typedef void (*agent_structured_cb)(const char *type, JsonObject *data, gpointer user_data);

agent_ipc_t *agent_ipc_new(void);
void agent_ipc_free(agent_ipc_t *ipc);
gboolean agent_ipc_connect(agent_ipc_t *ipc,
                           agent_message_cb callback,
                           agent_structured_cb structured_callback,
                           gpointer user_data);
void agent_ipc_send(agent_ipc_t *ipc, const char *message);
void agent_ipc_send_json(agent_ipc_t *ipc, const char *type, const char *key, const char *value);

#endif
