#include <errno.h>
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
#include "server.h"
#include "utils.h"

static int kop_server_init(uint16_t port) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  int reuseaddr = 1;

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr,
                 sizeof(reuseaddr)) < 0) {
    return -1;
  }

  if (sock < 0) {
    return -1;
  }

  struct sockaddr_in addr = (struct sockaddr_in){
      .sin_addr = {INADDR_ANY},
      .sin_port = htons(port),
      .sin_family = AF_INET,
  };

  if (bind(sock, (const struct sockaddr *)&addr, sizeof(addr)) < 0) {
    return -1;
  }

  return sock;
}

// HACK: idek might kms later because of this
volatile static bool gStop = false;

static void kop_server_shutdown(int sig) {
  KOP_DEBUG_LOG("received signal %s", strsignal(sig));
  (void)sig;
  gStop = true;
}

kop_error kop_server_new(kop_server *s, uint16_t port) {
  int sock = kop_server_init(port);
  if (sock < 0) {
    return ERR_CREATING_SOCKET;
  }

  s->sock_fd = sock;
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

  while (!gStop) {
    struct sockaddr client_addr = {0};
    socklen_t client_addr_size = 0;

    int client_sock = accept(server_sock, &client_addr, &client_addr_size);
    if (client_sock < 0) {
      return ERR_ACCEPTING;
    }

    struct timeval tv = (struct timeval){
        .tv_sec = 5,
        .tv_usec = 0,
    };
    setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

#ifdef DEBUG
    char name[INET_ADDRSTRLEN];
    char port[10];
    getnameinfo(&client_addr, client_addr_size, name, sizeof(name), port,
                sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);

    DEBUG_LOG("got client %s:%s", name, port);
#endif

    kop_error err = kop_handle_client(s, client_sock);
    if (err != NOERROR) {
      KOP_DEBUG_LOG("error while handling client %s", KOP_STRERROR(err));
    }

    close(client_sock);
  }

  return NOERROR;
}

void kop_server_delete(kop_server *s) {
  s->port = 0;

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
