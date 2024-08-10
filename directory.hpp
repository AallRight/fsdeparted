#ifndef DIRECTORY_HPP
#define DIRECTORY_HPP
#include "file.hpp"
int search_file_by_name(Inode *d_inode, char *name, int *id_return);
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

#endif