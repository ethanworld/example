#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include "lib.h"

static char posix_shm_name[] = "/shm_demo";
static char SHM_INIT_CONTENT[] = "hello,world";

/**
 * case1：不同进程通过shm_open创建共享内存对象并获取对应fd进行共享映射，修改能在进程间共享
 */
void posix_shm_case1()
{
    for (int i = 0; i < 3; i++) {
        if (fork() != 0) {
            continue;
        }
        int flag = O_RDWR;
        if (i == 0) {
            // shm_open 不会自动创建文件，必须显式指定 O_CREAT 才能新建
            flag |= O_CREAT;
        }
        int fd = shm_open(posix_shm_name, flag, 0);
        if (fd == -1) {  perror("shm_open:"); return; }

        if (i == 0) {
            if (ftruncate(fd, SHM_SIZE) == -1) {
                perror("ftruncate:"); return;
            }
            write(fd, SHM_INIT_CONTENT, strlen(SHM_INIT_CONTENT) + 1);
        }

        struct stat st = {};
        if (fstat(fd, &st) == -1) { perror("fstat:"); return; }

        char *addr = mmap(NULL, st.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        if (addr == MAP_FAILED) { perror("mmap:"); return;}
        if (close(fd) == -1) { perror("close:"); return; }

        sleep(i);
        printf("child i=%d, pid=%d, fd=%d, before modify addr is %s\n", i, fd, getpid(), addr);
        addr[0] = '0' + i;
        printf("child i=%d, pid=%d, fd=%d, after modify addr is %s\n", i, fd, getpid(), addr);

        // 子进程退出
        exit(EXIT_SUCCESS);
    }

    while (wait(NULL) != -1) {}
    
    PRINT_SUCCESSFUL;
}

int main(int argc, char *args[])
{
    posix_shm_case1();
}