#include <libwebsockets.h>
#include <pthread.h>
#include <uuid/uuid.h>

#include "session_manager.h"

struct SessionMessage {
    unsigned char* message;
    size_t len;
    struct SessionMessage* next;
};

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

    struct SessionMessage* viewer_messages;

    struct SessionMessage* broadcaster_messages;

    struct BroadcastSession *next;
};


// some global variables
// for now we will lock the entire list when we need to access it
// in the future we can use a read-write lock
pthread_mutex_t broadcast_lock = PTHREAD_MUTEX_INITIALIZER;
struct Broadcast* broadcasts = NULL;

pthread_mutex_t broadcast_session_lock = PTHREAD_MUTEX_INITIALIZER;
struct BroadcastSession* broadcast_sessions = NULL;

// need to free later
char* gen_sesh_id() {
    uuid_t uuid;
    uuid_generate_random(uuid);
    char* session_id = malloc(37);
    uuid_unparse(uuid, session_id);
    return session_id;
}

// not thread safe
struct Broadcast* _unsafe_find_broadcast(const char* broadcast_id) {
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
    pthread_mutex_lock(&broadcast_lock);
    if (_unsafe_find_broadcast(broadcast_id)) {
        pthread_mutex_unlock(&broadcast_lock);
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
    pthread_mutex_unlock(&broadcast_lock);
    printf("registered\n");
    return broadcast->broadcast_id;
}

const char* new_session(struct lws* broadcaster, struct lws* viewer) {
    struct BroadcastSession* session = malloc(sizeof(struct BroadcastSession));
    // generate a unique session id
    session->session_id = gen_sesh_id();
    session->broadcaster = broadcaster;
    session->viewer = viewer;

    pthread_mutex_lock(&broadcast_session_lock);
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
    pthread_mutex_unlock(&broadcast_session_lock);
    return session -> session_id;
}

struct BroadcastSession* find_session(const char* session_id) {
    pthread_mutex_lock(&broadcast_session_lock);
    struct BroadcastSession* current_session = broadcast_sessions;
    while (current_session) {
        if (strcmp(current_session->session_id, session_id) == 0) {
            pthread_mutex_unlock(&broadcast_session_lock);
            return current_session;
        }
        current_session = current_session->next;
    }
    pthread_mutex_unlock(&broadcast_session_lock);
    return NULL;
}

void queue_message_for_session(const char* session_id, int is_broadcaster, unsigned char* message, size_t len) {
    struct BroadcastSession* session = find_session(session_id);
    if (!session) {
        printf("Session not found\n");
        return;
    }
    struct SessionMessage* session_message = malloc(sizeof(struct SessionMessage));
    session_message->message = malloc(len);
    memcpy(session_message->message, message, len);
    session_message->len = len;
    session_message->next = NULL;

    pthread_mutex_lock(&broadcast_session_lock);
    if (is_broadcaster) {
        if (!session->broadcaster_messages) {
            session->broadcaster_messages = session_message;
        } else {
            struct SessionMessage* current_message = session->broadcaster_messages;
            while (current_message->next) {
                current_message = current_message->next;
            }
            current_message->next = session_message;
        }
    } else {
        if (!session->viewer_messages) {
            session->viewer_messages = session_message;
        } else {
            struct SessionMessage* current_message = session->viewer_messages;
            while (current_message->next) {
                current_message = current_message->next;
            }
            current_message->next = session_message;
        }
    }
    pthread_mutex_unlock(&broadcast_session_lock);
}

struct SessionMessage* pop_message_for_session(const char* session_id, int is_broadcaster) {
    struct BroadcastSession* session = find_session(session_id);
    if (!session) {
        printf("Session not found\n");
        return NULL;
    }
    struct SessionMessage* session_message = NULL;
    pthread_mutex_lock(&broadcast_session_lock);
    if (is_broadcaster) {
        session_message = session->broadcaster_messages;
        session->broadcaster_messages = session->broadcaster_messages->next;
    } else {
        session_message = session->viewer_messages;
        session->viewer_messages = session->viewer_messages->next;
    }
    pthread_mutex_unlock(&broadcast_session_lock);
    return session_message;
}

// for a given session id and viewer, find the broadcaster socket
struct lws* find_broadcaster(const char* session_id) {
    return find_session(session_id)->broadcaster;
}

struct lws* find_viewer(const char* session_id) {
    return find_session(session_id)->viewer;
}

const char* join_broadcast(struct lws* viewer, const char* broadcast_id) {
    pthread_mutex_lock(&broadcast_lock);
    struct Broadcast* broadcast = _unsafe_find_broadcast(broadcast_id);
    pthread_mutex_unlock(&broadcast_lock);
    if (!broadcast) {
        printf("Broadcast not found\n");
        return NULL;
    }
    // create session 
    return new_session(broadcast->broadcaster, viewer);
}

void debug_print_all_broadcasts() {
    struct Broadcast* ptr = broadcasts;
    while (ptr) {
        printf("Broadcast %s\n", ptr->broadcast_id);
        ptr = ptr->next;
    }
}

void debug_print_all_sessions() {
    struct BroadcastSession* ptr = broadcast_sessions;
    while (ptr) {
        printf("Session %s\n", ptr->session_id);
        ptr = ptr->next;
    }
}

/**
 * frees the broadcast with given socket id, and all sessions with the given id
*/
int free_broadcast(struct lws* wsi) {
    printf("freeing broadcast\n");
    pthread_mutex_lock(&broadcast_lock);
    struct Broadcast* broadcast = NULL;
    // find the broadcast with the given id
    struct Broadcast* ptr = broadcasts;
    struct Broadcast* prev = NULL;
    while (ptr) {
        if (ptr -> broadcaster == wsi) {
            broadcast = ptr;
            break;
        }
        prev = ptr;
        ptr = ptr->next;
    }
    if (!broadcast) {
        printf("Broadcast not found\n");
        pthread_mutex_unlock(&broadcast_lock);
        return 0;
    }
    if (prev == NULL) {
        broadcasts = ptr->next;
    } else {
        prev->next = ptr->next;
    }
    pthread_mutex_unlock(&broadcast_lock);
    pthread_mutex_lock(&broadcast_session_lock);
    // then find all the sessions with the given broadcaster socket
    struct BroadcastSession* sess_prev = NULL;
    struct BroadcastSession* found_session = NULL;
    struct BroadcastSession* sess_ptr = broadcast_sessions;
    while (sess_ptr) {
        if (sess_ptr->broadcaster == wsi) {
            // free the session
            if (sess_prev == NULL) {
                broadcast_sessions = sess_ptr->next;
                sess_ptr = broadcast_sessions;
            } else {
                sess_prev->next = sess_ptr->next;
                sess_ptr = sess_prev->next;
            }
            found_session = sess_ptr;
            break;
        } else {
            sess_prev = sess_ptr;
            sess_ptr = sess_ptr->next;
        }
    }
    pthread_mutex_unlock(&broadcast_session_lock);
    if (found_session) {
        free(found_session->session_id);
        free(found_session);
    }
    free(broadcast->broadcast_id);
    free(broadcast);
    // debug print 
    pthread_mutex_lock(&broadcast_lock);
    debug_print_all_broadcasts();
    pthread_mutex_unlock(&broadcast_lock);
    pthread_mutex_lock(&broadcast_session_lock);
    debug_print_all_sessions();
    pthread_mutex_unlock(&broadcast_session_lock);
    return 0;
}

int free_session_with_viewer(struct lws* viewer) {
    pthread_mutex_lock(&broadcast_session_lock);
    struct BroadcastSession* session = NULL;
    // find the session with the given id
    struct BroadcastSession* ptr = broadcast_sessions;
    struct BroadcastSession* prev = NULL;
    while (ptr) {
        if (ptr -> viewer == viewer) {
            session = ptr;
            break;
        }
        prev = ptr;
        ptr = ptr->next;
    }
    if (!session) {
        printf("Session not found\n");
        pthread_mutex_unlock(&broadcast_session_lock);
        return 0;
    }
    if (prev == NULL) {
        broadcast_sessions = ptr->next;
    } else {
        prev->next = ptr->next;
    }
    pthread_mutex_unlock(&broadcast_session_lock);
    free(session->session_id);
    free(session);
    return 0;
}