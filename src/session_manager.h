#pragma once
#include <libwebsockets.h>

enum MessageType {
    BROADCASTER_INIT,
    BROADCASTER_MESSAGE,
    VIEWER_JOIN,
    VIEWER_MESSAGE,
    PONG,
};

const char* new_broadcast(const char* broadcast_id, struct lws* broadcaster);

const char* new_session(struct lws* broadcaster, struct lws* viewer);

int free_broadcast(struct lws* broadcaster);

int free_session_with_viewer(struct lws* viewer);

struct lws* find_broadcaster(const char* session_id);

struct lws* find_viewer(const char* session_id);

const char* join_broadcast(struct lws* viewer, const char* broadcast_id);
