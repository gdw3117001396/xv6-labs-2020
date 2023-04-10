#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find(char *path, const char *filename){
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if((fd = open(path, 0)) < 0){
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if(fstat(fd, &st) < 0){
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    // 进入find的必须是目录，不是目录不会进入find,直接就是文件printf了
    if (st.type != T_DIR){
        fprintf(2, "Usage : find <dir> <file> %s\n", path);
        return;
    }

    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
        fprintf(2, "find: path too long\n");
        return;
    }
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';     // p指向最后一个'/'后面那个
    while (read(fd, &de, sizeof(de)) == sizeof(de)){
        if (de.inum == 0){
            continue;
        }
        memmove(p, de.name, DIRSIZ); //添加路径名称
        p[DIRSIZ] = 0; // 添加字符串'/0'结束标志
        if (stat(buf, &st) < 0){
            fprintf(2, "find: cannot stat %s\n", buf);
            continue;
        }
        if (st.type == T_DIR && strcmp(".", p) != 0 && strcmp("..", p) != 0){
            find(buf, filename);
        }else if (strcmp(filename, p) == 0){
            printf("%s\n", buf);
        }
    }
}
int main(int argc, char *argv[]){
    if (argc != 3){
        fprintf(2, "Usage : find <dir> <file>\n");
        exit(1);
    }
    find(argv[1], argv[2]);
    exit(0);
}