#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char* argv[]) 
{
    if (argc != 1) { 
        fprintf(2, "usage: pingpong\n");
        exit(1);
    }

    int p_father_to_child[2];
    int p_child_to_father[2];
    if (pipe(p_father_to_child) == -1 || pipe(p_child_to_father) == -1) {
        fprintf(2, "failed to open pipe\n");
        exit(1);
    }

    char buf = 'C';
    int exit_state = 0;

    // child process
    if (fork() == 0) {
        close(p_father_to_child[1]);
        close(p_child_to_father[0]);

        if (read(p_father_to_child[0], &buf, 1) != 1) {
            fprintf(2, "failed to read pipe in child\n");
            exit_state = 1;
        }

        fprintf(1, "%d: received ping\n", getpid());

        if (write(p_child_to_father[1], &buf, 1) != 1) {
            fprintf(2, "failed to write pipe in child\n");
            exit_state = 1;
        }

        exit(exit_state);
    }

    // father process
    close(p_father_to_child[0]);
    close(p_child_to_father[1]);

    if (write(p_father_to_child[1], &buf, 1) != 1) {
        fprintf(2, "failed to write pipe in parent\n");
        exit_state = 1;
    }

    wait(0);
    
    if (read(p_child_to_father[0], &buf, 1) != 1) {
        fprintf(2, "failed to read pipe in parent\n");
        exit_state = 1;
    }

    fprintf(1, "%d: received pong\n", getpid());

    exit(exit_state);
}