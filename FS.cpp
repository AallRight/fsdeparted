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

int generate_512bytes_data(int operations_types, int cylinder_num, int sector_num, char *data_256, char *data_512)
{
    memset(data_512, '\0', 512);
    int *data_in_int_512 = (int *)data_512;
    data_in_int_512[0] = operations_types;
    data_in_int_512[1] = cylinder_num;
    data_in_int_512[2] = sector_num;
    memcpy(data_512 + 256, data_256, 256);
}

// int parseLine(char *line, char *command_array[])
// {
//     char *p;
//     int count = 0;
//     p = strtok(line, " ");

//     while (p != NULL)
//     {
//         command_array[count] = p;
//         count++;
//         p = strtok(NULL, " ");
//     }
//     return count;
// }

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
char *diskfile;

#define BLOCK_NUM_INTOTAL 1024
#define TEST_LOCK 1

sem_t BLOCK_MUTEX[BLOCK_NUM_INTOTAL];
char BITMAP[BLOCK_NUM_INTOTAL];

typedef struct Inode
{
    int file_type;
    int file_size;
    int sector_id;
    int pre_inode_sector_id;
    int block_num;
    int name_inode;
    int direct_block[38];
    int indirect_block[12];
    int double_indirect_block[8];

} Inode;

int create_directory_in_current_dir(Inode *inode, Inode *father_inode, char *name);
int create_inode(Inode *inode, int *sector_id, int pre_inode_sector_id, int file_type);
int store_bitmap();
void initial_bitmap()
{
    for (int i = 0; i < BLOCK_NUM_INTOTAL; i++)
    {
        BITMAP[i] = '0';
    }
    for (int i = 0; i < BLOCK_NUM_INTOTAL / 256; i++)
    {
        BITMAP[i] = '1';
    }
    store_bitmap();
}

Inode share_usrroot_inode;
Inode usr_root_inode;
int shift_flag = 0;
char user_name_global[252];
char shift_name[252];

int init_semaphore()
{
    for (int i = 0; i < BLOCK_NUM_INTOTAL; i++)
    {
        sem_init(&BLOCK_MUTEX[i], 0, 1);
    }
}
int destroy_semaphore()
{
    for (int i = 0; i < BLOCK_NUM_INTOTAL; i++)
    {
        sem_destroy(&BLOCK_MUTEX[i]);
    }
}
int write_block(int sector_id, char *data_write)
{
    sem_wait(&BLOCK_MUTEX[sector_id]);
#if TEST_LOCK
    char buffer[512];
    generate_512bytes_data(2, sector_id / 100, sector_id % 100, data_write, buffer);
    char data_read[256];
    command_execute(0, buffer, diskfile, NULL);

#endif

    char data[256];

    sem_post(&BLOCK_MUTEX[sector_id]);
}
int read_block(int sector_id, char *data_read)
{
    sem_wait(&BLOCK_MUTEX[sector_id]);

#if TEST_LOCK
    char buffer[512];
    char tem[256];
    generate_512bytes_data(1, sector_id / 100, sector_id % 100, tem, buffer);
    command_execute(0, buffer, diskfile, data_read);

#endif

    sem_post(&BLOCK_MUTEX[sector_id]);
}
int store_bitmap()
{
    for (int i = 0; i < BLOCK_NUM_INTOTAL; i += 256)
    {
        char data[256];
        memcpy(data, BITMAP + i, 256);
        write_block(i / 256, data);
    }
    return 0;
}
int load_bitmap()
{
    for (int i = 0; i < BLOCK_NUM_INTOTAL; i += 256)
    {
        char data[256];
        read_block(i / 256, data);
        memcpy(BITMAP + i, data, 256);
    }
    return 0;
}

int get_free_block()
{
    load_bitmap();
    for (int i = 0; i < BLOCK_NUM_INTOTAL; i++)
    {
        if (BITMAP[i] == '0')
        {
            return i;
        }
    }
    return -1;
}

int get_inode(int sector_id, Inode *inode)
{
    load_bitmap();
    if (sector_id < 0 || sector_id >= BLOCK_NUM_INTOTAL || BITMAP[sector_id] == '0')
    {
        return -1;
    }
    char inode_data[256];
    read_block(sector_id, inode_data);
    memcpy(inode, inode_data, 256);
    return 0;
}

int write_inode_to_disk(Inode *inode)
{
    char inode_data[256];
    memcpy(inode_data, inode, 256);
    write_block(inode->sector_id, inode_data);
    return 0;
}

int get_sector_id_by_index(Inode *inode, int index, int *sector_id)
{
    if (index >= inode->block_num)
    {
        return -1;
    }
    if (index < 38)
    {
        *sector_id = inode->direct_block[index];
        return 0;
    }
    if (index < 38 + 12 * 64)
    {
        int indirect_index = index - 38;
        int first = indirect_index / 64;
        int second = indirect_index % 64;
        char indirect_block_data[256];
        read_block(inode->indirect_block[first], indirect_block_data);
        int indirect_block[64];
        memcpy(indirect_block, indirect_block_data, 256);
        *sector_id = indirect_block[second];
        return 0;
    }
    if (index < 38 + 12 * 64 + 8 * 64 * 64)
    {
        int double_indirect_index = index - 38 - 12 * 64;
        int first = (double_indirect_index / 64) / 64;
        int second = (double_indirect_index / 64) % 64;
        int third = double_indirect_index % 64;
        char indirect_block_data[256];
        int indirect_block[64];
        read_block(inode->double_indirect_block[first], indirect_block_data);
        memcpy(indirect_block, indirect_block_data, 256);
        read_block(indirect_block[second], indirect_block_data);
        memcpy(indirect_block, indirect_block_data, 256);
        *sector_id = indirect_block[third];
        return 0;
    }
    return -1;
}
int allocate_new_block_to_inode(Inode *inode, int *sector_id)
{
    if (inode->block_num >= 38 + 12 * 64 + 8 * 64 * 64)
    {
        return -1;
    }
    *sector_id = get_free_block();
    if (*sector_id == -1)
    {
        return -1;
    }
    inode->block_num++;
    BITMAP[*sector_id] = '1';
    store_bitmap();
    int index = inode->block_num - 1;
    if (index < 38)
    {
        inode->direct_block[index] = *sector_id;
        write_inode_to_disk(inode);
        return 0;
    }
    if (index < 38 + 64 * 12)
    {
        int indirect_index = index - 38;
        int first = indirect_index / 64;
        int second = indirect_index % 64;
        if (second == 0)
        {
            inode->indirect_block[first] = get_free_block();
            if (inode->indirect_block[first] == -1)
            {
                inode->block_num--;
                BITMAP[*sector_id] = '0';
                store_bitmap();
                return -1;
            }
            BITMAP[inode->indirect_block[first]] = '1';
            store_bitmap();
        }
        char indirect_block_data[256];
        read_block(inode->indirect_block[first], indirect_block_data);
        int indirect_block[64];
        memcpy(indirect_block, indirect_block_data, 256);
        indirect_block[second] = *sector_id;
        memcpy(indirect_block_data, indirect_block, 256);
        write_block(inode->indirect_block[first], indirect_block_data);
        write_inode_to_disk(inode);
        return 0;
    }

    if (index < 38 + 12 * 64 + 8 * 64 * 64)
    {
        int double_indirect_index = index - 38 - 12 * 64;
        int first = (double_indirect_index / 64) / 64;
        int second = (double_indirect_index / 64) % 64;
        int third = double_indirect_index % 64;
        if (second == 0 && third == 0 && inode->double_indirect_block[first] == -1)
        {
            inode->double_indirect_block[first] = get_free_block();
            if (inode->double_indirect_block[first] == -1)
            {
                inode->block_num--;
                BITMAP[*sector_id] = '0';
                store_bitmap();
                return -1;
            }
            BITMAP[inode->double_indirect_block[first]] = '1';
            store_bitmap();
        }

        if (third == 0)
        {
            char indirect_block_data[256];
            read_block(inode->double_indirect_block[first], indirect_block_data);
            int indirect_block[64];
            memcpy(indirect_block, indirect_block_data, 256);
            indirect_block[second] = get_free_block();
            if (indirect_block[second] == -1)
            {
                inode->block_num--;
                BITMAP[*sector_id] = '0';
                store_bitmap();
                return -1;
            }
            BITMAP[indirect_block[second]] = '1';
            store_bitmap();
            write_block(inode->double_indirect_block[first], indirect_block_data);
        }

        char indirect_block_data[256];
        char double_indirect_block_data[256];
        read_block(inode->double_indirect_block[first], indirect_block_data);
        int indirect_block[64];
        memcpy(indirect_block, indirect_block_data, 256);
        read_block(indirect_block[second], double_indirect_block_data);
        int double_indirect_block[64];
        memcpy(double_indirect_block, double_indirect_block_data, 256);
        double_indirect_block[third] = *sector_id;
        memcpy(double_indirect_block_data, double_indirect_block, 256);
        write_block(indirect_block[second], double_indirect_block_data);

        write_inode_to_disk(inode);
        return 0;
    }
    return -1;
}

int inode_write_data_to_disk(Inode *inode, int index, char *data)
{
    int sector_id;
    int flag = get_sector_id_by_index(inode, index, &sector_id);
    if (flag == -1 || sector_id < 0 || BITMAP[sector_id] == '0')
    {
        return -1;
    }
    write_block(sector_id, data);
    return 0;
}

int inode_read_data_to_disk(Inode *inode, int index, char *data)
{
    int sector_id;
    int flag = get_sector_id_by_index(inode, index, &sector_id);
    if (flag == -1 || sector_id < 0 || BITMAP[sector_id] == '0')
    {
        return -1;
    }
    read_block(sector_id, data);
    return 0;
}

int collect_the_block_from_end(Inode *inode)
{
    if (inode->block_num == 0)
    {
        return -1;
    }
    int index = inode->block_num - 1;
    if (index < 38)
    {
        BITMAP[inode->direct_block[index]] = '0';
        store_bitmap();
        inode->direct_block[index] = -1;
        inode->block_num--;
        write_inode_to_disk(inode);
        return 0;
    }
    if (index < 38 + 12 * 64)
    {
        int indirect_index = index - 38;
        int first = indirect_index / 64;
        int second = indirect_index % 64;
        char indirect_block_data[256];
        read_block(inode->indirect_block[first], indirect_block_data);
        int indirect_data[64];
        memcpy(indirect_data, indirect_block_data, 256);
        BITMAP[indirect_data[second]] = '0';
        store_bitmap();
        indirect_data[second] = -1;
        memcpy(indirect_block_data, indirect_data, 256);
        write_block(inode->indirect_block[first], indirect_block_data);
        inode->block_num--;
        if (second == 0)
        {
            BITMAP[inode->indirect_block[first]] = '0';
            store_bitmap();
            inode->indirect_block[first] = -1;
        }
        write_inode_to_disk(inode);
        return 0;
    }
    if (index < 38 + 12 * 64 + 8 * 64 * 64)
    {
        int double_indirect_index = index - 38 - 12 * 64;
        int first = (double_indirect_index / 64) / 64;
        int second = (double_indirect_index / 64) % 64;
        int third = double_indirect_index % 64;

        char inner_indirect_block_data[256];
        char outer_indirect_block_data[256];
        read_block(inode->double_indirect_block[first], inner_indirect_block_data);
        int *inner_indirect_block = (int *)inner_indirect_block_data;
        read_block(inner_indirect_block[second], outer_indirect_block_data);
        int *outer_indirect_block = (int *)outer_indirect_block_data;
        BITMAP[outer_indirect_block[third]] = '0';
        store_bitmap();
        outer_indirect_block[third] = -1;
        write_block(inner_indirect_block[second], outer_indirect_block_data);
        inode->block_num--;
        if (third == 0)
        {
            BITMAP[inner_indirect_block[second]] = '0';
            store_bitmap();
            inner_indirect_block[second] = -1;
            write_block(inode->double_indirect_block[first], inner_indirect_block_data);
        }
        if (second == 0 && third == 0)
        {
            BITMAP[inode->double_indirect_block[first]] = '0';
            store_bitmap();
            inode->double_indirect_block[first] = -1;
        }
        write_inode_to_disk(inode);
        return 0;
    }
    return -1;
}

int inode_init(Inode *inode, int sector_id, int pre_inode_sector_id, int file_type)
{
    inode->file_type = file_type;
    inode->sector_id = sector_id;
    inode->block_num = 0;
    inode->pre_inode_sector_id = pre_inode_sector_id;
    inode->name_inode = -1;
    inode->file_size = 0;
    for (int i = 0; i < 38; i++)
    {
        inode->direct_block[i] = -1;
    }
    for (int i = 0; i < 12; i++)
    {
        inode->indirect_block[i] = -1;
    }
    for (int i = 0; i < 8; i++)
    {
        inode->double_indirect_block[i] = -1;
    }
    write_inode_to_disk(inode);
    return 0;
}
int create_inode(Inode *inode, int *sector_id, int pre_inode_sector_id, int file_type)
{
    *sector_id = get_free_block();
    if (*sector_id == -1)
    {
        return -1;
    }
    BITMAP[*sector_id] = '1';
    store_bitmap();
    inode_init(inode, *sector_id, pre_inode_sector_id, file_type);
    write_inode_to_disk(inode);
    return 0;
}

typedef struct File_Name
{
    char name[252];
    int inode_id;
} File_Name;
int search_file_by_name(Inode *d_inode, char *name, int *id_return);
int initial_the_file_system()
{
    initial_bitmap();
    int sector_id;
    Inode root_inode;
    create_inode(&root_inode, &sector_id, -1, 3);
    Inode share_inode;
    int share_inode_sector_id;
    create_directory_in_current_dir(&share_inode, &root_inode, "share");
    share_inode.pre_inode_sector_id = -1;
    write_inode_to_disk(&share_inode);
}
int judge_bitmap_qualified()
{
    for (int i = 0; i < BLOCK_NUM_INTOTAL; i++)
    {
        if (BITMAP[i] != '0' && BITMAP[i] != '1')
        {
            return 0;
        }
    }
    return 1;
}
int open_file_system()
{
    init_semaphore();
    char root_inode_data[256];
    load_bitmap();
    if (!judge_bitmap_qualified())
    {
        initial_the_file_system();
    }
    int bitmap_block_num = BLOCK_NUM_INTOTAL / BLOCKSIZE;
    read_block(4, root_inode_data);
    Inode tem;
    memcpy(&tem, root_inode_data, 256);
    if (tem.file_type != 3)
    {
        initial_the_file_system();
        return 0;
    }
}
int fill_space_to_name(char *src, char *name_after)
{
    int src_size = strlen(src);
    strcpy(name_after, src);
    memset(name_after + src_size, '\0', 252 - src_size);
    return 0;
}
int create_normalized_name(char *src, File_Name *file_name, int file_sector_id)
{
    char name_after[252];
    fill_space_to_name(src, name_after);
    char tem[256];
    memcpy(tem, name_after, 252);
    int *tem_int = (int *)tem;
    tem_int[63] = file_sector_id;
    memcpy(file_name, tem, 256);
    return 0;
}
int add_name(Inode *father_inode, Inode *child_inode, char *name)
{
    if (father_inode->file_type == 2)
    {
        return -1;
    }
    int file_inode_id;
    if (search_file_by_name(father_inode, name, &file_inode_id) == 0)
    {
        return -1;
    }
    int new_block_id;
    allocate_new_block_to_inode(father_inode, &new_block_id);

    File_Name child_inode_filename;
    create_normalized_name(name, &child_inode_filename, child_inode->sector_id);

    write_block(new_block_id, (char *)&child_inode_filename);
    child_inode->name_inode = new_block_id;
    write_inode_to_disk(father_inode);
    write_inode_to_disk(child_inode);
    return 0;
}
int remove_name(Inode *inode, char *name)
{
    if (inode->file_type == 2)
    {
        return -1;
    }
    int directory_num = inode->block_num;
    int end_index = directory_num - 1;
    File_Name end_file_name;
    int end_block_id;
    get_sector_id_by_index(inode, end_index, &end_block_id);
    read_block(end_block_id, (char *)&end_file_name);
    for (int i = 0; i < directory_num; i++)
    {
        File_Name tem_name;
        int id;
        get_sector_id_by_index(inode, i, &id);
        read_block(id, (char *)&tem_name);
        if (strcmp(tem_name.name, name) == 0 && i != end_index)
        {
            write_block(id, (char *)&end_file_name);
            collect_the_block_from_end(inode);
            return 0;
        }
        else if (strcmp(tem_name.name, name) == 0 && i == end_index)
        {
            collect_the_block_from_end(inode);
            return 0;
        }
    }
    return -1;
}
int create_directory_in_current_dir(Inode *inode, Inode *father_inode, char *name)
{
    if (father_inode->file_type == 2)
    {
        return -1;
    }
    int sector_id_for_creation;
    int flag = create_inode(inode, &sector_id_for_creation, father_inode->sector_id, 1);
    if (flag == -1)
    {
        return -1;
    }

    File_Name directory_name;
    create_normalized_name(name, &directory_name, inode->sector_id);
    int name_flag = add_name(father_inode, inode, name);
    if (name_flag == -1)
    {
        return -1;
    }
    return 0;
}

int create_file_in_current_dir(Inode *inode, Inode *father_inode, char *name)
{
    if (father_inode->file_type == 2)
    {
        return -1;
    }
    int sector_id_for_creation;
    int flag = create_inode(inode, &sector_id_for_creation, father_inode->sector_id, 2);
    if (flag == -1)
    {
        return -1;
    }

    File_Name directory_name;
    create_normalized_name(name, &directory_name, inode->sector_id);
    int name_flag = add_name(father_inode, inode, name);
    if (name_flag == -1)
    {
        return -1;
    }
    return 0;
}

int search_file_by_name(Inode *d_inode, char *name, int *id_return)
{

    char name_for_search[252];
    memcpy(name_for_search, name, 252);
    if (d_inode->file_type == 2)
    {
        return -1;
    }
    int files_num = d_inode->block_num;
    for (int i = 0; i < files_num; i++)
    {
        File_Name name_block_for_search;
        int file_id;
        get_sector_id_by_index(d_inode, i, &file_id);
        read_block(file_id, (char *)&name_block_for_search);
        if (strcmp(name_block_for_search.name, name_for_search) == 0)
        {
            *id_return = name_block_for_search.inode_id;
            return 0;
        }
    }
    return -1;
}

int list_file_in_current_directory(Inode *inode, char name[256][256], int *name_num)
{
    if (inode->file_type == 2)
    {
        return -1;
    }
    int directory_num = inode->block_num;
    for (int i = 0; i < directory_num; i++)
    {
        File_Name tem_name;
        int id;
        get_sector_id_by_index(inode, i, &id);
        read_block(id, (char *)&tem_name);
        Inode tem_inode;
        int tem_filetype;
        get_inode(tem_name.inode_id, &tem_inode);
        tem_filetype = tem_inode.file_type;
        int *f_type_of_name = (int *)name[i];
        memcpy(name[i], &tem_name, 256);
        f_type_of_name[63] = tem_filetype;
    }
    *name_num = directory_num;
    return 0;
}
int get_current_route(Inode *inode, char *name_new, char *user_name, char *write_to_client)
{
    Inode tem_inode = *inode;
    if (inode->file_type == 2)
    {
        return -1;
    }
    char name[4096 + 256];
    memset(name, '\0', 4096 + 256);
    while (tem_inode.pre_inode_sector_id != -1)
    {
        File_Name tem_name;
        read_block(tem_inode.name_inode, (char *)&tem_name);
        char buffer[4096];
        char name_3840[4096 - 256];
        strcpy(name_3840, name);
        sprintf(buffer, "/%s%s", tem_name.name, name_3840);
        strcpy(name, buffer);
        get_inode(tem_inode.pre_inode_sector_id, &tem_inode);
    }
    printf("\033[0;32m%s\033[0;37m:\033[0;34m~%s\033[0;37m$ ", user_name, name);
    sprintf(write_to_client, "\033[0;32m%s\033[0;37m:\033[0;34m~%s\033[0;37m$ ", user_name, name);
    return 0;
}
int create_directory_for_client(Inode *current_directory_inode, Inode *directory_inode, char *name)
{
    int sector_id;

    return 0;
}
int clear_file_content(Inode *inode)
{
    if (inode->file_type != 2)
    {
        return -1;
    }
    int block_num_delete = inode->block_num;
    for (int i = 0; i < block_num_delete; i++)
    {
        collect_the_block_from_end(inode);
    }
    inode->block_num = 0;
    inode->file_size = 0;
    write_inode_to_disk(inode);
    return 0;
}

int delete_file_from_directory(Inode *directory_inode, char *name)
{
    Inode file_inode;

    int file_inode_id;
    if (search_file_by_name(directory_inode, name, &file_inode_id) == -1)
    {
        return -1;
    }
    get_inode(file_inode_id, &file_inode);
    if (clear_file_content(&file_inode) == -1)
    {
        return -1;
    }
    if (remove_name(directory_inode, name) == -1)
    {
        return -1;
    }
    BITMAP[file_inode.sector_id] = '0';
    store_bitmap();
    return 0;
}
int delete_directory_from_directory(Inode *father_inode, char *name);

int delete_directory_from_directory(Inode *father_inode, char *name)
{
    Inode child_directory_inode;
    int child_directory_inode_id;
    if (search_file_by_name(father_inode, name, &child_directory_inode_id) == -1)
    {
        return -1;
    }
    get_inode(child_directory_inode_id, &child_directory_inode);
    if (child_directory_inode.file_type == 2)
    {
        return -1;
    }
    int child_directory_sub_filenum = child_directory_inode.block_num;

    for (int i = 0; i < child_directory_sub_filenum; i++)
    {
        File_Name tem_name;
        Inode tem_inode;
        int id;
        get_sector_id_by_index(&child_directory_inode, i, &id);
        read_block(id, (char *)&tem_name);
        int tem_inode_id = tem_name.inode_id;
        get_inode(tem_inode_id, &tem_inode);
        if (tem_inode.file_type == 2)
        {
            delete_file_from_directory(&child_directory_inode, tem_name.name);
        }
        else
        {
            delete_directory_from_directory(&child_directory_inode, tem_name.name);
        }
    }
    remove_name(father_inode, name);
    BITMAP[child_directory_inode.sector_id] = '0';
    store_bitmap();
    return 0;
}
int list_directory(Inode *inode, char *data_return)
{
    if (inode->file_type == 2)
    {
        return -1;
    }
    char name[256][256];
    int file_num;
    list_file_in_current_directory(inode, name, &file_num);
    for (int i = 0; i < file_num; i++)
    {
        char file_name[252];
        memcpy(file_name, name[i], 252);
        int file_type;
        int *file_type_inname = (int *)name[i];
        file_type = file_type_inname[63];
        if (file_type == 2)
        {
            printf("\033[0;37m%s\n", file_name);
            sprintf(data_return, "%s\033[0;37m%s\n", data_return, file_name);
        }
        else
        {
            printf("\033[0;34m%s\n\033[0;37m", file_name);
            sprintf(data_return, "%s\033[0;34m%s\n\033[0;37m", data_return, file_name);
        }
    }
    return 0;
}

int write_file(Inode *inode, int length, char *data)
{
    if (inode->file_type != 2)
    {
        return -1;
    }
    clear_file_content(inode);
    int blocknum_total = (length + 255) / 256;
    inode->file_size = length;
    for (int i = 0; i < blocknum_total; i++)
    {
        int id;
        allocate_new_block_to_inode(inode, &id);
        char data_write_to_block[256];
        memcpy(data_write_to_block, data + i * 256, 256);
        write_block(id, data_write_to_block);
    }
    write_inode_to_disk(inode);
    return 0;
}

int read_file(Inode *inode, int length, char *data)
{
    if (inode->file_type != 2)
    {
        return -1;
    }
    int file_size = inode->file_size;
    if (length > file_size)
    {
        return -1;
    }

    int blocknum_total = (length + 256) / 256;
    for (int i = 0; i < blocknum_total; i++)
    {
        char data_read_from_block[256];
        inode_read_data_to_disk(inode, i, data_read_from_block);
        memcpy(data + i * 256, data_read_from_block, 256);
    }
    return file_size;
}

int change_to_father_directory(Inode *inode)
{
    if (inode->file_type == 2)
    {
        return -1;
    }
    if (inode->pre_inode_sector_id == -1)
    {
        return -1;
    }
    write_inode_to_disk(inode);
    get_inode(inode->pre_inode_sector_id, inode);
}
int change_to_child_directory(Inode *inode, char *name)
{
    if (inode->file_type == 2)
    {
        return -1;
    }
    int child_inode_id;
    if (search_file_by_name(inode, name, &child_inode_id) == -1)
    {
        return -1;
    }
    Inode child_inode;
    get_inode(child_inode_id, &child_inode);
    if (child_inode.file_type == 2)
    {
        return -1;
    }
    write_inode_to_disk(inode);
    memcpy((char *)inode, (char *)&child_inode, 256);
    return 0;
}

int make_file(Inode *directory_inode, char *file_name)
{
    Inode file_inode;

    return create_file_in_current_dir(&file_inode, directory_inode, file_name);
}
int make_directory(Inode *directory_inode, char *directory_name)
{
    Inode child_directory_inode;
    return create_directory_in_current_dir(&child_directory_inode, directory_inode, directory_name);
}
int remove_file(Inode *directory_inode, char *file_name)
{
    return delete_file_from_directory(directory_inode, file_name);
}
int remove_directory(Inode *directory_inode, char *directory_name)
{
    return delete_directory_from_directory(directory_inode, directory_name);
}
int parse_path(char *path, char path_after[256][252], int *num)
{
    *num = 0;
    char *p;
    char tem_path[256];
    strcpy(tem_path, path);
    p = strtok(path, "/");
    while (p != NULL)
    {
        strcpy(path_after[*num], p);
        (*num)++;
        p = strtok(NULL, "/");
    }
    return 0;
}

int change_directory(Inode *directory, char *path)
{
    char path_parsed[256][252];
    int path_num = 0;
    parse_path(path, path_parsed, &path_num);
    write_inode_to_disk(directory);
    Inode tem_inode = *directory;
    for (int i = 0; i < path_num; i++)
    {
        if (strcmp(path_parsed[i], "..") == 0)
        {
            if (change_to_father_directory(&tem_inode) == -1)
            {
                return -1;
            }
        }
        else if (strcmp(path_parsed[i], ".") == 0)
        {
            continue;
        }
        else
        {
            if (change_to_child_directory(&tem_inode, path_parsed[i]) == -1)
            {
                return -1;
            }
        }
    }
    *directory = tem_inode;
    return 0;
}

int list_directory_(Inode *directory, char name[256][256])
{
}
int catch_file(Inode *directory, char *file_name, char *data)
{
    int file_inode_id;
    if (search_file_by_name(directory, file_name, &file_inode_id) == -1)
    {
        return -1;
    }
    Inode file_inode;
    get_inode(file_inode_id, &file_inode);
    if (file_inode.file_type != 2)
    {
        return -1;
    }
    int file_length = read_file(&file_inode, file_inode.file_size, data);
    data[file_length] = '\0';
    return file_length;
}
int write_file_for_operations(Inode *directory, char *file_name, int length, char *data)
{
    int file_inode_id;
    if (search_file_by_name(directory, file_name, &file_inode_id) == -1)
    {
        return -1;
    }
    Inode file_inode;
    get_inode(file_inode_id, &file_inode);
    return write_file(&file_inode, length, data);
}

int insert_file_data(Inode *directory, char *file_name, int pos, int length, char *data)
{
    Inode file_inode;
    int file_inode_id;
    if (search_file_by_name(directory, file_name, &file_inode_id) == -1)
    {
        return -1;
    }
    get_inode(file_inode_id, &file_inode);
    if (pos > file_inode.file_size)
    {
        pos = file_inode.file_size;
    }
    int new_length = file_inode.file_size + length;
    char buffer[new_length + 256];
    read_file(&file_inode, file_inode.file_size, buffer);
    for (int i = new_length - 1; i >= pos + length; i--)
    {
        buffer[i] = buffer[i - length];
    }
    for (int i = 0; i < length; i++)
    {
        buffer[pos + i] = data[i];
    }
    return write_file(&file_inode, new_length, buffer);
}
int delete_data_in_file(Inode *directory, char *file_name, int pos, int length)
{
    Inode file_inode;
    int file_inode_id;
    if (search_file_by_name(directory, file_name, &file_inode_id) == -1)
    {
        return -1;
    }
    get_inode(file_inode_id, &file_inode);
    if (pos + length > file_inode.file_size)
    {
        length = file_inode.file_size - pos;
    }
    int pre_length = file_inode.file_size;
    int new_length = file_inode.file_size - length;
    char buffer[pre_length + 256];
    read_file(&file_inode, pre_length, buffer);
    memcpy(buffer + pos, buffer + pos + length, pre_length - length - pos + 1);
    memset(buffer + pre_length - length, '\0', sizeof(buffer) - new_length);
    write_file(&file_inode, new_length, buffer);
    return 0;
}
int parse_command(char *line, char *command_array[])
{
    char *p;
    int count = 0;
    p = strtok(line, " ");

    while (p != NULL && count <= 5)
    {
        command_array[count] = p;
        count++;
        p = strtok(NULL, " ");
    }
    if (command_array[0] == NULL)
    {
        return 1;
    }
    if (strcmp(command_array[0], "f") == 0)
    {
        return 1;
    }
    if (strcmp(command_array[0], "e") == 0)
    {
        return 1;
    }
    if (strcmp(command_array[0], "mk") == 0)
    {
        if (count < 2)
        {
            return -1;
        }
        return 2;
    }
    if (strcmp(command_array[0], "mkdir") == 0)
    {
        if (count < 2)
        {
            return -1;
        }
        return 2;
    }
    if (strcmp(command_array[0], "rm") == 0)
    {
        if (count < 2)
        {
            return -1;
        }
        return 2;
    }
    if (strcmp(command_array[0], "cd") == 0)
    {
        if (count < 2)
        {
            return -1;
        }
        return 2;
    }
    if (strcmp(command_array[0], "rmdir") == 0)
    {
        if (count < 2)
        {
            return -1;
        }
        return 2;
    }
    if (strcmp(command_array[0], "ls") == 0)
    {
        return 1;
    }
    if (strcmp(command_array[0], "shift") == 0)
    {
        return 1;
    }
    if (strcmp(command_array[0], "cat") == 0)
    {
        if (count < 2)
        {
            return -1;
        }
        return 2;
    }
    if (strcmp(command_array[0], "w") == 0)
    {
        if (count < 4)
        {
            return -1;
        }
        return 4;
    }
    if (strcmp(command_array[0], "i") == 0)
    {
        if (count < 5)
        {
            return -1;
        }
        return 5;
    }
    if (strcmp(command_array[0], "d") == 0)
    {
        if (count < 4)
        {
            return -1;
        }
        return 4;
    }
}
int file_system_command_execute(int client_sockfd, char *commandline, Inode *current_directory, Inode *root, int length, char *data_return)
{
    char *command_array[10];
    int num_of_command = parse_command(commandline, command_array);
    if (num_of_command == -1)
    {
        return -1;
    }
    if (command_array[0] == NULL)
    {
        return 1;
    }
    if (strcmp(command_array[0], "f") == 0)
    {
        initial_the_file_system();
        get_inode(4, current_directory);
        get_inode(4, root);
        return 1;
    }
    if (strcmp(command_array[0], "e") == 0)
    {
        return 0;
    }
    if (strcmp(command_array[0], "mk") == 0)
    {
        char name[252];
        strcpy(name, command_array[1]);
        if (make_file(current_directory, name) == -1)
        {
            return -1;
        }
        return 2;
    }
    if (strcmp(command_array[0], "mkdir") == 0)
    {
        char name[252];
        strcpy(name, command_array[1]);
        if (make_directory(current_directory, name) == -1)
        {
            return -1;
        }
        return 2;
    }
    if (strcmp(command_array[0], "rm") == 0)
    {
        char name[252];
        strcpy(name, command_array[1]);
        if (remove_file(current_directory, name) == -1)
        {
            return -1;
        }
        return 2;
    }
    if (strcmp(command_array[0], "cd") == 0)
    {
        char path[256];
        strcpy(path, command_array[1]);
        if (change_directory(current_directory, path) == -1)
        {
            return -1;
        }
        return 2;
    }
    if (strcmp(command_array[0], "shift") == 0)
    {
        if (shift_flag == 0)
        {
            shift_flag = 1;
            get_inode(5, current_directory);
            strcpy(shift_name, "share");
        }
        else
        {
            *current_directory = usr_root_inode;
            shift_flag = 0;
            strcpy(shift_name, user_name_global);
        }
        return 1;
    }
    if (strcmp(command_array[0], "rmdir") == 0)
    {
        char name[252];
        strcpy(name, command_array[1]);
        if (remove_directory(current_directory, name) == -1)
        {
            return -1;
        }
        return 2;
    }
    if (strcmp(command_array[0], "ls") == 0)
    {
        char file_data[1024];
        if (list_directory(current_directory, file_data) == -1)
        {
            return -1;
        }
        strcpy(data_return, file_data);
        return 1;
    }
    if (strcmp(command_array[0], "cat") == 0)
    {
        char file_data[1024];
        char name[252];
        strcpy(name, command_array[1]);
        if (catch_file(current_directory, name, file_data) == -1)
        {
            return -1;
        }
        printf("%s\n", file_data);
        sprintf(data_return, "%s\n", file_data);
        return 2;
    }
    if (strcmp(command_array[0], "w") == 0)
    {
        char name[252];
        strcpy(name, command_array[1]);
        int length = atoi(command_array[2]);
        char data[1024];
        if (length > sizeof(commandline) - (int)(command_array[3] - commandline))
        {
            length = sizeof(commandline) - (int)(command_array[3] - commandline);
        }
        if (length <= 1024)
        {
            memcpy(data, command_array[3], length);
        }
        else
        {
            memcpy(data, command_array[3], 1024);
        }
        if (write_file_for_operations(current_directory, name, length, data) == -1)
        {
            return -1;
        }
        return 4;
    }
    if (strcmp(command_array[0], "i") == 0)
    {
        char name[252];
        int length = atoi(command_array[3]);
        int pos = atoi(command_array[2]);
        strcpy(name, command_array[1]);
        char data[1024];
        if (length > sizeof(commandline) - (int)(command_array[4] - commandline))
        {
            length = sizeof(commandline) - (int)(command_array[4] - commandline);
        }
        if (length <= 1024)
        {
            memcpy(data, command_array[4], length);
        }
        else
        {
            memcpy(data, command_array[4], 1024);
        }
        if (insert_file_data(current_directory, name, pos, length, data) == -1)
        {
            return -1;
        }
        return 5;
    }
    if (strcmp(command_array[0], "d") == 0)
    {
        char name[252];
        strcpy(name, command_array[1]);
        int length = atoi(command_array[3]);
        int pos = atoi(command_array[2]);
        if (delete_data_in_file(current_directory, name, pos, length) == -1)
        {
            return -1;
        }
        return 4;
    }
}
int create_user(Inode *root_inode, char *user_name, char *user_password);
int remove_user(Inode *root_inode, char *user_name, char *user_password);
int enter_user(Inode *root_inode, Inode *current_directory, char *user_name, char *user_password);

void create_server(int *sockfd, int port)
{
    struct sockaddr_in server_addr;
    *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (*sockfd == -1)
    {
        fprintf(stderr, "Error: cannot create the socket\n");
        exit(1);
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);
    if (bind(*sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        fprintf(stderr, "Error: cannot bind the server to the port\n");
        close(*sockfd);
        exit(1);
    }
    listen(*sockfd, 5);
    printf("Success: create the server\n");
    printf("Server is listening on port %d\n", port);
}

int create_user(Inode *root_inode, char *user_name, char *user_password)
{
    Inode user_inode;
    char name_of_inode[252];
    strcpy(name_of_inode, user_name);
    strcat(name_of_inode, user_password);
    int flag = create_directory_in_current_dir(&user_inode, root_inode, name_of_inode);
    if (flag == -1)
    {
        return -1;
    }
    user_inode.pre_inode_sector_id = -1;
    write_inode_to_disk(&user_inode);
    return 0;
}
int remove_user(Inode *root_inode, char *user_name, char *user_password)
{
    Inode user_inode;
    char name_of_inode[252];
    strcpy(name_of_inode, user_name);
    strcat(name_of_inode, user_password);
    int flag = delete_directory_from_directory(root_inode, name_of_inode);
    if (flag == -1)
    {
        return -1;
    }
    return 0;
}
int enter_user(Inode *root_inode, Inode *current_directory, char *user_name, char *user_password)
{
    char name_of_inode[252];
    strcpy(name_of_inode, user_name);
    strcat(name_of_inode, user_password);
    int flag = change_to_child_directory(current_directory, name_of_inode);
    if (flag == -1)
    {
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    char *Disk_Server_Address;
    int BDS_port;
    int FS_port;
    char *filename = "disk.bin";
    int fd = open(filename, O_RDWR | O_CREAT, 0);
    if (fd < 0)
    {
        printf("Error:Could not open file '%s'.\n", filename);
    }
    long FILESIZE = BLOCKSIZE * SECTORS * CYLINDERS;
    if (ftruncate(fd, FILESIZE) == -1)
    {
        perror("ftruncate fail\n");
        exit(EXIT_FAILURE);
    }
    diskfile = (char *)mmap(NULL, FILESIZE,
                            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (diskfile == MAP_FAILED)
    {
        close(fd);
        printf("Error:Could not map file.\n");
    }

    open_file_system();
    Inode root_of_system;
    get_inode(4, &root_of_system);
    Inode current_directory = root_of_system;
    current_directory = root_of_system;

    while (1)
    {
        char route[2048];
        char return_data[2048];
        memset(return_data, '\0', 2048);
        get_current_route(&current_directory, "", "mysystem", route);
        char command_line[1024];
        fgets(command_line, sizeof(command_line), stdin);
        if (sizeof(command_line) == 0)
        {
            continue;
        }
        command_line[strlen(command_line) - 1] = '\0';
        int flag = file_system_command_execute(0, command_line, &current_directory, &root_of_system, 0, return_data);
        if (flag == -1)
        {
            printf("error to execute the command\n");
        }
        if (flag == 0)
        {
            break;
        }
        write_inode_to_disk(&current_directory);
        get_inode(4, &root_of_system);
    }
    return 0;
}
