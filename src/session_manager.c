#include <libwebsockets.h>
#include <pthread.h>
#include <uuid/uuid.h>

#include "session_manager.h"

#define MAX_VIEWERS 10
/*
enum SessionMessageType {
    VIEWER_OFFER,
    BROADCASTER_ANSWER,
    NEW_ICE_CANDIDATE
};
struct SessionMessage {
    enum SessionMessageType type;
    char* text;
};

enum MessageType {
    BROADCASTER_INIT,
    BROADCASTER_MESSAGE,
    VIEWER_MESSAGE,
};
*/

// some global variables
struct Broadcast {
    // unique identifier for the session
    char* broadcast_id;
    // web socket between the server and the broadcaster
    struct lws* broadcaster;

    struct Broadcast *next;
};

struct BroadcastSession {
    // unique identifier for the session
    char* session_id;
    // web socket between the server and the broadcaster
    struct lws* broadcaster;
    // array of web sockets between the server and the viewers
    struct lws* viewer;

    struct BroadcastSession *next;
};

struct Broadcast* broadcasts;

struct BroadcastSession* broadcast_sessions;

// need to free later
char* gen_sesh_id() {
    uuid_t uuid;
    uuid_generate_random(uuid);
    char* session_id = malloc(37);
    uuid_unparse(uuid, session_id);
    return session_id;
}

void init_session_manager() {
    broadcasts = NULL;
    broadcast_sessions = NULL;
}

struct Broadcast* find_broadcast(const char* broadcast_id) {
    struct Broadcast* current_broadcast = broadcasts;
    while (current_broadcast) {
        if (strcmp(current_broadcast->broadcast_id, broadcast_id) == 0) {
            return current_broadcast;
        }
        current_broadcast = current_broadcast->next;
    }
    return NULL;
}

// called when a new client starts broadcasting
const char* new_broadcast(const char* broadcast_id, struct lws* broadcaster) {
    // first check if the broadcast already exists
    if (find_broadcast(broadcast_id)) {
        return NULL;
    }
    printf("creating new broadcast with id %s\n", broadcast_id);
    struct Broadcast* broadcast = malloc(sizeof(struct BroadcastSession));
    size_t len_broadcast_id = strlen(broadcast_id) + 1;
    broadcast->broadcast_id = malloc(len_broadcast_id);
    memcpy(broadcast->broadcast_id, broadcast_id, len_broadcast_id);
    broadcast->broadcaster = broadcaster;

    printf("registering new broadcast with id %s\n", broadcast_id);
    // add the broadcast to the list of broadcasts
    if (!broadcasts) {
        broadcasts = broadcast;
    } else {
        struct Broadcast* current_broadcast = broadcasts;
        while (current_broadcast->next) {
            current_broadcast = current_broadcast->next;
        }
        current_broadcast->next = broadcast;
    }
    printf("registered\n");
    return broadcast->broadcast_id;
}

const char* new_session(struct lws* broadcaster, struct lws* viewer) {
    struct BroadcastSession* session = malloc(sizeof(struct BroadcastSession));
    // generate a unique session id
    session->session_id = gen_sesh_id();
    session->broadcaster = broadcaster;
    session->viewer = viewer;

    // add the session to the list of sessions
    if (!broadcast_sessions) {
        broadcast_sessions = session;
    } else {
        struct BroadcastSession* current_session = broadcast_sessions;
        while (current_session->next) {
            current_session = current_session->next;
        }
        current_session->next = session;
    }

    return session -> session_id;
}

// for a given session id and viewer, find the broadcaster socket
struct lws* find_broadcaster(const char* session_id, const struct lws* viewer) {
    // find the broadcaster socket
    struct BroadcastSession* session = NULL;
    struct BroadcastSession* ptr = broadcast_sessions;
    while (ptr) {
        if (strcmp(ptr->session_id, session_id) == 0) {
            session = ptr;
            break;
        }
        ptr = ptr->next;
    }
    if (!session) {
        printf("Session not found\n");
        return NULL;
    }
    return session->broadcaster;
}

const char* join_broadcast(struct lws* viewer, const char* broadcast_id) {
    struct Broadcast* broadcast = find_broadcast(broadcast_id);
    if (!broadcast) {
        printf("Broadcast not found\n");
        return NULL;
    }
    // create session 
    return new_session(broadcast->broadcaster, viewer);
}

void send_message(const char* session_id, unsigned char* message, size_t len, int fromBroadcaster){
    // find the session with the given id
    struct BroadcastSession* session = NULL;
    struct BroadcastSession* ptr = broadcast_sessions;
    while (ptr) {
        if (strcmp(ptr->session_id, session_id) == 0) {
            session = ptr;
            break;
        }
        ptr = ptr->next;
    }
    if (!session) {
        printf("Session not found\n");
        return;
    }
    // send message to broadcaster
    if (fromBroadcaster) {
        lws_write(session->viewer, message, len, LWS_WRITE_TEXT);
    } else {
        lws_write(session->broadcaster, message, len, LWS_WRITE_TEXT);
    }
    return;
}

void broadcast_message(struct lws* broadcaster, char* message) {
    // find the session with the given id
    int sent = 0;
    struct BroadcastSession* session = NULL;
    struct BroadcastSession* ptr = broadcast_sessions;
    while (ptr) {
        if (broadcaster == ptr->broadcaster) {
            // send message to viewer in the session
            lws_write(ptr->viewer, (unsigned char*)message, strlen(message)+1, LWS_WRITE_TEXT);
            sent += 1;
        }
        ptr = ptr->next;
    }
    if (sent == 0) {
        printf("Session not found\n");
    } else {
        printf("Broadcasted message %s to %d viewers\n", message, sent);
    }
    return;
}

/**
 * frees the broadcast with given id, and all sessions with the given id
*/
int free_broadcast(const char *broadcast_id) {
    struct Broadcast* broadcast = NULL;
    // find the broadcast with the given id
    struct Broadcast* ptr = broadcasts;
    while (ptr) {
        if (strcmp(ptr->broadcast_id, broadcast_id) == 0) {
            broadcast = ptr;
            break;
        }
        ptr = ptr->next;
    }
    if (!broadcast) {
        printf("Broadcast not found\n");
        return 0;
    }
    struct lws* broadcaster = broadcast -> broadcaster;

    // then find all the sessions with the given broadcaster socket
    struct BroadcastSession* sess_prev = NULL;
    struct BroadcastSession* sess_ptr = broadcast_sessions;
    while (sess_ptr) {
        if (sess_ptr->broadcaster == broadcaster) {
            // free the session
            if (sess_prev == NULL) {
                broadcast_sessions = sess_ptr->next;
                free(sess_ptr->session_id);
                free(sess_ptr);
                sess_ptr = broadcast_sessions;
            } else {
                sess_prev->next = sess_ptr->next;
                free(sess_ptr->session_id);
                free(sess_ptr);
                sess_ptr = sess_prev->next;
            }
        } else {
            sess_prev = sess_ptr;
            sess_ptr = sess_ptr->next;
        }
    }
    free(broadcast->broadcast_id);
    free(broadcast);
    return 0;
}

int free_session_by_id(char* session_id) {
    struct BroadcastSession* session = NULL;
    // find the session with the given id
    struct BroadcastSession* ptr = broadcast_sessions;
    while (ptr) {
        if (strcmp(ptr->session_id, session_id) == 0) {
            session = ptr;
            break;
        }
        ptr = ptr->next;
    }
    free(session->session_id);
    free(session);
    return 0;
}
