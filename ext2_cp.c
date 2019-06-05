#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include "ext2.h"
#include <libgen.h>

// source: file on native disk. destination: new file that is a copy of source, but in disk/destination instead
int copy(char* source, char * destination){
    // REMEMBER to make copies of the source, destination pointers if calling strtok or basename or dirname
    int stringLenSource = strnlen(source, 4096);
    // char* sourceDummy1  = strndup(source, stringLenSource);
    char* sourceDummy2  = strndup(source, stringLenSource);
    char* sourceBname = basename(sourceDummy2);

    int stringLenDest = strnlen(destination, 4096);
    char* destDummy1  = strndup(destination, stringLenDest);
    char* destDummy2  = strndup(destination, stringLenDest);
    char* destNameDummy = strndup(destination, stringLenDest);

    struct stat sb;
    struct ext2_inode* newInode; // Inode that will be initialized
    struct ext2_dir_entry* entryForParent; // new dir entry to be attached into the parent dir's block
    //struct ext2_dir_entry* entryForSelf; // new dir entry to be attached into the self's block
    struct ext2_inode* inodeTable = getInodeTable();
    struct ext2_inode destInode;

    int nextAvaiInodeOff = allocateNewInode();
    if (nextAvaiInodeOff == -ENOSPC){ //find the new available inode's offset.
        return ENOSPC;
    }

    // check if source is a legit path and it is a file. else ENOENT
    if (lstat(source, &sb) == -1) {
        perror("Lstat");
        return ENOENT;
    }

    if (!(S_ISREG(sb.st_mode) || S_ISLNK(sb.st_mode))){ // CHECK ON PIAZZA
        return ENOENT;
    }

    //source file variables
    int dataSize = sb.st_size; // the size of the source file.
    printf("dataSize is %d\n", dataSize);
    int numOfBlocksNeeded = 1 + ((dataSize - 1) / EXT2_BLOCK_SIZE); // figure out the number of blocks needed
    printf("Num of blocks needed %d\n", numOfBlocksNeeded);

    char* copyContent = malloc(dataSize);
    FILE* sourceFp = fopen(source, "r");
    if(sourceFp == NULL) return ENOENT;
    if (fread(copyContent, dataSize, 1, sourceFp) != 1) return ENOENT;

    if(getNumberOfFreeBlocks_SB() < numOfBlocksNeeded){ //compare the number of free blocks with the actual needed size.
        return ENOSPC;
    }

    int destInodeOffset = findInodeOffsetFromPath(destDummy2);
    // Cannot use destDummy2 anymore.
    destInode = inodeTable[destInodeOffset];

    // check if destination already exists.
    // if it does and it isn't a dir, return EEXIST
    char imode = getFileModeChar(destInode.i_mode);
    if (destInodeOffset != -ENOENT){//dest exists
        if (imode != 'd'){
            printf("Destination exists and is not a dir\n");
            return EEXIST;
        }
        // it is a dir, it does exist
        char* concatDummy = malloc(PATH_MAX);
        concatDummy = strndup(destDummy1, stringLenDest);
        strncat(concatDummy, sourceBname, strnlen(sourceBname, 4096)); //concatenate the basename of source, to be the file's new name
        //need to check if the newly concatenated path exist
        if (findInodeOffsetFromPath(concatDummy) != -ENOENT){
            printf("Concat path already exists: %s\n", concatDummy);
            return EEXIST;
        }

        //gets here when concatenated path is not an existing file
        //destPath is a valid directory, will be the parent
        //Hence, destInodeOff is actually the parent's offset

        //Initialize new Inode for the destFile
        newInode = initializeInode(nextAvaiInodeOff, EXT2_S_IFREG, dataSize);
        //Initialize new entry for parent attaching
        entryForParent = createNewDirectoryEntry(sourceBname, nextAvaiInodeOff, EXT2_FT_REG_FILE);
        int attachPrntRes = attachDirectoryEntry(&inodeTable[destInodeOffset], entryForParent); //this already sets the block number into the inode's i_block
        //int block_num = getLastBlockNumber(&inodeTable[destInodeOffset]);
        printf("Attach to parent returns: %d\n", attachPrntRes);

        //entryForSelf = createNewDirectoryEntry(sourceBname, nextAvaiInodeOff, EXT2_FT_REG_FILE);
        //entryForSelf->rec_len = EXT2_BLOCK_SIZE;
        //*fileEntryList = *entryForSelf;
        int copyresult = copyDataIntoNewDataBlocks(newInode, numOfBlocksNeeded, copyContent, dataSize);

        printf("Copy result for when dest is dir is: %d\n", copyresult);
        return copyresult;
    }

    // gets here when dest does not exist
    // check if destinations Parent is a legit path on disk. else ENOENT
    // get the source's parent, if invalid, return ENOENT
    char* destParentPath = dirname(destDummy1);
    int destParentOff = findInodeOffsetFromPath(destParentPath);
    if(destParentOff == -ENOENT){
        return ENOENT;
    }

    //Parent is valid, get the parent inode.
    struct ext2_inode destParent = inodeTable[destParentOff];
    newInode = initializeInode(nextAvaiInodeOff, EXT2_S_IFREG, dataSize);
    char* destBname = basename(destNameDummy);
    entryForParent = createNewDirectoryEntry(destBname, nextAvaiInodeOff, EXT2_FT_REG_FILE);
    int attachPrntRes2 = attachDirectoryEntry(&destParent, entryForParent); //this already sets the block number into the inode's i_block
    printf("Attach to parent returns: %d\n", attachPrntRes2);

    int copyresult = copyDataIntoNewDataBlocks(newInode, numOfBlocksNeeded, copyContent, dataSize);


    return copyresult;


}



int main(int argc, char **argv) {
    if(argc != 4 ) {
        fprintf(stderr, "Usage: %s <image file name> <source> <destination> \n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    if(strlen(argv[2]) > PATH_MAX || strlen(argv[3]) > PATH_MAX){
        return ENAMETOOLONG;
    }

    int copyResult = copy(argv[2], argv[3]);
    printf("Copy Result is: %d\n", copyResult);

    return copyResult;
}
