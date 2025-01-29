#include <errno.h>
#include <netinet/in.h>
#include <stdlib.h>

#include "http.h"

void kop_http_request_free(kop_http_request *req) {
  free((void *)req->body);
  free((void *)req->path);
  kop_headers_free(req->headers);
  req->body_len = 0;
  req->path = 0;
}

kop_error parse_http_request(int sock, kop_http_request *req) {
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

  if (body_len == 0) {
    req->body_len = body_len;
    req->body = strdup("");

    return NOERROR;
  }

  char *body = strdup(buf);
  if (body == NULL) {
    free((void *)path);
    kop_headers_free(headers);
    return ERR_OUT_OF_MEMORY;
  }

  while (strlen(body) < body_len) {
    ssize_t n = recv(sock, buf_arr, sizeof(buf_arr) - 1, 0);
    if (n < 0) {
      free((void *)path);
      free(body);
      kop_headers_free(headers);
      if (errno == EAGAIN) {
        return ERR_TIMEOUT;
      }
      return ERR_READING_DATA;
    }

    buf_arr[n] = '\0';
    buf = buf_arr;

    strcat(body, buf);
  }

  req->body_len = body_len;
  req->body = body;

  return NOERROR;
}
