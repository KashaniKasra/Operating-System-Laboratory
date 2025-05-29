#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char *argv[])
{
    unlink("result.txt");
    int fd = open("result.txt", O_CREATE | O_RDWR);
    int key = (90 + 14 + 57) % 26;

    if(fd < 0){
        printf(2, "strdiff: cannot open/create result.txt\n");
        exit();
    }

    for (int i = 1; i < argc; i++){
        for (int j = 0; j < strlen(argv[i]); j++){
            if(((int)argv[i][j]) > 64 && (int)argv[i][j] < 91)
                argv[i][j] = (char)(((int)argv[i][j] - 65 + key) % 26) + 65;
            else if(((int)argv[i][j]) > 96 && (int)argv[i][j] < 123)
                argv[i][j] = (char)(((int)argv[i][j] - 97 + key) % 26) + 97;
        }
        write(fd, argv[i], strlen(argv[i]));
        write(fd, " ", 1);
    }
    
    write(fd, "\n", 1);
    close(fd);
    exit();
}