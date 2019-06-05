#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include "ext2.h"
/*
-check if dest dir already exists, yes: (directory) EEXIST
-check if dest already exists, yes: (file )EEXIST

If not:
    Get a new inode:
    -check in the inode bitmap for a free inode, if no free inode, ENOSPC
    -Has a free inode, check in inode table for the corresponding inode (using offset find next one available, not number)
    -check in the inode table
    -if no free inode can be found, image integrity issue
    
    Free inode gotten:
    -check data blocks bitmap for a free block
    -no free blocks, return ENOSPC
    -Yes, go to the corresponding data block
    -allocate data block
    -create dir entries for "."(itself-> inode) and ".."(parent->    inode),

    Entries initialized, inode initialized:
    -increase the inode link count for this directory to 2,
    -increment the link count of parent inode by 1.

*/
int makeDirectory(char *destPath){//pass in the directory path to be made
    int stringLen = strnlen(destPath, 4096);
    char* pathDummy1 = strndup(destPath, stringLen);
    char* pathDummy2 = strndup(destPath, stringLen);
    char* destParentPath = dirname(pathDummy1);
    char* destDirName = basename(pathDummy2);
    int destParentInodeOff = findInodeOffsetFromPath(destParentPath);
    struct ext2_inode* inodetable = getInodeTable();

    if (destParentInodeOff == -ENOENT){ //if inode already exists
        return EEXIST;
    }

    if (destParentInodeOff == -ENOENT){ //if parent inode does not exist, the path is invalid
        printf("Parent does not exist\n");
        return ENOENT;
    }
    
    int dirInodeNum = allocateNewInode(); //get the free inode offset
    if (dirInodeNum == -ENOSPC){ //No more free inodes
        return ENOSPC;
    }

    struct ext2_inode parentInode = inodetable[destParentInodeOff];
    struct ext2_inode* dirInode = initializeInode(dirInodeNum, EXT2_S_IFDIR, EXT2_BLOCK_SIZE);
    dirInode->i_links_count += 1;

    int blockNumber = allocateNextAvailableBit(getBlockBitmap(), getTotalNumberOfBlocks()) + 1; //get the free block number
    if (blockNumber == -ENOSPC){
        return blockNumber;
    }

    dirInode->i_blocks = 2;
    dirInode->i_block[0] = blockNumber;

    //Gets here only if there is free inode and free data block
    //create the directory entries for "." and ".." links
    struct ext2_dir_entry* selfDir = createNewDirectoryEntry(destDirName, dirInodeNum, EXT2_FT_DIR);
    struct ext2_dir_entry* selfLink = createNewDirectoryEntry(".", dirInodeNum, EXT2_FT_SYMLINK);
    struct ext2_dir_entry* parentLink = createNewDirectoryEntry("..", destParentInodeOff, EXT2_FT_SYMLINK);

    //now...decrease the free_inode count and free_blocks count of super block by 1
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    sb->s_free_inodes_count -= 1;
    sb->s_free_blocks_count -= 1;
    selfLink -> rec_len = EXT2_BLOCK_SIZE;
    struct ext2_dir_entry* dirEntryList = (struct ext2_dir_entry*) getBlock(dirInode->i_block[0]);
    *dirEntryList = *selfLink;
    //hardcode 0, change later
    //using the block number to get to that block.

    //get the parent's rec_len (sum of size of structs), store that rec_len into the parentlink
    
    int attachResult1 = attachDirectoryEntry(&parentInode, selfDir); 
    //int attachResult2 = attachDirectoryEntry(dirInode, selfLink); //stuck in infinite loop
    
    int attachResult3 = attachDirectoryEntry(dirInode, parentLink);

    if(attachResult1 == -ENOENT || attachResult3 == -ENOENT){
        printf("Attach entry failed!!!\n");
        return -ENOENT;
    }

    return attachResult1;
}


int main(int argc, char **argv) {
    if(argc != 3) {
        fprintf(stderr, "Usage: <image file name> <absolute path> \n");
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    int inodeNum = findInodeOffsetFromPath(argv[2]);
    printf("inode offset is: %d\n", inodeNum);

    int return_stat = makeDirectory(argv[2]);
    printf("Mkdir result is: %d\n", return_stat);

    return 0;
}