#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "http.h"
#include "queue.h"
#include "server.h"
#include "utils.h"

static kop_error set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    return ERR_NONBLOCKING;
  }

  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    return ERR_NONBLOCKING;
  }

  return NOERROR;
}

static kop_error kop_server_init(int *server_sock, uint16_t port) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  int reuseaddr = 1;

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr,
                 sizeof(reuseaddr)) < 0) {
    return ERR_CREATING_SOCKET;
  }

  if (sock < 0) {
    return ERR_CREATING_SOCKET;
  }

  struct sockaddr_in addr = (struct sockaddr_in){
      .sin_addr = {INADDR_ANY},
      .sin_port = htons(port),
      .sin_family = AF_INET,
  };

  if (bind(sock, (const struct sockaddr *)&addr, sizeof(addr)) < 0) {
    return ERR_CREATING_SOCKET;
  }

  kop_error err = set_nonblocking(sock);
  if (err != NOERROR) {
    return err;
  }

  *server_sock = sock;

  return NOERROR;
}

// HACK: idek might kms later because of this
volatile static bool gStop = false;

static void kop_server_shutdown(int sig) {
  KOP_DEBUG_LOG("received signal %s", strsignal(sig));
  (void)sig;
  gStop = true;
}

kop_error kop_server_new(kop_server *s, uint16_t port) {
  kop_error err = kop_server_init(&s->sock_fd, port);
  if (err != NOERROR) {
    return err;
  }

  err = kop_queue_init(&s->queue, s->sock_fd);
  if (err != NOERROR) {
    return err;
  }

  s->port = port;

  kop_vector_init(kop_handler, s->handlers);

  s->shutdown = kop_server_shutdown;
  signal(SIGINT, s->shutdown);

  return NOERROR;
}

static kop_error kop_handle_client(kop_server *s, int client_sock) {
  kop_http_request req = {0};
  kop_error err;
  if ((err = parse_http_request(client_sock, &req)) != NOERROR) {
    return err;
  }

  KOP_DEBUG_LOG("got client with method '%s'",
                KOP_HTTP_METHOD_TO_STR(req.method));

  kop_vector_foreach(kop_http_header, req.headers, header) {
    KOP_DEBUG_LOG("                %s : %s", header->header, header->value);
  }

  KOP_DEBUG_LOG("                path '%s'", req.path);
  KOP_DEBUG_LOG("                body(len=%zu) '%s'", req.body_len, req.body);

  for (size_t i = 0; i < s->handlers.len; i++) {
    kop_handler handler = s->handlers.data[i];
    if (handler.method != req.method || (strcmp(handler.path, req.path) != 0)) {
      continue;
    }

    kop_context ctx = {
        .client_sock = client_sock,
        .req = req,
        .server = s,
        .resp = {.body_len = 0, .body = NULL, .code = HTTP_OK},
    };
    handler.handler(ctx);
    break;
  }

  kop_http_request_free(&req);

  KOP_DEBUG_LOG("client disconnect %d", client_sock);

  return NOERROR;
}

kop_error kop_server_run(kop_server *s) {
  if (listen(s->sock_fd, 10) < 0) {
    return ERR_LISTENING;
  }

  int server_sock = s->sock_fd;

  int nevents = 0;
  kop_queue_event events[10] = {0};

  kop_error err = NOERROR;

  while (!gStop) {
    err = kop_queue_wait(&s->queue, events, 10, &nevents);
    if (err != NOERROR) {
      gStop = true;
      break;
    }

    for (size_t i = 0; i < (size_t)nevents; i++) {
      kop_queue_event event = events[i];

      if (kop_queue_event_check_error(event)) {
        KOP_DEBUG_LOG("socket error: %s", kop_queue_event_strerror(event));

        if (kop_queue_event_is_server(&s->queue, event)) {
          // server is fucking dead
          goto server_dead;
        }

        kop_queue_event_close_client(event);
        continue;
      }

      if (kop_queue_event_is_server(&s->queue, event)) {
        for (;;) {
          struct sockaddr in_addr;
          socklen_t in_addr_len = sizeof(in_addr);
          int client = accept(server_sock, &in_addr, &in_addr_len);
          if (client < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
              goto server_dead;
            }
            // we processed all of the connections
            break;
          }

          KOP_DEBUG_LOG("accepted new connection on fd: %d", client);
          set_nonblocking(client);

          struct timeval tv = (struct timeval){
              .tv_sec = 5,
              .tv_usec = 0,
          };
          setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

          err = kop_queue_add_client_sock(&s->queue, client);
          if (err != NOERROR) {
            goto server_dead;
          }
        }
      } else {
        // a client socket
        int client_sock = kop_queue_event_get_sock(event);
        err = kop_handle_client(s, client_sock);
        if (err != NOERROR) {
          KOP_DEBUG_LOG("error handling client: %s", KOP_STRERROR(err));
        }
        close(client_sock);
      }
    }

    continue;
  server_dead:
    err = ERR_DEAD_SERVER;
    gStop = true;
  }

  return err;
}

void kop_server_delete(kop_server *s) {
  s->port = 0;

  kop_queue_close(&s->queue);

  close(s->sock_fd);
  s->sock_fd = 0;

  kop_vector_free(s->handlers);
}

void kop_get(kop_server *s, const char *path, kop_handler_func handler_func) {
  kop_handler handler = (kop_handler){
      .method = HTTP_GET,
      .path = path,
      .handler = handler_func,
  };

  kop_vector_append(kop_handler, s->handlers, handler);
}

void kop_post(kop_server *s, const char *path, kop_handler_func handler_func) {
  kop_handler handler = (kop_handler){
      .method = HTTP_POST,
      .path = path,
      .handler = handler_func,
  };

  kop_vector_append(kop_handler, s->handlers, handler);
}

void kop_put(kop_server *s, const char *path, kop_handler_func handler_func) {
  kop_handler handler = (kop_handler){
      .method = HTTP_PUT,
      .path = path,
      .handler = handler_func,
  };

  kop_vector_append(kop_handler, s->handlers, handler);
}

void kop_delete(kop_server *s, const char *path,
                kop_handler_func handler_func) {
  kop_handler handler = (kop_handler){
      .method = HTTP_DELETE,
      .path = path,
      .handler = handler_func,
  };

  kop_vector_append(kop_handler, s->handlers, handler);
}
