#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void child(int read_fd) {
  int p, n;
  int write_fd[2];
  int no_child = 1;
  pipe(write_fd);

  read(read_fd, &p, sizeof(int));
  printf("%d\n", p);
  while (read(read_fd, &n, sizeof(int)) == sizeof(int)) {
    if ((n % p) != 0) {
      if (no_child) {
        no_child = 0;
        int pid = fork();
        if (pid == 0) {
            // I am child
            close(write_fd[1]);
            child(write_fd[0]);
            break;
        } else if (pid < 0) {
            // Fork err
            fprintf(2, "error: fork failed");
        }
      }
      write(write_fd[1], &n, sizeof(int));
    }
  }
  close(write_fd[1]);
  wait();
}

int main() {
  int fds[2];
  close(0); // zatvorim standard input

  pipe(fds);

  int pid = fork();

  if (pid == 0) {
    // I am child
    // Close unneeded descriptors
    close(fds[1]); // dieta nebude do pipe zapisovat
    child(fds[0]);
  } else if (pid > 0) {
    // I am parent
    for (int i = 2; i <= 35; i++) {
        write(fds[1], &i, sizeof(int));
    }
    close(fds[1]);
    wait();
  } else {
    // Fork err
    fprintf(2, "error: fork\n");
  }
  exit();
}