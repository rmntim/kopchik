#ifndef KOP_UTILS_H_
#define KOP_UTILS_H_

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

#define kop_vector_free(v)                                                     \
  do {                                                                         \
    free(v.data);                                                              \
    v.data = NULL;                                                             \
    v.len = 0;                                                                 \
    v.cap = 0;                                                                 \
  } while (0)

#endif // !KOP_UTILS_H_
