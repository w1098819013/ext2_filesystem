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

int checkInodeBitmapIfOccupied(int inodeOffset, int inodeCount, unsigned char* inodeBitmap){
    if (!occupied(inodeBitmap, inodeOffset))
    {
        toggleOccupied(inodeBitmap, inodeOffset, 1);
        updateInodeCount(-1); // updates super block and group descriptor free inode counts
        printf("Fixed: inode [%d] not marked as in-use\n", inodeOffset + 1);
        return 1;
    }
    return 0;
}

int checkBlockBitmapIfOccupied(struct ext2_inode* inode, int inodeNumber, unsigned char * blockBitmap){
    int totalNumOfFixes = 0;
    
    for(int iblockOffset = 0; iblockOffset < inode->i_blocks/2; iblockOffset++)
    {
        int blockOffset = inode->i_block[iblockOffset] -1;
        if(!occupied(blockBitmap, blockOffset)){
            toggleOccupied(blockBitmap, blockOffset, 1);
            updateBlockCount(-1); // updates super block and group descriptor free blocks count
            totalNumOfFixes++; 
        }
    }
    
    if(totalNumOfFixes > 0)
        printf("Fixed: %i in-use data blocks not marked in data bitmap for inode: [%i]\n", totalNumOfFixes, inodeNumber);

    return totalNumOfFixes;
}

int fixFreeBlockCount(){
    int fixesMade = 0;
    unsigned char* bitmap =  getBlockBitmap();
    int numOfBytes = getTotalNumberOfBlocks() / 8;
    int bitmapFreeCount = 0;
    
    // go thru block bitmap, and count number of free blocks. 
    for(int i = 0; i < numOfBytes; i++){
        for(int j = 0; j < 8; j++){
            if((bitmap[i] & (1 << j)) == 0){
                bitmapFreeCount++;
            }
        }        
    }

    // if that num is diff than block group or super block, update them and print
    int blockGroupFreeCount = getNumberOfFreeBlocks_BG();
    int difference = bitmapFreeCount - blockGroupFreeCount;
    if(difference != 0){
        if(difference < 0) difference *=-1;  // absolute it 
        fixesMade += difference;
        setBlockCount_BG(blockGroupFreeCount);
        printf("Fixed: block group's free blocks counter was off by %i compared to the bitmap\n", difference);
    }

    int superBlockFreeCount = getNumberOfFreeBlocks_SB();
    difference = bitmapFreeCount - superBlockFreeCount;
    if(difference != 0){
        if(difference < 0) difference *=-1;  // absolute it 
        fixesMade += difference;
        setBlockCount_SB(blockGroupFreeCount);
        printf("Fixed: super block's free blocks counter was off by %i compared to the bitmap\n", difference);
    }

    return fixesMade;
}

int fixFreeInodeCount(){
    int fixesMade = 0;
    int bitmapFreeCount = 0;

    int inodeCount = getTotalNumberOfInodes();
    unsigned char* inodeBitmap = getInodeBitmap();

    for (int i = 0; i < inodeCount; i++){
        if (!occupied(inodeBitmap, i)){
            bitmapFreeCount++;
        }
    }
    
    // if that num is diff than block group or super block, update them and print
    int blockGroupFreeCount = getNumberOfFreeInodes_BG();
    int difference = bitmapFreeCount - blockGroupFreeCount;
    if(difference != 0){
        if(difference < 0) difference *=-1;  // absolute it 
        fixesMade += difference;
        setInodeCount_BG(blockGroupFreeCount);
        printf("Fixed: block group's free inodes counter was off by %i compared to the bitmap\n", difference);
    }

    int superBlockFreeCount = getNumberOfFreeInodes_SB();
    difference = bitmapFreeCount - superBlockFreeCount;
    if(difference != 0){
        if(difference < 0) difference *=-1;  // absolute it 
        fixesMade += difference;
        setInodeCount_SB(blockGroupFreeCount);
        printf("Fixed: super block's free inodes counter was off by %i compared to the bitmap\n", difference);
    }

    return fixesMade;
}

// Takes in the inodeNumber of a directory and searches through the directory for inconsistencies
int performDirTreeFixes(int inodeNumber){
    int totalNumOfFixes = 0;
    int totalNumOfInodes = getTotalNumberOfInodes();
    unsigned char* inodeBitmap = getInodeBitmap();
    unsigned char* blockBitmap = getBlockBitmap();
    struct ext2_inode* inodeTable = getInodeTable();
    struct ext2_inode directory = inodeTable[inodeNumber - 1];

    int lastDataBlock = directory.i_blocks/2;
    for(int blockOffset = 0; blockOffset < lastDataBlock; blockOffset++)
    {
        struct ext2_dir_entry *current = (struct ext2_dir_entry *)getBlock(directory.i_block[blockOffset]);
        int rec_sum = 0;
        while (rec_sum < EXT2_BLOCK_SIZE)
        {
            if(current->inode <= 0){
                rec_sum += current->rec_len;
                current = (struct ext2_dir_entry *)getNextPointer(current, current->rec_len);
                continue;
            }

            struct ext2_inode *inode = &(inodeTable[current->inode -1]);
            char inodeFileType = getFileModeChar(inode->i_mode);
            char fileName[current->name_len + 1];
            memset(fileName, '\0', sizeof(char) *current->name_len + 1);
            strncpy(fileName, current->name, current->name_len);
            if (inodeFileType != '0')
            {
                char dirFiletype = getFileType(current->file_type);
                if (inodeFileType != dirFiletype){
                    totalNumOfFixes++;
                    current->file_type = convertIModeToFileType(inode->i_mode); // trust inode
                    printf("Fixed: Entry type vs inode mismatch: inode [%d]\n", current->inode);
                }

                totalNumOfFixes += checkInodeBitmapIfOccupied(current->inode - 1, totalNumOfInodes, inodeBitmap);

                if (inode->i_dtime != 0){
                    totalNumOfFixes++;
                    inode->i_dtime = 0;
                    printf("Fixed: valid inode marked for deletion: [%i]\n", current->inode);
                }

                totalNumOfFixes += checkBlockBitmapIfOccupied(inode, current->inode, blockBitmap);

                if (inodeFileType == DIRECTORY_CHAR && strncmp(fileName, ".", current->name_len) != 0 
                && strncmp(fileName, "..", current->name_len) != 0){ // recurse into it
                    totalNumOfFixes += performDirTreeFixes(current->inode);
                }
            }
            // shift pointers forward and continue searching this block
            rec_sum += current->rec_len;
            current = (struct ext2_dir_entry *)getNextPointer(current, current->rec_len);
        }
    }

    return totalNumOfFixes;
}


int checkDiskIntegrity(){
    int totalNumOfFixes = 0;
    totalNumOfFixes += fixFreeBlockCount();
    totalNumOfFixes += fixFreeInodeCount();
    totalNumOfFixes += performDirTreeFixes(EXT2_ROOT_INO);

    if (totalNumOfFixes > 0)
        printf("%i file system inconsistencies repaired!\n",totalNumOfFixes);
    else
        printf("No file system inconsistencies detected!");

    return 0;
}

int main(int argc, char **argv) {
    if(argc != 2) {
        fprintf(stderr, "Usage: %s <image file name> \n", argv[0]);
        exit(1);
    }
    
    fprintf(stderr, "file path: %s \n", argv[1]);
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED || disk == NULL) {
        perror("mmap");
        exit(1);
    }
    return checkDiskIntegrity();
}