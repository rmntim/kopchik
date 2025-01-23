#ifndef KOP_HTTP_H_
#define KOP_HTTP_H_

#include <stddef.h>
#include <stdint.h>

struct kop_context;

typedef enum kop_http_method {
  HTTP_GET,
  HTTP_POST,
  HTTP_PUT,
  HTTP_DELETE,
} kop_http_method;

typedef enum kop_http_code {
  HTTP_OK = 200,
  HTTP_INTERNAL_SERVER_ERROR = 500,
} kop_http_code;

typedef struct kop_http_request {
  kop_http_method method;
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
