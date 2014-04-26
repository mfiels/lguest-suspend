#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

int uds_open(char *path) {
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

int uds_accept(int socket_fd, struct sockaddr_un *out) {
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

int uds_read(int connection_fd, char *out, int size) {
  int nbytes;
  
  nbytes = read(connection_fd, out, size);

  if (nbytes < 0) {
    perror("read failed");
    return -1;
  }

  return nbytes;
}

int main(void) {
  int socket_fd;
  int connection_fd;
  char buffer[256];
  int nbytes;

  socket_fd = uds_open(".lguest_control");

  connection_fd = uds_accept(socket_fd, NULL);
  
  nbytes = uds_read(connection_fd, buffer, 256);
  printf("got message of %s\n", buffer);

  close(connection_fd);
}
