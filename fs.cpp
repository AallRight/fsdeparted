#include "file.hpp"
#include "directory.hpp"
using namespace std;
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