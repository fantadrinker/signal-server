#include <libwebsockets.h>
#include <pthread.h>
#include "msg_frag_manager.h"

struct MessageFragment {
    struct lws* socket;
    unsigned char* message;
    size_t len;
    struct MessageFragment* next;
};

struct MessageFragment* fragments = NULL;
pthread_mutex_t fragments_lock = PTHREAD_MUTEX_INITIALIZER;

unsigned char* get_message_fragment(struct lws* sock, size_t* len) {
    pthread_mutex_lock(&fragments_lock);
    struct MessageFragment* ptr = fragments;
    while (ptr) {
        if (ptr->socket == sock) {
            *len = ptr->len;
            pthread_mutex_unlock(&fragments_lock);
            return ptr -> message;
        }
        ptr = ptr->next;
    }
    pthread_mutex_unlock(&fragments_lock);
    return NULL;
}

void create_or_append_fragment(struct lws* sock, unsigned char* frag, size_t len) {
    pthread_mutex_lock(&fragments_lock);
    struct MessageFragment* ptr = fragments;
    while (ptr) {
        if (ptr->socket == sock) {
            // append to the message
            ptr->message = realloc(ptr->message, ptr->len + len);
            memcpy(ptr->message + ptr->len, frag, len);
            ptr->len += len;
            pthread_mutex_unlock(&fragments_lock);
            return;
        }
        ptr = ptr->next;
    }
    // create a new message
    struct MessageFragment* fragment = malloc(sizeof(struct MessageFragment));
    fragment->socket = sock;
    fragment->message = malloc(len);
    memcpy(fragment->message, frag, len);
    fragment->len = len;
    fragment->next = fragments;
    fragments = fragment;
    pthread_mutex_unlock(&fragments_lock);
}

void debug_print_all_fragments() {
    pthread_mutex_lock(&fragments_lock);
    struct MessageFragment* ptr = fragments;
    while (ptr) {
        printf("debug_print_all_fragments: socket: %p, message: %s, len: %zu\n", ptr->socket, ptr->message, ptr->len);
        ptr = ptr->next;
    }
    pthread_mutex_unlock(&fragments_lock);
}

void free_fragments(struct lws* sock) {
    debug_print_all_fragments();
    pthread_mutex_lock(&fragments_lock);
    struct MessageFragment* ptr = fragments;
    struct MessageFragment* prev = NULL;

    while (ptr) {
        if (ptr->socket == sock) {
            if (prev) {
                prev->next = ptr->next;
            } else {
                fragments = ptr->next;
            }
            free(ptr->message);
            free(ptr);
            pthread_mutex_unlock(&fragments_lock);
            printf("free_fragments: freed fragments\n");
            return;
        }
        prev = ptr;
        ptr = ptr->next;
    }
    pthread_mutex_unlock(&fragments_lock);
    printf("free_fragments: socket not found\n");
}