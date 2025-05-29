#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[])
{
    if(argc != 1)
    {
        printf(2, "ERROR: Too many arguments!\n");
        exit();
    }

    int pid = getpid();

    if(fork() == 0)
    {
        printf(1, "Child process\n");
        exit();
    }
    else
    {
        printf(1, "Parent process\n");
        wait();
    }

    sleep(1);

    int palindrome = create_palindrome(123);
    
    int result = sort_syscalls(pid);

    if(result == -1)
    {
        printf(2, "ERROR: Sort system calls failed!\n");
        exit();
    }

    exit();
}