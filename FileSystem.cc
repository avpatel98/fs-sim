#include "FileSystem.h"

#define COMMAND_MAX_SIZE		2048
#define BUFF_SIZE               1024

uint8_t data_buffer[BUFF_SIZE];

int fs_get_arg(char* arg_str)
{
    arg_str = strtok(NULL, " \t");
    if (arg_str == NULL)
    {
        return -1;
    }
    return 0;
}

int fs_validate_name_length(char *name)
{
    size_t name_len = str_len(name);
    if (name_len > 5)
    {
        return -1;
    }
    return 0;
}

int fs_validate_block_num(int block_num)
{
    if ((block_num < 1) || block_num > 127)
    {
        return -1;
    }
    return 0;
}

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
    uint32_t line_num = 0;

    // Read one line from input file at a time
    while (fgets(command_str, COMMAND_MAX_SIZE, fp) != NULL)
    {
        size_t command_len = strlen(command_str);
        if (command_len > 0)
        {
            if (command_str[command_len - 1] == '\n')
            {
                // Strip newline character
                command_str[command_len - 1] = '\0';

                char *command = strtok(command_str, " \t");

                if (strcmp(command, "M") == 0)
                {
                    char *new_disk_name;

                    if (fs_get_arg(new_disk_name) == 0)
                    {
                        if (strtok(NULL, " \t") == NULL)
                        {
                            fs_mount(new_disk_name);
                            continue;
                        }
                    }
                }
                else if (strcmp(command, "C") == 0)
                {
                    char *name;
                    char *size_str;
                    int size;

                    if (fs_get_arg(name) == 0)
                    {
                        if (fs_validate_name_length(name) == 0)
                        {
                            if (fs_get_arg(size_str) == 0)
                            {
                                size = atoi(size_str);
                                if (strtok(NULL, " \t") == NULL)
                                {
                                    fs_create(name, size);
                                    continue;
                                }
                            }
                        }
                    }
                }
                else if (strcmp(command, "D") == 0)
                {
                    char *name = strtok(command_str, " \t");


                    strtok(NULL, " \t");

                    if (fs_validate_name_length == 0)
                    {
                        fs_mount(new_disk_name);
                        continue;
                    }
                    fs_delete(name);
                }
                else if (strcmp(command, "R") == 0)
                {
                    char *name = strtok(command_str, " \t");
                    int block_num = atoi(strtok(NULL, " \t"));
                    fs_read(name, block_num);
                }
                else if (strcmp(command, "W") == 0)
                {
                    char *name = strtok(command_str, " \t");
                    int block_num = atoi(strtok(NULL, " \t"));
                    fs_write(name, block_num);
                }
                else if (strcmp(command, "B") == 0)
                {
                    uint8_t *buff = (uint8_t *) strtok(command_str, " \t");
                    fs_buff(buff);
                }
                else if (strcmp(command, "L") == 0)
                {
                    fs_ls();
                }
                else if (strcmp(command, "E") == 0)
                {
                    char *name = strtok(command_str, " \t");
                    int new_size = atoi(strtok(NULL, " \t"));
                    fs_resize(name, new_size);
                }
                else if (strcmp(command, "E") == 0)
                {
                    fs_defrag();
                }
                else if (strcmp(command, "Y") == 0)
                {
                    char *name = strtok(command_str, " \t");
                    fs_cd(name);
                }
            }
        }
    }

    fclose(fp);

    return 0;
}
