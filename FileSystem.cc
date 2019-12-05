#include "FileSystem.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <set>
#include <map>
#include <vector>
#include <queue>


// Max command size
#define CMD_MAX_SIZE            2048

// Check bit macro
#define CHECK_BIT(var, pos)		((var) & (1 << (pos)))

// Disk parameters
int disk_fd = -1;
Super_block disk_sb;
char disk_name[50] = "";

uint8_t curr_dir = 0;
std::map< uint8_t, std::set<uint8_t> > dir_map;

uint8_t data_buffer[1024];

// Custom comparator for priority_queue
struct custom_compare
{
	bool operator()(const uint8_t a, const uint8_t b)
	{
		return disk_sb.inode[a].start_block > disk_sb.inode[b].start_block;
	}
};


/**
* @brief 	Tokenize command and return number of tokens
* @param 	command_str - command to tokenize
* @param 	argv - char* array that will contain command arguments
* @return 	number of command arguments
*/
uint8_t fs_tokenize(char *command_str, char **argv)
{
	char *token;
    uint8_t num;
	char delim[4] = " \t";

	token = strtok(command_str, &delim[0]);

	// Do not tokenize at whitespace if command is B
	if (strcmp(token, "B") == 0)
	{
		delim[0] = 0;
	}

	for (num = 1; num < 5; num++)
	{
		argv[num - 1] = token;

		token = strtok(NULL, &delim[0]);
        if (token == NULL)
        {
            break;
        }
	}

    return num;
}


/**
* @brief 	Search for name in current directory and return inode index if
* 			found
* @param 	name - name of file or directory to find
* @return 	-1 if not found, otherwise inode index
*/
int fs_search_curr_dir(char name[5])
{
	if (!dir_map[curr_dir].empty())
	{
		for (std::set<uint8_t>::iterator it = dir_map[curr_dir].begin(); it != dir_map[curr_dir].end(); it++)
		{
			Inode *inode = &disk_sb.inode[*it];
			if (strncmp(name, inode->name, 5) == 0)
			{
				return (int) *it;
			}
		}
	}

	return -1;
}


/**
* @brief 	Set bits to value in free block list of disk superblock
* @param 	start_block - first block to set to value
* @param 	end_block - end block to set to value
* @param 	value - 0 or 1
*/
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


/**
* @brief 	Perform consistency checks on disk and mount if valid
* @param 	new_disk_name - name of disk to mount
*/
void fs_mount(char *new_disk_name)
{
    int fd = open(new_disk_name, O_RDWR);
    if (fd < 0)
    {
		// Unable to open disk
		fprintf(stderr, "Error: Cannot find disk %s\n", new_disk_name);
        return;
    }

	// Get superblock of disk
	Super_block new_disk_sb;
    read(fd, &new_disk_sb, 1024);

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
					// Superblock marked not used in free block list
					close(fd);
					fprintf(stderr, "Error: File system in %s is inconsistent (error code: 1)\n", new_disk_name);
                    return;
                }
                continue;
            }

            uint8_t block_num = (i * 8) + j;
            uint8_t used = 0;

            for (uint8_t k = 0; k < 126; k++)
            {
                Inode *inode = &new_disk_sb.inode[k];
				if (CHECK_BIT(inode->used_size, 7))
                {
					if (CHECK_BIT(inode->dir_parent, 7) == 0)
                    {
                        uint8_t size = inode->used_size & 0x7F;
                        if (size > 0)
                        {
                            if ((block_num >= inode->start_block) && (block_num <= (inode->start_block + size - 1)))
                            {
								if (CHECK_BIT(fb_byte, 7 - j) == 0)
								{
									// Used block marked not used in free block list
									close(fd);
									fprintf(stderr, "Error: File system in %s is inconsistent (error code: 1)\n", new_disk_name);
                                    return;
                                }
                                if (used)
                                {
									// Block marked used for two files
									close(fd);
									fprintf(stderr, "Error: File system in %s is inconsistent (error code: 1)\n", new_disk_name);
                                    return;
                                }
                                used = 1;
                            }
                        }
                    }
                }
            }

			// Unused block marked used in free block list
			if (CHECK_BIT(fb_byte, 7 - j) && (used == 0))
            {
				close(fd);
				fprintf(stderr, "Error: File system in %s is inconsistent (error code: 1)\n", new_disk_name);
                return;
            }
        }
    }

    // Consistency Check 2
    for (uint8_t i = 0; i < 126; i++)
    {
        Inode *inode = &new_disk_sb.inode[i];
        if (CHECK_BIT(inode->used_size, 7))
        {
			uint8_t parent = inode->dir_parent & 0x7F;
			for (uint8_t j = 0; j < 126; j++)
            {
                if (i != j)
                {
                    Inode *cmp_inode = &new_disk_sb.inode[j];
                    if (CHECK_BIT(cmp_inode->used_size, 7))
                    {
						uint8_t cmp_parent = cmp_inode->dir_parent & 0x7F;
						if (parent == cmp_parent)
						{
							if (strncmp(inode->name, cmp_inode->name, 5) == 0)
	                        {
								// Two files with same name in same directory
								close(fd);
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
        Inode *inode = &new_disk_sb.inode[i];
        if (CHECK_BIT(inode->used_size, 7) == 0)
        {
            for (uint8_t i = 0; i < 5; i++)
            {
                if (inode->name[i] != '\0')
                {
					// Non-zero characters in name for unused inode
					close(fd);
					fprintf(stderr, "Error: File system in %s is inconsistent (error code: 3)\n", new_disk_name);
                    return;
                }
            }

            if ((inode->used_size != 0) || (inode->start_block != 0) || (inode->dir_parent != 0))
            {
				// Non-zero parameters for unused inode
				close(fd);
				fprintf(stderr, "Error: File system in %s is inconsistent (error code: 3)\n", new_disk_name);
                return;
            }
        }
        else
        {
            uint8_t non_zero_present = 0;

            for (uint8_t i = 0; i < 5; i++)
            {
                if (inode->name[i] != '\0')
                {
                    non_zero_present = 1;
                    break;
                }
            }

            if (non_zero_present == 0)
            {
				// All zero characters in name for used inode
				close(fd);
				fprintf(stderr, "Error: File system in %s is inconsistent (error code: 3)\n", new_disk_name);
                return;
            }
        }
    }

    // Consistency Check 4
    for (uint8_t i = 0; i < 126; i++)
    {
        Inode *inode = &new_disk_sb.inode[i];
        if (CHECK_BIT(inode->used_size, 7))
        {
			if (CHECK_BIT(inode->dir_parent, 7) == 0)
            {
                if ((inode->start_block < 1) || (inode->start_block > 127))
                {
					// Invalid start block for file
					close(fd);
					fprintf(stderr, "Error: File system in %s is inconsistent (error code: 4)\n", new_disk_name);
                    return;
                }
            }
        }
    }

    // Consistency Check 5
    for (uint8_t i = 0; i < 126; i++)
    {
        Inode *inode = &new_disk_sb.inode[i];
        if (CHECK_BIT(inode->used_size, 7))
        {
            if (CHECK_BIT(inode->dir_parent, 7))
            {
                uint8_t size = inode->used_size & 0x7F;
                if ((inode->start_block != 0) || (size != 0))
                {
					// Non-zero start block or size for directory
					close(fd);
					fprintf(stderr, "Error: File system in %s is inconsistent (error code: 5)\n", new_disk_name);
                    return;
                }
            }
        }
    }

    // Consistency Check 6
    for (uint8_t i = 0; i < 126; i++)
    {
        Inode *inode = &new_disk_sb.inode[i];
        if (CHECK_BIT(inode->used_size, 7))
        {
            uint8_t parent = inode->dir_parent & 0x7F;

            if ((parent == 126) || (parent > 127))
            {
				// Invalid parent inode index
				close(fd);
				fprintf(stderr, "Error: File system in %s is inconsistent (error code: 6)\n", new_disk_name);
                return;
            }

            if ((parent >= 0) && (parent <= 125))
            {
                Inode *parent_inode = &new_disk_sb.inode[parent];
                if ((CHECK_BIT(parent_inode->used_size, 7) == 0) || (CHECK_BIT(parent_inode->dir_parent, 7) == 0))
                {
					// Invalid parent inode
					close(fd);
					fprintf(stderr, "Error: File system in %s is inconsistent (error code: 6)\n", new_disk_name);
                    return;
                }
            }
        }
    }

	if (disk_fd >= 0)
	{
		// Unmount old disk
		close(disk_fd);
		dir_map.clear();
	}

	// Mount new disk
	disk_fd = fd;
    disk_sb = new_disk_sb;
	strcpy(disk_name, new_disk_name);

	// Set current directory to root directory
    curr_dir = 127;

	// Generate directory map for new disk
    dir_map.insert(std::pair< uint8_t, std::set<uint8_t> >(curr_dir, std::set<uint8_t>()));
    for (uint8_t i = 0; i < 126; i++)
    {
        Inode *inode = &disk_sb.inode[i];
        if (CHECK_BIT(inode->used_size, 7))
        {
			uint8_t parent = inode->dir_parent & 0x7F;

			if (CHECK_BIT(inode->dir_parent, 7) == 0)
            {
				if (dir_map.find(parent) == dir_map.end())
				{
					dir_map.insert(std::pair< uint8_t, std::set<uint8_t> >(parent, std::set<uint8_t>()));
				}
            }
			else if (dir_map.find(i) == dir_map.end())
			{
				dir_map.insert(std::pair< uint8_t, std::set<uint8_t> >(i, std::set<uint8_t>()));
			}

			dir_map[parent].insert(i);
        }
    }
}


/**
* @brief 	Create file with provided name and size
* @param 	name - file name
* @param 	size - file size
*/
void fs_create(char name[5], int size)
{
	if (disk_fd < 0)
	{
		// No disk mounted
		fprintf(stderr, "Error: No file system is mounted\n");
		return;
	}

	for (uint8_t i = 0; i < 126; i++)
    {
        Inode *inode = &disk_sb.inode[i];
		if (CHECK_BIT(inode->used_size, 7) == 0)
		{
			if ((strcmp(name, ".") == 0) || (strcmp(name, "..") == 0))
			{
				// Reserved names
				fprintf(stderr, "Error: File or directory %s already exists\n", name);
				return;
			}

			if (fs_search_curr_dir(name) >= 0)
			{
				// Duplicate file name
				fprintf(stderr, "Error: File or directory %s already exists\n", name);
				return;
			}

			uint8_t start_block_num = 0;

			if (size > 0)
			{
				uint8_t found_space = 0;

				for (uint8_t j = 0; j < 16; j++)
				{
					char fb_byte = disk_sb.free_block_list[j];

					for (uint8_t k = 0; k < 8; k++)
					{
						uint8_t block_num = (j * 8) + k;
						if (CHECK_BIT(fb_byte, 7 - k) == 0)
						{
							if (start_block_num == 0)
							{
								// First empty block
								start_block_num = block_num;
							}

							if ((start_block_num + size - 1) == block_num)
							{
								// Reserve blocks in free block list
								fs_set_free_blocks(start_block_num, block_num, 1);
								found_space = 1;
								break;
							}
						}
						else
						{
							start_block_num = 0;
						}
					}

					if (found_space)
					{
						break;
					}
				}

				if (found_space == 0)
				{
					// Not enough contiguous empty blocks
					fprintf(stderr, "Error: Cannot allocate %d on %s\n", size, disk_name);
					return;
				}

				inode->dir_parent = curr_dir;
			}
			else
			{
				// Add directory to directory map
				dir_map.insert(std::pair< uint8_t, std::set<uint8_t> >(i, std::set<uint8_t>()));
				inode->dir_parent = 0x80 | curr_dir;
			}

			// Set inode parameters
			strncpy(inode->name, name, 5);
			inode->used_size = 0x80 | size;
			inode->start_block = start_block_num;

			// Update superblock on disk
			lseek(disk_fd, 0, SEEK_SET);
			write(disk_fd, disk_sb.free_block_list, 16);
			lseek(disk_fd, i * 8, SEEK_CUR);
			write(disk_fd, inode, 8);

			dir_map[curr_dir].insert(i);

			return;
		}
	}

	// No avaliable inode
	fprintf(stderr, "Error: Superblock in disk %s is full, cannot create %s\n", disk_name, name);
}


/**
* @brief 	Deletes files and directories recursively
* @param 	inode_index - index of inode to be deleted
*/
void fs_delete_r(uint8_t inode_index)
{
	Inode *inode = &disk_sb.inode[inode_index];

	if (CHECK_BIT(inode->dir_parent, 7))
	{
		// Recusively delete directories and files within directory
		if (!dir_map[inode_index].empty())
		{
			for (std::set<uint8_t>::iterator it = dir_map[inode_index].begin(); it != dir_map[inode_index].end(); it++)
			{
				fs_delete_r(*it);
			}
		}
	}
	else
	{
		// Delete file data
		size_t size = inode->used_size & 0x7F;
		uint8_t empty_buff[1024] = {0};
		lseek(disk_fd, inode->start_block * 1024, SEEK_SET);
		for (uint8_t i = 0; i < size; i++)
		{
			write(disk_fd, empty_buff, 1024);
		}

		// Update free block list on disk
		fs_set_free_blocks(inode->start_block, inode->start_block + size - 1, 0);
		lseek(disk_fd, 0, SEEK_SET);
		write(disk_fd, disk_sb.free_block_list, 16);
	}

	// Delete from parent directory
	uint8_t parent = inode->dir_parent & 0x7F;
	dir_map[parent].erase(inode_index);

	// Delete inode
	memset(inode->name, 0, 5);
	inode->used_size = 0;
	inode->start_block = 0;
	inode->dir_parent = 0;

	// Update inode on disk
	lseek(disk_fd, (inode_index + 2) * 8, SEEK_SET);
	write(disk_fd, inode, 8);
}


/**
* @brief 	Deletes files and directories
* @param 	name - index of file or directory to be deleted
*/
void fs_delete(char name[5])
{
	if (disk_fd < 0)
	{
		// No disk mounted
		fprintf(stderr, "Error: No file system is mounted\n");
		return;
	}

	int inode_index = fs_search_curr_dir(name);
	if (inode_index < 0)
	{
		// Cannot find file or directory with given name
		fprintf(stderr, "Error: File or directory %s does not exist\n", name);
		return;
	}

	// Delete file or directory recursively
	fs_delete_r((uint8_t) inode_index);
}


/**
* @brief 	Read block from file
* @param 	name - file to read from
* @param	block_num - block number relative to start block of file
*/
void fs_read(char name[5], int block_num)
{
	if (disk_fd < 0)
	{
		// No disk mounted
		fprintf(stderr, "Error: No file system is mounted\n");
		return;
	}

	int inode_index = fs_search_curr_dir(name);
	if (inode_index < 0)
	{
		// Cannot find file with given name
		fprintf(stderr, "Error: File %s does not exist\n", name);
		return;
	}

	Inode *inode = &disk_sb.inode[inode_index];
	if (CHECK_BIT(inode->dir_parent, 7))
	{
		// Given name belongs to directory
		fprintf(stderr, "Error: File %s does not exist\n", name);
		return;
	}

	if (block_num >= (inode->used_size & 0x7F))
	{
		// Block number is outside file blocks
		fprintf(stderr, "Error: %s does not have block %d\n", name, block_num);
		return;
	}

	// Read block into buffer
	lseek(disk_fd, (inode->start_block + block_num) * 1024, SEEK_SET);
	read(disk_fd, data_buffer, 1024);
}


/**
* @brief 	Write block to file
* @param 	name - file to write to
* @param	block_num - block number relative to start block of file
*/
void fs_write(char name[5], int block_num)
{
	if (disk_fd < 0)
	{
		// No disk mounted
		fprintf(stderr, "Error: No file system is mounted\n");
		return;
	}

	int inode_index = fs_search_curr_dir(name);
	if (inode_index < 0)
	{
		// Cannot find file with given name
		fprintf(stderr, "Error: File %s does not exist\n", name);
		return;
	}

	Inode *inode = &disk_sb.inode[inode_index];
	if (CHECK_BIT(inode->dir_parent, 7))
	{
		// Given name belongs to directory
		fprintf(stderr, "Error: File %s does not exist\n", name);
		return;
	}

	if (block_num >= (inode->used_size & 0x7F))
	{
		// Block number is outside file blocks
		fprintf(stderr, "Error: %s does not have block %d\n", name, block_num);
		return;
	}

	// Write buffer to block
	lseek(disk_fd, (inode->start_block + block_num) * 1024, SEEK_SET);
	write(disk_fd, data_buffer, 1024);
}


/**
* @brief 	Flush and fill buffer with provided data
* @param 	buff - provided data
*/
void fs_buff(uint8_t buff[1024])
{
	if (disk_fd < 0)
	{
		// No disk mounted
		fprintf(stderr, "Error: No file system is mounted\n");
		return;
	}

	// Flush and copy data into buffer
	memset(data_buffer, 0, 1024);
	memcpy(data_buffer, buff, strlen((char *) buff));
}


/**
* @brief 	Print files and directories in current directory
*/
void fs_ls(void)
{
	if (disk_fd < 0)
	{
		// No disk mounted
		fprintf(stderr, "Error: No file system is mounted\n");
		return;
	}

	// Number of children in current directory
	int num_of_children = dir_map[curr_dir].size() + 2;
	printf("%-5s %3d\n", ".", num_of_children);

	// Number of children in parent directory
	if (curr_dir != 127)
	{
		Inode *inode = &disk_sb.inode[curr_dir];
		uint8_t parent = inode->dir_parent & 0x7F;
		num_of_children = dir_map[parent].size() + 2;
	}
	printf("%-5s %3d\n", "..", num_of_children);

	if (!dir_map[curr_dir].empty())
	{
		char name[6];
		for (std::set<uint8_t>::iterator it = dir_map[curr_dir].begin(); it != dir_map[curr_dir].end(); it++)
		{
			Inode *inode = &disk_sb.inode[*it];

			strncpy(name, inode->name, 5);
			name[5] = 0;

			if (CHECK_BIT(inode->dir_parent, 7))
			{
				// Number of children in directory
				num_of_children = dir_map[*it].size() + 2;
				printf("%-5s %3d\n", name, num_of_children);
			}
			else
			{
				// Size of file
				uint8_t size = inode->used_size & 0x7F;
				printf("%-5s %3d KB\n", name, size);
			}
		}
	}
}


/**
* @brief 	Resize file of provided name with new size
* @param 	name - file name
* @param 	new_size - new file size
*/
void fs_resize(char name[5], int new_size)
{
	if (disk_fd < 0)
	{
		// No disk mounted
		fprintf(stderr, "Error: No file system is mounted\n");
		return;
	}

	int inode_index = fs_search_curr_dir(name);
	if (inode_index < 0)
	{
		// Cannot find file with given name
		fprintf(stderr, "Error: File %s does not exist\n", name);
		return;
	}

	Inode *inode = &disk_sb.inode[inode_index];
	if (CHECK_BIT(inode->dir_parent, 7))
	{
		// Given name belongs to directory
		fprintf(stderr, "Error: File %s does not exist\n", name);
		return;
	}

	uint8_t size = inode->used_size & 0x7F;
	if (new_size > size)
	{
		uint8_t found_space = 1;

		for (uint8_t i = inode->start_block + size; i < (inode->start_block + new_size); i++)
		{
			uint8_t list_index = i / 8;
			uint8_t bit_index = 7 - (i % 8);
			char fb_byte = disk_sb.free_block_list[list_index];

			if (CHECK_BIT(fb_byte, bit_index))
			{
				found_space = 0;
				break;
			}
		}

		if (found_space)
		{
			// Found enough space after already allocated block
			fs_set_free_blocks(inode->start_block + size, inode->start_block + new_size - 1, 1);
			inode->used_size = 0x80 | new_size;

			// Update superblock on disk
			lseek(disk_fd, 0, SEEK_SET);
			write(disk_fd, disk_sb.free_block_list, 16);
			lseek(disk_fd, inode_index * 8, SEEK_CUR);
			write(disk_fd, inode, 8);

			return;
		}

		// Save existing free block list and remove allocated blocks from list
		char saved_free_block_list[16];
		memcpy(saved_free_block_list, disk_sb.free_block_list, 16);
		fs_set_free_blocks(inode->start_block, inode->start_block + size - 1, 0);

		uint8_t start_block_num = 0;

		for (uint8_t i = 0; i < 16; i++)
		{
			char fb_byte = disk_sb.free_block_list[i];

			for (uint8_t j = 0; j < 8; j++)
			{
				uint8_t block_num = (i * 8) + j;
				if (CHECK_BIT(fb_byte, 7 - j) == 0)
				{
					if (start_block_num == 0)
					{
						// First empty block
						start_block_num = block_num;
					}

					if ((start_block_num + new_size - 1) == block_num)
					{
						// Reserve blocks in free block list
						fs_set_free_blocks(start_block_num, block_num, 1);
						found_space = 1;
						break;
					}
				}
				else
				{
					start_block_num = 0;
				}
			}

			if (found_space)
			{
				// Move data
				uint8_t buff[1024];
				uint8_t empty_buff[1024] = {0};
				for (uint8_t i = 0; i < size; i++)
				{
					lseek(disk_fd, (inode->start_block + i) * 1024, SEEK_SET);
					read(disk_fd, buff, 1024);

					lseek(disk_fd, (start_block_num + i) * 1024, SEEK_SET);
					write(disk_fd, buff, 1024);

					lseek(disk_fd, (inode->start_block + i) * 1024, SEEK_SET);
					write(disk_fd, empty_buff, 1024);
				}

				// TODO
				// fs_set_free_blocks(inode->start_block, inode->start_block + size - 1, 0);

				// Update inode
				inode->used_size = 0x80 | new_size;
				inode->start_block = start_block_num;

				// Update superblock on disk
				lseek(disk_fd, 0, SEEK_SET);
				write(disk_fd, disk_sb.free_block_list, 16);
				lseek(disk_fd, inode_index * 8, SEEK_CUR);
				write(disk_fd, inode, 8);

				return;
			}
		}

		// Restore free block list and reject new size
		memcpy(disk_sb.free_block_list, saved_free_block_list, 16);
		fprintf(stderr, "File %s cannot expand to size %d\n", name, new_size);
	}
	else if (new_size < size)
	{
		// Delete data from blocks to deallocate
		size_t size = inode->used_size & 0x7F;
		uint8_t empty_buff[1024] = {0};
		lseek(disk_fd, (inode->start_block + new_size) * 1024, SEEK_SET);
		for (uint8_t i = 0; i < (size - new_size); i++)
		{
			write(disk_fd, empty_buff, 1024);
		}

		// Update superblock
		fs_set_free_blocks(inode->start_block + new_size, inode->start_block + size - 1, 0);
		inode->used_size = 0x80 | new_size;

		// Update superblock on disk
		lseek(disk_fd, 0, SEEK_SET);
		write(disk_fd, disk_sb.free_block_list, 16);
		lseek(disk_fd, inode_index * 8, SEEK_CUR);
		write(disk_fd, inode, 8);
	}
}


/**
* @brief 	Defragment disk
*/
void fs_defrag(void)
{
	if (disk_fd < 0)
	{
		// No disk mounted
		fprintf(stderr, "Error: No file system is mounted\n");
		return;
	}

	std::priority_queue<uint8_t, std::vector<uint8_t>, custom_compare> inodes;

	// Arrange inodes in order they appear on disk
	for (uint8_t i = 0; i < 126; i++)
	{
		Inode *inode = &disk_sb.inode[i];
		if (CHECK_BIT(inode->used_size, 7))
		{
			if (CHECK_BIT(inode->dir_parent, 7) == 0)
			{
				inodes.push(i);
			}
		}
	}

	// Set first available block to 1
	uint8_t next_available_block = 1;

	while(!inodes.empty())
	{
		uint8_t inode_index = inodes.top();

		Inode *inode = &disk_sb.inode[inode_index];
		uint8_t size = inode->used_size & 0x7F;

		// Check if file is occupying next "avaliable" block
		if (next_available_block < inode->start_block)
		{
			// Shift data
			uint8_t buff[1024];
			uint8_t empty_buff[1024] = {0};
			for (uint8_t i = 0; i < size; i++)
			{
				lseek(disk_fd, (inode->start_block + i) * 1024, SEEK_SET);
				read(disk_fd, buff, 1024);

				lseek(disk_fd, (next_available_block + i) * 1024, SEEK_SET);
				write(disk_fd, buff, 1024);

				lseek(disk_fd, (inode->start_block + i) * 1024, SEEK_SET);
				write(disk_fd, empty_buff, 1024);
			}

			// Update inode on disk
			inode->start_block = next_available_block;
			lseek(disk_fd, (inode_index + 2) * 8, SEEK_SET);
			write(disk_fd, inode, 8);
		}

		// Update next "available" block
		next_available_block = inode->start_block + size;

		inodes.pop();
	}

	// Update free block list on disk
	fs_set_free_blocks(0, next_available_block - 1, 1);
	if (next_available_block < 128)
	{
		fs_set_free_blocks(next_available_block, 127, 0);
	}
	lseek(disk_fd, 0, SEEK_SET);
	write(disk_fd, disk_sb.free_block_list, 16);
}


/**
* @brief 	Change working directory
* @param 	name - name of directory to change to
*/
void fs_cd(char name[5])
{
	if (disk_fd < 0)
	{
		// No disk mounted
		fprintf(stderr, "Error: No file system is mounted\n");
		return;
	}

	if (strcmp(name, ".") == 0)
	{
		// Already in current working directory
		return;
	}

	if (strcmp(name, "..") == 0)
	{
		if (curr_dir != 127)
		{
			// Change to parent directory
			Inode *inode = &disk_sb.inode[curr_dir];
			uint8_t parent = inode->dir_parent & 0x7F;
			curr_dir = parent;
		}
		return;
	}

	int inode_index = fs_search_curr_dir(name);
	if (inode_index < 0)
	{
		// Cannot find directory with given name
		fprintf(stderr, "Error: Directory %s does not exist\n", name);
		return;
	}

	Inode *inode = &disk_sb.inode[inode_index];
	if (CHECK_BIT(inode->dir_parent, 7) == 0)
	{
		// Given name belongs to file
		fprintf(stderr, "Error: Directory %s does not exist\n", name);
		return;
	}

	// Update current working directory
	curr_dir = inode_index;
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
	// TODO
	// while (fgets(cmd_str, CMD_MAX_SIZE, stdin) != NULL)
    {
        size_t cmd_len = strlen(cmd_str);

        if (cmd_len > 0)
        {
            if (cmd_str[cmd_len - 1] == '\n')
            {
                // Strip newline character
                cmd_str[cmd_len - 1] = '\0';
			}

            char *cmd_args[4] = {NULL};
            uint8_t cmd_args_num = fs_tokenize(cmd_str, cmd_args);

            char *cmd = cmd_args[0];

            if (strcmp(cmd, "M") == 0)
            {
				if (cmd_args_num == 2)
                {
                    char *new_disk_name = cmd_args[1];

                    fs_mount(new_disk_name);
					line_num++;
                    continue;
                }
            }
            else if (strcmp(cmd, "C") == 0)
            {
				if (cmd_args_num == 3)
                {
                    char *name = cmd_args[1];
                    int size = atoi(cmd_args[2]);

                    if ((strlen(name) <= 5) && (size >= 0) && (size <= 127))
                    {
						fs_create(name, size);
						line_num++;
                        continue;
                    }
                }
            }
            else if (strcmp(cmd, "D") == 0)
            {
                if (cmd_args_num == 2)
                {
                    char *name = cmd_args[1];

                    if (strlen(name) <= 5)
                    {
                        fs_delete(name);
						line_num++;
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

                    if ((strlen(name) <= 5) && (block_num >= 0) && (block_num <= 126))
                    {
                        fs_read(name, block_num);
						line_num++;
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

                    if ((strlen(name) <= 5) && (block_num >= 0) && (block_num <= 126))
                    {
                        fs_write(name, block_num);
						line_num++;
                        continue;
                    }
                }
            }
            else if (strcmp(cmd, "B") == 0)
            {
                if (cmd_args_num == 2)
                {
                    if (strlen(cmd_args[1]) <= 1024)
                    {
                        uint8_t *buff = (uint8_t *) cmd_args[1];

                        fs_buff(buff);
						line_num++;
                        continue;
                    }
                }
            }
            else if (strcmp(cmd, "L") == 0)
            {
                if (cmd_args_num == 1)
                {
                    fs_ls();
					line_num++;
                    continue;
                }
            }
            else if (strcmp(cmd, "E") == 0)
            {
                if (cmd_args_num == 3)
                {
                    char *name = cmd_args[1];
                    int new_size = atoi(cmd_args[2]);

                    if ((strlen(name) <= 5) && (new_size > 0) && (new_size <= 127))
                    {
						fs_resize(name, new_size);
						line_num++;
                        continue;
                    }
                }
            }
            else if (strcmp(cmd, "O") == 0)
            {
				if (cmd_args_num == 1)
                {
                    fs_defrag();
					line_num++;
                    continue;
                }
            }
            else if (strcmp(cmd, "Y") == 0)
            {
                if (cmd_args_num == 2)
                {
                    char *name = cmd_args[1];

                    if (strlen(name) <= 5)
                    {
                        fs_cd(name);
						line_num++;
                        continue;
                    }
                }
            }
        }

		// Invalid command
        fprintf(stderr, "Command Error: %s, %d\n", file_name, line_num);
        line_num++;
    }

	// Close disk
	if (disk_fd >= 0)
	{
		close(disk_fd);
	}
    fclose(fp);

    return 0;
}
