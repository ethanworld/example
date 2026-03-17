#include <sys/wait.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>   // O_CREAT/O_EXCL 定义
#include "lib.h"

/**
 * case1: 匿名管道，fd[0]固定是读端，fd[1]固定是写端
 */
void pipe_case1()
{
    int fds[2];
    if (pipe(fds) == -1) perror_exit("pipe");
    printf("create pipe, fd[0]=%d, fd[1]=%d\n", fds[0], fds[1]);

    if (fork() == 0) {
        // 关闭读端
        close(fds[0]);
        
        // 即便write比read晚3秒，read也会在空管道阻塞到至少一个字节被写入
        sleep(3);
        char buf[10] = {0};
        for (int i = 0; i < 3; i++) {
            buf[0] = 'a' + i;
            printf("pid=%d, write is %s\n", getpid(), buf);    
            // PIPEDES[1]写操作
            write(fds[1], buf, 10);
        }

        exit(EXIT_SUCCESS);
    }

    // 关闭写端
    close(fds[1]);

    char buf[10] = {0};
    /**
     * PIPEDES[0]读操作
     * 试图从一个当前为空的管道中读取数据将会被阻塞直到至少有一个字节被写入到管道中为止 
     */
    while (read(fds[0], buf, 10) != 0) {
        printf("pid=%d, read is %s\n", getpid(), buf);    
    }

    PRINT_SUCCESSFUL;
}

/**
 * case2: 匿名管道，将管道作为一种进程同步的方法
 * 利用空管道阻塞read + fd全部close后会返回文件结束接触阻塞的特性
 */
void pipe_case2()
{
    int fds[2];
    if (pipe(fds) == -1) perror_exit("pipe");
    printf("create pipe, fd[0]=%d, fd[1]=%d\n", fds[0], fds[1]);

    for (int i = 0; i < 3; i++) {
        if (fork() != 0) {
            continue;
        }
        // 关闭读端
        close(fds[0]);

        // 子进程完成工作后关闭写端
        printf("i=%d, pid=%d, child process do something firstly\n", i, getpid());
        sleep(i);
        close(fds[1]);
        exit(EXIT_SUCCESS);
    }

    // 关闭写端
    close(fds[1]);

    /**
     * 当所有子进程都关闭了管道的写入端的文件描述符之后，父进程在管道上的 read()就会结束并返回文件结束
     * 在此之前，因为是空管道，父进程会阻塞在read操作上
     */
    char buf[1] = {0};
    if (read(fds[0], buf, 1) != 0) {
        printf("pid=%d, did not get EOF\n", getpid());    
    }

    printf("pid=%d, parent process do something after all child ok\n", getpid());
    PRINT_SUCCESSFUL;
}

/**
 * case3: 匿名管道，利用dup2的fd复制重定向能力，将标准输入输出通过管道对接
 */
void pipe_case3()
{
    int fds[2];
    if (pipe(fds) == -1) perror_exit("pipe");
    printf("create pipe, fd[0]=%d, fd[1]=%d\n", fds[0], fds[1]);

    for (int i = 0; i < 2; i++) {
        if (fork() != 0) {
            continue;
        }

        if (i == 0) {
            // 关闭读端
            close(fds[0]);
            // 将管道写端重定向到标准输出
            if (dup2(fds[1], STDOUT_FILENO) == -1) perror_exit("dup2");
            execlp("ls", "ls", (char *)NULL);
        }

        if (i == 1) {
            // 关闭写端
            close(fds[1]);
            // 将管道读端重定向到标准输入
            if (dup2(fds[0], STDIN_FILENO) == -1) perror_exit("dup2");
            /**
             * execlp 会加载指定的可执行程序，替换当前进程的代码段、数据段和堆栈，原进程的代码会被完全覆盖，
             * 仅保留进程 ID、文件描述符等系统资源。
             */
            execlp("wc", "wc", "-l", (char *)NULL);
        }

        exit(EXIT_SUCCESS);
    }


    close(fds[0]);
    close(fds[1]);

    while (wait(NULL) != -1) {}
    PRINT_SUCCESSFUL;
}

/**
 * case4: 命名管道
 */
void pipe_case4()
{
    static char fifo_name[] = "/fifo_demo";

    // 创建FIFO
    mkfifo(fifo_name, 0666);
    
    for (int i = 0; i < 2; i++) {
        if (fork() != 0) {
            continue;
        }

        if (i == 0) {
            // 打开FIFO写端
            int fd = open(fifo_name, O_WRONLY);
            if (fd == -1) perror_exit("open");

            const char *msg = "Hello from FIFO!";
            write(fd, msg, strlen(msg) + 1);
            printf("i=%d, pid=%d, fd=%d, write FIFO, msg is %s\n", i, getpid(), fd, msg);
            // 关闭文件描述符
            close(fd);
        }

        if (i == 1) {
            // 打开FIFO读端
            int fd = open(fifo_name, O_RDONLY);
            if (fd == -1) perror_exit("open");

            // 读取数据
            char buf[100];
            read(fd, buf, sizeof(buf));
            printf("i=%d, pid=%d, fd=%d, read FIFO, msg is %s\n", i, getpid(), fd, buf);

            // 关闭文件描述符
            close(fd);
        }

        exit(EXIT_SUCCESS);
    }
    
    while (wait(NULL) != -1) {}
    // 删除FIFO
    unlink(fifo_name);
    PRINT_SUCCESSFUL;
}

int main()
{
    pipe_case1();
    pipe_case2();
    pipe_case3();
    pipe_case4();
}