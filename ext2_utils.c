#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"
#include <errno.h>
#include <string.h>

int RoundToTheNearestFour(int size){
    //takes in an integer
    //round to the nearest four
    if (size % 4 == 0){
        return size;
    }
    return (size + 4) - (size)%4;
}

//returns 1 if blockNum can be found in inode, 0 otherwise
int searchBlockNumber(struct ext2_inode* inode, int blockNum){
    int i;
    for (i = 0; i < 15; i++){
        if (inode->i_block[i] == blockNum){
            return 1;
        }
    }
    return 0;
}

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
void* getBlock(unsigned int number){
    return (void*) (disk + (number * EXT2_BLOCK_SIZE));
}

unsigned char * getBlockBitmap(){
    // struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + 2*EXT2_BLOCK_SIZE);    
    // unsigned char *blockbitmap = (unsigned char *)getBlock(gd->bg_block_bitmap);
    unsigned char *blockbitmap = (unsigned char *)getBlock(3);
    return blockbitmap;
}

void updateBlockCount(int valueToAdd){
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    sb->s_free_blocks_count += valueToAdd;
    struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + 2*EXT2_BLOCK_SIZE);
    gd->bg_free_blocks_count += valueToAdd;
}

unsigned int getNumberOfFreeBlocks_SB(){
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    return sb->s_free_blocks_count;
}

void setBlockCount_SB(int newValue){
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    sb->s_free_blocks_count = newValue;
}

unsigned short getNumberOfFreeBlocks_BG(){
    struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + 2*EXT2_BLOCK_SIZE);
    return gd->bg_free_blocks_count;
}

void setBlockCount_BG(int newValue){
    struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + 2*EXT2_BLOCK_SIZE);
    gd->bg_free_blocks_count = newValue; 
}

unsigned int getTotalNumberOfBlocks(){
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    return sb->s_blocks_count;
}

unsigned char * getInodeBitmap(){
    struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + 2*EXT2_BLOCK_SIZE);
    unsigned char *bitmap = (unsigned char *)getBlock(gd->bg_inode_bitmap);
    return bitmap;
}

struct ext2_inode* getInodeTable(){
    struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + 2*EXT2_BLOCK_SIZE);
    struct ext2_inode* inodeTable = (struct ext2_inode*)getBlock(gd->bg_inode_table);
    return inodeTable;
}

unsigned int getTotalNumberOfInodes(){
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    return sb->s_inodes_count;
}

unsigned int getNumberOfFreeInodes_SB(){
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    return sb->s_free_inodes_count;
}

void setInodeCount_SB(int newValue){
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    sb->s_free_inodes_count = newValue;
}

unsigned short getNumberOfFreeInodes_BG(){
    struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + 2*EXT2_BLOCK_SIZE);
    return gd->bg_free_inodes_count;
}

void setInodeCount_BG(int newValue){
    struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + 2*EXT2_BLOCK_SIZE);
    gd->bg_free_inodes_count = newValue; 
}


void updateInodeCount(int valueToAdd){
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    sb->s_free_inodes_count += valueToAdd;
    struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + 2*EXT2_BLOCK_SIZE);
    gd->bg_free_inodes_count += valueToAdd;
}

//use the inode indexing and check in the inode bitmap
//if inode is occupied
int occupied(unsigned char *bitmap, int bit){
    unsigned char mask = 1 << (bit % 8);
    return bitmap[bit / 8] & mask;
}

void toggleOccupied(unsigned char *bitmap, int bit, int on){
    unsigned char mask = 1 << (bit % 8);
    if (on) bitmap[bit/8] |= mask;
    else  bitmap[bit/8] &= ~mask;
}

/*
-returns corresponding character to inode's i_mode, 
-'0' if inode points to anything other than files or directories or symbolic links.
*/
char getFileModeChar(unsigned short imode){
    if (imode & EXT2_S_IFREG){
        return FILE_CHAR;
    }
    else if (imode & EXT2_S_IFDIR){
        return DIRECTORY_CHAR;
    }
    else if (imode & EXT2_S_IFLNK){
        return SYMBOLIC_CHAR;
    }
    else{
        return '0';
    }
}

char getFileType(unsigned char filetype){
    if ((unsigned short)filetype == EXT2_FT_REG_FILE){
        return FILE_CHAR;
    }
    else if ((unsigned short)filetype & EXT2_FT_DIR){
        return DIRECTORY_CHAR;
    }
    else{
        return SYMBOLIC_CHAR;
    }
}

unsigned char convertIModeToFileType(unsigned short imode){
    if (imode & EXT2_S_IFREG){
        return EXT2_FT_REG_FILE;
    }
    else if (imode & EXT2_S_IFDIR){
        return EXT2_FT_DIR;
    }
    else if (imode & EXT2_S_IFLNK){
        return EXT2_FT_SYMLINK;
    }
    else{
        return '0';
    }
}


// Helper function for pointer arithmetic
void* getNextPointer(void* pointer, unsigned short offset){
    return ((char*) pointer) + offset; 
}

struct ext2_dir_entry* createNewDirectoryEntry(char *LinkName, int sourceInodeOffset, unsigned char file_type){
    struct ext2_dir_entry* newEntry = malloc(sizeof(struct ext2_dir_entry));
    printf("Create new dir entry recieved a hardlink name of %s\n", LinkName);
    newEntry->inode = sourceInodeOffset+1;
    newEntry->name_len = strlen(LinkName); 
    // exclude the null terminator, 
    // strlen counts everything upto but not include null terminator
    newEntry->rec_len = RoundToTheNearestFour(sizeof(struct ext2_dir_entry) + newEntry->name_len);
    strncpy(newEntry->name, LinkName, newEntry->name_len); 
    //this copy contains the null terminator, find a way to chop it off

    // printf("old_entry name: %s\n", hardLinkName);
    // printf("newEntry name: %.*s\n", newEntry->name_len, newEntry->name);

    newEntry->file_type = file_type;

    return newEntry;
}

// Returns first available inode offset
int allocateNextAvailableInodeBit(){
    int inodeCount = getTotalNumberOfInodes();
    unsigned char* inodeBitmap = getInodeBitmap();

    for (int i = EXT2_GOOD_OLD_FIRST_INO; i < inodeCount; i++){
        if (!occupied(inodeBitmap, i)){
            toggleOccupied(inodeBitmap, i, 1);
            return i;
        }
    }
    return -ENOSPC;
}

// finds the free (data block)/inode, sets in bitmap, returns the OFFSET (not actual number)
// return ENOSPC if no available block/inode is found
int allocateNextAvailableBit(unsigned char* bitmap, unsigned int sizeInBits){
    int numOfBytes = sizeInBits / 8;
    for(int i = 0; i < numOfBytes; i++)
    {
        for(int j = 0; j < 8; j++)
        {
            if((bitmap[i] & (1 << j)) == 0){//if bit is 0 then it's free
                bitmap[i] |= (1 << j);
                return (i*8 + j); //returns the OFFSET
            }
        }        

    }
    return -ENOSPC;
}

// Returns offset of a new block that is allocated to this inode, if space is available
int allocateNewDataBlock(struct ext2_inode* inode){
    //struct ext2_inode* inode = getInode(inodeOffset);
    //check i_block[] for free slot
    //if yes, check Block bitmap for any free blocks, first free block found, allocate
    int numberOfBlocksUsed = inode->i_blocks / 2;
    if (numberOfBlocksUsed > 12){// TODO change to "13" for indirect pointer implementation
        return -ENOSPC;
    }
    printf("Inode's i_blocks count %d\n", inode->i_blocks);
    printf("Allocate New block at index %d\n", numberOfBlocksUsed);
    int availableBlock = allocateNextAvailableBit(getBlockBitmap(), getTotalNumberOfBlocks());
    if(availableBlock < 0) return -ENOSPC;

    int blockNumExists = searchBlockNumber(inode, availableBlock+1);
    if (blockNumExists){ //block num already exists
        updateBlockCount(-1);
        return availableBlock;
    }

    updateBlockCount(-1);
    inode->i_blocks += 2;
    inode->i_block[numberOfBlocksUsed] = availableBlock + 1; // convert offset to number 

    return availableBlock;
}

// Returns offset of a new inode that is allocated, if space is available
int allocateNewInode(){
    if (getNumberOfFreeInodes_SB() == 0){
        return -ENOSPC;
    }
    int availableInodeOffset = allocateNextAvailableInodeBit();
    if(availableInodeOffset < 0) return -ENOSPC;
    updateInodeCount(-1);
    return availableInodeOffset;
}

struct ext2_inode* initializeInode(int newInodeOffset, unsigned short imode, unsigned int i_size){
        // set variables of inode - file type (sym link), blocks, block[],  etc
    // TODO create inode Initializer function 
    struct ext2_inode* newInode = &(getInodeTable()[newInodeOffset]);    
    newInode->i_mode = imode;
	newInode->i_uid = 0;
	newInode->i_size = i_size; //TODO  if dir, this is sizeof 1 data block. if file, it is size of file (use stat) if it
                   //  is symlink, it is the length of the path in characters. could be up to 4096 chars!;
	newInode->i_gid = 0;
	newInode->i_links_count = 1;
	newInode->osd1 = 1;          /* OS dependent 1 */
	newInode->i_file_acl = 0;
	newInode->i_dir_acl = 0;
	newInode->i_faddr = 0;
	newInode->i_dtime = 0;
    return newInode;
}

int attachDirectoryEntry(struct ext2_inode* directory, struct ext2_dir_entry* newEntry){
    // go thru all data blocks of this directory
    // try to insert this new dir_entry into it
    // struct ext2_inode* dirInode = getInode(dirInodeOffset);//inode not gotten correctly?
    int lastDataBlock = directory->i_blocks/2;
    int inodeBlockOffset = lastDataBlock-1;
    //printf("last data block offset: %d\n", inodeBlockOffset);
    //printf("data block number: %d\n", directory->i_block[inodeBlockOffset]);

    struct ext2_dir_entry* dirEntryList = (struct ext2_dir_entry*) getBlock(directory->i_block[inodeBlockOffset]);
    
    int rec_sum = 0;

    while(rec_sum < EXT2_BLOCK_SIZE)
    {
        rec_sum += dirEntryList->rec_len; //not moving pointers correctly.
        printf("rec_sum: %d\n", rec_sum);
        if(rec_sum == EXT2_BLOCK_SIZE){
            //At the end of the block
            int sizeOfLastEntry = RoundToTheNearestFour(sizeof(struct ext2_dir_entry) + dirEntryList->name_len);
            int padding = dirEntryList->rec_len - sizeOfLastEntry;
            
            if(padding >= newEntry->rec_len){ //padding is sufficient
                newEntry -> rec_len = padding;
                dirEntryList -> rec_len = sizeOfLastEntry;
                struct ext2_dir_entry* locationForNewEntry = (struct ext2_dir_entry*) getNextPointer(dirEntryList, sizeOfLastEntry);
                //printf("newEntry name: %s\n",newEntry->name);
                *locationForNewEntry = *newEntry;
                strncpy(locationForNewEntry->name, newEntry->name, newEntry->name_len);//Is terminated though. need to be fixed
                
                return 0;
            }
            else{
                int newBlockOffset = allocateNewDataBlock(directory);
                printf("Allocate Data Block return val: %d\n", newBlockOffset);
                if(newBlockOffset < 0){ // ERROR
                    return newBlockOffset;
                }
                newEntry->rec_len = EXT2_BLOCK_SIZE;
                struct ext2_dir_entry* newBlock = (struct ext2_dir_entry*)getBlock(newBlockOffset);
                *newBlock = *newEntry;
                return 0;
            }
        }
        dirEntryList = (struct ext2_dir_entry*) getNextPointer(dirEntryList, dirEntryList->rec_len);
    }
    return -ENOENT; // TODO determine what this should be. This line should never be hit
}



// Returns inode number of the dir entry it detached. ENOENT on failure
int detachDirectoryEntry(struct ext2_inode* directory, char* fileName){
    // go thru all data blocks of this directory and remove entry once it is found
    int lastDataBlock = directory->i_blocks/2;
    for(int blockOffset = 0; blockOffset < lastDataBlock; blockOffset++)
    {
        struct ext2_dir_entry *previous = NULL;
        struct ext2_dir_entry *current = (struct ext2_dir_entry *)getBlock(directory->i_block[blockOffset]);
        int rec_sum = 0;
        int inodeNumber = -ENOENT;
        while (rec_sum < EXT2_BLOCK_SIZE)
        {
            // found it
            if(strcmp(current->name, fileName) == 0){

                inodeNumber = current->inode;
                // do stuff and return
                if(previous == NULL){ // then this entry is the first one in the data block. special case
                // as per documentation: 
                //If the first entry within the block is removed, a blank record will be created and point to the next directory entry or to the end of the block.
                    
                    // Will need to "zero" the dir entry, especially inode number
                    current->inode = 0;
                    // TODO not sure if we need to do anything else. like go to original inode and zero its data blocks..
                }else{
                    previous->rec_len += current->rec_len; 
                }
                return inodeNumber;
            }
            // shift pointers forward and continue searching this block
            rec_sum += current->rec_len; 
            previous = current; 
            current =(struct ext2_dir_entry *)getNextPointer(current, current->rec_len);

            printf("rec_sum: %d\n", rec_sum);
            }
    }
    return -ENOENT; // TODO determine what this should be. This line should never be hit
}

// inode itself is a pointer to the block that contains the data
/*
- i_block[]: a structure in inode that contains the block number of the block that is in use. 
- 0 if not used.
*/
void print_inode(struct ext2_inode *inodeTable, unsigned char *bitmap, int inodeNumber)
{
    struct ext2_inode inode = inodeTable[inodeNumber - 1];
    char fileType = getFileModeChar(inode.i_mode);
    printf("[%d] type: %c size: %d links: %d blocks: %d\n",
           inodeNumber, fileType, inode.i_size, inode.i_links_count, inode.i_blocks);

    for (int i_block_count = 0; i_block_count < 15; i_block_count++)
    {
        if (inode.i_block[i_block_count] != 0)
        {
            printf("[%d] Blocks:  %d\n", inodeNumber, inode.i_block[i_block_count]);
        }
        else
        { // stop printing once the block number is "0" (un-used)
            break;
        }
    }
}

void print_all_inodes(struct ext2_inode *inodeTable, unsigned char *inodeBitmap, unsigned int inodeCount)
{
    // First print root inode
    print_inode(inodeTable, inodeBitmap, EXT2_ROOT_INO);

    // for every non-reserved inode, if it is occupied, print info.
    for (int i = EXT2_GOOD_OLD_FIRST_INO; i < inodeCount; i++)
    {
        if (occupied(inodeBitmap, i))
        {
            print_inode(inodeTable, inodeBitmap, i + 1);
        }
    }
}

void print_directory_block(struct ext2_inode *inodeTable, unsigned int inodeNumber)
{
    struct ext2_inode inode = inodeTable[inodeNumber - 1];
    if (getFileModeChar(inode.i_mode) != DIRECTORY_CHAR)
    {
        return;
    }

    unsigned int dirBlockNumber = inode.i_block[0];

    // NOTE: this ^ assumes that the directory only takes up at most 1 block.
    // CHECK A4 handout to ensure this is a safe assumption to make (Yes for this exercise)
    // Not safe, directory may take up more than 1 block (this is for the assignment only)

    struct ext2_dir_entry *cur_dir_entry = (struct ext2_dir_entry *)getBlock(dirBlockNumber);
    int rec_sum = 0;
    printf("   DIR BLOCK NUM: %d (for inode %d)\n", dirBlockNumber, inodeNumber);

    while (rec_sum != EXT2_BLOCK_SIZE)
    { //not at end of dir_entry list
        char ftype = getFileType(cur_dir_entry->file_type);

        printf("Inode: %d rec_len: %d name_len: %d type= %c ", cur_dir_entry->inode, cur_dir_entry->rec_len,
               cur_dir_entry->name_len, ftype);

        printf("name=%.*s\n", cur_dir_entry->name_len, cur_dir_entry->name);

        rec_sum += cur_dir_entry->rec_len;
        cur_dir_entry = (struct ext2_dir_entry *)getNextPointer(cur_dir_entry, cur_dir_entry->rec_len);
    }
}

void print_all_directory_blocks(struct ext2_inode *inodeTable, unsigned char *inodeBitmap, unsigned int inodeCount)
{

    // First print root inode's directory block
    printf("\n");
    printf("Directory Blocks:\n");
    print_directory_block(inodeTable, EXT2_ROOT_INO);
   
    //Go through the inode table should suffice
    //Only print the inodes that are pointing to a directory block
    //Which is why "lost+found" is not printed (See first under "Inodes" in TE9 handout)
    //Pay Special attention to [Inode_number]

    for (int inodeOffset = EXT2_GOOD_OLD_FIRST_INO; inodeOffset < inodeCount; inodeOffset++){
        if (occupied(inodeBitmap, inodeOffset)){
            print_directory_block(inodeTable, inodeOffset+1);
        }
    }
}


//function that returns the inode offset of the dir/file that the person is refering to.
//lookup basename, cuts off the last name from path for mkdir
int findInodeOffsetFromPath(char *path){ //path has to be absolute and null terminated
    const char delim[2] = "/";
    char *token;
    if (path[0] != '/'){//root checker
        return -ENOENT;
    }

    else{
        struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + 2*EXT2_BLOCK_SIZE);
        struct ext2_inode* inodeTable = (struct ext2_inode*)getBlock(gd->bg_inode_table);
        unsigned int inodeOffset = EXT2_ROOT_INO-1;

        struct ext2_inode parentInode = inodeTable[inodeOffset]; //parent inode

        // traverse the entry linked list

        token = strtok(path, delim);
        while (token != NULL){//
            unsigned int dirBlockNumber = parentInode.i_block[0];
            struct ext2_dir_entry* cur_dir_entry = (struct ext2_dir_entry*) getBlock(dirBlockNumber);
            int rec_sum = 0;
            while (rec_sum != EXT2_BLOCK_SIZE){//assumption that dir takes one block, modify later
                // printf("token: [%s]\n", token);
                // printf("actual name: [%.*s]\n", cur_dir_entry->name_len, cur_dir_entry->name);
               
                if (strncmp(token, cur_dir_entry->name, strlen(token)) == 0){
                    inodeOffset = cur_dir_entry->inode -1;
                    parentInode = inodeTable[inodeOffset];
                    break;
                }
                rec_sum += cur_dir_entry->rec_len;
                cur_dir_entry = (struct ext2_dir_entry*) getNextPointer(cur_dir_entry, cur_dir_entry->rec_len); 
                if(rec_sum == EXT2_BLOCK_SIZE){
                    // got to end of dir entry list and didn't find a match, so this dir doesn't exist
                    return -ENOENT;
                }
            }
        
            token = strtok(NULL, delim);
            if (token != NULL && getFileType(cur_dir_entry->file_type) != DIRECTORY_CHAR){
                return -ENOENT;
            }
        }
        return inodeOffset;
    }

}

struct ext2_inode* getInode(int InodeOffset){
    struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + 2*EXT2_BLOCK_SIZE);
    struct ext2_inode* inodeTable = (struct ext2_inode*)getBlock(gd->bg_inode_table);
    if ((InodeOffset != EXT2_ROOT_INO -1) && (InodeOffset < EXT2_GOOD_OLD_FIRST_INO -1 || InodeOffset > 31)){
        return NULL;
    }
    return &(inodeTable[InodeOffset]);
}


struct ext2_inode* getInodeFromPath (char* path){
    int stringLen = strnlen(path, 4096);
    char* pathDummy = strndup(path, stringLen);
    int inodeOffset = findInodeOffsetFromPath(pathDummy);
    printf("path = %s, Inode offset is: %d\n", path, inodeOffset);

    return getInode(inodeOffset);
}

// Copies the data in char* into new blocks that it allocates for the inode
int copyDataIntoNewDataBlocks(struct ext2_inode* newInode, int numOfBlocksNeeded, char* dataToCopy, int lengthofData)
{
    int remainingCharCount = lengthofData;
    for(int i = 0; i < numOfBlocksNeeded; i++){
        // get a new data block 
        int blockOffset = allocateNewDataBlock(newInode);
        if(blockOffset < 0) return -ENOSPC;	

        char* dataBlockPointer = (char*)getBlock(blockOffset + 1);
        strncpy(dataBlockPointer, dataToCopy, EXT2_BLOCK_SIZE);
        remainingCharCount -= EXT2_BLOCK_SIZE;
        if(remainingCharCount > 0){
            printf("before data to copy increment \n");
            dataToCopy += EXT2_BLOCK_SIZE; // move dataToCopy pointer forward
            printf("remaining char count = %d\n", remainingCharCount);
        }else{
            return 0;
        }
    }

    return 0;
}
