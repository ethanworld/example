#include <stdio.h>
#include <stdlib.h>
#include <mqueue.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/syscall.h>
#include "lib.h"

static char posix_msq_name[] = "/msq_demo";

/**
 * 消息队列实现基本进程间通信，mq_open、mq_send、mq_receive以及mq_getattr基本用法
 */
void msq_posix_case1()
{
    struct mq_attr attr;
    attr.mq_maxmsg = 10; // 定义了使用mq_send()向消息队列添加消息的数量上限
    attr.mq_msgsize = 1024; // 定义了加入消息队列的每条消息的大小的上限
    /**
     * mq_maxmsg 和 mq_msgsize 特性是在消息队列被创建时就确定下来的，并且之后也无法修改这两个特性
     * 内核根据这两个值来确定消息队列所需的最大内存量。
     */
    mqd_t mqd = mq_open(posix_msq_name, O_WRONLY | O_RDONLY | O_CREAT | O_EXCL, 0666, &attr);
    for (int i = 0; i < 2; i++) {
        pid_t pid = fork();
        if (pid != 0) {
            continue;
        }

        if (i == 0) {
            mqd_t mqd = mq_open(posix_msq_name, O_WRONLY, 0666, NULL);
            static char content[] = "hello,world";
            printf("i=%d, pid=%d, mqd=%d, send msg:%s\n", i, getpid(), mqd, content);
            /**
             * msg_len 参数指定了 msg_ptr 指向的消息的长度，其值必须小于或等于队列的 mq_msgsize特性，
             * 否则 mq_send()就会返回 EMSGSIZE 错误。长度为零的消息是允许的。
             */
            if (mq_send(mqd, content, strlen(content) + 1, 0) == -1 ) perror_exit("mq_send");
        }
        
        if (i == 1) {
            mqd_t mqd = mq_open(posix_msq_name, O_RDONLY, 0666, NULL);
            struct mq_attr attr;
            if (mq_getattr(mqd, &attr) == -1) perror_exit("mq_getattr");

            // 按队列的最大消息长度分配缓冲区
            char *msg_buf = malloc(attr.mq_msgsize);
            if (msg_buf == NULL) {
                perror_exit("malloc");
            }

            int prio = 0;
            /**
             * 不管消息的实际大小是什么，msg_len（即 msg_ptr 指向的缓冲区的大小）必须要大于或等于队列的 mq_msgsize 特性，
             * 否则 mq_receive()就会失败并返回 EMSGSIZE 错误
             */
            if (mq_receive(mqd, msg_buf, attr.mq_msgsize, &prio) == -1 ) perror_exit("mq_receive");
            printf("i=%d, pid=%d, msgsize=%d, recv msg:%s\n", i, getpid(), attr.mq_msgsize, msg_buf);
        }
        
        if (mq_close(mqd) == -1) perror_exit("mq_close");
        exit(EXIT_SUCCESS);
    }

    while (wait(NULL) != -1) {}

    // 关闭该进程打开的消息队列
    if (mq_close(mqd) == -1) perror_exit("mq_close");
    
    // 删除一个消息队列名并当所有进程关闭该队列时对队列进行标记以便删除
    if (mq_unlink(posix_msq_name) == -1) perror_exit("mq_unlink");

    PRINT_SUCCESSFUL;
}

struct sigevent sev = {0};
int stop_flag = 0;

// 信号处理函数
void handle_signal(int sig) {
    mqd_t mqd = mq_open(posix_msq_name, O_RDONLY);
    char buf[256];
    unsigned int prio;
    ssize_t len = mq_receive(mqd, buf, 256, &prio);
    if (len != -1) {
        buf[len] = '\0';
        printf("pid=%d, mqd=%d, recv msg:%s\n", getpid(), mqd, buf);
        stop_flag = 1;
    } else {
        perror_exit("mq_receive");
    }

    // 通知只触发一次：收到通知后，需要重新调用 mq_notify 再次注册。
    mq_notify(mqd, &sev);
    if (mq_close(mqd) == -1) perror_exit("mq_close");
}

/**
 * case2: 通过信号接收异步消息通知，无需主线程阻塞在mq_receive
 */
void msq_posix_case2()
{
    struct mq_attr attr;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = 256; 
    mqd_t mqd = mq_open(posix_msq_name, O_WRONLY | O_RDONLY | O_CREAT | O_EXCL, 0666, &attr);

    // 设置信号通知
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGUSR1;
    // 注册信号处理函数
    signal(SIGUSR1, handle_signal);

    // 为该进程注册消息通知
    mq_notify(mqd, &sev);

    if (fork() == 0) {
        sleep(3);
        static char content[] = "hello,world";
        printf("pid=%d, mqd=%d, send msg:%s\n", getpid(), mqd, content);
        if (mq_send(mqd, content, strlen(content) + 1, 0) == -1 ) perror_exit("mq_send");
        exit(EXIT_SUCCESS);
    }
    
    while (!stop_flag) { sleep(1); /* do something */ }

    if (mq_close(mqd) == -1) perror_exit("mq_close");
    if (mq_unlink(posix_msq_name) == -1) perror_exit("mq_unlink");
    PRINT_SUCCESSFUL;
}

// 线程回调函数
void thread_func(union sigval val) {
    mqd_t mqd = mq_open(posix_msq_name, O_RDONLY);
    char buf[256];
    unsigned int prio;
    ssize_t len = mq_receive(mqd, buf, 256, &prio);
    if (len != -1) {
        buf[len] = '\0';
        // 内核会创建新线程处理该消息，因此pid和tid会不一样
        printf("pid=%d, tid=%d, mqd=%d, recv msg:%s\n", getpid(), syscall(SYS_gettid), mqd, buf);
        stop_flag = 1;
    } else {
        perror_exit("mq_receive");
    }

    if (mq_close(mqd) == -1) perror_exit("mq_close");
}

/**
 * case3: 通过线程接收异步消息通知，无需主线程阻塞在mq_receive
 */
void msq_posix_case3()
{
    struct mq_attr attr;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = 256; 
    mqd_t mqd = mq_open(posix_msq_name, O_WRONLY | O_RDONLY | O_CREAT | O_EXCL, 0666, &attr);

    printf("pid=%d, tid=%d, create mqd=%d\n", getpid(), syscall(SYS_gettid), mqd);
    
    // 设置消息通知线程回调
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = thread_func;

    // 为该进程注册消息通知
    mq_notify(mqd, &sev);

    if (fork() == 0) {
        sleep(3);
        static char content[] = "hello,world";
        printf("pid=%d, tid=%d, mqd=%d, send msg:%s\n", getpid(), syscall(SYS_gettid), mqd, content);
        if (mq_send(mqd, content, strlen(content) + 1, 0) == -1 ) perror_exit("mq_send");
        exit(EXIT_SUCCESS);
    }
    
    while (!stop_flag) { sleep(1); /* do something */ }

    if (mq_close(mqd) == -1) perror_exit("mq_close");
    if (mq_unlink(posix_msq_name) == -1) perror_exit("mq_unlink");
    PRINT_SUCCESSFUL;
}

int main()
{
    msq_posix_case1();
    msq_posix_case2();
    msq_posix_case3();
}