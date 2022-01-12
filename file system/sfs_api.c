#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "sfs_api.h"
#include "disk_emu.h"

/* --IMPORTANT INFORMATION REGARDING THE SFS--

BLOCK SIZE: 512 BYTES
DISK SIZE: 527 BLOCKS <- 1 (SUPERBLOCK) + 13 (I-NODE TABLE) + 512 (DATA BLOCKS) + 1 (BYTEMAP)
MAX FILE SIZE: 140 BLOCKS
MAX # OF FILES: 100

*/


#define BLOCK_SIZE 512
#define NUM_BLOCKS 527
#define MAX_NUM_OF_FILES 100
#define INODE_SIZE 64
#define DATA_BLOCKS_OFFSET 14
#define NUM_DIRECTORY_ENTRIES_PER_BLOCK 16
#define NUM_INODES_PER_BLOCK 8
#define NUM_DIRECT_POINTERS_PER_INODE 12
#define NUM_INODE_BLOCKS 13

struct dir_entry {
    char file_name[28];
    int file_ptr;
};

struct dir_block {
    struct dir_entry entries[NUM_DIRECTORY_ENTRIES_PER_BLOCK];
};

struct inode {
    unsigned char active;
    int file_size;
    int ptrs[NUM_DIRECT_POINTERS_PER_INODE];
    int indirect_ptr;
};

struct inode_block {
    struct inode nodes[NUM_INODES_PER_BLOCK];
};

struct superblock {
    int blk_sz;
    int fs_sz;
    int inode_table_sz;
    int root_dir;
};

struct fdt_entry {
    int rw_ptr;
    int file_ptr;
};

static struct fdt_entry fdt[MAX_NUM_OF_FILES];

/* --HELPER FUNCTION--

SEARCHES FREE BITMAP FOR NEXT AVAILABLE BLOCK
RETURNS INDEX OF FREE BLOCK OR,
RETURNS -1 IF ALL BLOCKS FULL
RETURNS -2 FOR ALL OTHER FAILURE

*/

int getnextfreeblock(){
    void* buffer = (void*) malloc(BLOCK_SIZE); 
    if(read_blocks(NUM_BLOCKS - 1, 1, buffer) != 1) 
    return -2;
    unsigned char* bytemap = (unsigned char*) buffer;
    for(int i = 0; i < BLOCK_SIZE; i++){
        if(*(bytemap+i) == 0)
        return i;
    }
    return -1;
}

/* --HELPER FUNCTION--

MARKS BLOCK AS OCCUPIED ON THE BITMAP
RETURNS 0 ON SUCCESS,
RETURNS 1 ON FAILURE

*/

int markblocktaken(int block_number){
    void* buffer = (void*) malloc(BLOCK_SIZE);
    if(read_blocks(NUM_BLOCKS - 1, 1, buffer) != 1) 
    return 1;
    unsigned char* bytemap = (unsigned char*) buffer;
    *(bytemap+block_number) = 1;
    if(write_blocks(NUM_BLOCKS - 1, 1, bytemap) != 1)
    return 1;
    else
    return 0;
}

/* --HELPER FUNCTION--

MARKS BLOCK AS FREE ON THE BITMAP
RETURNS 0 ON SUCCESS,
RETURNS 1 ON FAILURE

*/

int markblockfree(int block_number){
    void* buffer = (void*) malloc(BLOCK_SIZE);
    if(read_blocks(NUM_BLOCKS - 1, 1, buffer) != 1) 
    return 1;
    unsigned char* bytemap = (unsigned char*) buffer;
    *(bytemap+block_number) = 0;
    if(write_blocks(NUM_BLOCKS - 1, 1, bytemap) != 1)
    return 1;
    else
    return 0;
}

/* --HELPER FUNCTION--

GETS THE INODE_NUM'TH I-NODE IN THE I-NODE TABLE
RETURNS I-NODE ON SUCCESS,
RETURNS NULL ON FAILURE

*/

struct inode* getinode(int inode_num){
    int block_num = inode_num / NUM_INODES_PER_BLOCK;
    int index = inode_num % NUM_INODES_PER_BLOCK;
    if(block_num > 13 || block_num < 0){
        printf("Failed to retrieve i-node\n");
        return NULL;
    }
    else if(index >= 8 || index < 0){
        printf("Failed to retrieve i-node\n");
        return NULL;
    }
    else{
        void* buffer = (void*)malloc (BLOCK_SIZE);
        read_blocks(1 + block_num, 1, buffer);
        struct inode_block* b = (struct inode_block*)buffer;
        return &(b->nodes[index]);
    }
}

void mksfs(int fresh){

    //--FRESH FLAG RAISED, CREATE NEW DISK--
    if(fresh){
        int disk_creation_flag = init_fresh_disk("sfs_disk", BLOCK_SIZE, NUM_BLOCKS);
        if(disk_creation_flag == -1)
        return;
        else if(disk_creation_flag == 0)
        printf("Created new disk file sfs_disk\n");
        else
        return;

        //--CREATE SUPERBLOCK--
        struct superblock* sb = malloc (BLOCK_SIZE);
        sb->blk_sz = BLOCK_SIZE;
        sb->fs_sz = NUM_BLOCKS;
        sb->inode_table_sz = 13;
        sb->root_dir = 0;
        write_blocks(0, 1, sb);
        free(sb);

        //--CREATE FREE BYTEMAP--
        unsigned char* bytemap = malloc (BLOCK_SIZE);
        for(int i = 0; i < BLOCK_SIZE; i++){
            *(bytemap+i) = 0;
        }
        write_blocks(NUM_BLOCKS - 1, 1, bytemap);
        free(bytemap);

        //--CREATE I-NODE TABLE--
        for(int i = 1; i <= NUM_INODE_BLOCKS; i++){
            struct inode_block* temp_block = malloc (BLOCK_SIZE);
            for(int k = 0; k < NUM_INODES_PER_BLOCK; k++){
                struct inode* temp_node = malloc (INODE_SIZE);
                temp_node->active = 0;
                temp_block->nodes[k] = *temp_node;
            }
            write_blocks(i, 1, temp_block);
            free(temp_block);
        }

        //--CREATE ROOT DIRECTORY--
        struct inode* dir_node = malloc (INODE_SIZE); //create an i-node for the directory
        int freeblock = DATA_BLOCKS_OFFSET + getnextfreeblock(); //create a directory block
        struct dir_block* dir = malloc(BLOCK_SIZE);
        write_blocks(freeblock, 1, dir); //store directory block onto disk
        markblocktaken(freeblock - DATA_BLOCKS_OFFSET);
        dir_node->active = 1;
        dir_node->file_size = BLOCK_SIZE;
        dir_node->ptrs[0] = freeblock;

        void* buffer = (void*) malloc (BLOCK_SIZE); 
        read_blocks(1, 1, buffer); //pull 1st block of i-node table from disk
        ((struct inode_block*)buffer)->nodes[0] = *dir_node; //store i-node into i-node table 
        write_blocks(1, 1, buffer); //push updated i-node table onto disk

        free(dir);
        free(dir_node);
        free(buffer);
    }

}

int sfs_getnextfilename(char* fname);

int sfs_getfilesize(const char* path);

int sfs_fopen(char* name){
    //--CHECK IF FILE WITH name ALREADY EXISTS IN DIRECTORY--
    //--GET DIRECTORY INODE--
    struct inode* directory = getinode(0); 
    int inode_index = -1;
    //--ITERATE THROUGH THE DATA BLOCKS THAT THE DIRECTORY IS STORED IN--
    for(int i = 0; i < NUM_DIRECT_POINTERS_PER_INODE; i++){
        void* buffer = (void*)malloc(BLOCK_SIZE);
        read_blocks(directory->ptrs[i], 1, buffer);
        struct dir_block* db = (struct dir_block*) buffer;
        //--ITERATE THROUGH THE DIRECTORY ENTRIES THAT ARE STORED IN EACH DATA BLOCK
        for(int k = 0; k < NUM_DIRECTORY_ENTRIES_PER_BLOCK; k++){ 
            //--GET POINTER TO FILE INODE--
            if(strcmp((db->entries[k]).file_name, name) == 0){
                inode_index = (db->entries[k]).file_ptr;
                printf("File found in directory\n");
                for(int p = 0; p < MAX_NUM_OF_FILES; p++){
                    if(fdt[p].file_ptr == db->entries[k].file_ptr){
                        printf("File with file ptr %d found in FDT\n", fdt[p].file_ptr);
                        return p;
                    }
                }
            }
        }
        free(buffer);
    }
    //--FILE WAS NOT FOUND IN DIRECTORY--
    if(inode_index == -1){
        //--LOOK FOR EMPTY I-NODE SLOT--
        unsigned char found = 0;
        for(int i = 1; i <= NUM_INODE_BLOCKS && !found; i++){
            void* buffer = (void*)malloc(BLOCK_SIZE);
            read_blocks(i, 1, buffer);
            struct inode_block* blk = (struct inode_block*)buffer;
            for(int k = 0; k < NUM_INODES_PER_BLOCK; k++){
                if(blk->nodes[k].active == 0){
                    //--CREATE NEW I-NODE IN EMPTY SLOT--
                    blk->nodes[k].active = 1;
                    blk->nodes[k].file_size = 0;
                    for(int j = 0; j < NUM_DIRECT_POINTERS_PER_INODE; j++){
                        blk->nodes[k].ptrs[j] = 0;
                    }
                    blk->nodes[k].indirect_ptr = 0;
                    write_blocks(i, 1, blk);
                    inode_index = (i-1) * NUM_INODES_PER_BLOCK + k;
                    printf("Created a new file @ i-node index %d\n", inode_index);
                    //--LF BUG--
                    struct inode* bug = getinode(inode_index);
                    printf("New i-node: active = %d, file_size = %d\n", bug->active, bug->file_size);
                    //----------
                    found = 1;
                    break;
                }
            }
            free(buffer);
        }
        //--FIND EMPTY SLOT IN DIRECTORY--
        int finished = 0;
        for(int i = 0; i < NUM_DIRECT_POINTERS_PER_INODE && !finished; i++){ //iterate through the direct pointers of the directory i-node
            void* buffer = (void*)malloc(BLOCK_SIZE);
            if(buffer == NULL){
                //create new directory page
            }
            read_blocks(directory->ptrs[i], 1, buffer);
            struct dir_block* db = (struct dir_block*) buffer;
            for(int k = 0; k < NUM_DIRECTORY_ENTRIES_PER_BLOCK; k++){ //iterate through the entries in the directory block
                if(db->entries[k].file_ptr == 0){
                    strcpy(db->entries[k].file_name, name);
                    db->entries[k].file_ptr = inode_index;
                    write_blocks(directory->ptrs[i], 1, (void*) db);
                    printf("Directory entry created: file name = %s, file ptr = %d\n", db->entries[k].file_name, db->entries[k].file_ptr);
                    finished = 1;
                    break;
                }
            }
            //free(buffer);
        }
    }
    //--CREATE NEW FILE DESCRIPTOR AND ADD TO FDT--
    for(int i = 0; i < 100; i++){
        if(fdt[i].file_ptr == 0){
            fdt[i].file_ptr = inode_index;
            fdt[i].rw_ptr = getinode(inode_index)->file_size;
            printf("File with file ptr %d added to the %d th index of the FDT\n", fdt[i].file_ptr, i);
            return i;
        }        
    }
    return -1;
}

int sfs_fclose(int fileID){
    if(fileID >= MAX_NUM_OF_FILES || fileID < 0){
        return -1;
    }
    fdt[fileID].file_ptr = 0;
    fdt[fileID].rw_ptr = 0;
    return 0;
}

int sfs_fwrite(int fileID, const char* buf, int length){
    if(fileID >= MAX_NUM_OF_FILES || fileID < 0){
        return -1;
    }
    else if(fdt[fileID].file_ptr == 0){
        return -1;
    }
    else{
        struct inode* file_inode = getinode(fdt[fileID].file_ptr);
        //--ALLOCATE BLOCKS TO THE FILE NECESSARY FOR THE WRITE--
        if(file_inode->file_size < fdt[fileID].rw_ptr + length){
            for(int i = 0; i < NUM_DIRECT_POINTERS_PER_INODE; i++){
                if(file_inode->ptrs[i] == 0){
                    int freeblock = DATA_BLOCKS_OFFSET + getnextfreeblock();
                    file_inode->ptrs[i] = freeblock;
                    file_inode->file_size = fdt[fileID].rw_ptr + length;
                    struct inode_block* temp = malloc(BLOCK_SIZE);
                    read_blocks(fdt[fileID].file_ptr/NUM_INODES_PER_BLOCK + 1, 1, temp);
                    temp->nodes[fdt[fileID].file_ptr%NUM_INODES_PER_BLOCK] = *file_inode;
                    write_blocks(fdt[fileID].file_ptr/NUM_INODES_PER_BLOCK + 1, 1, temp);
                    markblocktaken(freeblock - DATA_BLOCKS_OFFSET);
                    printf("Block %d allocated to file %d\n", freeblock, fdt[fileID].file_ptr);
                    break;
                }
            }
        }
        int start_block = fdt[fileID].rw_ptr / BLOCK_SIZE;
        int read_position = fdt[fileID].rw_ptr % BLOCK_SIZE;
        void* buffer = malloc (BLOCK_SIZE);
        read_blocks(file_inode->ptrs[start_block], 1, buffer);
        char* data_block = (char*) buffer;
        int i = 0;
        for(; i < length; i++){
            *(data_block + i + read_position) = *(buf + i);
        }
        printf("Writing %s to block %d\n", data_block, file_inode->ptrs[start_block]);
        write_blocks(file_inode->ptrs[start_block], 1, data_block);
        return i;
    }
}

int sfs_fread(int fileID, char* buf, int length){
    if(fileID >= MAX_NUM_OF_FILES || fileID < 0){
        return -1;
    }
    else if(fdt[fileID].file_ptr == 0){
        return -1;
    }
    else{
        struct inode* file_inode = getinode(fdt[fileID].file_ptr);
        int start_block = fdt[fileID].rw_ptr / BLOCK_SIZE;
        int read_position = fdt[fileID].rw_ptr % BLOCK_SIZE;
        void* buffer = malloc (BLOCK_SIZE);
        read_blocks(file_inode->ptrs[start_block], 1, buffer);
        char* data_block = (char*) buffer;
        int i = 0;
        for(; i < length; i++){
            if(*(data_block + i + read_position) == '\0' || i+1 == length){
                *(buf + i) = '\0';
                break;
            }
            *(buf + i) = *(data_block + i + read_position);
        }
        printf("%s was read from block %d\n", data_block, file_inode->ptrs[start_block]);
        return i;
    }
}

int sfs_fseek(int fileID, int loc){
    if(fileID >= MAX_NUM_OF_FILES || fileID < 0){
        return -1;
    }
    else if(fdt[fileID].file_ptr == 0){
        return -1;
    }
    else{
        fdt[fileID].rw_ptr = loc;
        return 0;
    }
}

int sfs_remove(char* file){
    struct inode* directory = getinode(0);
    for(int i = 0; i < NUM_DIRECT_POINTERS_PER_INODE; i++){
        void* buffer = malloc(BLOCK_SIZE);
        read_blocks(directory->ptrs[i], 1, buffer);
        struct dir_block* db = (struct dir_block*)buffer;
        for(int k = 0; k < NUM_DIRECTORY_ENTRIES_PER_BLOCK; k++){
            //--FILE FOUND--
            if(strcmp(db->entries[k].file_name, file) == 0){
                //--RELEASE FDT ENTRIES HELD BY FILE--
                for(int j = 0; j < MAX_NUM_OF_FILES; j++){
                    if(fdt[j].file_ptr == db->entries->file_ptr){
                        fdt[j].file_ptr = 0;
                        fdt[j].rw_ptr = 0;
                    }
                }
                for(int p = 0; p < 28; p++){
                    db->entries[k].file_name[p] = '\0';
                }
                db->entries[k].file_ptr = 0;
                printf("File %s was removed\n", file);
                write_blocks(directory->ptrs[i], 1, db);
                return 0;
            }
        }
    }
    return -1;
}
