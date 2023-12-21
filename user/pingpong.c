#include "kernel/types.h"
#include "user/user.h"

int main()
{
    char buf_parent[512];
    char buf_child[512];
    buf_parent[0] = 'a';
    
    int p_parent[2];
    int p_child[2];
    pipe(p_parent);
    pipe(p_child);
    
    if (fork() == 0) {
        close(p_parent[1]);
        close(p_child[0]);

        read(p_parent[0], buf_child, 1);
        printf("%d: received ping\n", getpid());
        write(p_child[1], buf_child, 1);

        close(p_parent[0]);
        close(p_child[1]);
    } else {
        close(p_child[1]);
        close(p_parent[0]);

        write(p_parent[1], buf_parent, 1);
        read(p_child[0], buf_parent, 1);
        printf("%d: received pong\n", getpid());

        close(p_parent[1]);
        close(p_child[0]);
    }

    exit(0);
}