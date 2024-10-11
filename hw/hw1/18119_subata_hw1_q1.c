#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main (void) {
    int p_ptc[2], p_ctp[2];     // ptc = parent-to-child, ctp = child-to-parent
    char byte[1];               // char datatype has size of 1 byte
    int n = 3;                  // no. of times byte gets sent from each side
    int i;

    pipe(p_ptc);                // child will read from p_ptc[0], parent will write to p_ptc[1]
    pipe(p_ctp);                // parent will read from p_ctp[0], child will write to p_ctp[1]

    if (fork() == 0)  {         // child
        close(p_ptc[1]);        // won't write to ptc pipe
        close (p_ctp[0]);       // won't read from ctp pipe

        for (i = 0; i < n; i++)
        {
            read(p_ptc[0], byte, 1);
            printf("child receives: %c\n", byte[0]);

            byte[0] = 'c';
            printf("child sends: %c\n", byte[0]);
            write(p_ctp[1], byte, 1);
        }
        
        close(p_ctp[1]);
        close(p_ptc[0]);

        sleep(1);               // making sure everything is finished printing before exiting
        exit(0);
    } else {                    // parent
        close(p_ptc[0]);        // won't read from ptc pipe
        close(p_ctp[1]);        // won't write to ctp pipe

        for (i = 0; i < n; i++)
        {
            byte[0] = 'p';
            printf("parent sends: %c\n", byte[0]);
            write(p_ptc[1], byte, 1);       // byte sent to child

            read(p_ctp[0], byte, 1);
            printf("parent receives: %c\n", byte[0]);
        }

        close(p_ptc[1]);        // closing remaining pipe ends
        close(p_ctp[0]);

        wait(0);                // waiting for child
    }

    exit(0);
}
