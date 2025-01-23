#ifndef KOP_HTTP_H_
#define KOP_HTTP_H_

#include <stddef.h>

#define KOP_HTTP_METHOD_TO_STR(method) kop_http_method_str[method]

struct kop_context;

typedef struct kop_http_header {
  const char *header;
  const char *value;
} kop_http_header;

typedef struct kop_http_headers {
  kop_http_header *data;
  size_t len;
  size_t cap;
} kop_http_headers;

typedef enum kop_http_method {
  HTTP_GET,
  HTTP_POST,
  HTTP_PUT,
  HTTP_DELETE,
} kop_http_method;

static const char *kop_http_method_str[] = {
    [HTTP_GET] = "GET",
    [HTTP_POST] = "POST",
    [HTTP_PUT] = "PUT",
    [HTTP_DELETE] = "DELETE",
};

typedef enum kop_http_code {
  HTTP_OK = 200,
  HTTP_INTERNAL_SERVER_ERROR = 500,
} kop_http_code;

typedef struct kop_http_request {
  kop_http_method method;
  kop_http_headers headers;
  const char *path;
  const char *body;
  size_t body_len;
} kop_http_request;

typedef struct kop_http_response {
  kop_http_code code;
  char *body;
  size_t body_len;
} kop_http_response;

#endif // !KOP_HTTP_H_
