#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include "lib.h"

/**
 * case1: 消息队列实现基本进程间通信，msgrcv在不同进程会产生竞争
 */
void msq_system_v_case1()
{
    char *pathname = "/home/ubuntu";
    int proj_id = 12345;
    key_t key = ftok(pathname, proj_id);

    for (int i = 0; i < 3; i++) {
        if (fork() != 0) {
            continue;
        }
        int msgid = msgget(key, IPC_CREAT | 0666);
        if (msgid == -1) perror_exit("msgget");
        struct msgbuf buf = {0};

        if (i == 0) {
            strcpy(buf.mtext, "hello,world");
            buf.mtype = 1;
            if (msgsnd(msgid, &buf, MSG_SIZE, 0) == -1) perror_exit("msgsnd");
            printf("i=%d, pid=%d, msgid=%d send msg\n", i, getpid(), msgid);
        }
        
        if (i == 1) {
            sleep(1);
            // msg只会被消费一次
            ssize_t len = msgrcv(msgid, &buf, MSG_SIZE, 1, 0);
            if (len == -1) perror_exit("msgrcv");
            printf("i=%d, pid=%d, msgid=%d send recv: %s\n", i, getpid(), msgid, buf.mtext);

            strcpy(buf.mtext, "HELLO,world");
            buf.mtype = 1;
            // msg消费后其他进程msgrcv会阻塞，此处继续发送
            if (msgsnd(msgid, &buf, strlen(buf.mtext), 0) == -1) perror_exit("msgsnd");
            printf("i=%d, pid=%d, msgid=%d send msg\n", i, getpid(), msgid);
        }

        if (i == 2) {
            sleep(2);
            ssize_t len = msgrcv(msgid, &buf, MSG_SIZE, 1, 0);
            if (len == -1) perror_exit("msgrcv");
            printf("i=%d, pid=%d, msgid=%d recv: %s\n", i, getpid(), msgid, buf.mtext);

            // 最后删除队列
            if (msgctl(msgid, IPC_RMID, NULL) == -1) perror_exit("msgctl");
        }

        exit(EXIT_SUCCESS);
    }

    while (wait(NULL) != -1) {}
    PRINT_SUCCESSFUL;
}

/**
 * case2: 基于不同msgtype向同一个队列发送和接受消息，不同msgtype不会相互阻塞
 */
void msq_system_v_case2()
{
    char *pathname = "/home/ubuntu";
    int proj_id = 12345;
    key_t key = ftok(pathname, proj_id);
    
    int msgid = msgget(key, IPC_CREAT | IPC_EXCL | 0666);
    if (msgid == -1) perror_exit("msgget");
    struct msgbuf buf = {0};

    for (int i = 0; i < 3; i++) {
        pid_t pid = fork();
        if (pid != 0) {
            // 父进程按子进程pid向队列发送消息
            sprintf(buf.mtext, "pid=%d, hello world", pid);
            buf.mtype = pid;
            if (msgsnd(msgid, &buf, MSG_SIZE, 0) == -1) perror_exit("msgsnd");
            printf("i=%d, msgid=%d send msg to pid=%d\n", i, msgid, pid);
            continue;
        }

        // 子进程按自身pid从队列读消息
        sleep(1);
        ssize_t len = msgrcv(msgid, &buf, MSG_SIZE, getpid(), 0);
        if (len == -1) perror_exit("msgrcv");
        printf("i=%d, pid=%d, msgid=%d recv: %s\n", i, getpid(), msgid, buf.mtext);

        exit(EXIT_SUCCESS);
    }

    while (wait(NULL) != -1) {}
    
    // 最后删除队列
    if (msgctl(msgid, IPC_RMID, NULL) == -1) perror_exit("msgctl");
    PRINT_SUCCESSFUL;
}

int main()
{
    msq_system_v_case1();
    msq_system_v_case2();
}