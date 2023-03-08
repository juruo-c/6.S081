#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

#define LINE 1024

int read_line(int fd, char* buf, int len) {
    char* p = buf;
    int i = 0;

    while (i < len - 1) {
        int res = read(fd, p, 1);
        if (res == -1) return -1;
        else if (res == 0)
            break;

        p ++;
        i ++;
        if (*(p - 1) == '\n') break;
    }
    *p = 0;
    return i;
}

int main(int argc, char* argv[])
{
    if (argc == 1) {
        fprintf(2, "usage: xargs command [args]\n");
        exit(1);
    }

    char* args[MAXARG + 1];
    memset(args, 0, sizeof args);
    int args_num = argc - 1;
    int i, j, k;

    for (i = 1; i < argc; i ++ )
        args[i - 1] = argv[i];        

    char buf[LINE];
    int res;

    while ( (res = read_line(0, buf, LINE)) != 0 ) {
        if (res == -1) {
            fprintf(2, "xargs: cannot read line from standard input\n");
            exit(1);
        }

        if (buf[res - 1] != '\n' && res == LINE) {
            fprintf(2, "xargs: line too long\n");
            exit(1);
        }

        // split the args
        buf[-- res] = '\0';
        i = 0, k = args_num;
        while (i < res) {
            if (buf[i] == ' ') i ++;
            else {
                j = i;
                while (j < res && buf[j] != ' ')
                    j ++;

                // buf[i ~ j - 1] is an argument
                if (k == MAXARG) {
                    fprintf(2, "xargs: too many arguments\n");
                }
                args[k] = (char*)malloc(sizeof(char) * (j - i + 1));
                memcpy(args[k], buf + i, j - i);
                args[k][j - i + 1] = '\0';
                k ++;

                i = j;
            }
        }

        if (fork() == 0) {
            exec(args[0], args);
            fprintf(2, "xargs: command %s not found\n", args[0]);
        }

        wait(0);
        // free the malloc space
        for (i = args_num; i < k; i ++ ) 
            free(args[i]), args[i] = 0;
    }

    exit(0);
}
