#include "FileSystem.h"

#define CMD_MAX_SIZE            2048
#define BUFF_SIZE               1024

uint8_t data_buffer[BUFF_SIZE];

uint8_t fs_tokenize(char *command_str, char **tokens)
{
	char *token;
    uint8_t num;

	token = strtok(command_str, " /t");
	for (num = 1; num < 6; num++)
	{
		tokens[num - 1] = token;
		token = strtok(NULL, " /t");
        if (token == NULL)
        {
            break;
        }
	}

    return num;
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
    char *file_name = argv[1];
    FILE *fp = fopen(file_name, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "Error: Failed to open input file\n");
        return -1;
    }

    char cmd_str[CMD_MAX_SIZE];
    int line_num = 1;

    // Read one line from input file at a time
    while (fgets(cmd_str, CMD_MAX_SIZE, fp) != NULL)
    {
        size_t cmd_len = strlen(cmd_str);
        if (cmd_len > 0)
        {
            if (cmd_str[cmd_len - 1] == '\n')
            {
                // Strip newline character
                cmd_str[cmd_len - 1] = '\0';

                char *cmd_args[5] = {NULL};
                uint8_t cmd_args_num = fs_tokenize(cmd_str, cmd_args);

                char *cmd = cmd_args[0];

                if (strcmp(cmd, "M") == 0)
                {
                    if (cmd_args_num == 2)
                    {
                        char *new_disk_name = cmd_args[1];

                        fs_mount(new_disk_name);
                        continue;
                    }
                }
                else if (strcmp(cmd, "C") == 0)
                {
                    if (cmd_args_num == 3)
                    {
                        char *name = cmd_args[1];
                        int size = atoi(cmd_args[2]);

                        if (fs_validate_name_length(name) == 0)
                        {
                            fs_create(name, size);
                            continue;
                        }
                    }
                }
                else if (strcmp(cmd, "D") == 0)
                {
                    if (cmd_args_num == 2)
                    {
                        char *name = cmd_args[1];

                        if (fs_validate_name_length(name) == 0)
                        {
                            fs_delete(name);;
                            continue;
                        }
                    }
                }
                else if (strcmp(cmd, "R") == 0)
                {
                    if (cmd_args_num == 3)
                    {
                        char *name = cmd_args[1];
                        int block_num = atoi(cmd_args[2]);

                        if ((fs_validate_name_length(name) == 0)
                            && (fs_validate_block_num(block_num) == 0))
                        {
                            fs_read(name, block_num);
                            continue;
                        }
                    }
                }
                else if (strcmp(cmd, "W") == 0)
                {
                    if (cmd_args_num == 3)
                    {
                        char *name = cmd_args[1];
                        int block_num = atoi(cmd_args[2]);

                        if ((fs_validate_name_length(name) == 0)
                            && (fs_validate_block_num(block_num) == 0))
                        {
                            fs_write(name, block_num);;
                            continue;
                        }
                    }
                }
                else if (strcmp(cmd, "B") == 0)
                {
                    if (cmd_args_num == 2)
                    {
                        uint8_t *buff = (uint8_t *) cmd_args[1];

                        fs_buff(buff);
                        continue;
                    }
                }
                else if (strcmp(cmd, "L") == 0)
                {
                    if (cmd_args_num == 1)
                    {
                        fs_ls();
                        continue;
                    }
                }
                else if (strcmp(cmd, "E") == 0)
                {
                    if (cmd_args_num == 3)
                    {
                        char *name = cmd_args[1];
                        int new_size = atoi(cmd_args[2]);

                        if (fs_validate_name_length(name) == 0)
                        {
                            fs_resize(name, new_size);
                            continue;
                        }
                    }
                }
                else if (strcmp(cmd, "E") == 0)
                {
                    if (cmd_args_num == 1)
                    {
                        fs_defrag();
                        continue;
                    }
                }
                else if (strcmp(cmd, "Y") == 0)
                {
                    if (cmd_args_num == 2)
                    {
                        char *name = cmd_args[1];

                        if (fs_validate_name_length(name) == 0)
                        {
                            fs_cd(name);;
                            continue;
                        }
                    }
                }
            }
        }

        fprintf(stderr, "Command Error: %s, %d\n", file_name, line_num);
        line_num++;
    }

    fclose(fp);

    return 0;
}
