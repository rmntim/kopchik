#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "http.h"
#include "utils.h"

void kop_http_request_free(kop_http_request *req) {
  free((void *)req->body);
  free((void *)req->path);
  kop_headers_free(req->headers);
  req->body_len = 0;
  req->path = 0;
}

static kop_error read_data(int sock, char **buf, size_t *n) {
  const size_t BUF_SIZE = 4096;
  char tmp_buf[BUF_SIZE];
  bool first = true;

  for (;;) {
    ssize_t nbytes = read(sock, tmp_buf, BUF_SIZE - 1);
    if (nbytes < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        KOP_DEBUG_LOG("finished reading data from client%s", "");
        break;
      } else {
        return ERR_READING_DATA;
      }
    } else if (nbytes == 0) {
      KOP_DEBUG_LOG("finished with %d", sock);
      break;
    }
    tmp_buf[nbytes] = '\0';

    if (first) {
      *buf = strdup(tmp_buf);
      assert(*buf && "ur ram is dead");
      first = false;
    } else {
      *buf = realloc(*buf, *n + nbytes);
      assert(*buf && "ur ram is dead");
      strcat(*buf, tmp_buf);
    }

    *n += nbytes;
  }

  return NOERROR;
}

kop_error parse_http_request(int sock, kop_http_request *req) {
  char *buf = 0;
  size_t n = 0;

  if (read_data(sock, &buf, &n) != NOERROR) {
    free(buf);
    return ERR_READING_DATA;
  }

  if (buf == NULL || n == 0) {
    return ERR_READING_DATA;
  }

  void *defer_buf = buf;

  const char *method = strsep(&buf, " ");
  if (buf == NULL) {
    free(defer_buf);
    return ERR_MALFORMED_METHOD;
  }

  kop_http_method http_method = kop_http_method_from_str(method);
  if (http_method == HTTP_BAD_METHOD) {
    free(defer_buf);
    return ERR_MALFORMED_METHOD;
  }

  req->method = http_method;

  const char *tmp_path = strsep(&buf, " ");
  char *path = strdup(tmp_path);
  if (path == NULL) {
    free(defer_buf);
    return ERR_OUT_OF_MEMORY;
  }

  req->path = path;

  const char *http_version = strsep(&buf, "\r\n");
  if (buf == NULL) {
    free(defer_buf);
    free((void *)path);
    return ERR_MALFORMED_HTTP_VERSION;
  }

  if (strncmp(http_version, "HTTP/1.1", strlen("HTTP/1.1")) != 0) {
    free((void *)path);
    free(defer_buf);
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
      free(defer_buf);
      kop_headers_free(headers);
      return ERR_OUT_OF_MEMORY;
    }

    for (; *buf != '\0' && *buf != '\r' && *buf == ' '; ++buf)
      ;

    if (*buf == '\0' || *buf == '\r') {
      free((void *)path);
      free(defer_buf);
      free(header_key);
      kop_headers_free(headers);
      return ERR_MALFORMED_HEADER;
    }

    const char *tmp_header_value = strsep(&buf, "\r\n");
    char *header_value = strdup(tmp_header_value);
    if (header_value == NULL) {
      free((void *)path);
      free(defer_buf);
      free(header_key);
      kop_headers_free(headers);
      return ERR_OUT_OF_MEMORY;
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

  if (body_len == 0) {
    req->body_len = body_len;
    req->body = strdup("");

    return NOERROR;
  }

  char *body = strdup(buf);
  if (body == NULL) {
    free((void *)path);
    free(defer_buf);
    kop_headers_free(headers);
    return ERR_OUT_OF_MEMORY;
  }

  if (body + body_len > (char *)defer_buf + n) {
    free((void *)path);
    free(defer_buf);
    kop_headers_free(headers);
    return ERR_MALFORMED_BODY;
  }

  body[body_len] = '\0';

  req->body_len = body_len;
  req->body = body;

  return NOERROR;
}
