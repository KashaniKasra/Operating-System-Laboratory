#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[])
{
    if(argc != 3)
    {
        printf(2, "ERROR: Too many arguments!\n");
        exit();
    }

    int result = move_file(argv[1], argv[2]);

    if(result == -1)
    {
        printf(2, "ERROR: File already exists!\n");
        exit();
    }

    exit();
}