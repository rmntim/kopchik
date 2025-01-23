#include "server.h"
#include "http.h"
#include <assert.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static kop_error kop_handlers_init(kop_handlers *handlers) {
  // TODO: custom initial cap?
  handlers->cap = 4;

  void *bytes = malloc(sizeof(kop_handler) * handlers->cap);
  if (bytes == NULL) {
    return ERR_OUT_OF_MEMORY;
  }

  handlers->data = bytes;
  handlers->len = 0;

  return NOERROR;
}

static void kop_handlers_deinit(kop_handlers *handlers) {
  free(handlers->data);
  handlers->cap = 0;
  handlers->len = 0;
  handlers->data = NULL;
}

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

kop_error kop_server_new(kop_server *s, uint16_t port) {
  int sock = kop_server_init(port);
  if (sock < 0) {
    return ERR_CREATING_SOCKET;
  }

  s->sock_fd = sock;
  s->port = port;

  kop_error err = kop_handlers_init(&s->handlers);
  if (err != NOERROR) {
    return err;
  }

  return NOERROR;
}

static kop_error parse_http_request(int sock, kop_http_request *req) {
  char buf_arr[4096];
  char *buf = buf_arr;

  ssize_t n = recv(sock, buf, sizeof(buf_arr), 0);
  if (n < 0) {
    return ERR_READING_DATA;
  }

  const char *method = strsep(&buf, " ");

  if (KOP_IS_HTTP_METHOD(method, "GET")) {
    req->method = HTTP_GET;
  } else if (KOP_IS_HTTP_METHOD(method, "POST")) {
    req->method = HTTP_POST;
  } else if (KOP_IS_HTTP_METHOD(method, "PUT")) {
    req->method = HTTP_PUT;
  } else if (KOP_IS_HTTP_METHOD(method, "DELETE")) {
    req->method = HTTP_DELETE;
  } else {
    return ERR_PARSING_REQ;
  }

  const char *path = strsep(&buf, " ");
  size_t path_len = strlen(path) + 1;
  char *req_path = malloc(path_len);
  if (req_path == NULL) {
    return ERR_OUT_OF_MEMORY;
  }

  strlcpy(req_path, path, path_len);
  req->path = req_path;

  const char *http_version = strsep(&buf, "\r\n");
  if (strncmp(http_version, "HTTP/1.1", sizeof("HTTP/1.1") - 1) != 0) {
    free(req_path);
    return ERR_UNSUPPORTED_HTTP_VERSION;
  }

  buf += 1;

  if (strncmp(buf, "\r\n", 2) != 0) {
    free(req_path);
    return ERR_HEADERS;
  }

  buf += 2;

  size_t body_len = buf_arr + sizeof(buf_arr) - 1 - buf;
  char *body = malloc(body_len);
  if (body == NULL) {
    free(req_path);
    return ERR_OUT_OF_MEMORY;
  }

  strlcpy(body, buf, body_len);

  req->body_len = body_len;
  req->body = body;

  return NOERROR;
}

static void kop_http_request_free(kop_http_request *req) {
  free((void *)req->body);
  free((void *)req->path);
  req->body_len = 0;
  req->path = 0;
}

static kop_error kop_handle_client(kop_server *s, int client_sock) {
  kop_http_request req = {0};
  kop_error err;
  if ((err = parse_http_request(client_sock, &req)) != NOERROR) {
    return err;
  }

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

  return NOERROR;
}

kop_error kop_server_run(kop_server *s) {
  if (listen(s->sock_fd, 10) < 0) {
    return ERR_LISTENING;
  }

  bool stop = false;
  int server_sock = s->sock_fd;

  while (!stop) {
    struct sockaddr client_addr = {0};
    socklen_t client_addr_size = 0;

    int client_sock = accept(server_sock, &client_addr, &client_addr_size);
    if (client_sock < 0) {
      return ERR_ACCEPTING;
    }

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

  kop_handlers_deinit(&s->handlers);
}

void kop_get(kop_server *s, const char *path, kop_handler_func handler) {
  (void)s;
  (void)path;
  (void)handler;
  assert(0 && "kop_get unimplemented");
}

void kop_post(kop_server *s, const char *path, kop_handler_func handler) {
  (void)s;
  (void)path;
  (void)handler;
  assert(0 && "kop_post unimplemented");
}

void kop_put(kop_server *s, const char *path, kop_handler_func handler) {
  (void)s;
  (void)path;
  (void)handler;
  assert(0 && "kop_put unimplemented");
}

void kop_delete(kop_server *s, const char *path, kop_handler_func handler) {
  (void)s;
  (void)path;
  (void)handler;
  assert(0 && "kop_delete unimplemented");
}
