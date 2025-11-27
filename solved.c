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
 * FS block size =3D disk block size
 * block#
 * 0            super block (includes the number of inodes)
 * 1 ...        start of bitmap with free/used blocks
 * after bitmap follows blocks with inodes (root dir is always inode 0)
 * after inodes follows the data blocks
 */

#define BLOCKSZ    (DISK_BLOCK_SIZE)
#define SBLOCK     0  // superblock is in disk block 0
#define BITMAPSTART 1   // free/use block bitmap starts in block 1
#define INODESTART  (rootSB.first_inodeblk)  // inodes start in this block
#define ROOTINO    0  // root dir is described in inode 0

#define FS_MAGIC    (0xf50f5025) // when formated the SB starts with this n=
umber
#define DIRBLOCK_PER_INODE 11    // number of direct block indexes in inode
#define MAXFILENAME        62    // max name size in a dirent

#define INODESZ    ((int)sizeof(struct fs_inode))
#define INODES_PER_BLOCK       (BLOCKSZ/INODESZ)
#define DIRENTS_PER_BLOCK      (BLOCKSZ/sizeof(struct fs_dirent))

enum inode_type {
    IFFREE =3D 0,  // inode is free
    IFDIR  =3D 4,  // inode is dir
    IFREG  =3D 8   // inode is regular file
};

#define FREE 0

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

/*****************************************************/

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
// superblock, a block of inodes, a block of dirents, or data (a byte array=
)
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

/*****************************************************/

/** checks that the global rootSB contains a valid super block of a
formated disk
 *  returns -1 if error; 0 if it's OK
 */
int check_rootSB() {
    if (rootSB.magic !=3D FS_MAGIC) {
        printf("disc not mounted\n");
        return -1;
    }
    return 0;
}

/** load from disk the inode ino_number into ino (must be an
initialized pointer);
 *  returns 0 if inode read. The ino.type =3D=3D FREE if ino_number is of
a free inode;
 *  returns -1 ino_number outside the existing limits.
 */
int inode_load(int ino_number, struct fs_inode *ino) {
    union fs_block block;

    if (ino_number<0 || ino_number >=3D rootSB.inode_cnt) {
        printf("inode_load: inode number too big\n");
        ino->type =3D FREE;
        return -1;
    }
    int inodeBlock =3D rootSB.first_inodeblk + (ino_number / INODES_PER_BLO=
CK);
    disk_read(inodeBlock, block.data);
    *ino =3D block.inode[ino_number % INODES_PER_BLOCK];
    return 0;
}

/** save to disk the inode ino to the ino_number position;
 *  returns 0 if saved;
 *  if ino_number is outside limits, nothing is done and returns -1
 */
int inode_save(int ino_number, struct fs_inode *ino) {
    union fs_block block;

    if (ino_number<0 || ino_number >=3D rootSB.inode_cnt) {
        printf("inode_save: inode number too big\n");
        return -1;
    }
    int inodeBlock =3D rootSB.first_inodeblk + (ino_number / INODES_PER_BLO=
CK);
    disk_read(inodeBlock, block.data); // read full block
    block.inode[ino_number % INODES_PER_BLOCK] =3D *ino; // update inode
    disk_write(inodeBlock, block.data); // write block
    return 0;
}

/** finds an inode not in use (disk is not changed)
 *  returns the inode number;  or -1 if no more inodes.
 */
int inode_alloc() {
    int inodeBlock =3D 0;
    do {
        union fs_block block;
        disk_read(INODESTART + inodeBlock, block.data);
        for (int i =3D 0; i < INODES_PER_BLOCK; i++)
            if (block.inode[i].type =3D=3D IFFREE) {
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
    if ( inode_load(ino_number, &inode) =3D=3D -1 )
        return -1;
    inode.type =3D IFFREE;
    return inode_save(ino_number, &inode);
}



/** finds a free disk data block in the bitmap and marks it in use;
 *  returns the block number; returns -1 if no more free blocks.
 */
int block_alloc() {
    union fs_block block;
    int bitmapBlock =3D 0;

    do {
        disk_read(BITMAPSTART + bitmapBlock, block.data);
        for (int i =3D 0; i < BLOCKSZ * 8 && bitmapBlock * BLOCKSZ * 8 +
i < rootSB.block_cnt; i++)
            if (bitmap_get(block.data, i) =3D=3D 0) {
                bitmap_set(block.data, i); // found one free, mark it in us=
e
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
    int bitmapBlock =3D nblock / (BLOCKSZ * 8); // bitmap block where this =
bit is
    int offsetBlock =3D nblock % (BLOCKSZ * 8); // offset inside this block
    if (bitmapBlock >=3D rootSB.bmap_size || bitmapBlock < 0)
        return -1; // outside disk size; ignore it

    // printf("block_free: %d\n", nblock);
    disk_read(BITMAPSTART + bitmapBlock, block.data);
    bitmap_clear(block.data, offsetBlock);
    disk_write(BITMAPSTART + bitmapBlock, block.data); // update disk
    return 0;
}

/*****************************************************/

/** finds the disk block number that contains the byte at the given offset
 *  for the file or directory described by the given inode;
 *  returns the disk block number, or -1 if error.
 */
int offset2block(struct fs_inode *inode, int offset) {
    int blkindex =3D offset / BLOCKSZ; // What is the block for this offset=
?

    if (blkindex < DIRBLOCK_PER_INODE) { // is in a direct index
        return inode->dir_block[blkindex];
    } else if (blkindex < DIRBLOCK_PER_INODE + BLOCKSZ / sizeof(uint16_t)) =
{
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

/*****************************************************/

/** find name in the directory given by dir_inode;
 *  returns its inode number (from dirent); or -1 if error or not found
 */
int dir_findname(struct fs_inode *dir_inode, char *name) {
    if (dir_inode->type!=3DIFDIR) return -1; // not a directory
    int remaining_dirents =3D dir_inode->size / sizeof(struct fs_dirent);
    int offset =3D 0;
    union fs_block block;

    while ( remaining_dirents>0 ) {
        int currBlock =3D offset2block(dir_inode, offset);
        disk_read(currBlock, block.data);
        for (int d =3D 0; d < DIRENTS_PER_BLOCK && d < remaining_dirents; d=
++) {
            if (block.dirent[d].d_ino!=3DFREE
                && strncmp(block.dirent[d].d_name, name, MAXFILENAME) =3D=
=3D 0)
                return block.dirent[d].d_ino;  // found!
        }
        remaining_dirents -=3D DIRENTS_PER_BLOCK;
        offset +=3D DIRENTS_PER_BLOCK * sizeof(struct fs_dirent);
    }
    return -1;  // not found
}



/*****************************************************/


//extra aux functions

static int path_to_inode(char *pathname) {
    struct fs_inode inode;
    char path_copy[1024];
    char *token;
    int current_ino =3D ROOTINO;

    if (pathname =3D=3D NULL || pathname[0] =3D=3D '\0')
        return ROOTINO;

    strncpy(path_copy, pathname, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] =3D '\0';

    char *path =3D path_copy;
    if (path[0] =3D=3D '/')
        path++;

    if (path[0] =3D=3D '\0')
        return ROOTINO;

    token =3D strtok(path, "/");
    while (token !=3D NULL) {
        if (inode_load(current_ino, &inode) =3D=3D -1)
            return -1;

        if (inode.type !=3D IFDIR)
            return -1;

        current_ino =3D dir_findname(&inode, token);
        if (current_ino =3D=3D -1)
            return -1;

        token =3D strtok(NULL, "/");
    }

    return current_ino;
}


int split_path(char *pathname, char *filename) {
    char path_copy[1024];
    char *last_slash;
    int parent_inode_number;

    if (pathname =3D=3D NULL || pathname[0] =3D=3D '\0')
        return -1;

    strncpy(path_copy, pathname, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] =3D '\0';

    last_slash =3D strrchr(path_copy, '/');

    if (last_slash =3D=3D NULL) {
        strncpy(filename, path_copy, MAXFILENAME - 1);
        filename[MAXFILENAME - 1] =3D '\0';
        return ROOTINO;
    }

    strncpy(filename, last_slash + 1, MAXFILENAME - 1);
    filename[MAXFILENAME - 1] =3D '\0';

    if (last_slash =3D=3D path_copy) {
        return ROOTINO;
    }

    *last_slash =3D '\0';
    parent_inode_number =3D path_to_inode(path_copy);

    return parent_inode_number;
}

int dir_add_entry(struct fs_inode *dir_inode, int dir_ino, int
new_ino, char *name) {
    union fs_block block;
    int offset =3D 0;
    int remaining_dirents =3D dir_inode->size / sizeof(struct fs_dirent);

    while (remaining_dirents > 0) {
        int currentBlock =3D offset2block(dir_inode, offset);
        if (currentBlock <=3D0) break;

        disk_read(currentBlock, block.data);
        for (int d =3D 0; d < DIRENTS_PER_BLOCK && d < remaining_dirents; d=
++) {
            if (block.dirent[d].d_ino =3D=3D FREE) {
                block.dirent[d].d_ino =3D new_ino;
                strncpy(block.dirent[d].d_name, name, MAXFILENAME - 1);
                block.dirent[d].d_name[MAXFILENAME - 1] =3D '\0';
                disk_write(currentBlock, block.data);
                return 0;
            }
        }
        remaining_dirents -=3D DIRENTS_PER_BLOCK;
        offset +=3D DIRENTS_PER_BLOCK * sizeof(struct fs_dirent);
    }

    int used_dirents =3D dir_inode->size / sizeof(struct fs_dirent);
    int block_index  =3D used_dirents / DIRENTS_PER_BLOCK;
    int pos_in_block =3D used_dirents % DIRENTS_PER_BLOCK;
    int target_block;


    if (pos_in_block =3D=3D 0) {
        int new_block =3D block_alloc();
        if (new_block =3D=3D -1) return -1;

        memset(block.data, 0, BLOCKSZ);
        disk_write(new_block, block.data);

        if (block_index < DIRBLOCK_PER_INODE) {
            dir_inode->dir_block[block_index] =3D new_block;
        } else if (block_index < DIRBLOCK_PER_INODE + BLOCKSZ /
sizeof(uint16_t)) {
            uint16_t indir_data[BLOCKSZ / sizeof(uint16_t)];
            if (dir_inode->indir_block =3D=3D 0) {
                dir_inode->indir_block =3D block_alloc();
                if (dir_inode->indir_block =3D=3D -1) {
                    block_free(new_block);
                    return -1;
                }
                union fs_block zblock;
                memset(zblock.data, 0, BLOCKSZ);
                disk_write(dir_inode->indir_block, zblock.data);
                memset(indir_data, 0, BLOCKSZ);
            } else {
                disk_read(dir_inode->indir_block, (char*)indir_data);
            }
            indir_data[block_index - DIRBLOCK_PER_INODE] =3D new_block;
            disk_write(dir_inode->indir_block, (char*)indir_data);
        } else {
            block_free(new_block);
            return -1;
        }
        target_block =3D new_block;
    }
    else {
        target_block =3D offset2block(dir_inode, used_dirents *
sizeof(struct fs_dirent));
        if (target_block =3D=3D -1) return -1;
    }

    disk_read(target_block, block.data);

    block.dirent[pos_in_block].d_ino =3D new_ino;
    strncpy(block.dirent[pos_in_block].d_name, name, MAXFILENAME - 1);
    block.dirent[pos_in_block].d_name[MAXFILENAME - 1] =3D '\0';

    disk_write(target_block, block.data);

    dir_inode->size +=3D sizeof(struct fs_dirent);
    inode_save(dir_ino, dir_inode);

    return 0;
}


/** list the content of directory dirname
 *  dirname may start with "/" or not;
 *  dirname may be one name or a pathname with subdirectories.
 */
int fs_ls(char *dirname) {
    if (check_rootSB() =3D=3D -1) return -1;

    struct fs_inode dir_inode, file_inode;
    union fs_block block;
    int ino_number;
    char type_char;

    if (dirname =3D=3D NULL || dirname[0] =3D=3D '\0')
        dirname =3D "/";

    ino_number =3D path_to_inode(dirname);
    if (ino_number =3D=3D -1) {
        return -1;
    }

    if (inode_load(ino_number, &dir_inode) =3D=3D -1)
        return -1;

    if (dir_inode.type !=3D IFDIR) {
        return -1;
    }

    printf("listing dir %s (inode %d):\n", dirname, ino_number);
    printf("ino:type:nlk    bytes name\n");

    int remaining_dirents =3D dir_inode.size / sizeof(struct fs_dirent);
    int offset =3D 0;

    while (remaining_dirents > 0) {
        int currentBlock =3D offset2block(&dir_inode, offset);
        if (currentBlock <=3D 0) return -1;

        disk_read(currentBlock, block.data);
        for (int d =3D 0; d < DIRENTS_PER_BLOCK && d < remaining_dirents; d=
++) {
            if (block.dirent[d].d_ino !=3D FREE) {
                if (inode_load(block.dirent[d].d_ino, &file_inode) =3D=3D 0=
) {
                    if (file_inode.type =3D=3D IFDIR)
                        type_char =3D 'D';
                    else if (file_inode.type =3D=3D IFREG)
                        type_char =3D 'F';
                    else
                        type_char =3D '?';

                    printf("%3d:%4c:%3d%9d %s\n",
                           block.dirent[d].d_ino,
                           type_char,
                           file_inode.nlinks,
                           file_inode.size,
                           block.dirent[d].d_name);
                }
            }
        }
        remaining_dirents -=3D DIRENTS_PER_BLOCK;
        offset +=3D DIRENTS_PER_BLOCK * sizeof(struct fs_dirent);
    }

    return 0;
}


/** creates a new link to an existing file;
 *  returns the file inode number or -1 if error.
 */
int fs_link(char *filename, char *newlink) {
    if (check_rootSB() =3D=3D -1) return -1;
    if (filename =3D=3D NULL || filename[0] =3D=3D '\0') return -1; //
verificar dps se os 2 sao preciso
    if (newlink =3D=3D NULL || newlink[0] =3D=3D '\0') return -1;

    struct fs_inode file_inode, parent_inode;
    char new_name[MAXFILENAME];
    int file_ino, parent_inode_number;

    file_ino =3D path_to_inode(filename);
    if (file_ino =3D=3D -1) {
        return -1;
    }

    if (inode_load(file_ino, &file_inode) =3D=3D -1)
        return -1;

    if (file_inode.type !=3D IFREG) {
        return -1;
    }

    parent_inode_number =3D split_path(newlink, new_name);
    if (new_name[0] =3D=3D '\0'){
        return -1;
    }
    if (parent_inode_number =3D=3D -1) {
        return -1;
    }

    if (inode_load(parent_inode_number, &parent_inode) =3D=3D -1)
        return -1;

    if (parent_inode.type !=3D IFDIR) {
        return -1;
    }

    if (dir_findname(&parent_inode, new_name) !=3D -1) {
        return -1;
    }

    if (dir_add_entry(&parent_inode, parent_inode_number, file_ino,
new_name) =3D=3D -1) {
        return -1;
    }

    file_inode.nlinks++;
    if (inode_save(file_ino, &file_inode) =3D=3D -1){
        return -1;
    }
    return file_ino;
}

/** creates a new file;
 *  returns the allocated inode number or -1 if error.
 */
int fs_create(char *filename) {
    if (check_rootSB() =3D=3D -1) return -1;
    if (filename =3D=3D NULL || filename[0] =3D=3D '\0') return -1;


    struct fs_inode parent_inode, new_inode;
    char name[MAXFILENAME];
    int parent_inode_number, new_ino;

    parent_inode_number =3D split_path(filename, name);
        if (name[0] =3D=3D '\0') {
        return -1;
    }
    if (parent_inode_number =3D=3D -1) {
        return -1;
    }



    if (inode_load(parent_inode_number, &parent_inode) =3D=3D -1)
        return -1;

    if (parent_inode.type !=3D IFDIR) {
        return -1;
    }

    if (dir_findname(&parent_inode, name) !=3D -1) {
        return -1;
    }

    new_ino =3D inode_alloc();
    if (new_ino =3D=3D -1) {
        return -1;
    }

    memset(&new_inode, 0, sizeof(new_inode));
    new_inode.type =3D IFREG;
    new_inode.size =3D 0;
    new_inode.nlinks =3D 1;

    if (inode_save(new_ino, &new_inode) =3D=3D -1) {
        inode_free(new_ino);
        return -1;
    }

    if (dir_add_entry(&parent_inode, parent_inode_number, new_ino,
name) =3D=3D -1) {
        inode_free(new_ino);
        return -1;
    }

    return new_ino;
}


/** creates a new directory;
 *  returns the allocated inode number or -1 if error.
 */
int fs_mkdir(char *dirname) {
    if (check_rootSB() =3D=3D -1) return -1;
    if (dirname =3D=3D NULL || dirname[0] =3D=3D '\0'){ // dps confirmar se=
 =C3=A9 preciso
       return -1;
}
    struct fs_inode parent_inode, new_inode;
    char name[MAXFILENAME];
    int parent_inode_number, new_ino;

    parent_inode_number =3D split_path(dirname, name);
    if (name[0] =3D=3D '\0') {
        return -1;
    }
    if (parent_inode_number =3D=3D -1) {
        return -1;
    }

    if (inode_load(parent_inode_number, &parent_inode) =3D=3D -1)
        return -1;

    if (parent_inode.type !=3D IFDIR) {
        return -1;
    }

    if (dir_findname(&parent_inode, name) !=3D -1) {
        return -1;
    }

    new_ino =3D inode_alloc();
    if (new_ino =3D=3D -1) {
        return -1;
    }

    memset(&new_inode, 0, sizeof(new_inode));
    new_inode.type =3D IFDIR;
    new_inode.size =3D 0;
    new_inode.nlinks =3D 1;

    if (inode_save(new_ino, &new_inode) =3D=3D -1) {
        inode_free(new_ino);
        return -1;
    }

    if (dir_add_entry(&parent_inode, parent_inode_number, new_ino,
name) =3D=3D -1) {
        inode_free(new_ino);
        return -1;
    }

    return new_ino;
}




/** unlinks filename (this is for files);
 *  free inode and data blocks if it is last link.
 *  returns filename inode number or -1 if error.
 */

int fs_unlink(char *filename) {
    if (check_rootSB() =3D=3D -1) return -1;
    if (filename =3D=3D NULL || filename[0] =3D=3D '\0') return -1;

    struct fs_inode parent_inode, file_inode;
    union fs_block block;
    char name[MAXFILENAME];
    int parent_inode_number, file_ino;
    int offset =3D 0;

    parent_inode_number =3D split_path(filename, name);
    if (name[0] =3D=3D '\0'){
        return -1;
    }
    if (parent_inode_number =3D=3D -1) {
        return -1;
    }

    if (inode_load(parent_inode_number, &parent_inode) =3D=3D -1)
        return -1;

    if (parent_inode.type !=3D IFDIR) {
        return -1;
    }

    file_ino =3D dir_findname(&parent_inode, name);
    if (file_ino =3D=3D -1) {
        return -1;
    }

    if (inode_load(file_ino, &file_inode) =3D=3D -1)
        return -1;

    if (file_inode.type !=3D IFREG) {
        return -1;
    }

    int remaining_dirents =3D parent_inode.size / sizeof(struct fs_dirent);
    offset =3D 0;
    int found =3D 0;

    while (remaining_dirents > 0 && !found) {
        int currentBlock =3D offset2block(&parent_inode, offset);
        if (currentBlock =3D=3D -1) return -1;

        disk_read(currentBlock, block.data);
        for (int d =3D 0; d < DIRENTS_PER_BLOCK && d < remaining_dirents; d=
++) {
            if (block.dirent[d].d_ino =3D=3D file_ino &&
                strncmp(block.dirent[d].d_name, name, MAXFILENAME) =3D=3D 0=
) {
                block.dirent[d].d_ino =3D FREE;
                disk_write(currentBlock, block.data);
                found =3D 1;
                break;
            }
        }
        remaining_dirents -=3D DIRENTS_PER_BLOCK;
        offset +=3D DIRENTS_PER_BLOCK * sizeof(struct fs_dirent);
    }

    if (!found) {
        return -1;
    }

    file_inode.nlinks--;

    if (file_inode.nlinks =3D=3D 0) {
        int num_blocks =3D (file_inode.size + BLOCKSZ - 1) / BLOCKSZ;

        for (int i =3D 0; i < DIRBLOCK_PER_INODE && i < num_blocks; i++) {
            if (file_inode.dir_block[i] !=3D 0)
                block_free(file_inode.dir_block[i]);
        }

        if (num_blocks > DIRBLOCK_PER_INODE && file_inode.indir_block !=3D =
0) {
            uint16_t indir_data[BLOCKSZ / sizeof(uint16_t)];
            disk_read(file_inode.indir_block, (char*)indir_data);

            int indirect_blocks =3D num_blocks - DIRBLOCK_PER_INODE;
            for (int i =3D 0; i < indirect_blocks && i < BLOCKSZ /
sizeof(uint16_t); i++) {
                if (indir_data[i] !=3D 0)
                    block_free(indir_data[i]);
            }

            block_free(file_inode.indir_block);
        }

        if (inode_free(file_ino) =3D=3D -1){
            return -1;
        }
    } else {
        if (inode_save(file_ino, &file_inode) =3D=3D -1){
        return -1;
    }
    }

    return file_ino;
}



/*****************************************************/

/** dump Super block (usually block 0) from disk to stdout for debugging
 */
void dumpSB(int numb) {
    union fs_block block;

    disk_read(numb, block.data);
    printf("Disk superblock %d:\n", numb);
    printf("    magic =3D %x\n", block.super.magic);
    printf("    disk size %d blocks\n", block.super.block_cnt);
    printf("    block size %d bytes\n", block.super.block_size);
    printf("    bmap_size: %d\n", block.super.bmap_size);
    printf("    first inode block: %d\n", block.super.first_inodeblk);
    printf("    inode_blocks: %d (%d inodes)\n", block.super.inode_blocks,
           block.super.inode_cnt);
    printf("    first data block: %d\n", block.super.first_datablk);
    printf("    data blocks: %d\n", block.super.block_cnt -
block.super.first_datablk);
}

/** prints information details about file system for debugging
 */
void fs_debug() {
    union fs_block block;

    dumpSB(SBLOCK);
    if (check_rootSB() =3D=3D -1) return;

    disk_read(SBLOCK, block.data);
    rootSB =3D block.super;
    printf("**************************************\n");
    printf("blocks in use - bitmap:\n");
    int nblocks =3D rootSB.block_cnt;
    for (int i =3D 0; i < rootSB.bmap_size; i++) {
        disk_read(BITMAPSTART + i, block.data);
        bitmap_print(block.data, MIN(BLOCKSZ*8, nblocks));
        nblocks -=3D BLOCKSZ * 8;
    }
    printf("**************************************\n");
    printf("inodes in use:\n");
    for (int i =3D 0; i < rootSB.inode_blocks; i++) {
        disk_read(INODESTART + i, block.data);
        for (int j =3D 0; j < INODES_PER_BLOCK; j++)
            if (block.inode[j].type !=3D IFFREE) {
                printf(" %d:type=3D%d;size=3D%d;nlinks=3D%d\n",
                    j + i * INODES_PER_BLOCK,
                    block.inode[j].type, block.inode[j].size,
                    block.inode[j].nlinks);
            }
    }
    printf("**************************************\n");
}


/*****************************************************/

/** format the disk =3D initialize the disk with the FS structures;
 *   rootSB is also initialized for this FS (mounted)
 */
int fs_format() {
    union fs_block freebitmap;
    int nblocks, root_inode;

    if (check_rootSB() =3D=3D 0) {
        printf("Cannot format a mounted disk!\n");
        return -1;
    }
    nblocks =3D disk_size();

    // disk must be at least 4 blocks size...
    memset(&rootSB, 0, sizeof(rootSB)); // empty rootSB
    rootSB.magic =3D FS_MAGIC;
    rootSB.block_cnt =3D nblocks; // disk size in blocks
    rootSB.block_size =3D BLOCKSZ;

    // bitmap needs 1 bit per block (in a disk block there are 8*BLOCKSZ bi=
ts)
    // number of blocks needed for nblocks' bitmap (rounded up):
    rootSB.bmap_size =3D nblocks / (8 * BLOCKSZ) + (nblocks % (8 * BLOCKSZ)=
 !=3D 0);

    rootSB.first_inodeblk =3D 1 + rootSB.bmap_size;

    int inodes =3D (nblocks + 3) / 4; // number of inodes at least 1/4
the number of blocks
    assert(inodes>0); // at least 1 inode
    rootSB.inode_blocks =3D inodes / INODES_PER_BLOCK + (inodes %
INODES_PER_BLOCK !=3D 0); // round up
    rootSB.inode_cnt =3D rootSB.inode_blocks * INODES_PER_BLOCK;

    rootSB.first_datablk =3D rootSB.first_inodeblk + rootSB.inode_blocks;

    /* update superblock in disk (block 0)*/
    disk_write(SBLOCK, (char*)&rootSB);
    dumpSB(SBLOCK); // print what is now stored on the disk

    /* initialize bitmap blocks */
    memset(&freebitmap, 0, sizeof(freebitmap));
    for (int i =3D 0; i < rootSB.first_datablk; i++)
        bitmap_set(freebitmap.data, i);
    disk_write(BITMAPSTART, freebitmap.data);
    memset(&freebitmap, 0, sizeof(freebitmap));
    for (int i =3D 1; i < rootSB.bmap_size; i++)
        disk_write(BITMAPSTART + i, freebitmap.data);

    /* initialize inodes table blocks */
    for (int i =3D 0; i < rootSB.inode_blocks; i++)
        disk_write(INODESTART + i, freebitmap.data);

    /* create root dir */
    root_inode =3D inode_alloc();
    if (root_inode !=3D 0)
        printf("ERROR: ROOT INODE %d!? IS NOT 0!?\n", root_inode);
    struct fs_inode rootdir =3D {
        .type =3D IFDIR,
        .size =3D 0,
        .dir_block =3D {0},
        .indir_block =3D 0
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

    if (rootSB.magic =3D=3D FS_MAGIC) {
        printf("A disc is already mounted!\n");
        return -1;
    }
    if (disk_init(device, size) < 0) return -1; // open disk image or
create if it does not exist
    disk_read(SBLOCK, block.data);
    if (block.super.magic !=3D FS_MAGIC) {
        printf("Unformatted disc! Not mounted.\n");
        return -1;
    }
    rootSB =3D block.super;
    return 0;
}