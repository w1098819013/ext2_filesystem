#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"

unsigned char *disk;

void print_bitmap(unsigned char* bitmap, unsigned int sizeInBits)
{
    int numOfBytes = sizeInBits / 8;
    for(int i = 0; i < numOfBytes; i++)
    {
        for(int j = 0; j < 8; j++)
        {
            if(bitmap[i] & (1 << j)){
                printf("1");
            }else{
                printf("0");
            }
        }        
        printf(" ");
    }
    printf("\n");
}

//retrieve the specific block in the block group using pointer arithmetics.
//each block is of EXT2_BLOCK_SIZE size.
void* getBlock(unsigned char* disk, unsigned int offset){
    return (void*) (disk + offset * EXT2_BLOCK_SIZE);
}

//use the inode indexing and check in the inode bitmap
//if inode is occupied
int occupied(unsigned char *bitmap, int bit){
    unsigned char mask = 1 << (bit % 8);
    return bitmap[bit / 8] & mask;
}

/*
-returns corresponding character to inode's i_mode, 
-'0' if inode points to anything other than files or directories.
*/
char getFileModeChar(unsigned short imode){
    if (imode & EXT2_S_IFREG){
        return 'f';
    }
    else if (imode & EXT2_S_IFDIR){
        return 'd';
    }
    else{
        return '0';
    }
}

// inode itself is a pointer to the block that contains the data
/*
- i_block[]: a structure in inode that contains the block number of the block that is in use. 
- 0 if not used.
*/
void print_inode(struct ext2_inode* inodeTable, unsigned char *bitmap, int inodeNumber){
    struct ext2_inode inode = inodeTable[inodeNumber - 1];
    char fileType = getFileModeChar(inode.i_mode);
    printf("[%d] type: %c size: %d links: %d blocks: %d\n", 
        inodeNumber, fileType, inode.i_size, inode.i_links_count, inode.i_blocks);
    
    for (int i_block_count = 0; i_block_count < 15; i_block_count++){
        if (inode.i_block[i_block_count] != 0){
            printf("[%d] Blocks: %d\n", inodeNumber, inode.i_block[i_block_count]);
        }
        else{ // stop printing once the block number is "0" (un-used)
            break;
        }
    }
}

void print_all_inodes(struct ext2_inode* inodeTable, unsigned char* inodeBitmap, unsigned int inodeCount){
    // First print root inode
    print_inode(inodeTable, inodeBitmap, EXT2_ROOT_INO);

    // for every non-reserved inode, if it is occupied, print info.
    for (int i = EXT2_GOOD_OLD_FIRST_INO; i < inodeCount; i++){
        if (occupied(inodeBitmap, i)){
            print_inode(inodeTable, inodeBitmap, i+1);
        }
    }
}


int main(int argc, char **argv) {

    if(argc != 2) {
        fprintf(stderr, "Usage: %s <image file name>\n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    printf("Inodes: %d\n", sb->s_inodes_count);
    printf("Blocks: %d\n", sb->s_blocks_count);

    printf("Block group:\n");
    struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + 2*EXT2_BLOCK_SIZE);
    printf("    block bitmap: %d\n", gd->bg_block_bitmap);
    printf("    inode bitmap: %d\n", gd->bg_inode_bitmap);
    printf("    inode table: %d\n", gd->bg_inode_table);
    printf("    free blocks: %d\n", gd->bg_free_blocks_count);
    printf("    free inodes: %d\n", gd->bg_free_inodes_count);
    printf("    used_dirs: %d\n", gd->bg_used_dirs_count);

    printf("Block bitmap: ");
    unsigned char *blockbitmap = (unsigned char *)getBlock(disk, gd->bg_block_bitmap);
    print_bitmap(blockbitmap, sb->s_blocks_count);
    
    printf("Inode bitmap: ");
    unsigned char *inodeBitmap = (unsigned char *)getBlock(disk, gd->bg_inode_bitmap);
    print_bitmap(inodeBitmap, sb->s_inodes_count);

    struct ext2_inode* inodeTable = (struct ext2_inode*)getBlock(disk, gd->bg_inode_table);
    printf("Inodes:\n");
    print_all_inodes(inodeTable, inodeBitmap, sb->s_inodes_count);

    return 0;
}
