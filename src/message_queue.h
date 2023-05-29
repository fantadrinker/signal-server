#pragma once
#include <libwebsockets.h>

void queue_message_for_sock(struct lws* sock, const char* message, size_t len);

unsigned char* get_message_for_sock(struct lws* sock, size_t* len);