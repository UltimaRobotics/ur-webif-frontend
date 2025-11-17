
#include "direct_template.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>

/* Global client instance */
ur_rpc_client_t* g_global_client = NULL;
pthread_mutex_t g_global_client_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Static variables for reconnection control */
static bool g_reconnect_enabled = true;

/* Weak symbol implementation - can be overridden by user */
void handle_data(const char* topic, const char* payload, size_t payload_len) {
    printf("Default handle_data called for topic: %s\n", topic);
    printf("Payload: %.*s\n", (int)payload_len, payload);
    printf("Override this function to implement custom message handling\n");
}

/* Internal thread function */
static void* direct_client_thread_func(void* arg) {
    direct_client_thread_t* thread_ctx = (direct_client_thread_t*)arg;
    if (!thread_ctx) return NULL;

    direct_client_log_info("Client thread started for config: %s", thread_ctx->config_path);

    while (thread_ctx->running) {
        pthread_mutex_lock(&thread_ctx->mutex);

        // Initialize library if not done
        static bool lib_initialized = false;
        if (!lib_initialized) {
            if (ur_rpc_init() != UR_RPC_SUCCESS) {
                direct_client_log_error("Failed to initialize UR-RPC library");
                pthread_mutex_unlock(&thread_ctx->mutex);
                break;
            }
            lib_initialized = true;
            direct_client_log_info("UR-RPC library initialized");
        }

        // Load configuration
        if (!thread_ctx->config) {
            thread_ctx->config = ur_rpc_config_create();
            if (!thread_ctx->config) {
                direct_client_log_error("Failed to create configuration");
                pthread_mutex_unlock(&thread_ctx->mutex);
                break;
            }

            if (ur_rpc_config_load_from_file(thread_ctx->config, thread_ctx->config_path) != UR_RPC_SUCCESS) {
                direct_client_log_error("Failed to load configuration from: %s", thread_ctx->config_path);
                pthread_mutex_unlock(&thread_ctx->mutex);
                break;
            }
            direct_client_log_info("Configuration loaded successfully");
        }

        // Create topic configuration
        if (!thread_ctx->topic_config) {
            thread_ctx->topic_config = ur_rpc_topic_config_create();
            if (!thread_ctx->topic_config) {
                direct_client_log_error("Failed to create topic configuration");
                pthread_mutex_unlock(&thread_ctx->mutex);
                break;
            }

            ur_rpc_topic_config_set_prefixes(thread_ctx->topic_config, "ur_rpc", "client_service");
            ur_rpc_topic_config_set_suffixes(thread_ctx->topic_config, "request", "response", "notification");
        }

        // Create client
        if (!thread_ctx->client) {
            thread_ctx->client = ur_rpc_client_create(thread_ctx->config, thread_ctx->topic_config);
            if (!thread_ctx->client) {
                direct_client_log_error("Failed to create RPC client");
                pthread_mutex_unlock(&thread_ctx->mutex);
                break;
            }
            
            // Set message handler - use custom handler if provided, otherwise use default
            if (thread_ctx->custom_handler) {
                ur_rpc_client_set_message_handler(thread_ctx->client, (ur_rpc_message_handler_t)thread_ctx->custom_handler, thread_ctx->custom_handler_user_data);
                direct_client_log_info("RPC client created with custom message handler");
            } else {
                ur_rpc_client_set_message_handler(thread_ctx->client, direct_default_message_handler, thread_ctx);
                direct_client_log_info("RPC client created with default message handler");
            }

            // Set as global client
            pthread_mutex_lock(&g_global_client_mutex);
            g_global_client = thread_ctx->client;
            pthread_mutex_unlock(&g_global_client_mutex);
        }

        pthread_mutex_unlock(&thread_ctx->mutex);

        // Connect to broker
        pthread_mutex_lock(&thread_ctx->mutex);
        bool is_connected = thread_ctx->connected;
        pthread_mutex_unlock(&thread_ctx->mutex);
        
        if (!is_connected) {
            direct_client_log_info("Connecting to MQTT broker...");
            
            if (ur_rpc_client_connect(thread_ctx->client) == UR_RPC_SUCCESS) {
                if (ur_rpc_client_start(thread_ctx->client) == UR_RPC_SUCCESS) {
                    // Wait for connection
                    int connection_attempts = 0;
                    while (connection_attempts < 20 && thread_ctx->running) {
                        if (ur_rpc_client_is_connected(thread_ctx->client)) {
                            pthread_mutex_lock(&thread_ctx->mutex);
                            thread_ctx->connected = true;
                            pthread_cond_broadcast(&thread_ctx->connection_cv);
                            pthread_mutex_unlock(&thread_ctx->mutex);
                            
                            direct_client_log_info("Successfully connected to broker");
                            
                            // Load and subscribe to topics
                            direct_client_load_and_subscribe_topics(thread_ctx);
                            
                            // Start heartbeat
                            direct_client_start_heartbeat(thread_ctx);
                            
                            break;
                        }
                        sleep(1);
                        connection_attempts++;
                    }
                    
                    pthread_mutex_lock(&thread_ctx->mutex);
                    bool connected_now = thread_ctx->connected;
                    pthread_mutex_unlock(&thread_ctx->mutex);
                    
                    if (!connected_now) {
                        direct_client_log_error("Failed to connect after %d attempts", connection_attempts);
                    }
                }
            }
        }

        // Main operation loop
        pthread_mutex_lock(&thread_ctx->mutex);
        is_connected = thread_ctx->connected;
        pthread_mutex_unlock(&thread_ctx->mutex);
        
        if (is_connected && ur_rpc_client_is_connected(thread_ctx->client)) {
            // Client is connected and running
            sleep(5); // Check every 5 seconds
        } else {
            // Connection lost, try to reconnect
            if (g_reconnect_enabled && thread_ctx->reconnect_attempts < thread_ctx->max_reconnect_attempts) {
                pthread_mutex_lock(&thread_ctx->mutex);
                thread_ctx->connected = false;
                pthread_cond_broadcast(&thread_ctx->connection_cv);
                pthread_mutex_unlock(&thread_ctx->mutex);
                
                thread_ctx->reconnect_attempts++;
                
                direct_client_log_info("Connection lost. Reconnect attempt %d/%d", 
                                     thread_ctx->reconnect_attempts, thread_ctx->max_reconnect_attempts);
                
                // Cleanup current client
                if (thread_ctx->client) {
                    ur_rpc_client_stop(thread_ctx->client);
                    ur_rpc_client_disconnect(thread_ctx->client);
                    ur_rpc_client_destroy(thread_ctx->client);
                    thread_ctx->client = NULL;
                    
                    pthread_mutex_lock(&g_global_client_mutex);
                    g_global_client = NULL;
                    pthread_mutex_unlock(&g_global_client_mutex);
                }
                
                // Wait before reconnect
                sleep(thread_ctx->reconnect_delay_ms / 1000);
            } else {
                direct_client_log_error("Max reconnection attempts reached or reconnection disabled");
                break;
            }
        }
    }

    // Cleanup
    pthread_mutex_lock(&thread_ctx->mutex);
    if (thread_ctx->client) {
        direct_client_stop_heartbeat(thread_ctx);
        ur_rpc_client_stop(thread_ctx->client);
        ur_rpc_client_disconnect(thread_ctx->client);
    }
    thread_ctx->connected = false;
    pthread_mutex_unlock(&thread_ctx->mutex);

    direct_client_log_info("Client thread terminated");
    return NULL;
}

/* Global client management functions */
int direct_client_init_global(const char* config_path) {
    pthread_mutex_lock(&g_global_client_mutex);
    
    if (g_global_client) {
        pthread_mutex_unlock(&g_global_client_mutex);
        return UR_RPC_SUCCESS; // Already initialized
    }
    
    // Initialize library
    if (ur_rpc_init() != UR_RPC_SUCCESS) {
        pthread_mutex_unlock(&g_global_client_mutex);
        return UR_RPC_ERROR_CONFIG;
    }
    
    // Create and load configuration
    ur_rpc_client_config_t* config = ur_rpc_config_create();
    if (!config) {
        pthread_mutex_unlock(&g_global_client_mutex);
        return UR_RPC_ERROR_MEMORY;
    }
    
    if (ur_rpc_config_load_from_file(config, config_path) != UR_RPC_SUCCESS) {
        ur_rpc_config_destroy(config);
        pthread_mutex_unlock(&g_global_client_mutex);
        return UR_RPC_ERROR_CONFIG;
    }
    
    // Create topic configuration
    ur_rpc_topic_config_t* topic_config = ur_rpc_topic_config_create();
    if (!topic_config) {
        ur_rpc_config_destroy(config);
        pthread_mutex_unlock(&g_global_client_mutex);
        return UR_RPC_ERROR_MEMORY;
    }
    
    ur_rpc_topic_config_set_prefixes(topic_config, "ur_rpc", "client_service");
    ur_rpc_topic_config_set_suffixes(topic_config, "request", "response", "notification");
    
    // Create client
    g_global_client = ur_rpc_client_create(config, topic_config);
    if (!g_global_client) {
        ur_rpc_config_destroy(config);
        ur_rpc_topic_config_destroy(topic_config);
        pthread_mutex_unlock(&g_global_client_mutex);
        return UR_RPC_ERROR_MEMORY;
    }
    
    // Set message handler
    ur_rpc_client_set_message_handler(g_global_client, direct_default_message_handler, NULL);
    
    // Connect and start
    int result = ur_rpc_client_connect(g_global_client);
    if (result == UR_RPC_SUCCESS) {
        result = ur_rpc_client_start(g_global_client);
    }
    
    pthread_mutex_unlock(&g_global_client_mutex);
    return result;
}

void direct_client_cleanup_global(void) {
    pthread_mutex_lock(&g_global_client_mutex);
    
    if (g_global_client) {
        ur_rpc_client_stop(g_global_client);
        ur_rpc_client_disconnect(g_global_client);
        ur_rpc_client_destroy(g_global_client);
        g_global_client = NULL;
    }
    
    pthread_mutex_unlock(&g_global_client_mutex);
    ur_rpc_cleanup();
}

ur_rpc_client_t* direct_client_get_global(void) {
    pthread_mutex_lock(&g_global_client_mutex);
    ur_rpc_client_t* client = g_global_client;
    pthread_mutex_unlock(&g_global_client_mutex);
    return client;
}

/* Thread management functions */
direct_client_thread_t* direct_client_thread_create(const char* config_path) {
    if (!config_path) return NULL;
    
    direct_client_thread_t* thread_ctx = calloc(1, sizeof(direct_client_thread_t));
    if (!thread_ctx) return NULL;
    
    thread_ctx->config_path = strdup(config_path);
    if (!thread_ctx->config_path) {
        free(thread_ctx);
        return NULL;
    }
    
    thread_ctx->running = false;
    thread_ctx->connected = false;
    thread_ctx->client = NULL;
    thread_ctx->config = NULL;
    thread_ctx->topic_config = NULL;
    thread_ctx->reconnect_attempts = 0;
    thread_ctx->max_reconnect_attempts = 5;
    thread_ctx->reconnect_delay_ms = 5000;
    thread_ctx->custom_handler = NULL;
    thread_ctx->custom_handler_user_data = NULL;
    
    if (pthread_mutex_init(&thread_ctx->mutex, NULL) != 0) {
        free(thread_ctx->config_path);
        free(thread_ctx);
        return NULL;
    }
    
    if (pthread_cond_init(&thread_ctx->connection_cv, NULL) != 0) {
        pthread_mutex_destroy(&thread_ctx->mutex);
        free(thread_ctx->config_path);
        free(thread_ctx);
        return NULL;
    }
    
    return thread_ctx;
}

void direct_client_thread_destroy(direct_client_thread_t* thread_ctx) {
    if (!thread_ctx) return;
    
    // Stop thread if running
    if (thread_ctx->running) {
        direct_client_thread_stop(thread_ctx);
    }
    
    pthread_mutex_lock(&thread_ctx->mutex);
    
    // Cleanup resources
    if (thread_ctx->client) {
        ur_rpc_client_destroy(thread_ctx->client);
    }
    if (thread_ctx->config) {
        ur_rpc_config_destroy(thread_ctx->config);
    }
    if (thread_ctx->topic_config) {
        ur_rpc_topic_config_destroy(thread_ctx->topic_config);
    }
    
    free(thread_ctx->config_path);
    
    pthread_mutex_unlock(&thread_ctx->mutex);
    pthread_cond_destroy(&thread_ctx->connection_cv);
    pthread_mutex_destroy(&thread_ctx->mutex);
    
    free(thread_ctx);
}

int direct_client_thread_start(direct_client_thread_t* thread_ctx) {
    if (!thread_ctx || thread_ctx->running) return UR_RPC_ERROR_INVALID_PARAM;
    
    thread_ctx->running = true;
    thread_ctx->reconnect_attempts = 0;
    
    if (pthread_create(&thread_ctx->thread_id, NULL, direct_client_thread_func, thread_ctx) != 0) {
        thread_ctx->running = false;
        return UR_RPC_ERROR_THREAD;
    }
    
    return UR_RPC_SUCCESS;
}

int direct_client_thread_stop(direct_client_thread_t* thread_ctx) {
    if (!thread_ctx || !thread_ctx->running) return UR_RPC_ERROR_INVALID_PARAM;
    
    thread_ctx->running = false;
    
    // Wait for thread to finish
    pthread_join(thread_ctx->thread_id, NULL);
    
    return UR_RPC_SUCCESS;
}

bool direct_client_thread_is_running(const direct_client_thread_t* thread_ctx) {
    return thread_ctx ? thread_ctx->running : false;
}

bool direct_client_thread_is_connected(const direct_client_thread_t* thread_ctx) {
    return thread_ctx ? thread_ctx->connected : false;
}

bool direct_client_thread_wait_for_connection(direct_client_thread_t* thread_ctx, int timeout_ms) {
    if (!thread_ctx) return false;
    
    pthread_mutex_lock(&thread_ctx->mutex);
    
    if (thread_ctx->connected) {
        pthread_mutex_unlock(&thread_ctx->mutex);
        return true;
    }
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    
    while (!thread_ctx->connected && thread_ctx->running) {
        int result = pthread_cond_timedwait(&thread_ctx->connection_cv, &thread_ctx->mutex, &ts);
        if (result == ETIMEDOUT) {
            break;
        } else if (result != 0 && result != EINTR) {
            break;
        }
    }
    
    bool connected = thread_ctx->connected;
    pthread_mutex_unlock(&thread_ctx->mutex);
    
    return connected;
}

/* Reconnection mechanism */
int direct_client_set_reconnect_params(direct_client_thread_t* thread_ctx, int max_attempts, int delay_ms) {
    if (!thread_ctx) return UR_RPC_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock(&thread_ctx->mutex);
    thread_ctx->max_reconnect_attempts = max_attempts;
    thread_ctx->reconnect_delay_ms = delay_ms;
    pthread_mutex_unlock(&thread_ctx->mutex);
    
    return UR_RPC_SUCCESS;
}

int direct_client_trigger_reconnect(direct_client_thread_t* thread_ctx) {
    if (!thread_ctx) return UR_RPC_ERROR_INVALID_PARAM;
    
    pthread_mutex_lock(&thread_ctx->mutex);
    thread_ctx->connected = false;
    thread_ctx->reconnect_attempts = 0; // Reset attempt counter
    pthread_mutex_unlock(&thread_ctx->mutex);
    
    return UR_RPC_SUCCESS;
}

/* Message handling */
void direct_client_set_message_handler(direct_client_thread_t* thread_ctx, direct_message_handler_t handler, void* user_data) {
    if (!thread_ctx) return;
    
    // Store the custom handler in the thread context
    // This allows setting the handler before the client is created
    pthread_mutex_lock(&thread_ctx->mutex);
    thread_ctx->custom_handler = handler;
    thread_ctx->custom_handler_user_data = user_data;
    
    // If client already exists, set the handler immediately
    if (thread_ctx->client) {
        ur_rpc_client_set_message_handler(thread_ctx->client, (ur_rpc_message_handler_t)handler, user_data);
        direct_client_log_info("Message handler updated on existing client");
    }
    pthread_mutex_unlock(&thread_ctx->mutex);
}

void direct_default_message_handler(const char* topic, const char* payload, size_t payload_len, void* user_data) {
    // Call the weak symbol function that can be overridden
    handle_data(topic, payload, payload_len);
}

/* Async data sending functions */
int direct_client_send_async_rpc(const char* method, const char* service, const cJSON* params, ur_rpc_authority_t authority) {
    ur_rpc_client_t* client = direct_client_get_global();
    if (!client || !ur_rpc_client_is_connected(client)) {
        return UR_RPC_ERROR_NOT_CONNECTED;
    }
    
    ur_rpc_request_t* request = ur_rpc_request_create();
    if (!request) return UR_RPC_ERROR_MEMORY;
    
    int result = ur_rpc_request_set_method(request, method, service);
    if (result == UR_RPC_SUCCESS) {
        result = ur_rpc_request_set_authority(request, authority);
    }
    if (result == UR_RPC_SUCCESS && params) {
        result = ur_rpc_request_set_params(request, params);
    }
    
    if (result == UR_RPC_SUCCESS) {
        result = ur_rpc_call_async(client, request, NULL, NULL);
    }
    
    ur_rpc_request_destroy(request);
    return result;
}

int direct_client_send_async_rpc_with_callback(const char* method, const char* service, const cJSON* params, 
                                              ur_rpc_authority_t authority, ur_rpc_response_handler_t callback, void* user_data) {
    ur_rpc_client_t* client = direct_client_get_global();
    if (!client || !ur_rpc_client_is_connected(client)) {
        return UR_RPC_ERROR_NOT_CONNECTED;
    }
    
    ur_rpc_request_t* request = ur_rpc_request_create();
    if (!request) return UR_RPC_ERROR_MEMORY;
    
    int result = ur_rpc_request_set_method(request, method, service);
    if (result == UR_RPC_SUCCESS) {
        result = ur_rpc_request_set_authority(request, authority);
    }
    if (result == UR_RPC_SUCCESS && params) {
        result = ur_rpc_request_set_params(request, params);
    }
    
    if (result == UR_RPC_SUCCESS) {
        result = ur_rpc_call_async(client, request, callback, user_data);
    }
    
    ur_rpc_request_destroy(request);
    return result;
}

int direct_client_send_notification(const char* method, const char* service, const cJSON* params, ur_rpc_authority_t authority) {
    ur_rpc_client_t* client = direct_client_get_global();
    if (!client || !ur_rpc_client_is_connected(client)) {
        return UR_RPC_ERROR_NOT_CONNECTED;
    }
    
    return ur_rpc_send_notification(client, method, service, authority, params);
}

int direct_client_publish_raw_message(const char* topic, const char* payload, size_t payload_len) {
    ur_rpc_client_t* client = direct_client_get_global();
    if (!client || !ur_rpc_client_is_connected(client)) {
        return UR_RPC_ERROR_NOT_CONNECTED;
    }
    
    return ur_rpc_publish_message(client, topic, payload, payload_len);
}

/* Topic subscription management */
int direct_client_load_and_subscribe_topics(direct_client_thread_t* thread_ctx) {
    if (!thread_ctx || !thread_ctx->client || !thread_ctx->config) {
        return UR_RPC_ERROR_INVALID_PARAM;
    }
    
    direct_client_log_info("Loading topics from configuration...");
    
    const ur_rpc_topic_list_t* sub_topics = &thread_ctx->config->json_added_subs;
    
    if (sub_topics->topics && sub_topics->count > 0) {
        direct_client_log_info("Found %d subscription topics in configuration", sub_topics->count);
        
        for (int i = 0; i < sub_topics->count; i++) {
            if (sub_topics->topics[i]) {
                int result = ur_rpc_subscribe_topic(thread_ctx->client, sub_topics->topics[i]);
                if (result == UR_RPC_SUCCESS) {
                    direct_client_log_info("Subscribed to: %s", sub_topics->topics[i]);
                } else {
                    direct_client_log_error("Failed to subscribe to %s: %s", 
                                          sub_topics->topics[i], ur_rpc_error_string(result));
                }
            }
        }
    } else {
        direct_client_log_info("No subscription topics found in configuration");
    }
    
    direct_client_log_info("Topic subscription completed");
    return UR_RPC_SUCCESS;
}

int direct_client_subscribe_topic(const char* topic) {
    ur_rpc_client_t* client = direct_client_get_global();
    if (!client || !ur_rpc_client_is_connected(client)) {
        return UR_RPC_ERROR_NOT_CONNECTED;
    }
    
    return ur_rpc_subscribe_topic(client, topic);
}

int direct_client_unsubscribe_topic(const char* topic) {
    ur_rpc_client_t* client = direct_client_get_global();
    if (!client || !ur_rpc_client_is_connected(client)) {
        return UR_RPC_ERROR_NOT_CONNECTED;
    }
    
    return ur_rpc_unsubscribe_topic(client, topic);
}

/* Heartbeat management */
int direct_client_start_heartbeat(direct_client_thread_t* thread_ctx) {
    if (!thread_ctx || !thread_ctx->client) {
        return UR_RPC_ERROR_INVALID_PARAM;
    }
    
    int result = ur_rpc_heartbeat_start(thread_ctx->client);
    if (result == UR_RPC_SUCCESS) {
        direct_client_log_info("Heartbeat started successfully");
    } else {
        direct_client_log_error("Failed to start heartbeat: %s", ur_rpc_error_string(result));
    }
    
    return result;
}

int direct_client_stop_heartbeat(direct_client_thread_t* thread_ctx) {
    if (!thread_ctx || !thread_ctx->client) {
        return UR_RPC_ERROR_INVALID_PARAM;
    }
    
    int result = ur_rpc_heartbeat_stop(thread_ctx->client);
    if (result == UR_RPC_SUCCESS) {
        direct_client_log_info("Heartbeat stopped");
    }
    
    return result;
}

/* Statistics and monitoring */
int direct_client_get_statistics(direct_client_statistics_t* stats) {
    if (!stats) return UR_RPC_ERROR_INVALID_PARAM;
    
    ur_rpc_client_t* client = direct_client_get_global();
    if (!client) {
        memset(stats, 0, sizeof(direct_client_statistics_t));
        return UR_RPC_ERROR_NOT_CONNECTED;
    }
    
    ur_rpc_statistics_t ur_stats;
    int result = ur_rpc_client_get_statistics(client, &ur_stats);
    if (result == UR_RPC_SUCCESS) {
        stats->messages_sent = ur_stats.messages_sent;
        stats->messages_received = ur_stats.messages_received;
        stats->requests_sent = ur_stats.requests_sent;
        stats->responses_received = ur_stats.responses_received;
        stats->errors_count = ur_stats.errors_count;
        stats->uptime_seconds = ur_stats.uptime_seconds;
        stats->last_activity = ur_stats.last_activity;
        stats->is_connected = ur_rpc_client_is_connected(client);
    }
    
    return result;
}

void direct_client_print_statistics(const direct_client_statistics_t* stats) {
    if (!stats) return;
    
    printf("=== Client Statistics ===\n");
    printf("Messages sent: %lu\n", stats->messages_sent);
    printf("Messages received: %lu\n", stats->messages_received);
    printf("Requests sent: %lu\n", stats->requests_sent);
    printf("Responses received: %lu\n", stats->responses_received);
    printf("Errors: %lu\n", stats->errors_count);
    printf("Uptime: %lu seconds\n", stats->uptime_seconds);
    printf("Connected: %s\n", stats->is_connected ? "Yes" : "No");
    printf("Last activity: %s", ctime(&stats->last_activity));
    printf("========================\n");
}

/* Utility functions */
const char* direct_client_get_status_string(ur_rpc_connection_status_t status) {
    return ur_rpc_connection_status_to_string(status);
}

void direct_client_log_info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    printf("[DIRECT_CLIENT_INFO] ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

void direct_client_log_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    printf("[DIRECT_CLIENT_ERROR] ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

