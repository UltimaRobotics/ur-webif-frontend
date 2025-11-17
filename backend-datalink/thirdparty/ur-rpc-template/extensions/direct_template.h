
#ifndef DIRECT_TEMPLATE_H
#define DIRECT_TEMPLATE_H

#include "ur-rpc-template.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Message handler callback function type - defined before struct that uses it */
typedef void (*direct_message_handler_t)(const char* topic, const char* payload, size_t payload_len, void* user_data);

/* Thread control structure */
typedef struct {
    pthread_t thread_id;
    bool running;
    bool connected;
    char* config_path;
    ur_rpc_client_t* client;
    ur_rpc_client_config_t* config;
    ur_rpc_topic_config_t* topic_config;
    pthread_mutex_t mutex;
    pthread_cond_t connection_cv;
    int reconnect_attempts;
    int max_reconnect_attempts;
    int reconnect_delay_ms;
    /* Custom message handler - set before thread starts */
    direct_message_handler_t custom_handler;
    void* custom_handler_user_data;
} direct_client_thread_t;

/* Global client instance for multi-place usage */
extern ur_rpc_client_t* g_global_client;
extern pthread_mutex_t g_global_client_mutex;

/* Weak symbol message handler - to be overridden by user */
void handle_data(const char* topic, const char* payload, size_t payload_len) __attribute__((weak));

/* Core client management functions */
int direct_client_init_global(const char* config_path);
void direct_client_cleanup_global(void);
ur_rpc_client_t* direct_client_get_global(void);

/* Thread management functions */
direct_client_thread_t* direct_client_thread_create(const char* config_path);
void direct_client_thread_destroy(direct_client_thread_t* thread_ctx);
int direct_client_thread_start(direct_client_thread_t* thread_ctx);
int direct_client_thread_stop(direct_client_thread_t* thread_ctx);
bool direct_client_thread_is_running(const direct_client_thread_t* thread_ctx);
bool direct_client_thread_is_connected(const direct_client_thread_t* thread_ctx);
bool direct_client_thread_wait_for_connection(direct_client_thread_t* thread_ctx, int timeout_ms);

/* Reconnection mechanism */
int direct_client_set_reconnect_params(direct_client_thread_t* thread_ctx, int max_attempts, int delay_ms);
int direct_client_trigger_reconnect(direct_client_thread_t* thread_ctx);

/* Message handling */
void direct_client_set_message_handler(direct_client_thread_t* thread_ctx, direct_message_handler_t handler, void* user_data);
void direct_default_message_handler(const char* topic, const char* payload, size_t payload_len, void* user_data);

/* Async data sending functions */
int direct_client_send_async_rpc(const char* method, const char* service, const cJSON* params, ur_rpc_authority_t authority);
int direct_client_send_async_rpc_with_callback(const char* method, const char* service, const cJSON* params, 
                                              ur_rpc_authority_t authority, ur_rpc_response_handler_t callback, void* user_data);
int direct_client_send_notification(const char* method, const char* service, const cJSON* params, ur_rpc_authority_t authority);
int direct_client_publish_raw_message(const char* topic, const char* payload, size_t payload_len);

/* Topic subscription management */
int direct_client_load_and_subscribe_topics(direct_client_thread_t* thread_ctx);
int direct_client_subscribe_topic(const char* topic);
int direct_client_unsubscribe_topic(const char* topic);

/* Heartbeat management */
int direct_client_start_heartbeat(direct_client_thread_t* thread_ctx);
int direct_client_stop_heartbeat(direct_client_thread_t* thread_ctx);

/* Statistics and monitoring */
typedef struct {
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t requests_sent;
    uint64_t responses_received;
    uint64_t errors_count;
    uint64_t uptime_seconds;
    time_t last_activity;
    bool is_connected;
} direct_client_statistics_t;

int direct_client_get_statistics(direct_client_statistics_t* stats);
void direct_client_print_statistics(const direct_client_statistics_t* stats);

/* Utility functions */
const char* direct_client_get_status_string(ur_rpc_connection_status_t status);
void direct_client_log_info(const char* format, ...);
void direct_client_log_error(const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif /* DIRECT_TEMPLATE_H */

