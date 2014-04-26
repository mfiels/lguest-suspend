#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>

int main(void) {
  /**
   * Simpe UDS client server example: http://www.thomasstover.com/uds.html
   **/
  struct sockaddr_un address;
  int socket_fd;
  int nbytes;
  char buffer[256];
  int rv;

  socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);

  memset(&address, 0, sizeof(struct sockaddr_un));

  address.sun_family = AF_UNIX;
  strcpy(address.sun_path, ".lguest_control");

  rv = connect(socket_fd, (struct sockaddr *) &address, sizeof(address));
  if (rv != 0) {
    perror("failed to connect");
    return 1;
  }

  strcpy(buffer, "test");
  nbytes = strlen(buffer) + 1;
  printf("writing %s...\n", buffer);
  write(socket_fd, buffer, nbytes);
}
