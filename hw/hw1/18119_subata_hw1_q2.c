#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int main(int argc, char *argv[]) {
    int p[2];  
    
    if (pipe(p) < 0) {
        printf("pipe creation failed\n");
        exit(0);
    }
    
    int pid = fork();
    if (pid == 0) {                 // child 
        close(p[0]);                // won't read from pipe
        countsyscalls(p[1]);        // passes p[1] to kernel to write to
        exec(argv[1], argv + 1);    // execute the target program
        printf("exec failed\n");
        exit(0);
    } else {                        // parent: read from pipe to count syscalls
        close(p[1]);                // won't write to pipe
        
        uint64 buf[1];              // 
        int syscall_counter = 0;

        while (read(p[0], buf, sizeof(buf[0])) > 0) {       // while pipe is being written to, increment counter
            syscall_counter++;
        }
        printf("number of system calls made: %d\n", syscall_counter);     // print counter
        close(p[0]);
        wait(0);
    }
    
    exit(0);
}
