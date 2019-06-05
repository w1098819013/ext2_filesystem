#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <libgen.h>
#include <time.h>
#include "ext2.h"


// removes a file on disk image
int removeFileFromImage(char* fileToRemove)
{
    // Check if full path actually exists
    struct ext2_inode* sourceInode = getInodeFromPath(fileToRemove);
    if (sourceInode == NULL){
        printf("***inode is null:%s \n", fileToRemove);
        return ENOENT;
    }

    if(getFileModeChar(sourceInode->i_mode) == DIRECTORY_CHAR) return EISDIR;

    // Make sure parent directory exists
    char* sourcePathDummy = strndup(fileToRemove, strnlen(fileToRemove, 4096));
    char* sourcesParent = dirname(sourcePathDummy);
    if(strncmp(sourcesParent, ".", 2) == 0){ // unable to parse source name
        printf("parent %s\n", sourcePathDummy);
        return ENOENT;
    }

    struct ext2_inode* sourceParentInode = (struct ext2_inode*)getInodeFromPath(sourcesParent);
    if (sourceParentInode == NULL){
        printf("parent inode is null\n");
        return ENOENT;
    }

    // char* filePathCopy = strdup(fileToRemove);
    // char* fileName = basename(filePathCopy);
    // if(strncmp(fileName, ".", 2) == 0){
    //     printf("basename wasn't able to parse filename:%s\n",filePathCopy);
    //     return ENOENT;
    // }
    // no need to zero out data blocks, must set i_dtime in the inode, 
    // removing a directory entry must adjust its predecessor to indicate the next valid directory entry, etc
    
    // need to free up inode bitmap bit, block bitmap bits

    sourcePathDummy = strndup(fileToRemove, strnlen(fileToRemove, 4096));
    char* base = basename(sourcePathDummy);
    if(strncmp(base, ".", 2) == 0){ // unable to parse source name
        printf("base %s\n", sourcePathDummy);
        return ENOENT;
    }

    int inodeNumber = detachDirectoryEntry(sourceParentInode, base);
    if(inodeNumber<= 0) return ENOENT;

    toggleOccupied(getInodeBitmap(), inodeNumber - 1, 0);
    updateInodeCount(1);
    unsigned char* blockBitmap = getBlockBitmap();
    for(int iBlockOffset = 0; iBlockOffset < sourceInode->i_blocks/2; iBlockOffset++)
    {
        int blockOffset = sourceInode->i_block[iBlockOffset];
        toggleOccupied(blockBitmap, blockOffset -1, 0);
        updateBlockCount(1);
    }
    
    sourceInode->i_dtime = time(NULL);
    return 0;
}


int main(int argc, char **argv) {
    if(argc != 3 ) {
        fprintf(stderr, "Usage: %s <image file name> <path of file to delete>  \n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    if(strlen(argv[2]) > PATH_MAX){
        return ENAMETOOLONG;
    } 
    return removeFileFromImage(argv[2]);
}