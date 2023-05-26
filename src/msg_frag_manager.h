/**
 * manages message fragments for each socket
*/

// Path: signal-server/src/msg_frag_manager.c
#pragma once
#include <libwebsockets.h>

struct Message {
    unsigned char* message;
    size_t len;
};

unsigned char* get_message_fragment(struct lws* sock, size_t* len);

void create_or_append_fragment(struct lws* sock, unsigned char* frag, size_t len);

void free_fragments(struct lws* sock);