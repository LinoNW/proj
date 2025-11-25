#ifndef DISK_H
#define DISK_H

#define DISK_BLOCK_SIZE 1024

int disk_init( const char *filename, int nblocks );
unsigned disk_size();
void disk_read( unsigned blocknum, char *data );
void disk_write( unsigned blocknum, const char *data );
void disk_close();


#endif
