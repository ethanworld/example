#include <stdio.h>
#include <semaphore.h>
#include <fcntl.h>   // O_CREAT/O_EXCL 定义
#include <stdlib.h>
#include <pthread.h>
#include <sys/mman.h>
#include "lib.h"

static char posix_sem_name[] = "/sem_demo";

/**
 * case1：命名信号量，不同进程基于相同name可以获取同一个posix命名信号量
 */
void posix_sem_case1()
{
    for (int i = 0; i < 3; i++) {
        if (fork() != 0) {
            continue;
        }

        // 除索引为1以外的进程均等待1s
        if (i != 1) {
            sleep(1);
        }

        // 创建或获取信号量，不同进程给的初始值不同，为i
        sem_t *sem = sem_open(posix_sem_name, O_CREAT, 0666, i + 123);

        int val = 0;
        if (sem_getvalue(sem, &val) == -1) perror_exit("sem_getvalue");

        // 预期不同进程虽然sem_open给不同初值，但实则只有第一个创建并设置成功，其他进程均为创建时的val
        printf("i=%d, pid=%d, sem val is %d\n", i, getpid(), val);

        if (sem_close(sem) == -1) perror_exit("sem_close"); 

        exit(EXIT_SUCCESS);
    }

    while (wait(NULL) != -1) {}
    
    if (sem_unlink(posix_sem_name) == -1) perror_exit("sem_unlink");
    
    PRINT_SUCCESSFUL;
}

/**
 * case2：命名信号量，POSIX 信号量也是一个整数并且系统不会允许其值小于0，否则会阻塞
 */
void posix_sem_case2()
{

    for (int i = 0; i < 3; i++) {
        if (fork() != 0) {
            continue;
        }
        // 创建信号量，初始值为1
        sem_t *sem = sem_open(posix_sem_name, O_CREAT, 0666, 1);

        // P操作，信号量减1
        printf("i=%d, pid=%d, sem wait\n", i, getpid());
        if (sem_wait(sem) == -1) perror_exit("sem_wait");

        // 临界区代码，通过sleep观察阻塞效果
        sleep(1);
        int val = 0;
        if (sem_getvalue(sem, &val) == -1) perror_exit("sem_getvalue");
        printf("i=%d, pid=%d, sem val is %d\n", i, getpid(), val);

        // V操作，信号量加1
        printf("i=%d, pid=%d, sem post\n", i, getpid());
        if (sem_post(sem) == -1) perror_exit("sem_wait");

        if (sem_close(sem) == -1) perror_exit("sem_close"); 

        exit(EXIT_SUCCESS);
    }

    while (wait(NULL) != -1) {}
    
    if (sem_unlink(posix_sem_name) == -1) perror_exit("sem_unlink");

    PRINT_SUCCESSFUL;
}

static void thread_func_3(void *arg)
{
    static int cnt = 0;
    sem_t *sem = (sem_t *)arg;
    
    // P操作，信号量减1
    printf("tid=%d, pid=%d cnt=%d, sem wait\n", pthread_self(), getpid(), cnt);
    if (sem_wait(sem) == -1) perror_exit("sem_wait");

    // 临界区代码，通过sleep观察阻塞效果
    sleep(1);
    cnt++;
    int val = 0;
    if (sem_getvalue(sem, &val) == -1) perror_exit("sem_getvalue");
    printf("tid=%d, pid=%d, cnt=%d, sem val is %d\n", pthread_self(), getpid(), cnt, val);

    // V操作，信号量加1
    printf("tid=%d, pid=%d, sem post\n", pthread_self(), getpid());
    if (sem_post(sem) == -1) perror_exit("sem_wait");
}

/**
 * case3：未命名信号量，pshared配置为0，构造线程间共享信号量解决进程内多线程并发问题
 */
void posix_sem_case3()
{
    static sem_t sem;
    /**
     * 如果 pshared 等于 0，那么信号量将会在调用进程中的线程间进行共享。
     * 在这种情况下，sem 通常被指定成一个全局变量的地址或分配在堆上的一个变量的地址。
     * 线程共享的信号量具备进程持久性，它在进程终止时会被销毁。
     */
    if (sem_init(&sem, 0, 1) == -1) perror_exit("sem_init");

    pthread_t ts[10];
    for (int i = 0; i < 5; i++) {
        pthread_create(&ts[i], NULL, thread_func_3, &sem);
    }

    for (int i = 0; i < 5; i++) {
        pthread_join(ts[i], NULL);
    }

    if (sem_destroy(&sem) == -1) perror_exit("sem_destroy"); 
    PRINT_SUCCESSFUL;
}

/**
 * case4：未命名信号量，pshared配置为1，基于共享内存构造进程间共享信号量解决多进程并发问题
 */
void posix_sem_case4()
{
    // 采用共享匿名映射生成一块未命名信号量内存
    sem_t *sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (sem == MAP_FAILED) perror_exit("mmap");

    /**
     * 如果 pshared 不等于 0，那么信号量将会在进程间共享。
     * 在这种情况下，sem 必须是共享内存区域（一个 POSIX 共享内存对象、一个使用 mmap()创建的共享映射、或一个System V 共享内存段）中的某个位置的地址。
     * 信号量的持久性与它所处的共享内存的持久性是一样的。
     */
    if (sem_init(sem, 1, 1) == -1) perror_exit("sem_init");

    if (fork() == 0) {
        // 子进程PV操作，子进程抢锁后多等待3s，预期比父进程先输出打印内容
        if (sem_wait(sem) == -1) perror_exit("sem_wait");
        sleep(3);
        printf("this is child process\n");
        if (sem_post(sem) == -1) perror_exit("sem_post");
        exit(EXIT_SUCCESS);
    }

    // 父进程PV操作，父进程先sleep让子进程先抢到锁
    sleep(1);
    if (sem_wait(sem) == -1) perror_exit("sem_wait");
    printf("this is parent process\n");
    if (sem_post(sem) == -1) perror_exit("sem_post");

    wait(NULL);
    if (sem_destroy(sem) == -1) perror_exit("sem_destroy"); 
    PRINT_SUCCESSFUL;
}

/**
 * case5：未命名信号量，pshared配置为1，如果sem_t不是共享内存，fork机制也无法直接用该信号量用于控制，因为sem_t在PV操作后会触发COW，并非是同一个信号量
 */
void posix_sem_case5()
{
    static sem_t sem;
    int val = 0;
    /**
     * 如下虽然pshared配置成1，当子进程PV操作时，继承的sem会触发COW，与父进程则不再共享同一块内存
     * 因此在各自sem_post后，val计数是各自计数，结果均为11
     */
    if (sem_init(&sem, 1, 10) == -1) perror_exit("sem_init");

    if (fork() == 0) {
        // 子进程V操作
        if (sem_post(&sem) == -1) perror_exit("sem_post");
        if (sem_getvalue(&sem, &val) == -1) perror_exit("sem_getvalue");
        printf("this is child process, val=%d\n", val);

        exit(EXIT_SUCCESS);
    }

    // 父进程V操作
    if (sem_post(&sem) == -1) perror_exit("sem_post");
    if (sem_getvalue(&sem, &val) == -1) perror_exit("sem_getvalue");
    printf("this is parent process, val=%d\n", val);

    wait(NULL);
    if (sem_destroy(&sem) == -1) perror_exit("sem_destroy"); 
    PRINT_SUCCESSFUL;
}

int main(int argc, char *args[])
{
    posix_sem_case1();
    posix_sem_case2();
    posix_sem_case3();
    posix_sem_case4();
}