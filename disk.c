
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "disk.h"

static FILE *diskfile;
static unsigned nblocks = 0;
static unsigned nreads = 0;
static unsigned nwrites = 0;


/** opens filename as a virtual disk device;
 *  if n == -1 uses an already available "device";
 *  else creates a new "device" with n blocks;
 *  returns -1 if error, 0 if sucess
 */
int disk_init(const char *filename, int n) {
    diskfile = fopen(filename, "r+");
    if (diskfile != NULL) {
        fseek(diskfile, 0L, SEEK_END);   // ignore provided n
        n = ftell(diskfile);
        fprintf(stderr, "Disk image size=%d, %d blocks\n", n, n / DISK_BLOCK_SIZE);
        n = n / DISK_BLOCK_SIZE;
    }
    if (diskfile==NULL && n>0)
        diskfile = fopen(filename, "w+");
    if (diskfile==NULL)
        return -1;

    ftruncate(fileno(diskfile), n * DISK_BLOCK_SIZE);
    nblocks = n;
    nreads = 0;
    nwrites = 0;
    return 0;
}

/** returns the device size in blocks
 */
unsigned disk_size() { 
	return nblocks; 
}

/** checks that blocknum and data are valid
 */
static void sanity_check(unsigned blocknum, const void *data) {
    if (blocknum >= nblocks) {
        printf("DISK ERROR: blocknum (%d) is too big!\n", blocknum);
        abort();
    }

    if (!data) {
        printf("DISK ERROR: null data pointer!\n");
        abort();
    }
}

/** reads one disk block to data
 */
void disk_read(unsigned blocknum, char *data) {
    sanity_check(blocknum, data);

    fseek(diskfile, blocknum * DISK_BLOCK_SIZE, SEEK_SET);

    if (fread(data, DISK_BLOCK_SIZE, 1, diskfile) == 1) {
        nreads++;
    } else {
        printf("DISK ERROR: couldn't access simulated disk: %s\n", strerror(errno));
        abort();
    }
}

/** writes data to one disk block
 */
void disk_write(unsigned blocknum, const char *data) {
    sanity_check(blocknum, data);

    fseek(diskfile, blocknum * DISK_BLOCK_SIZE, SEEK_SET);
    //printf("write block %d (byte offset %d)\n", blocknum, blocknum * DISK_BLOCK_SIZE);
    if (fwrite(data, DISK_BLOCK_SIZE, 1, diskfile) == 1) {
        nwrites++;
    } else {
        printf("DISK ERROR: couldn't access simulated disk: %s\n", strerror(errno));
        abort();
    }
}

/** close device (closes the file that simulates the disk device)
 */
void disk_close() {
    if (diskfile) {
        //printf("%d disk block reads\n", nreads);
        //printf("%d disk block writes\n", nwrites);
        fclose(diskfile);
        diskfile = 0;
    }
}
