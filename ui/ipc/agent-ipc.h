#ifndef AGENT_IPC_H
#define AGENT_IPC_H

#include <glib.h>

typedef struct agent_ipc agent_ipc_t;

typedef void (*agent_message_cb)(const char *role, const char *message, gpointer user_data);

agent_ipc_t *agent_ipc_new(void);
void agent_ipc_free(agent_ipc_t *ipc);
gboolean agent_ipc_connect(agent_ipc_t *ipc, agent_message_cb callback, gpointer user_data);
void agent_ipc_send(agent_ipc_t *ipc, const char *message);

#endif
