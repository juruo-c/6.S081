#include "kernel/types.h"
#include "user/user.h"

#define RD 0
#define WR 1
const uint INT_SIZE = sizeof(int);

void getprime(int left_p[]) {
    // check if there has a prime
    int prime, i;
    if (read(left_p[RD], &prime, INT_SIZE) == 0) {
        close(left_p[RD]);
        exit(0);
    }

    fprintf(1, "prime %d\n", prime);
    
    // create the pipe to the right neighbor
    int right_p[2];
    pipe(right_p);

    // read the data from left neighbor
    while (read(left_p[RD], &i, INT_SIZE) != 0) {
        if (i % prime != 0) 
            write(right_p[WR], &i, INT_SIZE);
    }
    close(left_p[RD]);
    close(right_p[WR]);

    if (fork() == 0) {
        getprime(right_p);
    } else {
        close(right_p[RD]);
        wait(0);
    }

}

int main(int argc, char* argv[]) 
{
    if (argc != 1) { 
        fprintf(1, "usage: primes\n");
        exit(1);
    }

    int p[2], i;
    pipe(p);

    for (i = 2; i <= 35; i ++ ) {
        write(p[WR], &i, INT_SIZE);
    }
    close(p[WR]);

    if (fork() == 0) {
        getprime(p);
    } else {
        close(p[RD]);
        wait(0);
    }

    exit(0);
}