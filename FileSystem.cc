#include "FileSystem.h"

#define CMD_MAX_SIZE            2048
#define BUFF_SIZE               1024

#define CHECK_BIT(var, pos)		((var) & (1 << (pos)))

Super_block disk_sb;
char disk_name[50] = "";
uint8_t curr_dir = 0;

std::map< uint8_t, std::vector<uint8_t> > dir_map;
std::map<char *, uint8_t> files;

uint8_t data_buffer[BUFF_SIZE];

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

int fs_validate_name(char *name)
{
    size_t name_len = strlen(name);
    if (name_len < 5)
    {
		if (strcmp(name, ".") != 0)
		{
			if (strcmp(name, ".") != 0)
			{
				return 0;
			}
		}
    }
    return -1;
}

int fs_validate_block_num(int block_num)
{
    if ((block_num >= 1) && (block_num <= 127))
    {
        return 0;
    }
    return -1;
}

void fs_set_free_blocks(uint8_t start_block, uint8_t end_block, uint8_t value)
{
	for (uint8_t i = start_block; i <= end_block; i++)
	{
		uint8_t list_index = i / 8;
		uint8_t bit_index = 7 - (i % 8);

		if (value == 0)
		{
			disk_sb.free_block_list[list_index] &= ~(1 << bit_index);
		}
		else
		{
			disk_sb.free_block_list[list_index] |= (1 << bit_index);
		}
	}
}

int fs_check_for_file(char name[5])
{
	std::map< uint8_t, std::vector<uint8_t> >::iterator map_it;

	map_it = dir_map.find(curr_dir);
	if (!map_it->second.empty())
	{
		for (std::vector<uint8_t>::iterator it = map_it->second.begin(); it != map_it->second.end(); it++)
		{
			Inode *cmp_inode = &disk_sb.inode[*it];

			if (strncmp(name, cmp_inode->name, 5) == 0)
			{
				return (int) *it;
			}
		}
	}

	return -1;
}

void fs_mount(char *new_disk_name)
{
    int fd = open(new_disk_name, O_RDONLY);
    if (fd < 0)
    {
        fprintf(stderr, "Error: Cannot find disk %s\n", new_disk_name);
        return;
    }

    Super_block new_disk_sb;
    read(fd, &new_disk_sb, BUFF_SIZE);

    // Consistency Check 1
    for (uint8_t i = 0; i < 16; i++)
    {
		char fb_byte = new_disk_sb.free_block_list[i];

        for (uint8_t j = 0; j < 8; j++)
        {
            if ((i == 0) && (j == 0))
            {
				if (CHECK_BIT(fb_byte, 7 - j) == 0)
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
				if (CHECK_BIT(curr_inode->used_size, 7))
                {
					if (CHECK_BIT(curr_inode->dir_parent, 7) == 0)
                    {
                        uint8_t size = curr_inode->used_size & 0x7F;

                        if (size > 0)
                        {
                            if ((block_num >= curr_inode->start_block)
                                && (block_num <= (curr_inode->start_block + size - 1)))
                            {
								if (CHECK_BIT(fb_byte, 7 - j) == 0)
								{
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

			if (CHECK_BIT(fb_byte, 7 - j) && (used == 0))
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

        if (CHECK_BIT(curr_inode->used_size, 7))
        {
			uint8_t curr_parent = curr_inode->dir_parent & 0x7F;

			for (uint8_t j = 0; j < 126; j++)
            {
                if (i != j)
                {
                    Inode *cmp_inode = &new_disk_sb.inode[j];

                    if (CHECK_BIT(cmp_inode->used_size, 7))
                    {
						uint8_t cmp_parent = cmp_inode->dir_parent & 0x7F;

						if (curr_parent == cmp_parent)
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
    }

    // Consistency Check 3
    for (uint8_t i = 0; i < 126; i++)
    {
        Inode *curr_inode = &new_disk_sb.inode[i];

        if (CHECK_BIT(curr_inode->used_size, 7) == 0)
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

        if (CHECK_BIT(curr_inode->used_size, 7))
        {
			if (CHECK_BIT(curr_inode->dir_parent, 7) == 0)
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

        if (CHECK_BIT(curr_inode->used_size, 7))
        {
            if (CHECK_BIT(curr_inode->dir_parent, 7))
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

        if (CHECK_BIT(curr_inode->used_size, 7))
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

                if ((CHECK_BIT(parent_inode->used_size, 7) == 0)
                    || (CHECK_BIT(parent_inode->dir_parent, 7) == 0))
                {
                    fprintf(stderr, "Error: File system in %s is inconsistent (error code: 6)\n", new_disk_name);
                    return;
                }
            }
        }
    }

    // Mount disk
    disk_sb = new_disk_sb;
	strcpy(disk_name, new_disk_name);
    curr_dir = 127;

    dir_map.insert(std::pair< uint8_t, std::vector<uint8_t> >(curr_dir, std::vector<uint8_t>()));

    for (uint8_t i = 0; i < 126; i++)
    {
        Inode *curr_inode = &disk_sb.inode[i];

        if (CHECK_BIT(curr_inode->used_size, 7))
        {
            files.insert(std::pair<char *, uint8_t>(curr_inode->name, i));

            if (CHECK_BIT(curr_inode->dir_parent, 7) == 0)
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
	for (uint8_t i = 0; i < 126; i++)
    {
        Inode *curr_inode = &disk_sb.inode[i];

		if (CHECK_BIT(curr_inode->used_size, 7) == 0)
		{
			if (fs_check_for_file(name) >= 0)
			{
				// TODO: Test this line with name of length 5
				fprintf(stderr, "Error: File or directory %s already exists\n", name);
				return;
			}

			uint8_t start_block_num = 128;

			for (uint8_t j = 0; j < 16; j++)
		    {
				uint8_t fb_byte = (uint8_t) new_disk_sb.free_block_list[j];

				for (uint8_t k = 0; k < 8; k++)
		        {
					uint8_t block_num = (j * 8) + k;

					if (CHECK_BIT(fb_byte, 7 - k) == 0)
					{
						if (start_block_num != 128)
						{
							start_block_num = block_num;
						}

						if ((block_num + 1 - start_block_num) == size)
						{
							strncpy(curr_inode->name, name, 5);
							curr_inode->size = 0x80 | size;
							curr_inode->start_block = start_block_num;

							if (size == 0)
							{
								curr_inode->dir_parent = 0x80 | curr_dir;
							}
							else
							{
								curr_inode->dir_parent = curr_dir;
							}

							fs_set_free_blocks(start_block_num, block_num, 1);

							files.insert(std::pair<char *, uint8_t>(curr_inode->name, start_block_num));
							map_it->second.push_back(i);

							return;
						}
					}
					else
					{
						start_block_num = 128;
					}
				}
			}

			// TODO: Test this line with name of length 5
			fprintf(stderr, "Error: Cannot allocate %s on %s\n", name, disk_name);
			return;
		}
	}

	// TODO: Test this line with name of length 5
	fprintf(stderr, "Error: Superblock in disk %s is full, cannot create %s\n", disk_name, name);
}

void fs_delete_r(uint8_t inode)
{
	
}

void fs_delete(char name[5])
{
	int inode_index = fs_check_for_file(name);

	if (inode_index < 0)
	{
		// TODO: Test this line with name of length 5
		fprintf(stderr, "Error: File or directory %s does not exist\n", name);
	}
	else
	{
		fs_delete_r((uint8_t) inode_index);
	}
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

	// TODO: Validate size

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

                        if (fs_validate_name(name) == 0)
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

                        if (fs_validate_name(name) == 0)
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

                        if ((fs_validate_name(name) == 0)
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

                        if ((fs_validate_name(name) == 0)
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

                        if (fs_validate_name(name) == 0)
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

                        if (fs_validate_name(name) == 0)
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
