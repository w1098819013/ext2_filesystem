#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include "ext2.h"

// unsigned char *disk;
// unsigned int findInodeNumPath(char *path);
// void* getBlock(unsigned int offset);
// struct ext2_inode* getInodeTable();

int printFileContents(int inodeOffset){
    if(inodeOffset <= 0){
        printf("invalid inode offset\n");
        return -ENOENT;
    }
    struct ext2_inode* inodeTable = (struct ext2_inode*)getInodeTable();
    struct ext2_inode inode = inodeTable[inodeOffset];
    if(getFileModeChar(inode.i_mode) != 'f'){
        printf("wasn't a file. %c \n", getFileModeChar(inode.i_mode));
        return -ENOENT;
    }

    for (int i = 0; i < inode.i_blocks/ 2; i++){
        unsigned int blockPointer = inode.i_block[i];
        char* data = (char *)getBlock(blockPointer);
        int dataIndex = 0;
        while(data[dataIndex] != EOF && dataIndex != EXT2_BLOCK_SIZE){
            printf("%c", data[dataIndex]);
            dataIndex++;
        }
    }    
    return 0;
}

int main(int argc, char **argv) {
    if(argc != 3) {
        fprintf(stderr, "Usage: %s <image file name>\n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    int inodeOffset = findInodeOffsetFromPath(argv[2]);
    printf("inode offset is: %d\n", inodeOffset);
    printFileContents(inodeOffset);


    return 0;
}