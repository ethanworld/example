#include <stdio.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/unistd.h>
#include <sys/types.h>
#include <time.h>   // 必须包含（struct timespec）
#include "lib.h"

/**
 * case1: 创建或获取有2个信号量的信号集，基于同一个key可以在不同进程对同一个semid进行操作
 */
void system_v_sem_case1()
{
    int semid;
    char *pathname = "/home/ubuntu";
    int proj_id = 12345;
    key_t key = ftok(pathname, proj_id);

    for (int i = 0; i < 2; i++) {
        if (fork() !=0 ) {
            continue;
        }
        if (i == 0) {
            // 新建有2个信号量的信号集，如果已存在则报错
            semid = semget(key, 2, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
            if (semid == -1) perror_exit("semget");
            printf("pid=%d, i=%d, create semaphore id=%d\n", getpid(), i, semid);

            if (semctl(semid, 0, SETVAL, 111) == -1) perror_exit("semctl SETVAL");
            if (semctl(semid, 1, SETVAL, 222) == -1) perror_exit("semctl SETVAL");
            
            printf("pid=%d, i=%d, val 0 is %d, val 1 is %d\n", getpid(), i ,semctl(semid, 0, GETVAL), semctl(semid, 1, GETVAL));
        }
        
        if (i == 1) {
            sleep(3);
            semid = semget(key, 2, S_IRUSR | S_IWUSR);
            if (semid == -1) perror_exit("semget");

            printf("pid=%d, i=%d, get exist semaphore id=%d\n", getpid(), i, semid);
            printf("pid=%d, i=%d, val 0 is %d, val 1 is %d\n", getpid(), i ,semctl(semid, 0, GETVAL), semctl(semid, 1, GETVAL));
            
            // 删除信号集
            if (semctl(semid, 0, IPC_RMID) == -1) perror_exit("semctl IPC_RMID");
        }

        exit(EXIT_SUCCESS);
    }

    while (wait(NULL) != -1) {}

    PRINT_SUCCESSFUL;
}

/**
 * case2: 如果sem_op等于0，那么就对信号量值进行检查以确定它当前是否等于 0
 * 如果等于0，那么操作将立即结束，否则 semop()就会阻塞直到信号量值变成 0 为止
 */
void system_v_sem_case2()
{
    char *pathname = "/home/ubuntu";
    int proj_id = 12345;
    key_t key = ftok(pathname, proj_id);
    int semid = semget(key, 1, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    // 新建有1个信号量的信号集，如果已存在则报错
    if (semid == -1) perror_exit("semget");

    // 信号量初始化为1
    if (semctl(semid, 0, SETVAL, 1) == -1) perror_exit("semctl SETVAL");

    for (int i = 0; i < 2; i++) {
        if (fork() !=0 ) {
            continue;
        }
        // 可以定义成数组，此处只涉及一个信号量
        struct sembuf sop = {};
        sop.sem_num = 0; // 信号量在信号集中的索引
        sop.sem_op = 1; // 对信号量的操作
        sop.sem_flg = 0;
        
        semid = semget(key, 1, S_IRUSR | S_IWUSR);

        if (i == 0) {
            /**
             * 如果sem_op等于 0，那么就对信号量值进行检查以确定它当前是否等于 0。
             * 如果等于0，那么操作将立即结束，否则 semop()就会阻塞直到信号量值变成 0 为止。
             */
            sop.sem_op = 0;
            // 由于信号量初始值为1，因此会阻塞到信号量为0为止
            if (semop(semid, &sop, 1) == -1) perror_exit("semop");
            printf("pid=%d, i=%d, val is %d\n", getpid(), i ,semctl(semid, 0, GETVAL));
        }

        if (i == 1) {
            // idx=1的进程虽然sleep 3s，但是会先于idx=0打印
            sleep(3);
            
            sop.sem_op = -1;
            // 由于信号量初始值为1，此处会立即执行
            if (semop(semid, &sop, 1) == -1) perror_exit("semop");
            printf("pid=%d, i=%d, val is %d\n", getpid(), i ,semctl(semid, 0, GETVAL));
        }
        exit(EXIT_SUCCESS);
    }

    while (wait(NULL) != -1) {}

    // 删除信号集
    if (semctl(semid, 0, IPC_RMID) == -1) perror_exit("semctl IPC_RMID");

    PRINT_SUCCESSFUL;
}

/**
 * case3: 如果sem_op小于 0，那么就将信号量值减去sem_op。
 * 如果信号量的当前值大于或等于sem_op的绝对值，那么操作会立即结束。
 * 否则 semop()会阻塞直到信号量值增长到在执行操作之后不会导致出现负值的情况为止
 */
void system_v_sem_case3()
{
    char *pathname = "/home/ubuntu";
    int proj_id = 12345;
    key_t key = ftok(pathname, proj_id);
    int semid = semget(key, 1, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    // 新建有1个信号量的信号集，如果已存在则报错
    if (semid == -1) perror_exit("semget");

    // 信号量初始化为0
    if (semctl(semid, 0, SETVAL, 0) == -1) perror_exit("semctl SETVAL");

    for (int i = 0; i < 2; i++) {
        if (fork() !=0 ) {
            continue;
        }
        // 可以定义成数组，此处只涉及一个信号量
        struct sembuf sop = {};
        sop.sem_num = 0; // 信号量在信号集中的索引
        sop.sem_op = 1; // 对信号量的操作
        sop.sem_flg = 0;
        
        semid = semget(key, 1, S_IRUSR | S_IWUSR);

        if (i == 0) {
            /**
             * 如果sem_op小于 0，那么就将信号量值减去sem_op。
             * 如果信号量的当前值大于或等于sem_op的绝对值，那么操作会立即结束。
             * 否则 semop()会阻塞直到信号量值增长到在执行操作之后不会导致出现负值的情况为止
             */
            sop.sem_op = -1;
            // 由于信号量初始值为0，因此会阻塞到信号量增长到不会出现负值为止
            if (semop(semid, &sop, 1) == -1) perror_exit("semop");
            printf("pid=%d, i=%d, val is %d\n", getpid(), i ,semctl(semid, 0, GETVAL));
        }

        if (i == 1) {
            // idx=1的进程虽然sleep 3s，但是会先于idx=0打印
            sleep(3);
            sop.sem_op = 1;
            // sem_op大于零，会立即执行
            if (semop(semid, &sop, 1) == -1) perror_exit("semop");
            printf("pid=%d, i=%d, val is %d\n", getpid(), i ,semctl(semid, 0, GETVAL));
        }
        exit(EXIT_SUCCESS);
    }

    while (wait(NULL) != -1) {}

    // 删除信号集
    if (semctl(semid, 0, IPC_RMID) == -1) perror_exit("semctl IPC_RMID");

    PRINT_SUCCESSFUL;
}


/**
 * case4: 如果sem_op大于 0，则不会出现阻塞
 */
void system_v_sem_case4()
{
    char *pathname = "/home/ubuntu";
    int proj_id = 12345;
    key_t key = ftok(pathname, proj_id);
    int semid = semget(key, 1, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    // 新建有1个信号量的信号集，如果已存在则报错
    if (semid == -1) perror_exit("semget");

    // 信号量初始化为0
    if (semctl(semid, 0, SETVAL, 0) == -1) perror_exit("semctl SETVAL");

    for (int i = 0; i < 10; i++) {
        if (fork() !=0 ) {
            continue;
        }
        // 可以定义成数组，此处只涉及一个信号量
        struct sembuf sop = {};
        sop.sem_num = 0; // 信号量在信号集中的索引
        sop.sem_op = 1; // 对信号量的操作
        sop.sem_flg = 0;
        
        semid = semget(key, 1, S_IRUSR | S_IWUSR);

        // 信号量增长场景，不会出现阻塞
        if (semop(semid, &sop, 1) == -1) perror_exit("semop");
        printf("pid=%d, i=%d, val is %d\n", getpid(), i ,semctl(semid, 0, GETVAL));

        exit(EXIT_SUCCESS);
    }

    while (wait(NULL) != -1) {}

    // 删除信号集
    if (semctl(semid, 0, IPC_RMID) == -1) perror_exit("semctl IPC_RMID");

    PRINT_SUCCESSFUL;
}

/**
 * case5: sem_flg字段中指定IPC_NOWAIT标记来防止 semop()阻塞。如果semop()本来要发生阻塞的话就会返回 EAGAIN 错误
 */
void system_v_sem_case5()
{
    char *pathname = "/home/ubuntu";
    int proj_id = 12345;
    key_t key = ftok(pathname, proj_id);
    int semid = semget(key, 1, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    // 新建有1个信号量的信号集，如果已存在则报错
    if (semid == -1) perror_exit("semget");

    // 信号量初始化为0
    if (semctl(semid, 0, SETVAL, 0) == -1) perror_exit("semctl SETVAL");

    if (fork() ==0 ) {
        
        struct sembuf sop = {};
        sop.sem_num = 0;
        sop.sem_op = -1;
        sop.sem_flg = IPC_NOWAIT;

        /**
         * 如果sem_op小于 0，那么就将信号量值减去sem_op。
         */
        if (semop(semid, &sop, 1) == -1) perror_exit("semop");
        printf("pid=%d, val is %d\n", getpid(),semctl(semid, 0, GETVAL));

    }

    wait(NULL);

    // 删除信号集
    if (semctl(semid, 0, IPC_RMID) == -1) perror_exit("semctl IPC_RMID");

    PRINT_SUCCESSFUL;
}


/**
 * case6: semtimedop()系统调用与 semop()执行的任务一样，但它多了一个 timeout 参数，通过这个参数可以指定调用所阻塞的时间上限
 */
void system_v_sem_case6()
{
    char *pathname = "/home/ubuntu";
    int proj_id = 12345;
    key_t key = ftok(pathname, proj_id);
    int semid = semget(key, 1, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    // 新建有1个信号量的信号集，如果已存在则报错
    if (semid == -1) perror_exit("semget");

    // 信号量初始化为1
    if (semctl(semid, 0, SETVAL, 1) == -1) perror_exit("semctl SETVAL");

    for (int i = 0; i < 4; i++) {
        if (fork() !=0 ) {
            continue;
        }
        struct sembuf sop = {};
        sop.sem_num = 0;
        sop.sem_op = 1;
        sop.sem_flg = 0;
        
        semid = semget(key, 1, S_IRUSR | S_IWUSR);

        if (i == 0) {
            // 由于信号量初始值为1，因此会阻塞到信号量为0为止
            sop.sem_op = 0;
            if (semop(semid, &sop, 1) == -1) perror_exit("semop");
            printf("pid=%d, i=%d, val is %d\n", getpid(), i ,semctl(semid, 0, GETVAL));
        }

        if (i == 1) {
            struct timespec timeout = {
                .tv_sec = 1,
                .tv_nsec = 0
            };
            // 由于信号量初始值为1，因此会1s内阻塞到信号量为0为止，否则失败
            sop.sem_op = 0;
            if (semtimedop(semid, &sop, 1, &timeout) == -1) perror_exit("semtimedop");
            printf("pid=%d, i=%d, val is %d\n", getpid(), i ,semctl(semid, 0, GETVAL));
        }
        
        if (i == 2) {
            struct timespec timeout = {
                .tv_sec = 5,
                .tv_nsec = 0
            };
            // 由于信号量初始值为1，因此5s内会阻塞到信号量为0为止
            sop.sem_op = 0;
            if (semtimedop(semid, &sop, 1, &timeout) == -1) perror_exit("semtimedop");
            printf("pid=%d, i=%d, val is %d\n", getpid(), i ,semctl(semid, 0, GETVAL));
        }

        if (i == 3) {
            sleep(3);
            // 由于信号量初始值为1，此处会立即执行
            sop.sem_op = -1;
            if (semop(semid, &sop, 1) == -1) perror_exit("semop");
            printf("pid=%d, i=%d, val is %d\n", getpid(), i ,semctl(semid, 0, GETVAL));
        }
        exit(EXIT_SUCCESS);
    }

    while (wait(NULL) != -1) {}

    // 删除信号集
    if (semctl(semid, 0, IPC_RMID) == -1) perror_exit("semctl IPC_RMID");

    PRINT_SUCCESSFUL;
}

int main(int argc, char *args[])
{
    system_v_sem_case1();
    system_v_sem_case2();
    system_v_sem_case3();
    system_v_sem_case4();
    system_v_sem_case5();
    system_v_sem_case6();
}