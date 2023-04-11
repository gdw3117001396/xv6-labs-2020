#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"
#define MAXSIZE 1024

int main(int argc, char *argv[]){
    sleep(1);
    char buf[MAXSIZE];
    // 0表示从标准输入里面获取 | 前面命令的结果
    read(0, &buf, sizeof(buf));
    char *xargv[MAXARG] = {0};
    int xargc = 0;
    for (int i = 1; i < argc; ++i){
        xargv[xargc++] = argv[i];
    }    
    char *p = buf;
    int count = argc - 1;
    for (int i = 0; i < MAXSIZE; ++i){
        if (buf[i] == '\n'){
            int pid = fork();
            if (pid > 0){
                p = &buf[i + 1];
                wait(0);
            }else if (pid == 0){
                buf[i] = 0; // 等价于 \0表示字符结尾
                xargv[count] = p;
                exec(xargv[0], xargv);
                exit(0);
            }else{
                exit(1);
            }
        }
    }
    exit(0);
}