#include <libwebsockets.h>
#include <json.h>

#include "session_manager.h"
#include "msg_frag_manager.h"
#include "message_queue.h"
// some global variables
#define LOCAL_RESOURCE_PATH "/etc/ssl/certs"
char *resource_path = LOCAL_RESOURCE_PATH;

static int port = 443;

struct pss {
    int send_a_ping;
};

void lws_padded_write(struct lws *wsi, const char* message, size_t len) {
    unsigned char* buf = malloc(LWS_PRE + len);
    memcpy(buf + LWS_PRE, message, len);
    lws_write(wsi, buf + LWS_PRE, len, LWS_WRITE_TEXT);
    free(buf);
}

static int broadcaster_message_handler(struct lws *wsi, unsigned char *msg, size_t len) {
    printf("broadcast-protocol: Received data: %s\n", (char*)msg);
    printf("broadcast-protocol: Received data of length: %d\n", (int)len);

    struct json_object *jobj = json_tokener_parse((char*)msg);

    const char* broadcast_id = json_object_get_string(
        json_object_object_get(jobj, "broadcast_id")
    );
    enum MessageType type = json_object_get_int(
        json_object_object_get(jobj, "message_type")
    );
    printf("got message type %d\n", (int)type);
    char* message = NULL;
    size_t message_len = 0;
    
    switch (type) {
        case BROADCASTER_INIT:
            ; // empty statement to fix compiler error
            const char* br_id = new_broadcast(broadcast_id, wsi);
            if (br_id == NULL) {
                size_t len = strlen("Error creating broadcast") + 1;
                message = malloc(len);
                memcpy(message, "Error creating broadcast", len);
            }
            printf("created new broadcast %s with socket %p\n", br_id, wsi);
            struct json_object *response_obj = json_object_new_object();
            json_object_object_add(response_obj, "broadcast_id", json_object_new_string(br_id));
            json_object_object_add(response_obj, "message_type", json_object_new_string("broadcast_created")); // TODO: constant
            const char * response_str = json_object_to_json_string(response_obj);
            printf("response: %s\n", response_str);
            message_len = strlen(response_str);
            message = malloc(message_len);
            memcpy(message, response_str, message_len);
            json_object_put(response_obj);
            break;
        case BROADCASTER_MESSAGE:
            ; // empty statement to fix compiler error
            const char * session_id = json_object_get_string(
                json_object_object_get(jobj, "session_id")
            );
            printf("got session id %s, finding viewer\n", session_id);
            struct lws* viewer = find_viewer(session_id);
            if (!viewer) {
                printf("viewer is null\n");
                break;
            }
            printf("found viewer, sending message to viewer\n");
            queue_message_for_sock(viewer, msg, len);
            lws_callback_on_writable(viewer);
            break;
        default:
            message_len = strlen("Unknown message type: ") + 3;
            message = malloc(message_len);
            sprintf(message, "Unknown message type: %d\n", (int)type);
            break;
    }
    if (message) {
        lws_padded_write(wsi, message, message_len);
        free(message);
    }
    // clean up
    json_object_put(jobj); // frees object
    return 0;
}

static int viewer_message_handler(struct lws *wsi, unsigned char *msg, size_t len) {
    int retval = 0;
    printf("viewer protocal: Received data: %s\n", (char *)msg);
    printf("total length: %d\n", (int)len);
    // get the broadcast id, and extract information
    struct json_object *jobj = json_tokener_parse((char *)msg);

    const char* broadcast_id = json_object_get_string(
        json_object_object_get(jobj, "broadcast_id")
    );
    enum MessageType type = json_object_get_int(
        json_object_object_get(jobj, "message_type")
    );
    printf("got message type %d\n", (int)type);
    char* message = NULL;
    size_t message_len = 0;
    switch (type) {
        case VIEWER_JOIN:
            ; // empty statement to fix compiler error
            printf("joining broadcast\n");
            const char* sess_id = join_broadcast(wsi, broadcast_id);
            if (!sess_id) {
                message_len = strlen("Error joining broadcast") + 1;
                message = malloc(message_len);
                memcpy(message, "Error joining broadcast", len);
                retval = -1;
            }
            printf("created new session %s\n", sess_id);
            struct json_object *response_obj = json_object_new_object();
            json_object_object_add(response_obj, "message_type", json_object_new_string("session_created"));
            json_object_object_add(response_obj, "payload", json_object_new_string(sess_id));
            const char * response_str = json_object_to_json_string(response_obj);
            message_len = strlen(response_str);
            message = malloc(message_len);
            memcpy(message, response_str, message_len);
            json_object_put(response_obj);
            break;
        case VIEWER_MESSAGE:
            ; // empty statement to fix compiler error
            const char* session_id = json_object_get_string(
                json_object_object_get(jobj, "session_id")
            );
            struct lws* broadcaster = find_broadcaster(session_id);
            if (!broadcaster) {
                message_len = strlen("Error finding broadcaster") + 1;
                message = malloc(len);
                memcpy(message, "Error finding broadcaster", len);
            }
            printf("found broadcaster %p\n", broadcaster);
            queue_message_for_sock(broadcaster, msg, len);
            lws_callback_on_writable(broadcaster);
            printf("sent message to broadcaster\n");
            break;
        default:
            message_len = strlen("Unknown message type: ") + 3;
            message = malloc(message_len);
            sprintf(message, "Unknown message type: %d\n", (int)type);
            break;
    }
    printf("exiting event loop\n");
    if (message) {
        lws_padded_write(wsi, message, message_len);
        free(message);
    }
    json_object_put(jobj); // frees object
    if (!wsi) {
        printf("WebSocket connection is invalid\n");
        return -1;
    }
    return retval;
}

static int callback_echo(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            printf("Connection established\n");
            break;

        case LWS_CALLBACK_RECEIVE:
            printf("Received data: %s\n", (char *)in);

            // Echo the received data back to the client
            lws_write(wsi, in, len, LWS_WRITE_TEXT);
            break;

        default:
            break;
    }

    return 0;
}

static int callback_broadcaster(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    struct pss *pss = (struct pss *)user;
    int retval = 0;
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            printf("broadcast-protocol: Connection established\n");
            pss->send_a_ping = 1;
            lws_set_timer_usecs(wsi, 5 * LWS_USEC_PER_SEC);
            break;

        case LWS_CALLBACK_TIMER:
            if (pss->send_a_ping == 0) {
                printf("did not receive ping, is connection dead?\n");
            } else {
                unsigned char* buf = malloc(LWS_PRE + 4);
                memcpy(buf + LWS_PRE, "ping", 4);
                lws_write(wsi, buf + LWS_PRE, 4, LWS_WRITE_TEXT);
                free(buf);
                pss->send_a_ping = 0;
                lws_set_timer_usecs(wsi, 5 * LWS_USEC_PER_SEC);
            }
            break;
            
        case LWS_CALLBACK_RECEIVE:
            ; // empty statement to fix compiler error
            size_t existing_msg_len = 0;
            unsigned char* msg = get_message_fragment(wsi, &existing_msg_len);
            const size_t remaining = lws_remaining_packet_payload(wsi);
            if (!remaining && lws_is_final_fragment(wsi)) {
                if (!msg) {
                    // no fragments, just process the message
                    if (len == 4 && memcmp(in, "pong", len) == 0) {
                        lws_set_timer_usecs(wsi, 5 * LWS_USEC_PER_SEC);
                        pss -> send_a_ping = 1;
                        return 0;
                    }

                    if (len == 4 && memcmp(in, "ping", len) == 0) {
                        lws_padded_write(wsi, "pong", 4);
                        return 0;
                    }
                    broadcaster_message_handler(wsi, in, len);
                    break;
                }
                msg = realloc(msg, len + existing_msg_len);
                memcpy(msg + existing_msg_len, in, len);
                broadcaster_message_handler(wsi, msg, len + existing_msg_len);
                // we could free the message here, but it's going to be hard to manage
                printf("message processed, freeing fragments\n");
                free_fragments(wsi);
                printf("fragments freed\n");
            } else
                create_or_append_fragment(wsi, in, len);
            break;
        case LWS_CALLBACK_SERVER_WRITEABLE:
            printf("broadcaster protocol: server writeable\n");
            size_t msg_len = 0;
            unsigned char* bc_msg = get_message_for_sock(wsi, &msg_len);
            if (!bc_msg) {
                printf("no message to send\n");
                break;
            }
            printf("relaying message to broadcaster\n");
            lws_padded_write(wsi, bc_msg, msg_len);
            printf("message relayed\n");
            free(bc_msg);
            // recursive call until theres no message left
            lws_callback_on_writable(wsi);
            break;
        case LWS_CALLBACK_CLOSED:
            printf("Connection closed\n");
            free_broadcast(wsi);
            break;
        default:
            break;
    }

    return retval;
}

static int callback_viewer(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    struct pss *pss = (struct pss *)user;
    int retval = 0;
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            printf("viewer protocal: Connection established\n"); 
            pss->send_a_ping = 1;           
            lws_set_timer_usecs(wsi, 5 * LWS_USEC_PER_SEC);
            break;
        case LWS_CALLBACK_TIMER:
            if (pss->send_a_ping == 0) {
                printf("did not receive ping, is connection dead?\n");
            } else {
                unsigned char* buf = malloc(LWS_PRE + 4);
                memcpy(buf + LWS_PRE, "ping", 4);
                lws_write(wsi, buf + LWS_PRE, 4, LWS_WRITE_TEXT);
                free(buf);
                pss->send_a_ping = 0;
                lws_set_timer_usecs(wsi, 5 * LWS_USEC_PER_SEC);
            }
            break;
        case LWS_CALLBACK_RECEIVE:
            ; // empty statement to fix compiler error
            size_t existing_msg_len = 0;
            unsigned char* msg = get_message_fragment(wsi, &existing_msg_len);
            const size_t remaining = lws_remaining_packet_payload(wsi);
            if (!remaining && lws_is_final_fragment(wsi)) {
                if (!msg) {
                    // no fragments, just process the message
                    if (len == 4 && memcmp(in, "pong", len) == 0) {
                        lws_set_timer_usecs(wsi, 5 * LWS_USEC_PER_SEC);
                        pss -> send_a_ping = 1;
                        return 0;
                    }

                    if (len == 4 && memcmp(in, "ping", len) == 0) {
                        lws_padded_write(wsi, "pong", 4);
                        return 0;
                    }
                    viewer_message_handler(wsi, in, len);
                    break;
                }
                msg = realloc(msg, len + existing_msg_len);
                memcpy(msg + existing_msg_len, in, len);
                viewer_message_handler(wsi, msg, len + existing_msg_len);
                // we could free the message here, but it's going to be hard to manage
                free_fragments(wsi);
            } else
                create_or_append_fragment(wsi, in, len);
            break;
        case LWS_CALLBACK_SERVER_WRITEABLE:
            printf("viewer protocol: server writeable\n");
            size_t msg_len = 0;
            const char* vr_msg = get_message_for_sock(wsi, &msg_len);
            if (!vr_msg) {
                printf("no message to send\n");
                break;
            }
            printf("relaying message to viewer\n");
            lws_padded_write(wsi, vr_msg, msg_len);
            printf("message relayed\n");
            free(vr_msg);
            // recursive call until theres no message left
            lws_callback_on_writable(wsi);
            break;
        case LWS_CALLBACK_CLOSED:
            printf("viewer protocol: Connection closed\n");
            free_session_with_viewer(wsi);
        default:
            break;
    }
    return retval;
}

int main(void)
{
    uint64_t opts = 0;
    int use_ssl = 1;

    struct lws_context *context;
    struct lws_context_creation_info info;
    struct lws_protocols protocols[] = {
        { "echo-protocol", callback_echo, 0, 128 },
        { "broadcast-protocol", callback_broadcaster, sizeof(struct pss), 512},
        { "viewer-protocol", callback_viewer, sizeof(struct pss), 512},
        { NULL, NULL, 0, 0 } // Terminator
    };

    memset(&info, 0, sizeof(info));
    info.port = port;
    info.protocols = protocols; 
    if (use_ssl) {
        // set up ssl, it should work but apparently doesn't
        opts |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

        info.options = opts;
        info.ssl_cert_filepath = "/etc/ssl/certs/lws-sig-serv.crt";
        info.ssl_private_key_filepath = "/etc/ssl/certs/lws-sig-serv.key";
    }

    printf("Starting server on port %d\n, pid %d\n", port, getpid());

    // Create the WebSocket context
    context = lws_create_context(&info);
    if (context == NULL) {
        printf("Failed to create WebSocket context\n");
        return -1;
    }

    // Enter the event loop
    while (1) {
        lws_service(context, 1000);
    }

    lws_context_destroy(context);

    return 0;
}
