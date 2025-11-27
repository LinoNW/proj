#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "bitmap.h"
#include <assert.h>

#include "fs.h"
#include "disk.h"

/*******
 * FSO FS layout
 * FS block size = disk block size
 * block#
 * 0            super block (includes the number of inodes)
 * 1 ...        start of bitmap with free/used blocks
 * after bitmap follows blocks with inodes (root dir is always inode 0)
 * after inodes follows the data blocks
 */

#define BLOCKSZ		(DISK_BLOCK_SIZE)
#define SBLOCK		0	// superblock is in disk block 0
#define BITMAPSTART 1	// free/use block bitmap starts in block 1
#define INODESTART  (rootSB.first_inodeblk)  // inodes start in this block
#define ROOTINO		0 	// root dir is described in inode 0

#define FS_MAGIC    (0xf50f5025) // when formated the SB starts with this number
#define DIRBLOCK_PER_INODE 11	 // number of direct block indexes in inode
#define MAXFILENAME        62    // max name size in a dirent

#define INODESZ		((int)sizeof(struct fs_inode))
#define INODES_PER_BLOCK		(BLOCKSZ/INODESZ)
#define DIRENTS_PER_BLOCK		(BLOCKSZ/sizeof(struct fs_dirent))

enum inode_type {
    IFFREE = 0,  // inode is free
    IFDIR  = 4,  // inode is dir
    IFREG  = 8   // inode is regular file
};

#define FREE 0

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))



/*** FSO FileSystem in memory structures ***/

// Super block with file system parameters
struct fs_sblock {
    uint32_t magic;      // when formated this field should have FS_MAGIC
    uint32_t block_cnt;  // number of blocks in disk
    uint16_t block_size; // FS block size
    uint16_t bmap_size;  // number of blocks used for free/use bitmap
    uint16_t first_inodeblk; // first block with inodes
    uint16_t inode_cnt;  // number of inodes
    uint16_t inode_blocks;  // number of blocks with inodes
    uint16_t first_datablk; // first block with data or dir
};

// inode describing a file or directory
// note: code below may depend on these types sizes
struct fs_inode {
    uint16_t type;   // inode_type (DIR, REG, etc)
    uint16_t nlinks; // number of links to this inode
    uint32_t size;   // file size (bytes)
    uint16_t dir_block[DIRBLOCK_PER_INODE]; // direct data blocks
    uint16_t indir_block; // indirect index block
};

// directory entry
struct fs_dirent {
    uint16_t d_ino; // inode number
    char d_name[MAXFILENAME]; // name (a 0 terminated C string)
};

// generic block: a variable of this type may be used as a
// superblock, a block of inodes, a block of dirents, or data (a byte array)
union fs_block {
    struct fs_sblock super;
    struct fs_inode inode[INODES_PER_BLOCK];
    struct fs_dirent dirent[DIRENTS_PER_BLOCK];
    char data[BLOCKSZ];
};

/**  Super block from mounted File System (Global Variable)
 *   read at the beginning from disk block 0
 **/
struct fs_sblock rootSB;



/** checks that the global rootSB contains a valid super block of a formated disk
 *  returns -1 if error; 0 if it's OK
 */
int check_rootSB() {
    if (rootSB.magic != FS_MAGIC) {
        printf("disc not mounted\n");
        return -1;
    }
    return 0;
}

/** load from disk the inode ino_number into ino (must be an initialized pointer);
 *  returns 0 if inode read. The ino.type == FREE if ino_number is of a free inode;
 *  returns -1 ino_number outside the existing limits.
 */
int inode_load(int ino_number, struct fs_inode *ino) {
    union fs_block block;

    if (ino_number<0 || ino_number >= rootSB.inode_cnt) {
        printf("inode_load: inode number too big\n");
        ino->type = FREE;
        return -1;
    }
    int inodeBlock = rootSB.first_inodeblk + (ino_number / INODES_PER_BLOCK);
    disk_read(inodeBlock, block.data);
    *ino = block.inode[ino_number % INODES_PER_BLOCK];
    return 0;
}

/** save to disk the inode ino to the ino_number position;
 *  returns 0 if saved;
 *  if ino_number is outside limits, nothing is done and returns -1
 */
int inode_save(int ino_number, struct fs_inode *ino) {
    union fs_block block;

    if (ino_number<0 || ino_number >= rootSB.inode_cnt) {
        printf("inode_save: inode number too big\n");
        return -1;
    }
    int inodeBlock = rootSB.first_inodeblk + (ino_number / INODES_PER_BLOCK);
    disk_read(inodeBlock, block.data); // read full block
    block.inode[ino_number % INODES_PER_BLOCK] = *ino; // update inode
    disk_write(inodeBlock, block.data); // write block
    return 0;
}

/** finds an inode not in use (disk is not changed)
 *  returns the inode number;  or -1 if no more inodes.
 */
int inode_alloc() {
    int inodeBlock = 0;
    do {
        union fs_block block;
        disk_read(INODESTART + inodeBlock, block.data);
        for (int i = 0; i < INODES_PER_BLOCK; i++)
            if (block.inode[i].type == IFFREE) {
                return inodeBlock * INODES_PER_BLOCK + i;
            }
        inodeBlock++;
    } while (inodeBlock < rootSB.inode_blocks);

    return -1; // no more inodes
}

/** marks inode as FREE
 *  returns 0 if ok;  -1 if ino_number is not valid
 */
int inode_free(int ino_number) {
    // not very efficient
    struct fs_inode inode;
    if ( inode_load(ino_number, &inode) == -1 )
        return -1;
    inode.type = IFFREE;
    return inode_save(ino_number, &inode);
}



/** finds a free disk data block in the bitmap and marks it in use;
 *  returns the block number; returns -1 if no more free blocks.
 */
int block_alloc() {
    union fs_block block;
    int bitmapBlock = 0;

    do {
        disk_read(BITMAPSTART + bitmapBlock, block.data);
        for (int i = 0; i < BLOCKSZ * 8 && bitmapBlock * BLOCKSZ * 8 + i < rootSB.block_cnt; i++)
            if (bitmap_get(block.data, i) == 0) {
                bitmap_set(block.data, i); // found one free, mark it in use
                disk_write(BITMAPSTART + bitmapBlock, block.data);
                return bitmapBlock * BLOCKSZ * 8 + i;
            }
        bitmapBlock++;
    } while (bitmapBlock < rootSB.bmap_size);

    return -1; // no free space left on disk
}

/** marks nblock as free in the bitmap
 *  returns 0 if ok;  return -1 if error (nblock not valid).
 */
int block_free(int nblock) {
    union fs_block block;
    int bitmapBlock = nblock / (BLOCKSZ * 8); // bitmap block where this bit is
    int offsetBlock = nblock % (BLOCKSZ * 8); // offset inside this block
    if (bitmapBlock >= rootSB.bmap_size || bitmapBlock < 0)
        return -1; // outside disk size; ignore it

    // printf("block_free: %d\n", nblock);
    disk_read(BITMAPSTART + bitmapBlock, block.data);
    bitmap_clear(block.data, offsetBlock);
    disk_write(BITMAPSTART + bitmapBlock, block.data); // update disk
    return 0;
}



/** finds the disk block number that contains the byte at the given offset
 *  for the file or directory described by the given inode;
 *  returns the disk block number, or -1 if error.
 */
int offset2block(struct fs_inode *inode, int offset) {
    int blkindex = offset / BLOCKSZ; // What is the block for this offset?

    if (blkindex < DIRBLOCK_PER_INODE) { // is in a direct index
        return inode->dir_block[blkindex];
    } else if (blkindex < DIRBLOCK_PER_INODE + BLOCKSZ / sizeof(uint16_t)) {
        // blkindex is in the indirect block of indexes
        uint16_t data[BLOCKSZ / sizeof(uint16_t)];

        disk_read(inode->indir_block, (char*)data);
        return data[blkindex - DIRBLOCK_PER_INODE];
    } else {
        // there is no double indirects in this FS so blkindex is too big
        // printf("offset2block: offset too big!\n");
        return -1;
    }
}



/** find name in the directory given by dir_inode;
 *  returns its inode number (from dirent); or -1 if error or not found
 */
int dir_findname(struct fs_inode *dir_inode, char *name) {
    if (dir_inode->type!=IFDIR) return -1; // not a directory
    int remaining_dirents = dir_inode->size / sizeof(struct fs_dirent);
    int offset = 0;
    union fs_block block;

    while ( remaining_dirents>0 ) {
        int currBlock = offset2block(dir_inode, offset);
        disk_read(currBlock, block.data);
        for (int d = 0; d < DIRENTS_PER_BLOCK && d < remaining_dirents; d++) {
            if (block.dirent[d].d_ino!=FREE
                && strncmp(block.dirent[d].d_name, name, MAXFILENAME) == 0)
                return block.dirent[d].d_ino;  // found!
        }
        remaining_dirents -= DIRENTS_PER_BLOCK;
        offset += DIRENTS_PER_BLOCK * sizeof(struct fs_dirent);
    }
    return -1;  // not found
}



/*****************************************************/

// converts a path name into an inode number (-1 if error)
int get_inode(char *path_name)
{
    if (path_name == NULL || strcmp(path_name, "/") == 0 || strcmp(path_name, "") == 0)
    {
        return ROOTINO;
    }

    int curr_ino = ROOTINO;
    struct fs_inode curr_inode;

    char path_copy[strlen(path_name) + 1];
    strcpy(path_copy, path_name);
    char *token = strtok(path_copy, "/");
    
    //se for do-while pode dar erro se input for ´////´
    while (token != NULL)
    {
        if (inode_load(curr_ino, &curr_inode) == -1)
            return -1;
        if (curr_inode.type != IFDIR)
            return -1;

        int next_ino = dir_findname(&curr_inode, token);
        if (next_ino == -1)
            return -1;

        curr_ino = next_ino;
        token = strtok(NULL, "/");
    }

    return curr_ino;
}

int get_parent_inode(char *pathname)
{
    if (pathname == NULL || strlen(pathname) == 0)
        return ROOTINO;
    
    // Make a copy to avoid modifying the original
    char path_copy[strlen(pathname) + 1];
    strcpy(path_copy, pathname);
    
    // Find the last slash
    char *last_slash = strrchr(path_copy, '/');
    
    if (last_slash == NULL)
    {
        // No slash found, parent is root
        return ROOTINO;
    }
    
    if (last_slash == path_copy)
    {
        // Path like '/filename', parent is root
        return ROOTINO;
    }
    
    // Truncate at the last slash to get parent path
    *last_slash = '\0';
    
    // Get the inode of the parent directory
    int parent_ino = get_inode(path_copy);
    if (parent_ino == -1)
        return -1;
    
    return parent_ino;
}

char *get_filename(char *pathname)
{
    if (pathname == NULL)
        return NULL;
    
    char *last_slash = strrchr(pathname, '/');
    if (last_slash == NULL)
    {
        return pathname; // No slash found, entire pathname is filename
    }
    
    // Skip the slash and return filename
    char *filename = last_slash + 1;
    // If filename is empty (path ends with '/'), return NULL
    if (*filename == '\0')
        return NULL;
    return filename;
}

int add_entry_to_directory(int parent_ino, char *name, int child_ino)
{
    struct fs_inode parent_inode;
    if (inode_load(parent_ino, &parent_inode) == -1)
        return -1;

    union fs_block block;

    // First, search for a FREE entry in existing blocks
    int num_entries = parent_inode.size / sizeof(struct fs_dirent);
    int offset = 0;
    int last_read_block = -1;

    for (int entry_idx = 0; entry_idx < num_entries; entry_idx++)
    {
        offset = entry_idx * sizeof(struct fs_dirent);
        int currBlock = offset2block(&parent_inode, offset);

        if (currBlock == -1 || currBlock == 0)
            break;

        // Only read the block if it's different from the last one
        if (currBlock != last_read_block)
        {
            disk_read(currBlock, block.data);
            last_read_block = currBlock;
        }
        
        int idx_in_block = entry_idx % DIRENTS_PER_BLOCK;

        if (block.dirent[idx_in_block].d_ino == FREE)
        {
            // Reuse this deleted entry (but don't change size!)
            block.dirent[idx_in_block].d_ino = child_ino;
            strncpy(block.dirent[idx_in_block].d_name, name, MAXFILENAME);
            block.dirent[idx_in_block].d_name[MAXFILENAME - 1] = '\0';
            disk_write(currBlock, block.data);
            return 0;
        }
    }

    // No FREE entry found, need to add at the end
    // Calculate which block should receive the new entry
    int total_entries = parent_inode.size / sizeof(struct fs_dirent);
    int block_index = total_entries / DIRENTS_PER_BLOCK;
    int entry_index = total_entries % DIRENTS_PER_BLOCK;

    // Try direct blocks
    if (block_index < DIRBLOCK_PER_INODE)
    {
        int blknum = parent_inode.dir_block[block_index];
        if (blknum == 0)
        {
            // Allocate new block
            int new_block = block_alloc();
            if (new_block == -1)
                return -1;

            parent_inode.dir_block[block_index] = new_block;
            blknum = new_block;

            // Initialize block
            memset(block.data, 0, BLOCKSZ);
        }
        else
        {
            // Read existing block
            disk_read(blknum, block.data);
        }

        // Add entry at the correct position
        block.dirent[entry_index].d_ino = child_ino;
        strncpy(block.dirent[entry_index].d_name, name, MAXFILENAME);
        block.dirent[entry_index].d_name[MAXFILENAME - 1] = '\0';
        disk_write(blknum, block.data);

        // Update size and save inode
        parent_inode.size += sizeof(struct fs_dirent);
        inode_save(parent_ino, &parent_inode);
        return 0;
    }

    // Need indirect blocks
    int indirect_index = block_index - DIRBLOCK_PER_INODE;
    uint16_t indirect_block_data[BLOCKSZ / sizeof(uint16_t)];
    int indirect_block_num = parent_inode.indir_block;

    if (indirect_block_num == 0)
    {
        indirect_block_num = block_alloc();
        if (indirect_block_num == -1)
            return -1;

        parent_inode.indir_block = indirect_block_num;
        
        memset(indirect_block_data, 0, BLOCKSZ);
        disk_write(indirect_block_num, (char *)indirect_block_data);
    }
    else
    {
        disk_read(indirect_block_num, (char *)indirect_block_data);
    }

    int data_block_num = indirect_block_data[indirect_index];
    if (data_block_num == 0)
    {
        data_block_num = block_alloc();
        if (data_block_num == -1)
            return -1;

        indirect_block_data[indirect_index] = data_block_num;
        disk_write(indirect_block_num, (char *)indirect_block_data);

        // Initialize block
        memset(block.data, 0, BLOCKSZ);
    }
    else
    {
        // Read existing block
        disk_read(data_block_num, block.data);
    }

    // Add entry at the correct position
    block.dirent[entry_index].d_ino = child_ino;
    strncpy(block.dirent[entry_index].d_name, name, MAXFILENAME);
    block.dirent[entry_index].d_name[MAXFILENAME - 1] = '\0';
    disk_write(data_block_num, block.data);

    parent_inode.size += sizeof(struct fs_dirent);
    inode_save(parent_ino, &parent_inode);
    return 0;
}

/** list the content of directory dirname
 *  dirname may start with "/" or not;
 *  dirname may be one name or a pathname with subdirectories.
 */
int fs_ls(char *dirname)
{
    if (check_rootSB() == -1)
        return -1;
    if (dirname == NULL)
        dirname = "/";
    int number_of_ino = get_inode(dirname);
    if (number_of_ino == -1)
    {
        printf("No such directory with name %s\n", dirname);
        return -1;
    }
    struct fs_inode inode_of_dir;
    if (inode_load(number_of_ino, &inode_of_dir) == -1)
    {
        printf("Error loading inode %d\n", number_of_ino);
        return -1;
    }

    if (inode_of_dir.type != IFDIR)
    {
        printf("%s is not a directory\n", dirname);
        return -1;
    }

    int remaining_dirents = inode_of_dir.size / sizeof(struct fs_dirent);
    int offset = 0;
    union fs_block block;
    printf("listing dir %s (inode %d):\n", dirname, number_of_ino);
    printf("ino:type:nlk    bytes name\n");

    while (remaining_dirents > 0)
    {
        int currBlock = offset2block(&inode_of_dir, offset);

        if (currBlock == -1 || currBlock == 0)
            break;

        disk_read(currBlock, block.data);
        for (int d = 0; d < DIRENTS_PER_BLOCK && d < remaining_dirents; d++)
        {
            if (block.dirent[d].d_ino != FREE)
            {
                struct fs_inode entry_inode;
                if (inode_load(block.dirent[d].d_ino, &entry_inode) != -1)
                {
                    char type = '?';
                    if (entry_inode.type == IFDIR)
                    {
                        type = 'D';
                    }
                    else if (entry_inode.type == IFREG)
                    {
                        type = 'F';
                    }
                    printf("%3d:%4c:%3d%9d %s\n",
                           block.dirent[d].d_ino, type, entry_inode.nlinks,
                           entry_inode.size, block.dirent[d].d_name);
                }
            }
        }
        remaining_dirents -= DIRENTS_PER_BLOCK;
        offset += DIRENTS_PER_BLOCK * sizeof(struct fs_dirent);
    }
    return 0;
}

/** creates a new link to an existing file;
 *  returns the file inode number or -1 if error.
 */
int fs_link(char *filename, char *newlink)
{
    if (check_rootSB() == -1)
        return -1;

    int file_ino = get_inode(filename);
    if (file_ino == -1)
    {
        return -1; // file does not exist
    }

    struct fs_inode file_inode;
    if (inode_load(file_ino, &file_inode) == -1)
    {
        return -1; // error loading inode
    }

    if (file_inode.type != IFREG)
    {
        return -1; // has to be file
    }

    char *newlink_name = get_filename(newlink);
    if (newlink_name == NULL)
        return -1; // invalid filename
    
    int parent_ino = get_parent_inode(newlink);
    struct fs_inode parent_inode;
    if (parent_ino == -1)
    {
        return -1; // error getting parent inode
    }
    else if (inode_load(parent_ino, &parent_inode) == -1)
    {
        return -1; // error loading parent inode
    }
    else if (parent_inode.type != IFDIR)
    {
        return -1; // parent has to be a directory
    }
    else if (dir_findname(&parent_inode, newlink_name) != -1)
    {
        return -1; // newlink already exists
    }

    if (add_entry_to_directory(parent_ino, newlink_name, file_ino) == -1)
    {
        return -1; // error adding entry to directory
    }

    file_inode.nlinks++;
    inode_save(file_ino, &file_inode);

    return file_ino;
}

/** creates a new file;
 *  returns the allocated inode number or -1 if error.
 */
int fs_create(char *filename)
{
    if (check_rootSB() == -1)
        return -1;
    if (filename == NULL || strlen(filename) == 0)
    {
        return -1;
    }
    char *file_name = get_filename(filename);
    if (file_name == NULL)
        return -1;
    
    int parent_ino = get_parent_inode(filename);
    if (parent_ino == -1)
    {
        return -1;
    }
    struct fs_inode parent_inode;
    if (inode_load(parent_ino, &parent_inode) == -1)
    {
        return -1;
    }
    if (parent_inode.type != IFDIR)
    {
        return -1;
    }
    if (dir_findname(&parent_inode, file_name) != -1)
    {
        return -1; // file already exists
    }
    int new_file_ino = inode_alloc();
    if (new_file_ino == -1)
    {
        return -1;
    }

    struct fs_inode new_file_inode;
    memset(&new_file_inode, 0, sizeof(new_file_inode));

    new_file_inode.type = IFREG;
    new_file_inode.nlinks = 1;
    new_file_inode.size = 0;

    if (inode_save(new_file_ino, &new_file_inode) == -1)
    {
        return -1;
    }
    if (add_entry_to_directory(parent_ino, file_name, new_file_ino) == -1)
    {
        inode_free(new_file_ino); // cleanup
        return -1;
    }
    return new_file_ino;
}

/** creates a new directory;
 *  returns the allocated inode number or -1 if error.
 */
int fs_mkdir(char *dirname)
{
    if (check_rootSB() == -1)
        return -1;

    if (dirname == NULL || strlen(dirname) == 0)
    {
        return -1;
    }

    char *file_name = get_filename(dirname);
    if (file_name == NULL)
        return -1;

    int parent_ino = get_parent_inode(dirname);
    if (parent_ino == -1)
    {
        return -1;
    }

    struct fs_inode parent_inode;

    if (inode_load(parent_ino, &parent_inode) == -1)
    {
        return -1;
    }

    if (parent_inode.type != IFDIR)
    {
        return -1;
    }

    if (dir_findname(&parent_inode, file_name) != -1)
    {
        return -1; // directory already exists
    }

    int new_dir_ino = inode_alloc();
    if (new_dir_ino == -1)
    {
        return -1;
    }

    struct fs_inode new_dir_inode;
    memset(&new_dir_inode, 0, sizeof(new_dir_inode));

    new_dir_inode.type = IFDIR;
    new_dir_inode.nlinks = 1;
    new_dir_inode.size = 0;

    if (inode_save(new_dir_ino, &new_dir_inode) == -1)
    {
        return -1;
    }

    if (add_entry_to_directory(parent_ino, file_name, new_dir_ino) == -1)
    {
        inode_free(new_dir_ino); // cleanup
        return -1;
    }
    return new_dir_ino;
}

/** * Finds name in directory, marks it as FREE on disk, and returns the
 * inode number that was removed. Returns -1 if not found.
 */
int dir_remove_entry(struct fs_inode *dir_inode, char *name)
{
    int remaining_dirents = dir_inode->size / sizeof(struct fs_dirent);
    int offset = 0;
    union fs_block block;

    while (remaining_dirents > 0)
    {
        int currBlock = offset2block(dir_inode, offset);

        if (currBlock == -1 || currBlock == 0)
            return -1;

        disk_read(currBlock, block.data);

        for (int d = 0; d < DIRENTS_PER_BLOCK && d < remaining_dirents; d++)
        {
            if (block.dirent[d].d_ino != FREE &&
                strncmp(block.dirent[d].d_name, name, MAXFILENAME) == 0)
            {
                int removed_ino = block.dirent[d].d_ino;
                block.dirent[d].d_ino = FREE;
                memset(block.dirent[d].d_name, 0, MAXFILENAME);
                disk_write(currBlock, block.data);
                return removed_ino;
            }
        }
        remaining_dirents -= DIRENTS_PER_BLOCK;
        offset += DIRENTS_PER_BLOCK * sizeof(struct fs_dirent);
    }
    return -1;
}

/** unlinks filename (this is for files);
 *  free inode and data blocks if it is last link.
 *  returns filename inode number or -1 if error.
 */
int fs_unlink(char *filename)
{
    if (check_rootSB() == -1)
        return -1;

    char *link_name = get_filename(filename);
    if (link_name == NULL)
        return -1;
    
    int parent_ino = get_parent_inode(filename);

    struct fs_inode parent_inode;

    if (parent_ino == -1)
        return -1;

    if (inode_load(parent_ino, &parent_inode) == -1)
        return -1;

    if (parent_inode.type != IFDIR)
        return -1;

    // First, get the inode number and check if it's a regular file
    int linked_entry_ino = dir_findname(&parent_inode, link_name);
    if (linked_entry_ino == -1)
        return -1;

    struct fs_inode linked_entry_inode;
    if (inode_load(linked_entry_ino, &linked_entry_inode) == -1)
        return -1;

    if (linked_entry_inode.type != IFREG)
        return -1;

    // Now remove the directory entry
    int removed_ino = dir_remove_entry(&parent_inode, link_name);
    if (removed_ino == -1)
        return -1;

    linked_entry_inode.nlinks--;

    if (linked_entry_inode.nlinks == 0)
    {
        // Free direct blocks
        for (int i = 0; i < DIRBLOCK_PER_INODE; i++)
        {
            if (linked_entry_inode.dir_block[i] != 0)
            {
                block_free(linked_entry_inode.dir_block[i]);
            }
        }
        // Free indirect blocks
        if (linked_entry_inode.indir_block != 0)
        {
            uint16_t ind_data[BLOCKSZ / sizeof(uint16_t)];
            disk_read(linked_entry_inode.indir_block, (char *)ind_data);

            // Calculate exact number of used indirect blocks
            int total_blocks = (linked_entry_inode.size + BLOCKSZ - 1) / BLOCKSZ;
            int indirect_count = total_blocks - DIRBLOCK_PER_INODE;
            if (indirect_count < 0)
                indirect_count = 0;

            for (int k = 0; k < indirect_count && k < (BLOCKSZ / sizeof(uint16_t)); k++)
            {
                if (ind_data[k] != 0)
                    block_free(ind_data[k]);
            }
            block_free(linked_entry_inode.indir_block);
        }
        inode_free(linked_entry_ino); // Free the inode itself
    }
    else
    {
        inode_save(linked_entry_ino, &linked_entry_inode); // Update link count
    }

    return linked_entry_ino;
}


/** dump Super block (usually block 0) from disk to stdout for debugging
 */
void dumpSB(int numb) {
    union fs_block block;

    disk_read(numb, block.data);
    printf("Disk superblock %d:\n", numb);
    printf("    magic = %x\n", block.super.magic);
    printf("    disk size %d blocks\n", block.super.block_cnt);
    printf("    block size %d bytes\n", block.super.block_size);
    printf("    bmap_size: %d\n", block.super.bmap_size);
    printf("    first inode block: %d\n", block.super.first_inodeblk);
    printf("    inode_blocks: %d (%d inodes)\n", block.super.inode_blocks,
           block.super.inode_cnt);
    printf("    first data block: %d\n", block.super.first_datablk);
    printf("    data blocks: %d\n", block.super.block_cnt - block.super.first_datablk);
}

/** prints information details about file system for debugging
 */
void fs_debug() {
    union fs_block block;

    dumpSB(SBLOCK);
    if (check_rootSB() == -1) return;

    disk_read(SBLOCK, block.data);
    rootSB = block.super;
    printf("**************************************\n");
    printf("blocks in use - bitmap:\n");
    int nblocks = rootSB.block_cnt;
    for (int i = 0; i < rootSB.bmap_size; i++) {
        disk_read(BITMAPSTART + i, block.data);
        bitmap_print(block.data, MIN(BLOCKSZ*8, nblocks));
        nblocks -= BLOCKSZ * 8;
    }
    printf("**************************************\n");
    printf("inodes in use:\n");
    for (int i = 0; i < rootSB.inode_blocks; i++) {
        disk_read(INODESTART + i, block.data);
        for (int j = 0; j < INODES_PER_BLOCK; j++)
            if (block.inode[j].type != IFFREE) {
                printf(" %d:type=%d;size=%d;nlinks=%d\n",
                    j + i * INODES_PER_BLOCK,
                    block.inode[j].type, block.inode[j].size,
                    block.inode[j].nlinks);
            }
    }
    printf("**************************************\n");
}


/*****************************************************/

/** format the disk = initialize the disk with the FS structures;
 *   rootSB is also initialized for this FS (mounted)
 */
int fs_format() {
    union fs_block freebitmap;
    int nblocks, root_inode;

    if (check_rootSB() == 0) {
        printf("Cannot format a mounted disk!\n");
        return -1;
    }
    nblocks = disk_size();

    // disk must be at least 4 blocks size...
    memset(&rootSB, 0, sizeof(rootSB)); // empty rootSB
    rootSB.magic = FS_MAGIC;
    rootSB.block_cnt = nblocks; // disk size in blocks
    rootSB.block_size = BLOCKSZ;

    // bitmap needs 1 bit per block (in a disk block there are 8*BLOCKSZ bits)
    // number of blocks needed for nblocks' bitmap (rounded up):
    rootSB.bmap_size = nblocks / (8 * BLOCKSZ) + (nblocks % (8 * BLOCKSZ) != 0);

    rootSB.first_inodeblk = 1 + rootSB.bmap_size;

    int inodes = (nblocks + 3) / 4; // number of inodes at least 1/4 the number of blocks
    assert(inodes>0); // at least 1 inode
    rootSB.inode_blocks = inodes / INODES_PER_BLOCK + (inodes % INODES_PER_BLOCK != 0); // round up
    rootSB.inode_cnt = rootSB.inode_blocks * INODES_PER_BLOCK;

    rootSB.first_datablk = rootSB.first_inodeblk + rootSB.inode_blocks;

    /* update superblock in disk (block 0)*/
    disk_write(SBLOCK, (char*)&rootSB);
    dumpSB(SBLOCK); // print what is now stored on the disk

    /* initialize bitmap blocks */
    memset(&freebitmap, 0, sizeof(freebitmap));
    for (int i = 0; i < rootSB.first_datablk; i++)
        bitmap_set(freebitmap.data, i);
    disk_write(BITMAPSTART, freebitmap.data);
    memset(&freebitmap, 0, sizeof(freebitmap));
    for (int i = 1; i < rootSB.bmap_size; i++)
        disk_write(BITMAPSTART + i, freebitmap.data);

    /* initialize inodes table blocks */
    for (int i = 0; i < rootSB.inode_blocks; i++)
        disk_write(INODESTART + i, freebitmap.data);

    /* create root dir */
    root_inode = inode_alloc();
    if (root_inode != 0)
        printf("ERROR: ROOT INODE %d!? IS NOT 0!?\n", root_inode);
    struct fs_inode rootdir = {
        .type = IFDIR,
        .size = 0,
        .dir_block = {0},
        .indir_block = 0
    };
    inode_save(root_inode, &rootdir);

    return 0;
}

/** mount root FS;
 *  open device image or create it;
 *  loads superblock from device into global variable rootSB;
 *  returns -1 if error
 */
int fs_mount(char *device, int size) {
    union fs_block block;

    if (rootSB.magic == FS_MAGIC) {
        printf("A disc is already mounted!\n");
        return -1;
    }
    if (disk_init(device, size) < 0) return -1; // open disk image or create if it does not exist
    disk_read(SBLOCK, block.data);
    if (block.super.magic != FS_MAGIC) {
        printf("Unformatted disc! Not mounted.\n");
        return -1;
    }
    rootSB = block.super;
    return 0;
}


