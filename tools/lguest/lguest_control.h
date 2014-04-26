#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define LGUEST_CONTROL_FILE ".lguest_control"

typedef enum {
  LGCTRL_NONE,
  LGCTRL_SUSPEND,
  LGCTRL_RESUME
} lgctrl_t;

void send_signal_to_kernel(lgctrl_t current_signal);

static int lguest_control_fd;
static lgctrl_t lguest_control_signal;

static int uds_open(char *path) {
  struct sockaddr_un address;
  int socket_fd;
  int rv;

  unlink(path);

  memset(&address, 0, sizeof(address));
  address.sun_family = AF_UNIX;
  strcpy(address.sun_path, path);

  socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
  if (socket_fd < 0) {
    perror("socket failed");
    return -1;
  }

  rv = bind(socket_fd, (struct sockaddr *) &address, sizeof(address));
  if (rv != 0) {
    perror("bind failed");
    return -1;
  }

  rv = listen(socket_fd, 5);
  if (rv != 0) {
    perror("listen failed");
    return -1;
  }

  return socket_fd;
}

static int uds_accept(int socket_fd, struct sockaddr_un *out) {
  struct sockaddr_un address;
  socklen_t address_length;
  int connection_fd;

  address_length = sizeof(address);
  connection_fd = accept(socket_fd, (struct sockaddr *) &address, &address_length);
  if (connection_fd < 0) {
    perror("accept failed");
    return -1;
  }

  if (out != NULL) {
    *out = address;
  }

  return connection_fd;
}

static int uds_read(int connection_fd, char *out, int size) {
  int nbytes;
  
  nbytes = read(connection_fd, out, size);

  if (nbytes < 0) {
    perror("read failed");
    return -1;
  }

  return nbytes;
}

static void *lguest_control_listen() {
  char buff[256];
  int conn_fd;

  while (1) {
    conn_fd = uds_accept(lguest_control_fd, NULL);

    uds_read(conn_fd, buff, 256);

    // Strip out newlines
    char *p = buff;
    while (*p) {
      if (*p == '\n') {
        *p = 0;
        break;
      }
      p++;
    }

    if (strcmp("suspend", buff) == 0) {
      send_signal_to_kernel(LGCTRL_SUSPEND);
    } else if (strcmp("resume", buff) == 0) {
      send_signal_to_kernel(LGCTRL_RESUME);
    }
  }

  return NULL;
}

static void lguest_control_init() {
  pthread_t listen_thread;

  lguest_control_signal = LGCTRL_NONE;
  lguest_control_fd = uds_open(LGUEST_CONTROL_FILE);

  pthread_create(&listen_thread, NULL, lguest_control_listen, NULL);
}
