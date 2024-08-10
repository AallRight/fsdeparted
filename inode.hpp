#ifndef INODE_HPP
#define INODE_HPP



#include "disk.hpp"
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


#endif

