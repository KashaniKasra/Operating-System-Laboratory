#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_Of_PROCESSES 5

int main()
{
    int pid;

    for(int i = 0; i < NUM_Of_PROCESSES; i++) {
        pid = fork();

        if(pid == 0)
        {
            int x = 0;
            
            while(x < 123456789) {
                x ++;
                printf(1, "");
            }

            exit();
        }
        else if(pid == 6)
        {
            // set_initial_burst_confidence(pid, 15, 70);

            change_queue(pid, 0);
        }
    }

    print_process_information();

    for(int i = 0; i < NUM_Of_PROCESSES; i++)
        wait();

    print_process_information();

    exit();
}