#include "FileSystem.h"

#define CMD_MAX_SIZE            2048
#define BUFF_SIZE               1024

uint8_t data_buffer[BUFF_SIZE];
Super_block disk_sb;
std::map< uint8_t, std::vector<uint8_t> > dir_map;
std::map< char *, uint8_t > files;
uint8_t curr_work_dir = 127;

uint8_t fs_tokenize(char *command_str, char **tokens)
{
	char *token;
    uint8_t num;

	token = strtok(command_str, " \t");

	for (num = 1; num < 6; num++)
	{
		tokens[num - 1] = token;
		token = strtok(NULL, " \t");

        if (token == NULL)
        {
            break;
        }

        if (strcmp(tokens[0], "B") == 0)
        {
            tokens[1] = token;
            return 2;
        }
	}

    return num;
}

int fs_validate_name_length(char *name)
{
    size_t name_len = strlen(name);
    if (name_len > 5)
    {
        return -1;
    }
    return 0;
}

int fs_validate_block_num(int block_num)
{
    if ((block_num < 1) || (block_num > 127))
    {
        return -1;
    }
    return 0;
}

void fs_mount(char *new_disk_name)
{
    int fd = open(new_disk_name, O_RDONLY);
    if (fd == -1)
    {
        fprintf(stderr, "Error: Cannot find disk %s\n", new_disk_name);
        return;
    }

    Super_block new_disk_sb;
    read(fd, &new_disk_sb, BUFF_SIZE);

    // Consistency Check 1
    for (uint8_t i = 0; i < 16; i++)
    {
        //std::bitset<8> fb_byte(new_disk_sb.free_block_list[i]);
		uint8_t fb_byte = (uint8_t) new_disk_sb.free_block_list[i];

        for (uint8_t j = 0; j < 8; j++)
        {
            if ((i == 0) && (j == 0))
            {
                //if (fb_byte.test(7 - j) == false)
				if ((fb_byte & (1 << (7 - i))) == 0)
                {
                    fprintf(stderr, "Error: File system in %s is inconsistent (error code: 1)\n", new_disk_name);
                    return;
                }
                continue;
            }

            uint8_t block_num = (i * 8) + j;
            uint8_t used = 0;

            for (uint8_t k = 0; k < 126; k++)
            {
                Inode *curr_inode = &new_disk_sb.inode[k];
                if (curr_inode->used_size & (1 << 7))
                {
                    if ((curr_inode->dir_parent & (1 << 7)) == 0)
                    {
                        uint8_t size = curr_inode->used_size & 0x7F;

                        if (size > 0)
                        {
                            if ((block_num >= curr_inode->start_block)
                                && (block_num <= (curr_inode->start_block + size - 1)))
                            {
                                //if (fb_byte.test(7 - j) == false)
								if ((fb_byte & (1 << (7 - i))) == 0)                                {
                                    fprintf(stderr, "Error: File system in %s is inconsistent (error code: 1)\n", new_disk_name);
                                    return;
                                }
                                if (used)
                                {
                                    fprintf(stderr, "Error: File system in %s is inconsistent (error code: 1)\n", new_disk_name);
                                    return;
                                }
                                used = 1;
                            }
                        }
                    }
                }
            }

            //if (fb_byte.test(7 - j) && (used == 0))
			if ((fb_byte & (1 << (7 - i))) && (used == 0))
            {
                fprintf(stderr, "Error: File system in %s is inconsistent (error code: 1)\n", new_disk_name);
                return;
            }
        }
    }

    // Consistency Check 2
    for (uint8_t i = 0; i < 126; i++)
    {
        Inode *curr_inode = &new_disk_sb.inode[i];

        if (curr_inode->used_size & (1 << 7))
        {
            for (uint8_t j = 0; j < 126; j++)
            {
                if (i != j)
                {
                    Inode *cmp_inode = &new_disk_sb.inode[j];
                    if (cmp_inode->used_size & (1 << 7))
                    {
                        if (strncmp(curr_inode->name, cmp_inode->name, 5) == 0)
                        {
                            fprintf(stderr, "Error: File system in %s is inconsistent (error code: 2)\n", new_disk_name);
                            return;
                        }
                    }
                }
            }
        }
    }

    // Consistency Check 3
    for (uint8_t i = 0; i < 126; i++)
    {
        Inode *curr_inode = &new_disk_sb.inode[i];

        if ((curr_inode->used_size & (1 << 7)) == 0)
        {
            for (uint8_t i = 0; i < 5; i++)
            {
                if (curr_inode->name[i] != '\0')
                {
                    fprintf(stderr, "Error: File system in %s is inconsistent (error code: 3)\n", new_disk_name);
                    return;
                }
            }

            if ((curr_inode->used_size != 0)
                || (curr_inode->start_block != 0)
                || (curr_inode->dir_parent != 0))
            {
                fprintf(stderr, "Error: File system in %s is inconsistent (error code: 3)\n", new_disk_name);
                return;
            }
        }
        else
        {
            uint8_t non_zero_present = 0;

            for (uint8_t i = 0; i < 5; i++)
            {
                if (curr_inode->name[i] != '\0')
                {
                    non_zero_present = 1;
                    break;
                }
            }

            if (non_zero_present == 0)
            {
                fprintf(stderr, "Error: File system in %s is inconsistent (error code: 3)\n", new_disk_name);
                return;
            }
        }
    }

    // Consistency Check 4
    for (uint8_t i = 0; i < 126; i++)
    {
        Inode *curr_inode = &new_disk_sb.inode[i];

        if (curr_inode->used_size & (1 << 7))
        {
            if ((curr_inode->dir_parent & (1 << 7)) == 0)
            {
                if ((curr_inode->start_block < 1)
                    || (curr_inode->start_block > 127))
                {
                    fprintf(stderr, "Error: File system in %s is inconsistent (error code: 4)\n", new_disk_name);
                    return;
                }
            }
        }
    }

    // Consistency Check 5
    for (uint8_t i = 0; i < 126; i++)
    {
        Inode *curr_inode = &new_disk_sb.inode[i];

        if (curr_inode->used_size & (1 << 7))
        {
            if (curr_inode->dir_parent & (1 << 7))
            {
                uint8_t size = curr_inode->used_size & 0x7F;
                if ((curr_inode->start_block != 0) || (size != 0))
                {
                    fprintf(stderr, "Error: File system in %s is inconsistent (error code: 5)\n", new_disk_name);
                    return;
                }
            }
        }
    }

    // Consistency Check 6
    for (uint8_t i = 0; i < 126; i++)
    {
        Inode *curr_inode = &new_disk_sb.inode[i];

        if (curr_inode->used_size & (1 << 7))
        {
            uint8_t parent = curr_inode->dir_parent & 0x7F;

            if ((parent == 126) || (parent > 127))
            {
                fprintf(stderr, "Error: File system in %s is inconsistent (error code: 6)\n", new_disk_name);
                return;
            }

            if ((parent >= 0) && (parent <= 125))
            {
                Inode *parent_inode = &new_disk_sb.inode[parent];

                if (((parent_inode->used_size & (1 << 7)) == 0)
                    || (((parent_inode->dir_parent & (1 << 7)) == 0)))
                {
                    fprintf(stderr, "Error: File system in %s is inconsistent (error code: 6)\n", new_disk_name);
                    return;
                }
            }
        }
    }

    // Mount disk
    disk_sb = new_disk_sb;
    curr_work_dir = 127;

    dir_map.insert(std::pair< uint8_t, std::vector<uint8_t> >(curr_work_dir, std::vector<uint8_t>()));

    for (uint8_t i = 0; i < 126; i++)
    {
        Inode *curr_inode = &disk_sb.inode[i];

        if (curr_inode->used_size & (1 << 7))
        {
            files.insert(std::pair< char *, uint8_t >(curr_inode->name, i));

            if ((curr_inode->dir_parent & (1 << 7)) == 0)
            {
				uint8_t parent = curr_inode->dir_parent & 0x7F;
				std::map< uint8_t, std::vector<uint8_t> >::iterator it;

				it = dir_map.find(parent);
				if (it == dir_map.end())
				{
					dir_map.insert(std::pair< uint8_t, std::vector<uint8_t> >(parent, std::vector<uint8_t>(i)));
				}
				else
				{
					it->second.push_back(i);
				}
            }
        }
    }
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
                        size_t buff_len = strlen(cmd_args[1]);

                        if (buff_len <= BUFF_SIZE)
                        {
                            uint8_t *buff = (uint8_t *) cmd_args[1];

                            fs_buff(buff);
                            continue;
                        }
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
