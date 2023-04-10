#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#define WR 1
#define RD 0

// 关键就在于一定要关闭两个管道的4个端口并且关闭管道的时机要准确

void primes(int lpipe[2]){
    close(lpipe[WR]);
    int num;
    if (read(lpipe[RD], &num, sizeof(num))){
        printf("prime %d\n", num);
        int rpipe[2];
        pipe(rpipe);
        int temp;
        while (read(lpipe[RD], &temp, sizeof(temp))){
            if (temp % num){
                write(rpipe[WR], &temp, 
                sizeof(temp));
            }
        }
        close(lpipe[RD]);
        close(rpipe[WR]);
        int pid = fork();
        if (pid == 0){
            primes(rpipe);
        }else if (pid > 0){
            close(rpipe[RD]);
        }else {
            exit(1);
        }
    }
    exit(0);
}
int main(int argc, char *argv[]){
    int p[2];
    pipe(p);
    for (int i = 2; i <= 35; ++i){
        write(p[WR], &i, sizeof(i));
    }
    int pid = fork();
    if (pid < 0){
        exit(1);
    }else if (pid == 0){
        primes(p);
    }else {
        close(p[RD]);
        close(p[WR]);
        wait(0);
    }
    exit(0);
}