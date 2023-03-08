#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find_file(char* path, const char* filename) {
    struct dirent de;
    struct stat st;
    int fd;
    char buf[512], *p;

    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        exit(1);
    }

    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        exit(1);
    }

    if (st.type == T_FILE) {
        fprintf(2, "usgae: find <directory> <filename>\n");
        close(fd);
        exit(1);
    }
    
    if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
        fprintf(2, "find: path too long\n");
        close(fd);
        exit(1);
    }

    strcpy(buf, path);
    p = buf + strlen(buf);
    *p ++ = '/';

    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0) continue;
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;

        if (stat(buf, &st) < 0) {
            fprintf(2, "find: cannot stat %s\n", buf);
            continue;
        }

        if (st.type == T_FILE) {
            if (strcmp(p, filename) == 0) 
                fprintf(1, "%s\n", buf);
        } else if (st.type == T_DIR && strcmp(p, ".") != 0 && strcmp(p, "..") != 0) {
            find_file(buf, filename);
        }

    }
    close(fd);
}

int main(int argc, char* argv[]) 
{

    if (argc != 3) {
        fprintf(2, "usgae: find <directory> <filename>\n");
        exit(1);
    }

    find_file(argv[1], argv[2]);

    exit(0);
}