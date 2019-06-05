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
#include <libgen.h>
#include "ext2.h"


int makeHardLink(char* source, char* destination){
    // 1) Make sure that source is a valid path. Get Inode of source;
    struct ext2_inode* sourceInode = getInodeFromPath(source);
    printf("Source path is %s\n", source);
    if (sourceInode == NULL){
        printf("source inode is null\n");
        return ENOENT;
    }

    if(getFileModeChar(sourceInode->i_mode) == DIRECTORY_CHAR)
        return EISDIR;
    

    // 2) Make sure destination doesn't exist already,
    struct ext2_inode* destInode = (struct ext2_inode*)getInodeFromPath(destination);
    if(destInode != NULL){ 
        printf("dest inode exists already!\n");    
        return EEXIST;
    }

    // check validity of destination dirName
    char* destinationsDummy1 = strdup(destination);
    char* destinationsDummy2 = strdup(destination);
    char* destinationsParent = dirname(destinationsDummy1);
    char* destinationsLink = basename(destinationsDummy2);

    printf("parent dirname is: %s\n", destinationsDummy1);
    printf("dest name is: %s\n", destinationsDummy2);
    if(strncmp(destinationsParent, ".", 2) == 0){
        printf("destination's parent %s\n", destinationsParent);
        return ENOENT;
    }
   
    struct ext2_inode* destParentInode = (struct ext2_inode*)getInodeFromPath(destinationsParent);

    if (destParentInode == NULL){
        printf("parent inode is null\n");
        return ENOENT;
    }
    //name of to be created hard link
    if(strncmp(destinationsLink, ".", 2) == 0){
        printf("destBasename: %s\n", destinationsLink);
        return ENOENT;
    }

    //hard link
    int sourceInodeOffset = findInodeOffsetFromPath(source);
    unsigned char fileType = EXT2_FT_REG_FILE;
    struct ext2_dir_entry* hardlink = createNewDirectoryEntry(destinationsLink, sourceInodeOffset, fileType);

    int attachResult = attachDirectoryEntry(destParentInode, hardlink);
    if(attachResult < 0){
        printf("line 160: wrong result      %d\n", attachResult);
        return attachResult * -1; // flip negative error code to positive
    }
    // 4) Increment source inode.links
    sourceInode->i_links_count++;

    return 0;
}

int makeSymLink(char* source, char* destination){
    // validate destination dirName(source doesn't need validation)
    printf("source is: %s, destination is %s\n", source, destination);
    char *destPathCp = malloc(sizeof(char) * 400);
    strcpy(destPathCp, destination);

    char* destinationPathDummy = strndup(destination, strnlen(destination, 4096));
    char* destinationsParent = dirname(destinationPathDummy);
    if(strncmp(destinationsParent, ".", 2) == 0){ // unable to parse destination name
        printf("destination's parent %s\n", destinationsParent);
        return ENOENT;
    }
   
    // Make sure destination doesn't exist yet
    struct ext2_inode* checkDestination = getInodeFromPath(destination);
    if (checkDestination != NULL){
        printf("destination inode is NOT null:%s \n", destination);
        if(getFileModeChar(checkDestination->i_mode) == DIRECTORY_CHAR)
            return EISDIR;
        else
            return EEXIST;
    }

    struct ext2_inode* destParentInode = (struct ext2_inode*)getInodeFromPath(destinationsParent);
    if (destParentInode == NULL){
        printf("parent inode is null\n");
        return ENOENT;
    }

    int dataSize = sizeof(char) * strnlen(source, PATH_MAX);
    printf("dataSize is %d\n", dataSize);
    int numOfBlocksNeeded = 1 + ((dataSize - 1) / EXT2_BLOCK_SIZE); //  ceiling'd
    printf("numberOfBlocksNeeded is %d\n", numOfBlocksNeeded);

    if(getNumberOfFreeBlocks_SB() < numOfBlocksNeeded){
        return ENOSPC;
    }

    // Get a new free inode and initialize it
    int newInodeOffset = allocateNewInode();
    if(newInodeOffset < 0) return ENOSPC;
    printf("new inode offset is %i\n", newInodeOffset);
    struct ext2_inode* newInode = initializeInode(newInodeOffset, EXT2_S_IFLNK, dataSize);
        //name of to be created hard link
    
    destinationPathDummy = strndup(destination, strnlen(destination, 4096));
    char* destBasename = basename(destinationPathDummy);
    printf("destPathCp: %s\n", destPathCp);
    printf("destBasename: %s\n", destBasename);

    if(strncmp(destBasename, ".", 2) == 0){
        printf("destBasename: %s\n", destBasename);
        return ENOENT;
    }
    
    struct ext2_dir_entry* symLink = createNewDirectoryEntry(destBasename, newInodeOffset, EXT2_FT_SYMLINK);
    int attachResult = attachDirectoryEntry(destParentInode, symLink);
    if(attachResult < 0){
        printf("line 160: wrong result      %d\n", attachResult);
        return attachResult * -1; // flip negative error code to positive
    }
    int successfulCopy = copyDataIntoNewDataBlocks(newInode, numOfBlocksNeeded, source, dataSize);
    return successfulCopy * -1; 
}


int main(int argc, char **argv) {
    if(argc < 4 || argc > 5) {
        fprintf(stderr, "Hard Link Usage: %s <image file name> <source> <destination> \n", argv[0]);
        fprintf(stderr, "Symbolic Link Usage: %s <image file name> -s <source> <destination> \n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    
    if(argc == 4){
        printf("destination path from main: %s\n", argv[3]);
        if(strlen(argv[2]) > PATH_MAX || strlen(argv[3]) > PATH_MAX){
            return ENAMETOOLONG;
        } 
        return makeHardLink(argv[2], argv[3]);
    }
    else{ // Symbolic link
        if(strncmp(argv[2], "-s", 3) != 0){
            fprintf(stderr, "Invalid Flag.\nSymbolic Link Usage: %s <image file name> -s <source> <destination> \n", argv[0]);
            exit(1);
        }
        if(strlen(argv[3]) > PATH_MAX || strlen(argv[4]) > PATH_MAX){
            return ENAMETOOLONG;
        } 
        return makeSymLink(argv[3], argv[4]);
    }
}