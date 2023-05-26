#pragma once
#include <libwebsockets.h>

enum MessageType {
    BROADCASTER_INIT,
    BROADCASTER_MESSAGE,
    VIEWER_JOIN,
    VIEWER_MESSAGE,
};

const char* new_broadcast(const char* broadcast_id, struct lws* broadcaster);

const char* new_session(struct lws* broadcaster, struct lws* viewer);

int free_broadcast(const char *broadcast_id);

int free_session(char* session_id);

struct lws* find_broadcaster(const char* session_id, const struct lws* viewer);

void broadcast_message(struct lws* broadcaster, char* message);

void send_message(const char* session_id, unsigned char* message, size_t len, int fromBroadcaster);

const char* join_broadcast(struct lws* viewer, const char* broadcast_id);