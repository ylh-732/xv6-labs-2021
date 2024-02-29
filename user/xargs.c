#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(2, "Usage: xargs...\n");
        exit(1);
    }

    char* xargs_argv[argc];
    char buf[512];

    for (int i = 1; i < argc; i++) {
        xargs_argv[i - 1] = argv[i];
    }

    while (1) {
        gets(buf, 512);

        if (buf[0] == '\0') {
            break;
        }
        
        buf[strlen(buf) - 1] = '\0';
        xargs_argv[argc - 1] = buf;

        if (fork() == 0) {
            exec(argv[1], xargs_argv);
        } else {
            wait(0);
        }
    }

    exit(0);
}