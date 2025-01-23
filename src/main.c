#include "server.h"
#include <stdio.h>

#define PORT 8000

int main(void) {
  kop_server s;

  if (kop_server_new(&s, PORT) != NOERROR) {
    perror("server new");
    return -1;
  }

  int ret = 0;

  KOP_DEBUG_LOG("starting server on %d", PORT);

  if (kop_server_run(&s) != NOERROR) {
    perror("server run");
    ret = -1;
  }

  kop_server_delete(&s);
  return ret;
}
