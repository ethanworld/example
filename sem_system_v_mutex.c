#include <stdio.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/unistd.h>
#include <sys/types.h>
#include "lib.h"

/**
 * 创建或获取semid
 */
int create_mtx_lock() {
    char *pathname = "/home/ubuntu";
    int proj_id = 12345;
    key_t key = ftok(pathname, proj_id);

    int semid =  semget(key, 1, IPC_CREAT| S_IRUSR | S_IWUSR);
    // 初始为1
    semctl(semid, 0, SETVAL, 1);
    return semid;
}

/**
 * 获取锁，P操作对信号量减1，可能会阻塞
 */
int mtx_lock(int semid) {
    struct sembuf sem_op = {
        .sem_num = 0,   // 操作第0个信号量
        .sem_op = -1,   // P操作：减1
        .sem_flg = 0    // 阻塞等待（不设置 IPC_NOWAIT）
    };
    return semop(semid, &sem_op, 1);
}

/**
 * 释放锁，V操作对信号量加1，不会阻塞
 */
int mtx_unlock(int semid) {
    struct sembuf sem_op = {
        .sem_num = 0,   // 操作第0个信号量
        .sem_op = 1,   //  V操作：加1
        .sem_flg = 0    // 不会阻塞
    };
    return semop(semid, &sem_op, 1);
}

/**
 * 删除锁，释放信号量集合资源
 */
int release_mtx_lock(int semid) {
    return semctl(semid, 0, IPC_RMID);
}

int main() {
    int semid = create_mtx_lock();

    for (int i = 0; i < 3; i++) {
        if (fork() != 0) {
            continue;
        }
        printf("i=%d, pid=%d try to lock\n", i, getpid());
        // 加锁
        mtx_lock(semid);
        
        // 执行临界区代码
        sleep(1);
        printf("i=%d, pid=%d do something\n", i, getpid());

        // 释放锁
        printf("i=%d, pid=%d unlock\n", i, getpid());
        mtx_unlock(semid);

        exit(EXIT_SUCCESS);
    }
    
    while (wait(NULL) != -1) {}
    
    release_mtx_lock(semid);
}