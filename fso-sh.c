/*
 ============================================================================
 Name        : fso-sh.c
 Author      : vad
 Version     :
 Copyright   : FSO 2024 - 2025 - DI-FCT/UNL
 Description : toy file system browser/shell
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "fs.h"
#include "disk.h"



/** prints help message with available commands
*/
void print_help() {
    printf("Commands:\n");
    printf("    debug\n");
    printf("    ls [<dirname>]\n");
    printf("    create <filename>\n");
    printf("    rm <filename>\n");
    printf("    ln <filename> <newname>\n");
    printf("    mkdir  <dirname>\n");
    printf("    help or ?\n");
    printf("    quit or exit\n");
}


/**
 * MAIN
 * just a shell to browse and test our file system implementation
 */
int main(int argc, char *argv[]) {
    char line[1024];
    char cmd[1024];
    char arg1[1024];
    char arg2[1024];
    int inumber, args, nblocks;

    if (argc != 3 && argc != 2) {
        printf("use: %s diskfile          to use an existing disk\n", argv[0]);
        //printf("use: %s diskfile nblocks  to create a new disk with nblocks\n", argv[0]);
        return 1;
    }
    if (argc == 3)
        nblocks = atoi(argv[2]);
    else
        nblocks = -1;

    if (fs_mount(argv[1], nblocks) < 0) {
        printf("unable to initialize %s: %s\n", argv[1], strerror(errno));
        return 1;
    }

    while (1) {
        printf("fso-sh> ");
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin)==NULL)
            break;
        args = sscanf(line, "%s %s %s", cmd, arg1, arg2);
        if (args <= 0)
            continue;

        if (!strcmp(cmd, "debug")) {
            if (args == 1) {
                fs_debug();
            } else {
                printf("use: debug\n");
            }
        } else if (!strcmp(cmd, "ls")) {
            if (args == 1) {
                if (fs_ls("/")<0)
                    printf("list failed\n");
            } else if (args == 2) {
                if (fs_ls(arg1)<0)
                    printf("list failed\n");
            } else
                printf("use: ls [dirname]\n");
        } else if (!strcmp(cmd, "create")) {
            if (args == 2) {
                inumber = fs_create(arg1);
                if (inumber >= 0) // 0 should not be returned
                    printf("created inode %d\n", inumber);
                else
                    printf("create failed!\n");
            } else
                printf("use: create <filename>\n");
        } else if (!strcmp(cmd, "mkdir")) {
            if (args == 2) {
                inumber = fs_mkdir(arg1);
                if (inumber>=0) // 0 should not be returned
                    printf("created dir with inode %d\n", inumber);
                else
                    printf("create dir failed!\n");
            } else {
                printf("use: mkdir <dirname>\n");
            }
        } else if (!strcmp(cmd, "rm")) {
            if (args == 2) {
                inumber = fs_unlink(arg1);
                if (inumber >= 0) // 0 should not be returned
                    printf("removed one link to inode %d\n", inumber);
                else
                    printf("unlink failed!\n");
            } else
                printf("use: rm <filename>\n");
        } else if (!strcmp(cmd, "ln")) {
            if (args == 3) {
                inumber = fs_link(arg1, arg2);
                if (inumber >= 0) // 0 should not be returned
                    printf("new link to inode %d\n", inumber);
                else
                    printf("link failed!\n");
            } else
                printf("use: ln <filename> <newname>\n");
        } else if (!strcmp(cmd, "help") || !strcmp(cmd, "?")) {
            print_help();
        } else if (!strcmp(cmd, "quit") || !strcmp(cmd, "exit") || !strcmp(cmd, "q")) {
            break;
        } else {
            printf("unknown command: %s\n", cmd);
            printf("type 'help' or '?' for a list of commands.\n");
        }
    }

    printf("closing emulated disk.\n");
    disk_close();

    return EXIT_SUCCESS;
}
