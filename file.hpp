#ifndef FILE_HPP
#define FILE_HPP

#include "inode.hpp"
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


#endif