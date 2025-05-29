#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[])
{
    if(argc != 2)
    {
        printf(2, "ERROR: Too many arguments!\n");
        exit();
    }

    int num = atoi(argv[1]);

    int result = create_palindrome(num);

    if(result == -1)
    {
        printf(2, "ERROR: Create palindrome failed!\n");
        exit();
    }
    
    exit();
}