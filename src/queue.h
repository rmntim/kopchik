#ifndef KOP_QUEUE_H_
#define KOP_QUEUE_H_

#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "utils.h"

#if defined(KOP_LINUX)

#include <sys/epoll.h>
typedef struct epoll_event

#elif defined(KOP_BSD)

#include <sys/event.h>
typedef struct kevent

#else

#error "unsupported platform"
typedef void *

#endif
    kop_queue_event;

#define MAX_EVENTS 64

typedef struct kop_queue {
  int queue;
  int server_sock;
} kop_queue;

kop_error kop_queue_init(kop_queue *q, int server_sock);
kop_error kop_queue_wait(kop_queue *q, kop_queue_event *events, size_t nevents,
                         int *new_events);
kop_error kop_queue_add_client_sock(kop_queue *q, int client_sock);

static inline void kop_queue_close(kop_queue *q) {
  close(q->queue);
  q->queue = 0;
  q->server_sock = 0;
}

static inline bool kop_queue_event_check_error(kop_queue_event event) {
#if defined(KOP_LINUX)
  return (event.events & EPOLLERR) || (event.events & EPOLLHUP) ||
         (!(event.events & EPOLLIN));
#elif defined(KOP_BSD)
  return event.flags & EV_ERROR;
#endif
}

static inline bool kop_queue_event_is_server(kop_queue *q,
                                             kop_queue_event event) {
#if defined(KOP_LINUX)
  return event.data.fd == q->server_sock;
#elif defined(KOP_BSD)
  return event.ident == (uintptr_t)q->server_sock;
#endif
}

static inline bool kop_queue_event_is_client(kop_queue *q,
                                             kop_queue_event event) {
  // NOTE: kinda hacky, but it's the best way at the moment
  return !kop_queue_event_is_server(q, event);
}

static inline bool kop_queue_event_is_client_disconnect(kop_queue_event event) {
#if defined(KOP_LINUX)
  return event.events & EPOLLRDHUP;
#elif defined(KOP_BSD)
  return event.flags & EV_EOF;
#endif
}

static inline const char *kop_queue_event_strerror(kop_queue_event event) {
#if defined(KOP_LINUX)
  int error = 0;
  socklen_t errlen = sizeof(error);
  if (getsockopt(event.data.fd, SOL_SOCKET, SO_ERROR, (void *)&error,
                 &errlen) == 0) {
    return strerror(error);
  }
  return "unknown error";
#elif defined(KOP_BSD)
  return strerror(event.data);
#endif
}

static inline void kop_queue_event_close_client(kop_queue_event event) {
#if defined(KOP_LINUX)
  close(event.data.fd);
#elif defined(KOP_BSD)
  close(event.ident);
#endif
}

static inline int kop_queue_event_get_sock(kop_queue_event event) {
#if defined(KOP_LINUX)
  return event.data.fd;
#elif defined(KOP_BSD)
  return event.ident;
#endif
}

#endif // !KOP_QUEUE_H_
