#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include "lib.h"

static char SHM_FILEPATH[] = "/coding/example/shm_test_file";
static char SHM_INIT_CONTENT[] = "hello,world";

void write_file(char *expect_str)
{
    FILE *fp = fopen(SHM_FILEPATH, "w");
    if (fp == NULL) {
        perror("fopen fail");
        exit(EXIT_FAILURE);
    }

    if (fputs(expect_str, fp) == EOF) {
        perror("fputs fail");
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    fclose(fp);
    return 0;
}

void read_file(char *expect_str)
{
    FILE *fp = fopen(SHM_FILEPATH, "r");
    if (fp == NULL) {
        perror("fopen fail");
        exit(EXIT_FAILURE);
    }

    #define MAX_LINE_LEN 1024
    char buffer[MAX_LINE_LEN];
    // fgets：读取一行到buffer，最多读MAX_LINE_LEN-1个字符（留1个存'\0'）
    while (fgets(buffer, MAX_LINE_LEN, fp) != NULL) {
        printf("%s", buffer);  // 输出整行（fgets会保留换行符）
        if (expect_str != NULL && strncmp(expect_str, buffer, strlen(expect_str) != 0)) {
            printf("unexpected file content\n");
            exit(EXIT_FAILURE);
        }
    }
    printf("\n");

    fclose(fp);
    return 0;
}

/**
 * case1：共享文件映射，虚拟内存修改也会修改磁盘文件本身
 */
void mmap_shm_case1()
{
    // 文件内容初始化
    write_file(SHM_INIT_CONTENT);

    int fd = open(SHM_FILEPATH, O_RDWR);
    if (fd == -1) {  perror("open:"); return; }

    struct stat st = {};
    if (fstat(fd, &st) == -1) { perror("fstat:"); return; }

    // 基于文件fd，按MAP_SHARED创建虚拟内存映射
    char *addr = mmap(NULL, st.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap:");
        return;
    }

    if (close(fd) == -1) { perror("close:"); return; }

    printf("before modify, size=%d, addr is %s\n", st.st_size, addr);

    static char str[] = "HELLO";
    memcpy(addr, str, strlen(str));
    printf("after modify, size=%d, addr is %s\n", st.st_size, addr);

    printf("read file is:\n");
    read_file("HELLO,world");

    // 恢复文件内容
    write_file(SHM_INIT_CONTENT);

    PRINT_SUCCESSFUL;
}

/**
 * case2：共享匿名映射，经fork后，子进程能继承父进程的映射关系进行实现父子进程共享匿名内存
 */
void mmap_shm_case2()
{
    char *addr = mmap(NULL, SHM_SIZE, PROT_READ|PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (addr == MAP_FAILED) {
        perror("mmap:");
        return;
    }
    memcpy(addr, SHM_INIT_CONTENT, strlen(SHM_INIT_CONTENT) + 1);

    printf("init, addr is %s\n", addr);

    if (fork() == 0) {
        // 子进程能继承和修改来自父进程的共享内存
        printf("child pid=%d before modify, addr is %s\n", getpid(), addr);
        if (strcmp(addr, "hello,world") != 0) {
            exit(EXIT_FAILURE);
        }

        static char str[] = "HELLO";
        memcpy(addr, str, strlen(str));
        printf("child pid=%d after modify, addr is %s\n", getpid(), addr);
        
        // 子进程终止
        exit(EXIT_SUCCESS);
    }

    // 父进程读取子进程的修改
    wait(NULL);
    sleep(3);
    printf("parent pid=%d, addr is %s\n", getpid(), addr);
    if (strcmp(addr, "HELLO,world") != 0) {
        exit(EXIT_FAILURE);
    }

    PRINT_SUCCESSFUL;
}

/**
 * case3：共享匿名映射（/dev/zero），经fork后，子进程能继承父进程的映射关系进行实现父子进程共享匿名内存
 */
void mmap_shm_case3()
{
    int fd = open("/dev/zero", O_RDWR);
    if (fd == -1) { perror("open"); return; }

    char *addr = mmap(NULL, SHM_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap:");
        return;
    }
    
    if (close(fd) == -1) { perror("close"); return; };

    memcpy(addr, SHM_INIT_CONTENT, strlen(SHM_INIT_CONTENT) + 1);

    printf("init, addr is %s\n", addr);

    if (fork() == 0) {
        // 子进程能继承和修改来自父进程的共享内存
        printf("child pid=%d before modify, addr is %s\n", getpid(), addr);
        if (strcmp(addr, "hello,world") != 0) {
            exit(EXIT_FAILURE);
        }

        static char str[] = "HELLO";
        memcpy(addr, str, strlen(str));
        printf("child pid=%d after modify, addr is %s\n", getpid(), addr);
        
        // 子进程终止
        exit(EXIT_SUCCESS);
    }

    // 父进程读取子进程的修改
    wait(NULL);
    sleep(3);
    printf("parent pid=%d, addr is %s\n", getpid(), addr);
    if (strcmp(addr, "HELLO,world") != 0) {
        exit(EXIT_FAILURE);
    }

    PRINT_SUCCESSFUL;
}

/**
 * case4：私有文件映射，虚拟内存修改采用COW写时拷贝，不会将修改同步给文件
 */
void mmap_shm_case4()
{
    // 文件内容初始化
    write_file(SHM_INIT_CONTENT);

    int fd = open(SHM_FILEPATH, O_RDWR);
    if (fd == -1) {  perror("open:"); return; }

    struct stat st = {};
    if (fstat(fd, &st) == -1) { perror("fstat:"); return; }

    // 基于文件fd，按MAP_SHARED创建虚拟内存映射
    char *addr = mmap(NULL, st.st_size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap:");
        return;
    }
    if (close(fd) == -1) { perror("close"); return; };
    printf("before modify, size=%d, addr is %s\n", st.st_size, addr);

    static char str[] = "HELLO";
    memcpy(addr, str, strlen(str));
    printf("after modify, size=%d, addr is %s\n", st.st_size, addr);

    printf("read file is:\n");
    read_file("hello,world");

    // 恢复文件内容
    write_file(SHM_INIT_CONTENT);

    PRINT_SUCCESSFUL;
}

/**
 * case5：私有匿名映射，经fork后，子进程能继承父进程的映射关系，子进程修改后采用COW与父进程内存隔离
 */
void mmap_shm_case5()
{
    char *addr = mmap(NULL, SHM_SIZE, PROT_READ|PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (addr == MAP_FAILED) {
        perror("mmap:");
        return;
    }
    memcpy(addr, SHM_INIT_CONTENT, strlen(SHM_INIT_CONTENT) + 1);

    printf("init, addr is %s\n", addr);

    if (fork() == 0) {
        // 子进程能继承和修改来自父进程的共享内存
        printf("child pid=%d before modify, addr is %s\n", getpid(), addr);
        if (strcmp(addr, "hello,world") != 0) {
            exit(EXIT_FAILURE);
        }

        static char str[] = "HELLO";
        memcpy(addr, str, strlen(str));
        printf("child pid=%d after modify, addr is %s\n", getpid(), addr);
        
        // 子进程终止
        exit(EXIT_SUCCESS);
    }

    // 父进程读取子进程的修改
    wait(NULL);
    sleep(3);
    printf("parent pid=%d, addr is %s\n", getpid(), addr);
    if (strcmp(addr, "hello,world") != 0) {
        exit(EXIT_FAILURE);
    }

    PRINT_SUCCESSFUL;
}

/**
 * case6：私有匿名映射(/dev/zero)，经fork后，子进程能继承父进程的映射关系，子进程修改后采用COW与父进程内存隔离
 */
void mmap_shm_case6()
{
    int fd = open("/dev/zero", O_RDWR);
    if (fd == -1) { perror("open"); return; }

    char *addr = mmap(NULL, SHM_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap:");
        return;
    }
    if (close(fd) == -1) { perror("close"); return; };

    memcpy(addr, SHM_INIT_CONTENT, strlen(SHM_INIT_CONTENT) + 1);

    printf("init, addr is %s\n", addr);

    if (fork() == 0) {
        // 子进程能继承和修改来自父进程的共享内存
        printf("child pid=%d before modify, addr is %s\n", getpid(), addr);
        if (strcmp(addr, "hello,world") != 0) {
            exit(EXIT_FAILURE);
        }

        static char str[] = "HELLO";
        memcpy(addr, str, strlen(str));
        printf("child pid=%d after modify, addr is %s\n", getpid(), addr);
        
        // 子进程终止
        exit(EXIT_SUCCESS);
    }

    // 父进程读取子进程的修改
    wait(NULL);
    sleep(3);
    printf("parent pid=%d, addr is %s\n", getpid(), addr);
    if (strcmp(addr, "hello,world") != 0) {
        exit(EXIT_FAILURE);
    }

    PRINT_SUCCESSFUL;
}

/**
 * case7：不同进程通过同一个文件路径进行共享文件映射，修改能在进程间共享
 */
void mmap_shm_case7()
{
    // 文件内容初始化
    write_file(SHM_INIT_CONTENT);

    for (int i = 0; i < 3; i++) {
        if (fork() != 0) {
            continue;
        }
        int fd = open(SHM_FILEPATH, O_RDWR);
        if (fd == -1) {  perror("open:"); return; }

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
    
    printf("parent read file is:\n");
    read_file(NULL);

    // 恢复文件内容
    write_file(SHM_INIT_CONTENT);

    PRINT_SUCCESSFUL;
}

/**
 * case8：/dev/zero搭配MAP_SHARED无法实现进程共享，因为/dev/zero 是虚拟设备无存储，修改不会回写
 */
void mmap_shm_case8()
{
    for (int i = 0; i < 3; i++) {
        if (fork() != 0) {
            continue;
        }
        int fd = open("/dev/zero", O_RDWR);
        if (fd == -1) {  perror("open:"); return; }

        char *addr = mmap(NULL, SHM_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
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
    mmap_shm_case1();
    mmap_shm_case2();
    mmap_shm_case3();
    mmap_shm_case4();
    mmap_shm_case5();
    mmap_shm_case6();
    mmap_shm_case7();
    mmap_shm_case8();
}