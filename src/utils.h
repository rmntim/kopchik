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

#endif // !KOP_UTILS_H_
