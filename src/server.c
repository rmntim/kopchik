#include "server.h"
#include "http.h"
#include "utils.h"
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

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

static const char *find_header_or_default(kop_http_request *req,
                                          const char *header, const char *def) {
  for (size_t i = 0; i < req->headers.len; i++) {
    if (strcmp(req->headers.data[i].header, header) == 0) {
      return req->headers.data[i].value;
    }
  }

  return def;
}

#define kop_headers_free(headers)                                              \
  do {                                                                         \
    kop_vector_foreach(kop_http_header, headers, header) {                     \
      free((void *)header->header);                                            \
      free((void *)header->value);                                             \
    }                                                                          \
                                                                               \
    kop_vector_free(headers);                                                  \
  } while (0)

static kop_error parse_http_request(int sock, kop_http_request *req) {
  static char buf_arr[4096 + 1];
  char *buf = buf_arr;

  ssize_t n = recv(sock, buf, sizeof(buf_arr) - 1, 0);
  if (n < 0) {
    return ERR_READING_DATA;
  }

  buf_arr[n] = '\0';

  const char *method = strsep(&buf, " ");
  if (buf == NULL) {
    return ERR_MALFORMED_METHOD;
  }

  kop_http_method http_method = kop_http_method_from_str(method);
  if (http_method == HTTP_BAD_METHOD) {
    return ERR_MALFORMED_METHOD;
  }

  req->method = http_method;

  const char *tmp_path = strsep(&buf, " ");
  char *path = strdup(tmp_path);
  if (path == NULL) {
    return ERR_OUT_OF_MEMORY;
  }

  while (buf == NULL) {
    ssize_t n = recv(sock, buf_arr, sizeof(buf_arr) - 1, 0);
    if (n < 0) {
      free(path);
      return ERR_READING_DATA;
    }
    buf_arr[n] = '\0';
    buf = buf_arr;

    tmp_path = strsep(&buf, " ");
    strcat(path, tmp_path);
  }

  req->path = path;

  const char *http_version = strsep(&buf, "\r\n");
  if (buf == NULL) {
    free((void *)path);
    return ERR_MALFORMED_HTTP_VERSION;
  }

  if (strncmp(http_version, "HTTP/1.1", strlen("HTTP/1.1")) != 0) {
    free((void *)path);
    return ERR_UNSUPPORTED_HTTP_VERSION;
  }

  buf += 1;

  kop_http_headers headers = {0};
  kop_vector_init(kop_http_header, headers);

  while (strncmp(buf, "\r\n", 2) != 0) {
    const char *tmp_header_key = strsep(&buf, ":");
    char *header_key = strdup(tmp_header_key);
    if (header_key == NULL) {
      free((void *)path);
      kop_headers_free(headers);
      return ERR_OUT_OF_MEMORY;
    }

    while (buf == NULL) {
      ssize_t n = recv(sock, buf_arr, sizeof(buf_arr) - 1, 0);
      if (n < 0) {
        free(header_key);
        free((void *)path);
        kop_headers_free(headers);
        return ERR_READING_DATA;
      }
      buf_arr[n] = '\0';
      buf = buf_arr;

      tmp_header_key = strsep(&buf, ":");
      strcat(header_key, tmp_header_key);
    }

    for (; *buf != '\0' && *buf != '\r' && *buf == ' '; ++buf)
      ;

    if (*buf == '\0' || *buf == '\r') {
      free((void *)path);
      free(header_key);
      kop_headers_free(headers);
      return ERR_MALFORMED_HEADER;
    }

    const char *tmp_header_value = strsep(&buf, "\r\n");
    char *header_value = strdup(tmp_header_value);
    if (header_value == NULL) {
      free((void *)path);
      free(header_key);
      kop_headers_free(headers);
      return ERR_OUT_OF_MEMORY;
    }

    while (buf == NULL) {
      ssize_t n = recv(sock, buf_arr, sizeof(buf_arr) - 1, 0);
      if (n < 0) {
        free(header_key);
        free(header_value);
        free((void *)path);
        kop_headers_free(headers);
        return ERR_READING_DATA;
      }
      buf_arr[n] = '\0';
      buf = buf_arr;

      tmp_header_value = strsep(&buf, "\r\n");
      strcat(header_value, tmp_header_value);
    }

    buf += 1;

    kop_http_header header =
        (kop_http_header){.header = header_key, .value = header_value};

    kop_vector_append(kop_http_header, headers, header);
  }

  req->headers = headers;

  buf += 2;

  const char *content_length_str =
      find_header_or_default(req, "Content-Length", "0");
  uint64_t body_len = strtoll(content_length_str, NULL, 10);

  char *body = malloc(body_len + 1);
  if (body == NULL) {
    free((void *)path);
    kop_headers_free(headers);
    return ERR_OUT_OF_MEMORY;
  }

  strlcpy(body, buf, body_len + 1);

  req->body_len = body_len;
  req->body = body;

  return NOERROR;
}

static void kop_http_request_free(kop_http_request *req) {
  free((void *)req->body);
  free((void *)req->path);
  kop_headers_free(req->headers);
  req->body_len = 0;
  req->path = 0;
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
