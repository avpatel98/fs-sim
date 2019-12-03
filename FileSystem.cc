#include "FileSystem.h"

#define COMMAND_MAX_SIZE		2048
#define BUFF_SIZE               1024

uint8_t data_buffer[BUFF_SIZE];

void fs_mount(char *new_disk_name)
{
}

void fs_create(char name[5], int size)
{
}

void fs_delete(char name[5])
{
}

void fs_read(char name[5], int block_num)
{
}

void fs_write(char name[5], int block_num)
{
}

void fs_buff(uint8_t buff[1024])
{
}

void fs_ls(void)
{
}

void fs_resize(char name[5], int new_size)
{
}

void fs_defrag(void)
{
}

void fs_cd(char name[5])
{
}

int main(int argc, char **argv)
{
    // Only handle one input file
    if (argc != 2)
    {
        fprintf(stderr, "Error: Invalid number of arguments\n");
        return -1;
    }

    // Open file for reading
    FILE *fp = fopen(argv[1], "r");
    if (fp == NULL)
    {
        fprintf(stderr, "Error: Failed to open input file\n");
        return -1;
    }

    char command_str[COMMAND_MAX_SIZE];

    // Read one line from input file at a time
    while (fgets(command_str, COMMAND_MAX_SIZE, fp) != NULL)
    {
        // Strip newline character or continue if empty command
        size_t command_len = strlen(command_str);
        if (command_len <= 0)
        {
            continue;
        }
        if (command_str[command_len - 1] == '\n')
        {
            if (command_len == 1)
            {
                continue;
            }
            else
            {
                command_str[command_len - 1] = '\0';
            }
        }

        char command = command_str[0];

        if (command == 'M')
        {
            fs_mount(&command_str[1]);
        }
        else if (command == 'C')
        {
            
        }
    }

    fclose(fp);

    return 0;
}
