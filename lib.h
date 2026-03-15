
#include <stdio.h>

#define SHM_SIZE 4096
#define PRINT_SUCCESSFUL printf("[%s] successful...\n--------------------------------\n", __func__)

void perror_exit(char *str) {
    perror(str);
    exit(EXIT_FAILURE);
}