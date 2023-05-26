#include <libwebsockets.h>
#include <json.h>

#include "session_manager.h"
#include "msg_frag_manager.h"
// some global variables

static int port = 8000;

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
            printf("creating new broadcast\n");
            const char* br_id = new_broadcast(broadcast_id, wsi);
            if (br_id == NULL) {
                size_t len = strlen("Error creating broadcast") + 1;
                message = malloc(len);
                memcpy(message, "Error creating broadcast", len);
            }
            printf("created new broadcast %s\n", br_id);
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
            lws_write(viewer, msg, len, LWS_WRITE_TEXT);
            break;
        default:
            message_len = strlen("Unknown message type: ") + 3;
            message = malloc(message_len);
            sprintf(message, "Unknown message type: %d\n", (int)type);
            break;
    }
    if (message) {
        unsigned char *buf = malloc(LWS_SEND_BUFFER_PRE_PADDING + message_len + LWS_SEND_BUFFER_POST_PADDING);
        memcpy(buf + LWS_SEND_BUFFER_PRE_PADDING, message, message_len);
        lws_write(wsi, buf + LWS_SEND_BUFFER_PRE_PADDING, message_len, LWS_WRITE_TEXT);
        free(message);
        free(buf);
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
                size_t len = strlen("Error joining broadcast") + 1;
                message = malloc(len);
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
            break;
        case VIEWER_MESSAGE:
            ; // empty statement to fix compiler error
            const char* session_id = json_object_get_string(
                json_object_object_get(jobj, "session_id")
            );
            struct lws* broadcaster = find_broadcaster(session_id);
            if (!broadcaster) {
                size_t len = strlen("Error finding broadcaster") + 1;
                message = malloc(len);
                memcpy(message, "Error finding broadcaster", len);
            }
            printf("found broadcaster %p\n", broadcaster);
            lws_write(broadcaster, msg, len, LWS_WRITE_TEXT);
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
        unsigned char *buf = malloc(LWS_SEND_BUFFER_PRE_PADDING + message_len + LWS_SEND_BUFFER_POST_PADDING);
        memcpy(buf + LWS_SEND_BUFFER_PRE_PADDING, message, message_len);
        lws_write(wsi, buf + LWS_SEND_BUFFER_PRE_PADDING, message_len, LWS_WRITE_TEXT);
        free(message);
        free(buf);
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
    int retval = 0;
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            printf("broadcast-protocol: Connection established\n");
            break;

        case LWS_CALLBACK_RECEIVE:
            ; // empty statement to fix compiler error
            size_t existing_msg_len = 0;
            unsigned char* msg = get_message_fragment(wsi, &existing_msg_len);
            const size_t remaining = lws_remaining_packet_payload(wsi);
            if (!remaining && lws_is_final_fragment(wsi)) {
                if (msg) {
                    msg = realloc(msg, len + existing_msg_len);
                    memcpy(msg + existing_msg_len, in, len);
                } else {
                    // no fragments, just process the message
                    broadcaster_message_handler(wsi, in, len);
                    break;
                }
                broadcaster_message_handler(wsi, msg, len + existing_msg_len);
                // we could free the message here, but it's going to be hard to manage
                free_fragments(wsi);
            } else
                create_or_append_fragment(wsi, in, len);
            break;
        case LWS_CALLBACK_CLOSED:
            printf("Connection closed\n");
            // TODO: find broadcast session and clean it up
            free_broadcast(wsi);
            break;
        default:
            break;
    }

    return retval;
}

static int callback_viewer(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    int retval = 0;
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            printf("viewer protocal: Connection established\n");
            break;

        case LWS_CALLBACK_RECEIVE:
            ; // empty statement to fix compiler error
            size_t existing_msg_len = 0;
            unsigned char* msg = get_message_fragment(wsi, &existing_msg_len);
            const size_t remaining = lws_remaining_packet_payload(wsi);
            if (!remaining && lws_is_final_fragment(wsi)) {
                if (msg) {
                    msg = realloc(msg, len + existing_msg_len);
                    memcpy(msg + existing_msg_len, in, len);
                } else {
                    // no fragments, just process the message
                    viewer_message_handler(wsi, in, len);
                    break;
                }
                viewer_message_handler(wsi, msg, len + existing_msg_len);
                // we could free the message here, but it's going to be hard to manage
                free_fragments(wsi);
            } else
                create_or_append_fragment(wsi, in, len);
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
    struct lws_context *context;
    struct lws_context_creation_info info;
    struct lws_protocols protocols[] = {
        { "echo-protocol", callback_echo, 0, 128 },
        { "broadcast-protocol", callback_broadcaster, 0, 512},
        { "viewer-protocol", callback_viewer, 0, 512},
        { NULL, NULL, 0, 0 } // Terminator
    };

    memset(&info, 0, sizeof(info));
    info.port = port;
    info.protocols = protocols;

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
