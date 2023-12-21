#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

void append_line_args(int index, char* xargs_argv[]) {
    char buf[512];
    char c;

    int start = 0;
    int end = 0;
    
    xargs_argv[index] = 0; 

    while(read(0, &c, 1) == 1) {  
        if (c == ' ' || c == '\n') {
            buf[end++] = 0;
            xargs_argv[index++] = buf + start;
            start = end;

            if (c == '\n') {
                xargs_argv[index] = 0;
                printf("");
                break;
            }
        } else {
            buf[end++] = c;
        }    
    }
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(2, "Usage: xargs...\n");
        exit(1);
    }

    char* xargs_argv[MAXARG];

    for (int i = 1; i < argc; i++) {
        xargs_argv[i - 1] = argv[i];
    }

    while (1) {
        append_line_args(argc - 1, xargs_argv);

        if (xargs_argv[argc - 1] == 0) {
            break;
        }

        if (fork() == 0) {
            exec(argv[1], xargs_argv);
        } else {
            wait(0);
        }    
    }

    exit(0);
}