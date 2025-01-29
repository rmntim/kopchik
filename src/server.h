#ifndef KOP_SERVER_H_
#define KOP_SERVER_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "http.h"
#include "utils.h"

struct kop_server;

typedef struct kop_address {
  const char *ip;
  uint16_t port;
} kop_address;

typedef struct kop_context {
  struct kop_server *server;
  kop_http_request req;
  kop_http_response resp;
  int client_sock;
} kop_context;

typedef void (*kop_handler_func)(kop_context);

typedef struct kop_handler {
  kop_http_method method;
  const char *path;
  kop_handler_func handler;
} kop_handler;

typedef struct kop_handlers {
  kop_handler *data;
  size_t cap;
  size_t len;
} kop_handlers;

typedef void (*shutdown_func)(int);

typedef struct kop_server {
  int sock_fd;
  uint16_t port;
  kop_handlers handlers;
  shutdown_func shutdown;
} kop_server;

kop_error kop_server_new(kop_server *s, uint16_t port);
kop_error kop_server_run(kop_server *s);
void kop_server_delete(kop_server *s);

void kop_get(kop_server *s, const char *path, kop_handler_func handler);
void kop_post(kop_server *s, const char *path, kop_handler_func handler);
void kop_put(kop_server *s, const char *path, kop_handler_func handler);
void kop_delete(kop_server *s, const char *path, kop_handler_func handler);

#endif // KOP_SERVER_H_
