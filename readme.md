# A Trivial UNIX File System

Ayushi Patel, CMPUT 379

## File System Implementation

### Data Structures
The pre-defined structures were used to load the superblock with inodes from the disk during mounting. A map was used to keep track of the files and directories within each directory. The key was the parent inode index, while the value was a set containing the inode index of each file and directory under the parent. A priority_queue was used to order the inodes in order of their starting block to perform defragmentation easily.

### Helper Functions
A couple helper functions were created to assist with the basic file system operations.
* fs_tokenize(): used to split the input commands into tokens
* fs_search_curr_dir(): used to search the current directory for a file or directory with the given name and return the index of the inode if found
* fs_set_free_blocks(): used to set bits in the free block list of the superblock structure to the given value
* fs_delete_r(): used to recursively delete directories
* A custom comparator function was also written to define the compare operation for the above mentioned priority queue.

### Command Parsing
Commands are parsed at whitespace characters to obtain the command arguments. If the command is "B", the command is only parsed until the first whitespace character. A name argument is checked to ensure a length of 5 or less. A size argument is checked to ensure a value between 0 and 127. A new size argument is checked to ensure a value between 1 and 127. A block number argument is checked to ensure a value between 0 and 126.

### fs_mount
This function takes the provided the disk name and loads the superblock into a temporary structure. Each consistency check was performed as described below:
1. For every bit in the free block list, every inode is checked. If the bit is 0, it is ensured that no inode is associated with the corresponding block number. If the bit is 1, it is ensured that exactly one inode is associated with the corresponding block number. This is done by checking if the block number corresponding to the bit is within the range of [start_block, start_block + size).
2. For every used inode in the superblock, its name is compared with the name of every other used inode with the same parent to ensure no duplicates exist.
3. For every inode in the superblock, it is ensured it has valid parameters. If the used bit is 0, it is ensured that all bits in every field are zero. If the used bit is 1, it is ensured that there is at least one bit that is set in the name field.
4. For every used inode in the superblock that belongs to a file, it is ensured that the start block is within the range of [1, 127].
5. For every used inode in the superblock that belongs to a directory, it is ensured that the start block and size are zero.
6. For every used inode in the superblock, it ensured that its parent inode index is within the range of [0, 125] or 127. If it is in the range, it is ensured that inode at this index is marked used and a directory.

If the superblock passes all the consistency checks, the mounting process is carried out. If a file system is already mounted, the corresponding disk is closed and the directory map is cleared. The superblock is saved to the main superblock structure and the current working directory is set to the root directory. The directory map is filled with the directories and the files and directories they contain.

### fs_create
This function creates a file or directory with the given name in the current working directory if a file or directory with the same name does not already exist. The inodes in the superblock are checked in order to find the first available block. If an unused inode is found and the size is zero, a directory is created. The inode parameters are updated accordingly and saved to the disk. If an unused inode is found and the size is non-zero, the bits of the free block list are checked to find size number of 0s in a row. If found, the free block list and inode parameters are updated accordingly and saved to the disk.

### fs_delete
This function deletes a file or directory with the given name in the current working directory if a file or directory with the same name exist.

### fs_read

### fs_write

### fs_buff

### fs_ls

### fs_resize

### fs_defrag

### fs_cd

## System Calls
* open(): used to open the disk
* close(): used to close the disk
* lseek(): used to set the file offset to a required value
* read(): used to read blocks of 1024 bytes from the disk
* write(): used to write blocks of various sizes to the disk

## Assumptions
It is assumed that the size and block number provided as command arguments will be a numerical character and not a alphabetical character.

## Testing
In addition to the sample tests provided on eClass, other custom tests were written and used. The tests covered the identifiable edge cases and generated all the possible errors. Files and directories were created. The tests mainly focused on filling up the data blocks and observing the impact the action had on the create and resize functions. Directories with directories and files were deleted to ensure directories were deleted recursively. Files were deleted in a way to create gaps between data blocks. Defragmentation was carried out on the disk and the result was checked to make sure that the data shifted properly. The test cases also covered the basic update buffer, read, write, print files and directories, and change working directory operations. Valgrind was also used to check for memory leaks. Other than the "still reachable" leaks introduced by the STL containers, no other memory leaks were found.

## Sources
The lecture notes, the lab slides, the man pages and the teaching assistants' guidance were used to complete this assignment.
