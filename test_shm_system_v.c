#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/unistd.h>
#include <string.h>

#define SHM_SIZE 4096
#define PRINT_SUCCESSFUL printf("[%s] successful...\n--------------------------------\n", __func__)

int get_nattch(int shmid) {
    struct shmid_ds ds = {};
    if(shmctl(shmid, IPC_STAT, &ds) == -1) {
        perror("shmctl:");
        return -1;
    }
    return ds.shm_nattch;
}

/**
 * case1：同一个进程内，测试使用IPC_EXCL对同一个key重复shmget的影响
 */
void system_v_shm_case1()
{
    char *pathname = "/home/ubuntu";
    int proj_id = 12345;

    // step1: 通过ftok生成ipc key,由于ftok计算时会使用inode，因此pathname必须真实存在的路径
    key_t key = ftok(pathname, proj_id);
    if (key == -1) {
        perror("ftok:");
        return;
    }

    // step2：基于用户传入key，由OS创建对应的共享内存
    // 场景1：IPC_CREAT: 如果共享内存不存在，就创建共享内存；如果存在，则获取已创建的共享内存的标识符
    // 场景2：IPC_CREAT | IPC_EXCL	如果共享内存不存在则创建；如果存在则出错返回-1
    int shmid = shmget(key, SHM_SIZE, IPC_CREAT | IPC_EXCL | SHM_R | SHM_W);
    if (shmid == -1) {
        printf("unexpect first create shm failed\n");
        return;
    }

    // 对同一个key使用IPC_EXCL重复创建shm，预期失败
    shmid = shmget(key, SHM_SIZE, IPC_CREAT | IPC_EXCL | SHM_R | SHM_W);
    if (shmid != -1) {
        printf("unexpect repeat create shm success with IPC_EXCL\n");
        return;
    }

    // 对同一个key不使用IPC_EXCL重复创建shm，预期成功
    shmid = shmget(key, SHM_SIZE, IPC_CREAT | SHM_R | SHM_W);
    if (shmid == -1) {
        printf("unexpect repeat create shm failed without IPC_EXCL\n");
        return;
    }
    
    // step3：主动删除共享内存
    int ret = shmctl(shmid, IPC_RMID, NULL);
    if (ret == -1) {
        perror("shmctl:");
        return;
    }

    PRINT_SUCCESSFUL;
}

void fork_func_case2(int shmid, int i)
{
    static char str[] = "hello,world";

    // 将shm挂载到虚拟地址空间
    void *addr = shmat(shmid, NULL, 0);
    if (addr == NULL) {
        perror("shmat:");
        return;
    }

    // 偶数进程负责写内存，奇数进程负责读内存
    if (i % 2 == 0) {
        printf("i=%d, addr is %s\n", i, addr);
        memcpy(addr, str, strlen(str) + 1);
        printf("i=%d, addr is %s\n", i, addr);
    } else {
        sleep(3);
        printf("i=%d, addr is %s\n", i, addr);
    }

    // 卸载shm
    if(shmdt(addr) == -1) {
        perror("shmdt:");
        return;
    }
}

/**
 * case2：父进程创建共享内存，不同子进程分别挂载和分离，父进程最重负载释放，共享内存可以共享同一份数据
 */
void system_v_shm_case2()
{
    char *pathname = "/home/ubuntu";
    int proj_id = 12345;

    key_t key = ftok(pathname, proj_id);
    if (key == -1) {
        perror("ftok:");
        return;
    }
    
    // 创建shm或者获取已有shm
    int shmid = shmget(key, SHM_SIZE, IPC_CREAT | IPC_EXCL | SHM_R | SHM_W);
    if (shmid == -1) {
        perror("shmget:");
        return;
    }

    for (int i = 0; i < 2; i++) {
        if (fork() == 0) {
            fork_func_case2(shmid, i);
            // !!!关键：子进程必须退出，防止进入下一轮循环
            exit(0);
        }
    }

    // 等待所有子进程结束
    while ((wait(NULL)) != -1) {
    }

    // 主动触发删除shm
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl:");
        return;
    }

    PRINT_SUCCESSFUL;
}

void fork_func_case3(char* addr, int i)
{
    static char str[] = "hello,world";

    // 偶数进程负责写内存，奇数进程负责读内存
    if (i % 2 == 0) {
        printf("i=%d, addr is %s\n", i, addr);
        memcpy(addr, str, strlen(str) + 1);
        printf("i=%d, addr is %s\n", i, addr);
    } else {
        sleep(3);
        printf("i=%d, addr is %s\n", i, addr);
    }
}

/**
 * case3：父进程创建共享内存+挂载+主动触发删除 +分离，子进程无需挂载和分离，直接继承父进程虚拟地址空间，共享内存可以共享同一份数据
 */
void system_v_shm_case3()
{
    char *pathname = "/home/ubuntu";
    int proj_id = 12345;

    key_t key = ftok(pathname, proj_id);
    if (key == -1) {
        perror("ftok:");
        return;
    }
    
    // 创建shm或者获取已有shm
    int shmid = shmget(key, SHM_SIZE, IPC_CREAT | IPC_EXCL | SHM_R | SHM_W);
    if (shmid == -1) {
        perror("shmget:");
        return;
    }

    // 将shm挂载到虚拟地址空间
    void *addr = shmat(shmid, NULL, 0);
    if (addr == NULL) {
        perror("shmat:");
        return;
    }

    // 主动触发删除shm，删除后无法再继续挂载，该共享内存仍会存在且可被父进程和子进程继续使用，由shmdt分离时同时触发删除
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl:");
        return;
    }

    for (int i = 0; i < 2; i++) {
        if (fork() == 0) {
            fork_func_case3(addr, i);
            // !!!关键：子进程必须退出，防止进入下一轮循环
            exit(0);
        }
    }

    // 等待所有子进程结束
    while ((wait(NULL)) != -1) {
    }

    // 最后由主进程分离shm，同时会触发删除共享内存
    if(shmdt(addr) == -1) {
        perror("shmdt:");
        return;
    }

    PRINT_SUCCESSFUL;
}

/**
 * case4：在共享内存未被真正删除或者标记删除的情况下，不同进程调用相同的key会生成相同的shmid
 */
void system_v_shm_case4()
{
    char *pathname = "/home/ubuntu";
    int proj_id = 12345;

    key_t key = ftok(pathname, proj_id);
    if (key == -1) {
        perror("ftok:");
        return;
    }

    for (int i = 0; i < 4; i++) {
        if (fork() == 0) {
            int shmid = shmget(key, SHM_SIZE, IPC_CREAT | SHM_R | SHM_W);
            if (shmid == -1) {
                perror("shmget:");
                return;
            }

            void *addr = shmat(shmid, NULL, 0);
            if (addr == NULL) {
                perror("shmat:");
                return;
            }
            struct shmid_ds ds = {};
            if(shmctl(shmid, IPC_STAT, &ds) == -1) {
                perror("shmctl:");
                return;
            }
            
            // nattch记录当前该shm被多少个进程attch
            printf("child i=%d, pid=%d, key=%d, shmid=%d, addr=%p, nattach=%d\n", i, getpid(), key, shmid, addr, ds.shm_nattch);
            
            // 各个进程nattach统计后再分离
            sleep(5);
            if(shmdt(addr) == -1) {
                perror("shmdt:");
                return;
            }

            // !!!关键：子进程必须退出，防止进入下一轮循环
            exit(0);
        }
        sleep(1);
    }

    // 等待所有子进程结束
    while ((wait(NULL)) != -1) {}

    int shmid = shmget(key, SHM_SIZE, IPC_CREAT | SHM_R | SHM_W);
    if (shmid == -1) {
        perror("shmget:");
        return;
    }

    printf("parent pid=%d, key=%d, shmid=%d\n",  getpid(), key, shmid);

    // 由父进程做最后的删除动作
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl:");
        return;
    }

    PRINT_SUCCESSFUL;
}

/**
 * case5：对同一个shmid重复挂载实际上是对同一个物理共享内存创建多份不同的虚拟空间映射，该共享内存引用计数会增加，需要对应配套的分离解挂
 */
void system_v_shm_case5()
{
    static char str[] = "hello,world";
    char *pathname = "/home/ubuntu";
    int proj_id = 12345;

    key_t key = ftok(pathname, proj_id);
    if (key == -1) {
        perror("ftok:");
        return;
    }

    int shmid = shmget(key, SHM_SIZE, IPC_CREAT | SHM_R | SHM_W);
    if (shmid == -1) {
        perror("shmget:");
        return;
    }

    void *addr1 = shmat(shmid, NULL, 0);
    if (addr1 == NULL) {
        perror("shmat:");
        return;
    }
    memcpy(addr1, str, strlen(str) + 1);

    printf("first shmat addr=%p, nattach=%d, content=%s\n", addr1, get_nattch(shmid), addr1);

    void *addr2 = shmat(shmid, NULL, 0);
    if (addr2 == NULL) {
        perror("shmat:");
        return;
    }
    printf("second shmat addr=%p, nattach=%d, content=%s\n", addr2, get_nattch(shmid), addr2);
    
    if (addr1 != addr2 && strcmp(addr1, addr2) == 0) {
        printf("diff virtual addr, same physical addr\n");
    }

    if(shmdt(addr1) == -1) {
        perror("shmdt:");
        return;
    }
    printf("first shmdt addr=%p, nattach=%d\n", addr1, get_nattch(shmid));

    if(shmdt(addr2) == -1) {
        perror("shmdt:");
        return;
    }

    printf("second shmdt addr=%p, nattach=%d\n", addr2, get_nattch(shmid));

    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl:");
        return;
    }

    PRINT_SUCCESSFUL;
}

/**
 * case6：进程如果未显式shmdt分离，进程退出后也会自动分离减引用，但是删除共享内存需要显示调用
 */
void system_v_shm_case6()
{
    char *pathname = "/home/ubuntu";
    int proj_id = 12345;
    static char str[] = "hello,world";

    key_t key = ftok(pathname, proj_id);
    if (key == -1) {
        perror("ftok:");
        return;
    }

    if (fork() == 0) {
        int shmid = shmget(key, SHM_SIZE, IPC_CREAT | IPC_EXCL | SHM_R | SHM_W);
        if (shmid == -1) {
            perror("shmget:");
            return;
        }

        void *addr = shmat(shmid, NULL, 0);
        if (addr == NULL) {
            perror("shmat:");
            return;
        }
        memcpy(addr, str, strlen(str) + 1);
        printf("child pid=%d, key=%d, shmid=%d, nattch=%d, content=%s\n", getpid(), key, shmid, get_nattch(shmid), addr);

        // !!!关键：子进程必须退出，防止进入下一轮循环
        exit(0);
    }

    // 等待子进程结束
    wait(NULL);

    // 子进程退出时，未显式标删，预期主进程则无法基于IPC_EXCL创建
    int shmid = shmget(key, SHM_SIZE, IPC_CREAT | IPC_EXCL| SHM_R | SHM_W);
    if (shmid != -1) {
        printf("unexpected shmget success\n");
        return;
    }

    // 子进程退出时，未显式标删，主进程获取已有shmid
    shmid = shmget(key, SHM_SIZE, IPC_CREAT | SHM_R | SHM_W);
    if (shmid == -1) {
        perror("shmget:");
        return;
    }
    printf("parent pid=%d, key=%d, shmid=%d, nattch=%d\n", getpid(), key, shmid, get_nattch(shmid));

    void *addr = shmat(shmid, NULL, 0);
    if (addr == NULL) {
        perror("shmat:");
        return;
    }
    // 子进程由于未主动删除共享内存，所以父进程仍然可以读取该段内存的内容
    printf("parent pid=%d, key=%d, shmid=%d, nattch=%d, content=%s\n", getpid(), key, shmid, get_nattch(shmid), addr);

    if(shmdt(addr) == -1) {
        perror("shmdt:");
        return;
    }

    // 由父进程做最后的删除动作
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl:");
        return;
    }

    PRINT_SUCCESSFUL;
}


/**
 * case7：子进程退出前显式标删再解绑，后者也能触发内存删除，父进程使用相同的key搭配IPC_EXCL能创建共享内存，但是shmid会换新
 */
void system_v_shm_case7()
{
    char *pathname = "/home/ubuntu";
    int proj_id = 12345;
    static char str[] = "hello,world";

    key_t key = ftok(pathname, proj_id);
    if (key == -1) {
        perror("ftok:");
        return;
    }

    if (fork() == 0) {
        int shmid = shmget(key, SHM_SIZE, IPC_CREAT | IPC_EXCL | SHM_R | SHM_W);
        if (shmid == -1) {
            perror("shmget:");
            return;
        }

        void *addr = shmat(shmid, NULL, 0);
        if (addr == NULL) {
            perror("shmat:");
            return;
        }

        // 解绑前直接标删
        if (shmctl(shmid, IPC_RMID, NULL) == -1) {
            perror("shmctl:");
            return;
        }

        // 共享内存仍有效且可用
        memcpy(addr, str, strlen(str) + 1);
        printf("child pid=%d, key=%d, shmid=%d, nattch=%d, content=%s\n", getpid(), key, shmid, get_nattch(shmid), addr);
     
        // 标删后再解绑
        if(shmdt(addr) == -1) {
            perror("shmdt:");
            return;
        }

        // !!!关键：子进程必须退出，防止进入下一轮循环
        exit(0);
    }

    // 等待子进程结束
    wait(NULL);

    // 子进程退出前显式标删再解绑，后者也能触发内存删除，父进程使用相同的key搭配IPC_EXCL能创建共享内存，但是shmid会换新
    int shmid = shmget(key, SHM_SIZE, IPC_CREAT | IPC_EXCL| SHM_R | SHM_W);
    if (shmid == -1) {
        printf("unexpected shmget success\n");
        return;
    }
    printf("parent pid=%d, key=%d, shmid=%d, nattch=%d\n", getpid(), key, shmid, get_nattch(shmid));

    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl:");
        return;
    }

    PRINT_SUCCESSFUL;
}

/**
 * case8：子进程退出前显式标删但是未显式解绑，进程退出后会自动解绑并也能触发内存删除，父进程使用相同的key搭配IPC_EXCL能创建共享内存，但是shmid会换新
 */
void system_v_shm_case8()
{
    char *pathname = "/home/ubuntu";
    int proj_id = 12345;
    static char str[] = "hello,world";

    key_t key = ftok(pathname, proj_id);
    if (key == -1) {
        perror("ftok:");
        return;
    }

    if (fork() == 0) {
        int shmid = shmget(key, SHM_SIZE, IPC_CREAT | IPC_EXCL | SHM_R | SHM_W);
        if (shmid == -1) {
            perror("shmget:");
            return;
        }

        void *addr = shmat(shmid, NULL, 0);
        if (addr == NULL) {
            perror("shmat:");
            return;
        }

        // 解绑前直接标删
        if (shmctl(shmid, IPC_RMID, NULL) == -1) {
            perror("shmctl:");
            return;
        }

        // 共享内存仍有效且可用
        memcpy(addr, str, strlen(str) + 1);
        printf("child pid=%d, key=%d, shmid=%d, nattch=%d, content=%s\n", getpid(), key, shmid, get_nattch(shmid), addr);
     
        // 标删后未主动解绑，依赖进程退出时触发解绑

        // !!!关键：子进程必须退出，防止进入下一轮循环
        exit(0);
    }

    // 等待子进程结束
    wait(NULL);

    // 子进程退出前显式标删再解绑，后者也能触发内存删除，父进程使用相同的key搭配IPC_EXCL能创建共享内存，但是shmid会换新
    int shmid = shmget(key, SHM_SIZE, IPC_CREAT | IPC_EXCL| SHM_R | SHM_W);
    if (shmid == -1) {
        printf("unexpected shmget success\n");
        return;
    }
    printf("parent pid=%d, key=%d, shmid=%d, nattch=%d\n", getpid(), key, shmid, get_nattch(shmid));

    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl:");
        return;
    }

    PRINT_SUCCESSFUL;
}

int main(int argc, char *args[])
{
    system_v_shm_case1();
    system_v_shm_case2();
    system_v_shm_case3();
    system_v_shm_case4();
    system_v_shm_case5();
    system_v_shm_case6();
    system_v_shm_case7();
    system_v_shm_case8();
}