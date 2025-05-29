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
    
    int result = list_all_processes();

    if(result == -1)
    {
        printf(2, "ERROR: Sort system calls failed!\n");
        exit();
    }

    exit();
}