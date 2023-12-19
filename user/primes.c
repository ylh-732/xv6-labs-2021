#include "kernel/types.h"
#include "user/user.h"

void sieve_prime(int *left) {
    close(left[1]);

    int prime;

    if (read(left[0], &prime, sizeof(int)) == 0) {
        close(left[0]);
        exit(0);
    }

    printf("prime %d\n", prime);

    int right[2];
    pipe(right);

    if (fork() == 0) {
        sieve_prime(right);
    } else {
        int num;

        while (read(left[0], &num, sizeof(int))) {
            if (num % prime != 0) {
                write(right[1], &num, sizeof(int));
            }
        }

        close(left[0]);
        close(right[0]);
        close(right[1]);

        wait(0);
    }
}

int main()
{
    int right[2];
    pipe(right);

    if (fork() == 0) {
        sieve_prime(right);
    } else {
        for (int i = 2; i <= 35; i++) {
            write(right[1], &i, sizeof(int));
        }

        close(right[0]);
        close(right[1]);

        wait(0);
    }

    exit(0);
}