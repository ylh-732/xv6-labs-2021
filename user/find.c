#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char* fmtname(char *path) {
    char *p;

    // Find first character after last slash.
    for(p = path + strlen(path); p >= path && *p != '/'; p--) {
        ;
    }

    return p + 1;
}

void find(char *curr_path, const char *target_file_name) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(curr_path, 0)) < 0) {
        fprintf(2, "ls: cannot open %s\n", curr_path);
        return;
    }
    
    if (fstat(fd, &st) < 0) {
        fprintf(2, "ls: cannot stat %s\n", curr_path);
        close(fd);
        return;
    }

    switch(st.type) {
        case T_FILE:
            if (strcmp(fmtname(curr_path), target_file_name) == 0) {
                printf("%s\n", curr_path);
            }
            break;

        case T_DIR:
            if (strlen(curr_path) + 1 + DIRSIZ + 1 > sizeof(buf)){
                printf("ls: path too long\n");
                break;
            }

            strcpy(buf, curr_path);
            p = buf + strlen(buf);
            *p++ = '/';

            while(read(fd, &de, sizeof(de)) == sizeof(de)){
                if (de.inum == 0) {
                    continue;
                }
                if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) {
                    continue;
                }
                memmove(p, de.name, DIRSIZ);
                p[DIRSIZ] = 0;
                if (stat(buf, &st) < 0) {
                    printf("ls: cannot stat %s\n", buf);
                    continue;
                }
                
                find(buf, target_file_name);
            }
            break;
    }

    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc != 3){
        fprintf(2, "Usage: find...\n");
        exit(1);
    }

    find(argv[1], argv[2]);

    exit(0);
}
