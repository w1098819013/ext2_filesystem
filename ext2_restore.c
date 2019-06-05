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

int restore(char* fileToRestore){

    struct ext2_inode* sourceInode = getInodeFromPath(fileToRestore);
    char* sourcePathDummy = strndup(fileToRestore, strnlen(fileToRestore, 4096));
    char* sourcesParent = dirname(sourcePathDummy);
    if (sourceInode != NULL){
        return EEXIST;
    }

    //if(getFileModeChar(sourceInode->i_mode) == DIRECTORY_CHAR) return EISDIR; //trying to restore dir

    if(strncmp(sourcesParent, ".", 2) == 0){ // unable to parse source name
        return ENOENT;
    }
    struct ext2_inode* sourceParentInode = getInodeFromPath(sourcesParent);
    if (sourceParentInode == NULL){
        return ENOENT;
    }

    if (getFileModeChar(sourceParentInode->i_mode) != DIRECTORY_CHAR){
        return EISDIR;
    }

    sourcePathDummy = strndup(fileToRestore, strnlen(fileToRestore, 4096));
    char* fileName = basename(sourcePathDummy);
    if(strncmp(fileName, ".", 2) == 0){ // unable to parse source name
        return ENOENT;
    }

    //Get the restore name, the to-be-restored size is name_len+8 round to 4.
    int toBeRestoredLen = RoundToTheNearestFour(sizeof(struct ext2_dir_entry*) + strlen(fileName));
    int lastDataBlockIndex = sourceParentInode -> i_blocks / 2;
    //walk the parent's inode block for the entry

    for(int blockOffset = 0; blockOffset < lastDataBlockIndex+1; blockOffset++)
    {
        struct ext2_dir_entry *previous = NULL;
        struct ext2_dir_entry* current = (struct ext2_dir_entry*) getBlock(sourceParentInode->i_block[blockOffset]); //currentBlock
        int rec_sum = 0;
        int inodeNumber = -ENOENT;
        while (rec_sum < EXT2_BLOCK_SIZE)
        {
            if (current->rec_len != toBeRestoredLen && strncmp(current->name, ".", current->name_len) != 0 && strncmp(current->name, "..", current->name_len)){// GAP found!!!

                // if the current rec_len is not equal to the supposed to be rec_len
                // and current is not the last entry in the block
                int actualRec_len = RoundToTheNearestFour(sizeof(&current) + current->name_len);
                rec_sum += actualRec_len;
                previous = current;
                current = (struct ext2_dir_entry *)getNextPointer(current, actualRec_len);

                if ((rec_sum + current->rec_len) == 1024){
                    //last entry of the list
                    //there might be entry after it that was deleted
                    //how to detect that deleted one?
                    if (actualRec_len != toBeRestoredLen){
                        current =(struct ext2_dir_entry *)getNextPointer(current, previous->rec_len + actualRec_len);
                    }
                }

                if (strncmp(current->name, fileName, strlen(fileName)) == 0 && current->name_len == strlen(fileName)){// entry found!!!
                    //toggle the inode bit in bitmap to 1
                    //toggle the block bit in bitmap to 1
                    inodeNumber = current->inode; //check that this inode has been reused in bitmap
                    if (checkBitUsed(getInodeBitmap(), inodeNumber-1)){
                        return ENOENT;
                    }

                    unsigned char* blockBitmap = getBlockBitmap();
                    struct ext2_inode* inodeTable = getInodeTable();
                    struct ext2_inode cur_inode = inodeTable[inodeNumber-1];
                    int curBlockCount = cur_inode.i_blocks / 2 + 1;
                    toggleOccupied(getInodeBitmap(), inodeNumber -1, 1);

                    for (int dblockIndex = 0; dblockIndex < curBlockCount; dblockIndex++){
                        if (cur_inode.i_block[dblockIndex] != 0){
                            if(checkBitUsed(blockBitmap, cur_inode.i_block[dblockIndex] -1)){
                                return ENOENT;
                            }
                        }
                        toggleOccupied(blockBitmap, cur_inode.i_block[dblockIndex] - 1, 1);
                    }
                    cur_inode.i_dtime=0;
                    previous->rec_len = RoundToTheNearestFour(sizeof(struct ext2_dir_entry*) + previous->name_len);
                    return 0;
                }

            }
            rec_sum += current->rec_len;
            previous = current;
            current =(struct ext2_dir_entry *)getNextPointer(current, current->rec_len);
        }
    }

    return 0;
}

    /*
    * Search the gaps of the directory entries in the data block.
    * How to detect the gap?
    * 
    * Calculate the length of the directory entry as (8 bytes + name length) rounded up to the nearest multiple of 4. 
    * Then compare this value with what is stored in rec_len. 
    * If both values are equal, then there's no gaps.
    * 
    * The last entry in any block may have an extended length even if it isn't followed by a deleted entry
    */

int main(int argc, char **argv) {
    if(argc != 3 ) {
        fprintf(stderr, "Usage: %s <image file name> <Absolute path to be restored> \n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    int restoreResult = restore(argv[2]);
    return restoreResult;
}