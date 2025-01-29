#ifndef KOP_UTILS_H_
#define KOP_UTILS_H_

#include <stdio.h>

#ifdef KOP_DEBUG
#define KOP_DEBUG_LOG(msg, ...)                                                \
  do {                                                                         \
    fprintf(stderr, "[DEBUG] %s:%d " msg "\n", __FILE__, __LINE__,             \
            __VA_ARGS__);                                                      \
  } while (0)
#else
#define KOP_DEBUG_LOG(msg, ...)
#endif

#define kop_vector_init(type, v)                                               \
  do {                                                                         \
    v.len = 0;                                                                 \
    v.cap = 4;                                                                 \
    v.data = malloc(sizeof(type) * v.cap);                                     \
  } while (0)

#define kop_vector_append(type, vptr, value)                                   \
  do {                                                                         \
    if ((vptr).len == (vptr).cap) {                                            \
      (vptr).data = realloc((vptr).data, sizeof(type) * (vptr).cap * 2);       \
    }                                                                          \
                                                                               \
    (vptr).data[(vptr).len++] = value;                                         \
  } while (0)

#define kop_vector_foreach(type, vptr, it)                                     \
  for (type *it = (vptr).data; it < (vptr).data + (vptr).len; ++it)

#define kop_vector_free(v)                                                     \
  do {                                                                         \
    free(v.data);                                                              \
    v.data = NULL;                                                             \
    v.len = 0;                                                                 \
    v.cap = 0;                                                                 \
  } while (0)

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
  ERR_MALFORMED_HTTP_VERSION,
  ERR_MALFORMED_BODY,
  ERR_TIMEOUT,
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
    [ERR_MALFORMED_BODY] = "ERR_MALFORMED_BODY",
    [ERR_TIMEOUT] = "ERR_TIMEOUT",
};

#define KOP_STRERROR(err) kop_error_str[err]

#endif // !KOP_UTILS_H_
