#include "types.h"
#include "user.h"

int main()
{
    int pid = getpid();

    printf(1, "The process id is: %d\n", pid);

    exit();
}