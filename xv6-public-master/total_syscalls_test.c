#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main() {
    int pid, fd = open("testfile.txt", O_CREATE | O_WRONLY);

    printf(1, "Starting test...\n");

    for(int i = 0; i < 30; i++) {
        pid = fork();

        if(pid == 0) {
            write(fd, "hello", 5);
            exit();
        }
    }

    for(int i = 0; i < 30; i++) {
        wait();
    }

    close(fd);

    printf(1, "Test Done!\n");

    get_number_of_total_syscalls();

    return 0;
}