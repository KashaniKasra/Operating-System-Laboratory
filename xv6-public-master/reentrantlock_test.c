#include "types.h"
#include "stat.h"
#include "user.h"

int main()
{
    int pid, count = 0;

    for(int i = 0; i < 3; i++) {
        printf(1, "mmmm %d mmmmmm\n", pid );
        pid = fork();
        printf(1, "eeee %d deeee\n", pid );
        if(pid == 0) {
           reentrantlock_test(count);
            exit();
        }
    }

    for(int i = 0; i < 3; i++) {
        wait();
    }

    printf(1, "Test successfully done!\n");

    return 0;
}