#ifndef DISK_HPP
#define DISK_HPP
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <semaphore.h>
#define LOCK 0
#define BLOCKSIZE 256
#define SECTORS 100
#define CYLINDERS 100

char *diskfile;

#define BLOCK_NUM_INTOTAL 1024
#define TEST_LOCK 1

sem_t BLOCK_MUTEX[BLOCK_NUM_INTOTAL];
char BITMAP[BLOCK_NUM_INTOTAL];



int command_execute(int client_sockfd, char *buf, char *diskfile, char *return_data)
{
    int *buf_int = (int *)buf;

    if (buf_int[0] == 0)
    {
        return 0;
    }

    if (buf_int[0] == 2)
    {
        int cylinder_num = buf_int[1];
        int sector_num = buf_int[2];
        char write_string[256];
        memcpy(write_string, buf + 256, 256);
        memcpy(&diskfile[BLOCKSIZE * (cylinder_num * SECTORS + sector_num)], write_string, 256);
        return 1;
    }

    if (buf_int[0] == 1)
    {
        int cylinder_num = buf_int[1];
        int sector_num = buf_int[2];
        char read_string[BLOCKSIZE];
        memcpy(read_string, &diskfile[BLOCKSIZE * (cylinder_num * SECTORS + sector_num)], BLOCKSIZE);
        memcpy(return_data, read_string, 256);

        return 1;
    }
}

int generate_512bytes_data(int operations_types, int cylinder_num, int sector_num, char *data_256, char *data_512)
{
    memset(data_512, '\0', 512);
    int *data_in_int_512 = (int *)data_512;
    data_in_int_512[0] = operations_types;
    data_in_int_512[1] = cylinder_num;
    data_in_int_512[2] = sector_num;
    memcpy(data_512 + 256, data_256, 256);
}



#endif