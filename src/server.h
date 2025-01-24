#ifndef KOP_SERVER_H_
#define KOP_SERVER_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "http.h"

typedef enum kop_error {
  NOERROR = 0,
  ERR_CREATING_SOCKET,
  ERR_OUT_OF_MEMORY,
  ERR_LISTENING,
  ERR_ACCEPTING,
  ERR_CREATING_CONTEXT,
  ERR_PARSING_REQ,
  ERR_READING_DATA,
  ERR_UNSUPPORTED_HTTP_VERSION,
  ERR_HEADERS,
  ERR_INVALID_BODY,
  ERR_MALFORMED_HEADER,
  ERR_MALFORMED_METHOD,
  ERR_MALFORMED_HTTP_VERSION
} kop_error;

static const char *kop_error_str[] = {
    [NOERROR] = "no error",
    [ERR_CREATING_SOCKET] = "ERR_CREATING_SOCKET",
    [ERR_OUT_OF_MEMORY] = "ERR_OUT_OF_MEMORY",
    [ERR_LISTENING] = "ERR_LISTENING",
    [ERR_ACCEPTING] = "ERR_ACCEPTING",
    [ERR_CREATING_CONTEXT] = "ERR_CREATING_CONTEXT",
    [ERR_PARSING_REQ] = "ERR_PARSING_REQ",
    [ERR_READING_DATA] = "ERR_READING_DATA",
    [ERR_UNSUPPORTED_HTTP_VERSION] = "ERR_UNSUPPORTED_HTTP_VERSION",
    [ERR_HEADERS] = "ERR_HEADERS",
    [ERR_INVALID_BODY] = "ERR_INVALID_BODY",
    [ERR_MALFORMED_HEADER] = "ERR_MALFORMED_HEADER",
    [ERR_MALFORMED_METHOD] = "ERR_MALFORMED_METHOD",
    [ERR_MALFORMED_HTTP_VERSION] = "ERR_MALFORMED_HTTP_VERSION",
};

#define KOP_STRERROR(err) kop_error_str[err]

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
