#include "queue.h"
#include "utils.h"

kop_error kop_queue_init(kop_queue *q, int server_sock) {
#if defined(KOP_LINUX)
  int fd = epoll_create1(0);
#elif defined(KOP_BSD)
  int fd = kqueue();
#endif
  if (fd < 0) {
    return ERR_CREATING_QUEUE;
  }
  q->queue = fd;
  q->server_sock = server_sock;

  kop_queue_event event = {0};

#if defined(KOP_LINUX)
  event.data.fd = server_sock;
  event.events = EPOLLIN | EPOLLET;
  if (epoll_ctl(fd, EPOLL_CTL_ADD, server_sock, &event) < 0) {
    return ERR_CREATING_QUEUE;
  }
#elif defined(KOP_BSD)
  EV_SET(&event, server_sock, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);
  if (kevent(fd, &event, 1, NULL, 0, NULL) < 0) {
    return ERR_CREATING_QUEUE;
  }
#endif
  return NOERROR;
}

kop_error kop_queue_wait(kop_queue *q, kop_queue_event *events, size_t nevents,
                         int *new_events) {
#if defined(KOP_LINUX)
  int evts = epoll_wait(q->queue, events, nevents, -1);
  if (evts < 0) {
    return ERR_QUEUE_WAIT;
  }
#elif defined(KOP_BSD)
  int evts = kevent(q->queue, NULL, 0, events, nevents, NULL);
  if (evts < 0) {
    return ERR_QUEUE_WAIT;
  }
#endif
  *new_events = evts;
  return NOERROR;
}

kop_error kop_queue_add_client_sock(kop_queue *q, int client_sock) {
  kop_queue_event event = {0};

#if defined(KOP_LINUX)
  set_nonblocking(client_sock);
  event.data.fd = client_sock;
  event.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP;
  if (epoll_ctl(q->queue, EPOLL_CTL_ADD, client_sock, &event) < 0) {
    return ERR_QUEUE_ADD_CLIENT;
  }
#elif defined(KOP_BSD)
  EV_SET(&event, client_sock, EVFILT_READ | EVFILT_WRITE, EV_ADD, 0, 0, NULL);
  if (kevent(q->queue, &event, 1, NULL, 0, NULL) < 0) {
    return ERR_QUEUE_ADD_CLIENT;
  }
#endif

  return NOERROR;
}
