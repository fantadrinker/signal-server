#include <libwebsockets.h>
#include <pthread.h>

struct MessageQueue {
    struct lws* socket;
    unsigned char* message;
    size_t len;
    struct MessageQueue* next;
};

pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
struct MessageQueue* queue = NULL;

void queue_message_for_sock(struct lws* sock, const char* message, size_t len) {
    // create a new message
    struct MessageQueue* q = malloc(sizeof(struct MessageQueue));
    q->socket = sock;
    q->message = malloc(len);
    memcpy(q->message, message, len);
    q->len = len;
    pthread_mutex_lock(&queue_lock);
    q->next = queue;
    queue = q;
    pthread_mutex_unlock(&queue_lock);
}

unsigned char* get_message_for_sock(struct lws* sock, size_t* len) {
    struct MessageQueue* target = NULL;
    struct MessageQueue* prev = NULL;
    pthread_mutex_lock(&queue_lock);
    struct MessageQueue* ptr = queue;
    while (ptr) {
        if (ptr->socket == sock) {
            target = ptr;
            break;
        }
        prev = ptr;
        ptr = ptr->next;
    }
    if (!target) {
        printf("get_message_for_sock: no message for socket %p\n", sock);
        pthread_mutex_unlock(&queue_lock);
        return NULL;
    }
    if (prev) {
        prev->next = target->next;
    } else {
        queue = target->next;
    }
    pthread_mutex_unlock(&queue_lock);
    *len = target->len;
    unsigned char* retval = target->message;
    free(target);
    return retval;
}
