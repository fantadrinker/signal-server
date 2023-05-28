#include <libwebsockets.h>
#include "msg_frag_manager.h"

struct MessageFragment {
    struct lws* socket;
    unsigned char* message;
    size_t len;
    struct MessageFragment* next;
};

struct MessageFragment* fragments = NULL;

unsigned char* get_message_fragment(struct lws* sock, size_t* len) {
    struct MessageFragment* ptr = fragments;
    while (ptr) {
        if (ptr->socket == sock) {
            *len = ptr->len;
            return ptr -> message;
        }
        ptr = ptr->next;
    }
    return NULL;
}

void create_or_append_fragment(struct lws* sock, unsigned char* frag, size_t len) {
    struct MessageFragment* ptr = fragments;
    while (ptr) {
        if (ptr->socket == sock) {
            // append to the message
            ptr->message = realloc(ptr->message, ptr->len + len);
            memcpy(ptr->message + ptr->len, frag, len);
            ptr->len += len;
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
}

void debug_print_all_fragments() {
    struct MessageFragment* ptr = fragments;
    while (ptr) {
        printf("debug_print_all_fragments: socket: %p, message: %s, len: %zu\n", ptr->socket, ptr->message, ptr->len);
        ptr = ptr->next;
    }
}

void free_fragments(struct lws* sock) {
    struct MessageFragment* ptr = fragments;
    struct MessageFragment* prev = NULL;

    debug_print_all_fragments();

    while (ptr) {
        if (ptr->socket == sock) {
            if (prev) {
                prev->next = ptr->next;
            } else {
                fragments = ptr->next;
            }
            free(ptr->message);
            free(ptr);
            printf("free_fragments: freed fragments\n");
            return;
        }
        prev = ptr;
        ptr = ptr->next;
    }
    printf("free_fragments: socket not found\n");
}