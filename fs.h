#ifndef FS_H
#define FS_H

void fs_debug();
int  fs_format();
int  fs_mount(char *device, int size);
int  fs_ls(char *dirname);
int  fs_create( char *filename );
int  fs_mkdir( char *dirname );
int  fs_unlink( char *filename );
int  fs_link(char *filename, char *newlink);

#endif
