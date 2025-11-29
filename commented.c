/**
 * COMMENTED VERSION - FILESYSTEM IMPLEMENTATION
 * Every line explained in detail for learning purposes
 * 
 * @author Afonso Neves
 * @author Lino Nurnberger 71462
 */

/*****************************************************/
/*                  HELPER FUNCTIONS                 */
/*****************************************************/

/**
 * get_inode - Converts a file path into an inode number
 * 
 * This function takes a path like "/home/user/file.txt" and traverses
 * the directory structure to find which inode number corresponds to that path.
 * 
 * @param path_name: The path to search for (e.g., "/dir1/dir2/file")
 * @return: The inode number if found, or -1 if error/not found
 */
int get_inode(char *path_name) {
    // Check if path is NULL, "/" (root), or empty string ""
    // All of these cases mean we want the root directory
    if (path_name == NULL || strcmp(path_name, "/") == 0 || strcmp(path_name, "") == 0)
    {
        // ROOTINO is defined as 0 - the root directory is always inode 0
        return ROOTINO;
    }

    // Start at the root directory (inode 0)
    int curr_ino = ROOTINO;
    
    // Variable to store the inode information as we traverse
    struct fs_inode curr_inode;

    // Create a copy of the path because strtok modifies the string it processes
    // strlen(path_name) + 1 to include space for the null terminator '\0'
    char path_copy[strlen(path_name) + 1];
    
    // Copy the original path into our local copy
    strcpy(path_copy, path_name);
    
    // strtok splits a string by a delimiter (here "/")
    // First call returns the first "token" (piece between slashes)
    // For "/home/user/file", first token would be "home"
    char *current_dir = strtok(path_copy, "/");

    // Using while instead of do-while to handle edge cases like "////"
    // Keep looping while there are more tokens (more path components)
    while (current_dir != NULL)
    {
        // Load the current directory's inode from disk into curr_inode
        // Returns -1 if the inode number is invalid
        if (inode_load(curr_ino, &curr_inode) == -1)
            return -1;  // Error loading inode
        
        // Check if current inode is actually a directory
        // We can only traverse through directories, not files
        if (curr_inode.type != IFDIR)
            return -1;  // Can't search inside a file!

        // Look for the current_dir (filename/dirname) inside the current directory
        // dir_findname searches the directory entries for a name match
        int next_ino = dir_findname(&curr_inode, current_dir);
        
        // If dir_findname returns -1, the name wasn't found in this directory
        if (next_ino == -1)
            return -1;  // Path doesn't exist

        // Move to the next inode in the path
        curr_ino = next_ino;
        
        // Get the next token (next component of the path)
        // Subsequent calls to strtok with NULL continue from where we left off
        current_dir = strtok(NULL, "/");
    }

    // We've traversed the entire path successfully
    // curr_ino now contains the inode number of the final destination
    return curr_ino;
}


/**
 * get_parent_inode - Finds the inode number of the parent directory
 * 
 * For example, if given "/home/user/file.txt", this returns the inode
 * number of the "/home/user" directory.
 * 
 * @param pathname: Full path to a file or directory
 * @return: Inode number of parent directory, or -1 if error
 */
int get_parent_inode(char *pathname)
{
    // Create a local copy because we'll modify it
    // strlen gives us the length, +1 for the null terminator
    char path_copy[strlen(pathname) + 1];
    
    // Copy the pathname into our local buffer
    strcpy(path_copy, pathname);
    
    // strrchr searches backwards through the string for the last '/'
    // For "/home/user/file", this finds the '/' before "file"
    char *last_slash = strrchr(path_copy, '/');
    
    // If no slash found, path is just a filename like "myfile"
    // Parent of a top-level name is the root directory
    if (last_slash == NULL)
    {
        return ROOTINO;  // Return root inode (0)
    }
    
    // If the slash is at the beginning, like "/file"
    // The parent is the root directory
    if (last_slash == path_copy)
    {
        return ROOTINO;  // Return root inode (0)
    }
    
    // Replace the last slash with null terminator '\0'
    // This "cuts off" everything after the last slash
    // "/home/user/file" becomes "/home/user\0file" (but we only see "/home/user")
    *last_slash = '\0';
    
    // Now get the inode number for this parent path
    // get_inode will traverse the path to find the parent directory's inode
    return get_inode(path_copy);
}


/**
 * get_filename - Extracts just the filename from a full path
 * 
 * For "/home/user/file.txt", this returns a pointer to "file.txt"
 * 
 * @param pathname: Full path to extract filename from
 * @return: Pointer to the filename portion, or NULL if path ends with '/'
 */
char *get_filename(char *pathname)
{
    // Find the last '/' in the path (searching backwards)
    char *last_slash = strrchr(pathname, '/');
    
    // If no slash found, the entire pathname IS the filename
    // For example: "myfile" (no path, just a name)
    if (last_slash == NULL)
    {
        return pathname;  // Return pointer to the whole string
    }

    // Move pointer one position forward to skip the '/' character
    // If path is "/home/user/file", last_slash points to '/' before "file"
    // last_slash + 1 points to the 'f' in "file"
    char *filename = last_slash + 1;
    
    // Check if filename is empty (path ended with '/')
    // For example: "/home/user/" - there's no filename after the last slash
    // filename[0] accesses the first character after the slash
    if (filename[0] == '\0')  // '\0' is the null terminator (end of string)
        return NULL;  // No filename present, return NULL

    // Return pointer to the filename part of the string
    return filename;
}


/**
 * delete_file - Frees all data blocks and inode associated with a file
 * 
 * This is a helper function used when the last link to a file is removed.
 * It performs a complete cleanup: direct blocks, indirect blocks, and the inode itself.
 * 
 * @param ino_number: The inode number to free
 * @param inode: Pointer to the inode structure (already loaded)
 * @return: 0 if success, -1 if error
 */
static int delete_file(int ino_number, struct fs_inode *inode) {
    // STEP 1: Free all direct data blocks
    // Loop through the 11 direct block pointers
    for (int i = 0; i < DIRBLOCK_PER_INODE; i++)
    {
        // Check if this direct block is allocated (not 0)
        if (inode->dir_block[i] != 0)
        {
            // Free this block (mark it as available in the bitmap)
            block_free(inode->dir_block[i]);
        }
    }
    
    // STEP 2: Free indirect blocks (if any exist)
    if (inode->indir_block != 0)
    {
        // Array to hold the indirect block's contents (pointers to data blocks)
        uint16_t ind_data[BLOCKSZ / sizeof(uint16_t)];
        
        // Read the indirect block from disk
        // This block contains pointers to additional data blocks
        disk_read(inode->indir_block, (char *)ind_data);

        // Calculate EXACTLY how many indirect blocks are in use
        // This prevents us from freeing unallocated blocks
        
        // Total blocks needed for this file size
        // Formula: (size + BLOCKSZ - 1) / BLOCKSZ rounds up
        // Example: 1500 bytes with 1024 block size = (1500+1023)/1024 = 2 blocks
        int total_blocks = (inode->size + BLOCKSZ - 1) / BLOCKSZ;
        
        // Indirect blocks used = total - direct blocks
        // Since we have 11 direct blocks, anything beyond needs indirect
        int indirect_count = total_blocks - DIRBLOCK_PER_INODE;
        
        // Can't be negative (if file fits in direct blocks only)
        if (indirect_count < 0)
            indirect_count = 0;

        // Free each used indirect data block
        for (int k = 0; k < indirect_count && k < (BLOCKSZ / sizeof(uint16_t)); k++)
        {
            // If this slot has a block allocated (not 0)
            if (ind_data[k] != 0)
                block_free(ind_data[k]);  // Free it
        }
        
        // Free the indirect block itself (the block containing pointers)
        block_free(inode->indir_block);
    }
    
    // STEP 3: Free the inode itself
    // This marks the inode as available for reuse
    return inode_free(ino_number);
}


/**
 * get_entries_in_block - Calculates how many entries are in a specific block
 * 
 * Helper function to determine how full a directory block is.
 * Used when deciding where to add new entries.
 * 
 * @param dir_size: Total size of the directory in bytes
 * @param block_index: Which block we're asking about (0-10 for direct, 11+ for indirect)
 * @return: Number of entries in that block (0 to DIRENTS_PER_BLOCK)
 */
static int get_entries_in_block(int dir_size, int block_index) {
    // If directory is empty, no entries anywhere
    if (dir_size <= 0)
        return 0;
    
    // Calculate total number of directory entries
    // Each entry is sizeof(struct fs_dirent) bytes
    int total_entries = dir_size / sizeof(struct fs_dirent);
    
    // Calculate how many entries come BEFORE this block
    // If block_index is 2 and each block holds 8 entries:
    // entries_before = 2 * 8 = 16 entries came before
    int entries_before_this_block = block_index * DIRENTS_PER_BLOCK;
    
    // Entries in THIS block = total - entries before
    int entries_in_block = total_entries - entries_before_this_block;
    
    // Sanity checks:
    // Can't be negative (block is beyond the used range)
    if (entries_in_block < 0)
        return 0;
    
    // Can't exceed maximum entries per block
    if (entries_in_block > DIRENTS_PER_BLOCK)
        return DIRENTS_PER_BLOCK;
    
    return entries_in_block;
}


/**
 * try_direct_blocks - Tries to find space in the 11 direct blocks
 * 
 * Loops through the direct block pointers (slots 0-10 in the inode).
 * If a slot is empty, allocates a new block.
 * If a block has space, returns it.
 * 
 * @param parent_ino: Inode number of the parent directory
 * @param parent_inode: Pointer to parent directory's inode structure
 * @param block: Pointer to block buffer (will be filled with block data)
 * @param entries_in_block_out: Output parameter - how many entries are in the returned block
 * @return: Block number with space, 0 if all full, -1 on allocation error
 */
static int try_direct_blocks(int parent_ino, struct fs_inode *parent_inode,
                             union fs_block *block, int *entries_in_block_out) {

    // Loop through all 11 direct block slots
    for (int i = 0; i < DIRBLOCK_PER_INODE; i++) {
        // Get the block number stored in this slot
        int blknum = parent_inode->dir_block[i];
        
        // Check if this slot is empty (unallocated)
        if (blknum == 0) {
            // Need to allocate a new block
            blknum = block_alloc();
            
            // If block_alloc returns -1, disk is full
            if (blknum == -1)
                return -1;

            // Save the new block number in the inode
            parent_inode->dir_block[i] = blknum;
            
            // Write the updated inode back to disk
            inode_save(parent_ino, parent_inode);

            // Initialize the new block with zeros (clean slate)
            memset(block->data, 0, BLOCKSZ);
            
            // Write the zeroed block to disk
            disk_write(blknum, block->data);
        } else {
            // Block already exists, read its current contents
            disk_read(blknum, block->data);
        }

        // Calculate how many entries are currently in this block
        int entries_in_block = get_entries_in_block(parent_inode->size, i);
        
        // Check if this block has space for another entry
        if (entries_in_block < DIRENTS_PER_BLOCK) {
            // Found space! Return the block number and entry count
            *entries_in_block_out = entries_in_block;
            return blknum;
        }
        // Otherwise continue to next direct block
    }
    
    // All direct blocks are full
    return 0;
}


/**
 * try_indirect_blocks - Finds space in the indirect block structure
 * 
 * The indirect block is like a "phone book" of pointers to more data blocks.
 * This function allocates the indirect block if needed, then loops through
 * its slots to find or allocate a data block with space.
 * 
 * @param parent_ino: Inode number of the parent directory
 * @param parent_inode: Pointer to parent directory's inode structure
 * @param block: Pointer to block buffer (will be filled with block data)
 * @param entries_in_block_out: Output parameter - how many entries are in the returned block
 * @return: Block number with space, -1 on error
 */
static int try_indirect_blocks(int parent_ino, struct fs_inode *parent_inode,
                               union fs_block *block, int *entries_in_block_out) {
    // Array to hold the indirect block's contents (array of block numbers)
    uint16_t indirect_block_data[BLOCKSZ / sizeof(uint16_t)];
    
    // Get the indirect block number from the inode
    int indirect_block_num = parent_inode->indir_block;

    // Check if indirect block exists yet
    if (indirect_block_num == 0) {
        // Need to allocate the indirect block first
        indirect_block_num = block_alloc();
        if (indirect_block_num == -1)
            return -1;  // Disk full

        // Save the indirect block number in the inode
        parent_inode->indir_block = indirect_block_num;
        
        // Write updated inode to disk
        inode_save(parent_ino, parent_inode);

        // Initialize indirect block with zeros (empty slots)
        memset(indirect_block_data, 0, BLOCKSZ);
        
        // Write it to disk
        disk_write(indirect_block_num, (char *)indirect_block_data);
    } else {
        // Indirect block already exists, read it
        disk_read(indirect_block_num, (char *)indirect_block_data);
    }

    // Loop through all possible pointers in the indirect block
    // Each slot can point to one data block
    for (int i = 0; i < BLOCKSZ / sizeof(uint16_t); i++) {
        // Get the data block number from this slot
        int data_block_num = indirect_block_data[i];
        
        // Check if this slot is empty (no data block allocated)
        if (data_block_num == 0) {
            // Allocate a new data block
            data_block_num = block_alloc();
            if (data_block_num == -1)
                return -1;  // Disk full

            // Update the indirect block with the new data block number
            indirect_block_data[i] = data_block_num;
            
            // Write the updated indirect block back to disk
            disk_write(indirect_block_num, (char *)indirect_block_data);

            // Initialize the new data block with zeros
            memset(block->data, 0, BLOCKSZ);
            
            // Write it to disk
            disk_write(data_block_num, block->data);
        } else {
            // Data block exists, read it
            disk_read(data_block_num, block->data);
        }

        // Calculate block index: DIRBLOCK_PER_INODE (11) + i
        // This tells us this is the (11+i)th block overall
        int entries_in_block = get_entries_in_block(parent_inode->size, 
                                                          DIRBLOCK_PER_INODE + i);
        
        // Check if this block has space
        if (entries_in_block < DIRENTS_PER_BLOCK) {
            // Found space! Return the block number and entry count
            *entries_in_block_out = entries_in_block;
            return data_block_num;
        }
        // Otherwise continue to next slot in indirect block
    }

    // All indirect block slots are full
    return -1;
}


/**
 * compute_allocate - Coordinator function to allocate space for a new directory entry
 * 
 * This function orchestrates the search for space:
 * 1. First tries direct blocks (faster, simpler)
 * 2. If all direct blocks are full, tries indirect blocks
 * 
 * @param parent_ino: Inode number of the parent directory
 * @param parent_inode: Pointer to parent directory's inode structure
 * @param block: Pointer to block buffer (will be filled with block data)
 * @param entries_in_block_out: Output parameter - how many entries are in the returned block
 * @return: Block number where the new entry should be added, or -1 on error
 */
static int compute_allocate(int parent_ino, struct fs_inode *parent_inode,
                           union fs_block *block, int *entries_in_block_out) {
    // Try direct blocks first (the 11 direct pointers in inode)
    int blknum = try_direct_blocks(parent_ino, parent_inode, block, entries_in_block_out);
    
    // If we got a block number (not 0), return it
    // Note: could be -1 (error) or positive (success)
    if (blknum != 0)
        return blknum;
    
    // All direct blocks are full, now try indirect blocks
    return try_indirect_blocks(parent_ino, parent_inode, block, entries_in_block_out);
}


/**
 * add_entry_to_directory - Adds a new entry (file or subdirectory) to a directory
 * 
 * This is like adding a row to a table:
 * Parent Directory Table:
 * | Inode# | Name      |
 * |--------|-----------|
 * | 5      | file1.txt |
 * | 8      | dir2      |
 * | 12     | [NEW]     | <- We're adding this
 * 
 * @param parent_ino: Inode number of the directory we're adding to
 * @param name: Name of the new file/directory
 * @param child_ino: Inode number of the new file/directory
 * @return: 0 if success, -1 if error
 */
int add_entry_to_directory(int parent_ino, char *name, int child_ino) {
    // Variable to store the parent directory's inode information
    struct fs_inode parent_inode;
    
    // Load the parent directory's inode from disk
    // Returns -1 if parent_ino is invalid
    if (inode_load(parent_ino, &parent_inode) == -1)
        return -1;

    // Variable to hold a disk block (can contain directory entries)
    union fs_block block;

    // PHASE 1: Try to reuse a deleted entry (marked as FREE)
    // This is an optimization - reuse space from deleted files instead of growing
    // We search through all existing entries looking for one marked FREE
    
    // Calculate how many directory entries exist based on directory size
    // Each entry is sizeof(struct fs_dirent) bytes
    int num_entries = parent_inode.size / sizeof(struct fs_dirent);
    
    // Offset tracks our position in the directory (in bytes)
    int offset = 0;

    // Loop through each existing entry to look for a FREE slot
    for (int entry_idx = 0; entry_idx < num_entries; entry_idx++) {
        // Calculate byte offset for this entry
        offset = entry_idx * sizeof(struct fs_dirent);
        
        // Find which disk block contains this offset
        int currBlock = offset2block(&parent_inode, offset);

        // If currBlock is 0 or negative, something is wrong
        if (currBlock <= 0)
            break;

        // Read the block from disk into memory
        disk_read(currBlock, block.data);
        
        // Calculate position of this entry within the block
        // If block has 8 entries and entry_idx is 10, idx_in_block would be 2
        int idx_in_block = entry_idx % DIRENTS_PER_BLOCK;

        // Check if this entry is FREE (d_ino == 0 means deleted/unused)
        if (block.dirent[idx_in_block].d_ino == FREE) {
            // Found a free slot! Reuse it
            
            // Set the inode number for this entry
            block.dirent[idx_in_block].d_ino = child_ino;
            
            // Copy the name into the entry
            // strncpy copies at most MAXFILENAME characters
            strncpy(block.dirent[idx_in_block].d_name, name, MAXFILENAME);
            
            // Manually ensure null termination (strncpy doesn't guarantee this)
            block.dirent[idx_in_block].d_name[MAXFILENAME - 1] = '\0';
            
            // Write the modified block back to disk
            disk_write(currBlock, block.data);
            
            // Success! Entry added by reusing a deleted slot
            return 0;
        }
    }

    // PHASE 2: No FREE entry found, need to add at the end
    // Use the helper function compute_allocate to find or allocate a block with space
    
    // This variable will be filled by compute_allocate to tell us
    // how many entries are already in the block it returns
    int entries_in_block;
    
    // compute_allocate tries direct blocks first, then indirect blocks
    // Returns the block number where we should add the new entry
    int blknum = compute_allocate(parent_ino, &parent_inode, &block, &entries_in_block);
    
    // If blknum is -1, allocation failed (disk full)
    if (blknum == -1)
        return -1;
    
    // Add entry at the calculated position in the block
    // entries_in_block tells us which slot to use (0-7 for an 8-entry block)
    block.dirent[entries_in_block].d_ino = child_ino;
    
    // Copy the name
    strncpy(block.dirent[entries_in_block].d_name, name, MAXFILENAME);
    
    // Ensure null termination
    block.dirent[entries_in_block].d_name[MAXFILENAME - 1] = '\0';
    
    // Write the updated block back to disk
    disk_write(blknum, block.data);

    // Update the directory size to account for the new entry
    parent_inode.size += sizeof(struct fs_dirent);
    
    // Save the updated inode to disk
    inode_save(parent_ino, &parent_inode);
    
    // Success!
    return 0;
}


/**
 * dir_remove_entry - Removes an entry from a directory
 * 
 * Marks the entry as FREE so it can be reused later.
 * Like deleting a row from the directory table.
 * 
 * @param dir_inode: Pointer to the directory's inode
 * @param name: Name of the file/directory to remove
 * @return: Inode number of removed entry, or -1 if not found
 */
int dir_remove_entry(struct fs_inode *dir_inode, char *name)
{
    // Calculate how many entries are in this directory
    int remaining_dirents = dir_inode->size / sizeof(struct fs_dirent);
    
    // Start at the beginning of the directory
    int offset = 0;
    
    // Variable to hold disk blocks as we read them
    union fs_block block;

    // Keep searching while there are entries left to check
    while (remaining_dirents > 0)
    {
        // Find which block contains the current offset
        int currBlock = offset2block(dir_inode, offset);

        // Sanity check - block number should be valid
        if (currBlock <= 0)
            return -1;  // Something is wrong

        // Read the block from disk
        disk_read(currBlock, block.data);

        // Loop through entries in this block
        for (int d = 0; d < DIRENTS_PER_BLOCK && d < remaining_dirents; d++)
        {
            // Check two conditions:
            // 1. Entry is not already FREE (d_ino != 0)
            // 2. Name matches what we're looking for
            if (block.dirent[d].d_ino != FREE &&
                strncmp(block.dirent[d].d_name, name, MAXFILENAME) == 0)
            {
                // Found it!
                
                // Save the inode number before we delete it
                int removed_ino = block.dirent[d].d_ino;
                
                // Mark entry as FREE (set inode to 0)
                block.dirent[d].d_ino = FREE;
                
                // Clear the name field (for security/cleanliness)
                memset(block.dirent[d].d_name, 0, MAXFILENAME);
                
                // Write the modified block back to disk
                disk_write(currBlock, block.data);
                
                // Return the inode number we removed
                return removed_ino;
            }
        }
        
        // Move to next set of entries
        remaining_dirents -= DIRENTS_PER_BLOCK;
        offset += DIRENTS_PER_BLOCK * sizeof(struct fs_dirent);
    }
    
    // Searched entire directory, name not found
    return -1;
}


/*****************************************************/
/*              PUBLIC API FUNCTIONS                 */
/*****************************************************/


/**
 * fs_ls - List contents of a directory
 * 
 * Like the Unix "ls" command. Shows all files and subdirectories
 * in the given directory.
 * 
 * @param dirname: Path to directory to list (e.g., "/home/user")
 * @return: 0 if success, -1 if error
 */
int fs_ls(char *dirname) {
    // NOTE: We don't call check_rootSB() here because:
    // 1. The filesystem must be mounted before any operations (fs_mount)
    // 2. Mount already validates the superblock
    // 3. If unmounted, get_inode will fail anyway
    // This simplifies code while maintaining correctness.

    // Convert the path to an inode number
    // get_inode traverses the path and returns the inode number
    int number_of_ino = get_inode(dirname);
    
    // If get_inode returns -1, the path doesn't exist
    if (number_of_ino == -1)
    {
        return -1;  // Directory not found
    }
    
    // Variable to hold the directory's inode information
    struct fs_inode inode_of_dir;
    
    // Load the inode from disk into inode_of_dir
    if (inode_load(number_of_ino, &inode_of_dir) == -1)
    {
        return -1;  // Error loading inode
    }

    // Check that this inode is actually a directory, not a file
    // Can't list contents of a file!
    if (inode_of_dir.type != IFDIR)
    {
        return -1;  // Not a directory
    }

    // Calculate how many directory entries exist
    // Directory size in bytes / size of one entry = number of entries
    int remaining_dirents = inode_of_dir.size / sizeof(struct fs_dirent);
    
    // Offset tracks our position as we read through the directory
    int offset = 0;
    
    // Variable to hold disk blocks as we read them
    union fs_block block;
    
    // Print header for the listing
    printf("listing dir %s (inode %d):\n", dirname, number_of_ino);
    printf("ino:type:nlk    bytes name\n");

    // Loop through all entries in the directory
    while (remaining_dirents > 0)
    {
        // Find which disk block contains the current offset
        int currBlock = offset2block(&inode_of_dir, offset);

        // Sanity check - block should be valid
        if (currBlock <= 0)
            return -1;  // Error

        // Read the block from disk into memory
        disk_read(currBlock, block.data);
        
        // Loop through entries in this block
        for (int d = 0; d < DIRENTS_PER_BLOCK && d < remaining_dirents; d++)
        {
            // Only process entries that are not FREE (not deleted)
            if (block.dirent[d].d_ino != FREE)
            {
                // Variable to hold this entry's inode
                struct fs_inode entry_inode;
                
                // Load the entry's inode from disk
                if (inode_load(block.dirent[d].d_ino, &entry_inode) != -1)
                {
                    // Determine the type character to display
                    // Default to '?' for unknown types
                    char type = '?';
                    
                    // If it's a directory, use 'D'
                    if (entry_inode.type == IFDIR)
                    {
                        type = 'D';
                    }
                    // If it's a regular file, use 'F'
                    else if (entry_inode.type == IFREG)
                    {
                        type = 'F';
                    }
                    
                    // Print the entry information:
                    // - Inode number
                    // - Type (D or F)
                    // - Number of hard links
                    // - Size in bytes
                    // - Name
                    printf("%3d:%4c:%3d%9d %s\n",
                           block.dirent[d].d_ino, type, entry_inode.nlinks,
                           entry_inode.size, block.dirent[d].d_name);
                }
            }
        }
        
        // Move to next block of entries
        remaining_dirents -= DIRENTS_PER_BLOCK;
        offset += DIRENTS_PER_BLOCK * sizeof(struct fs_dirent);
    }
    
    // Success!
    return 0;
}


/**
 * fs_link - Create a hard link to an existing file
 * 
 * A hard link is like giving a file a second name.
 * Both names point to the same file data on disk.
 * Like having two different shortcuts to the same thing.
 * 
 * Example: ln /home/file.txt /home/copy.txt
 * Both names access the same file!
 * 
 * @param filename: Path to existing file
 * @param newlink: Path for the new link name
 * @return: Inode number if success, -1 if error
 */
int fs_link(char *filename, char *newlink) {
    // NOTE: No check_rootSB() call here - mount ensures validity.
    // This function relies on get_inode which will fail if filesystem is invalid.

    // Find the inode number of the existing file
    int file_ino = get_inode(filename);
    
    // If -1, the file doesn't exist
    if (file_ino == -1)
    {
        return -1;  // Original file does not exist
    }

    // Load the file's inode information
    struct fs_inode file_inode;
    if (inode_load(file_ino, &file_inode) == -1)
    {
        return -1;  // Error loading inode
    }

    // Check that we're linking to a regular file, not a directory
    // Hard links to directories are not allowed (would create loops!)
    if (file_inode.type != IFREG)
    {
        return -1;  // Can only link regular files
    }

    // Extract just the filename from the new link path
    // For "/home/link", this returns "link"
    char *newlink_name = get_filename(newlink);
    
    // If NULL, the path ended with '/' (invalid)
    if (newlink_name == NULL)
    {
        return -1;  // Invalid path (ends with '/')
    }

    // Find the parent directory where we'll create the link
    int parent_ino = get_parent_inode(newlink);
    
    // Variable to hold parent directory's inode
    struct fs_inode parent_inode;
    
    // Check if parent exists
    if (parent_ino == -1)
    {
        return -1;  // Parent directory doesn't exist
    }
    // Load parent inode
    else if (inode_load(parent_ino, &parent_inode) == -1)
    {
        return -1;  // Error loading parent inode
    }
    // Check parent is actually a directory
    else if (parent_inode.type != IFDIR)
    {
        return -1;  // Parent must be a directory
    }
    // Check if newlink name already exists in parent directory
    else if (dir_findname(&parent_inode, newlink_name) != -1)
    {
        return -1;  // Name already exists, can't overwrite
    }

    // Add the new link entry to the parent directory
    // This creates a new directory entry pointing to the SAME inode
    if (add_entry_to_directory(parent_ino, newlink_name, file_ino) == -1)
    {
        return -1;  // Error adding entry
    }

    // Increment the number of links (nlinks) to this file
    // This tracks how many names point to this file
    file_inode.nlinks++;
    
    // Save the updated inode (with new nlinks count) to disk
    if (inode_save(file_ino, &file_inode) == -1)
    {
        return -1;  // Error saving inode
    }

    // Success! Return the file's inode number
    return file_ino;
}


/**
 * fs_create - Create a new empty file
 * 
 * Like the Unix "touch" command or creating a new file in an editor.
 * Allocates an inode and adds entry to parent directory.
 * 
 * @param filename: Path for the new file (e.g., "/home/newfile.txt")
 * @return: Inode number of new file, or -1 if error
 */
int fs_create(char *filename) {
    // NOTE: No check_rootSB() needed - mount guarantees filesystem validity.
    // All subsequent operations will fail gracefully if unmounted.

    // Extract just the filename from the path
    char *file_name = get_filename(filename);
    
    // If NULL, path ended with '/' (invalid for a file)
    if (file_name == NULL)
    {
        return -1;  // Invalid path
    }

    // Find the parent directory
    int parent_ino = get_parent_inode(filename);
    if (parent_ino == -1)
    {
        return -1;  // Parent directory doesn't exist
    }
    
    // Load parent directory's inode
    struct fs_inode parent_inode;
    if (inode_load(parent_ino, &parent_inode) == -1)
    {
        return -1;  // Error loading parent
    }
    
    // Verify parent is actually a directory
    if (parent_inode.type != IFDIR)
    {
        return -1;  // Parent must be a directory
    }
    
    // Check if a file with this name already exists
    if (dir_findname(&parent_inode, file_name) != -1)
    {
        return -1;  // File already exists, can't create
    }
    
    // Allocate a new inode for the file
    // This finds an unused inode slot on disk
    int new_file_ino = inode_alloc();
    if (new_file_ino == -1)
    {
        return -1;  // No free inodes (disk full)
    }

    // Create and initialize the new inode structure
    struct fs_inode new_file_inode;
    
    // Zero out all fields (set everything to 0)
    memset(&new_file_inode, 0, sizeof(new_file_inode));

    // Set type to regular file
    new_file_inode.type = IFREG;
    
    // Start with 1 link (the name we're creating)
    new_file_inode.nlinks = 1;
    
    // New file starts empty (0 bytes)
    new_file_inode.size = 0;

    // Save the new inode to disk
    if (inode_save(new_file_ino, &new_file_inode) == -1)
    {
        return -1;  // Error saving inode
    }
    
    // Add entry to parent directory
    if (add_entry_to_directory(parent_ino, file_name, new_file_ino) == -1)
    {
        // CLEANUP: If adding to directory fails, free the inode we allocated
        // This prevents "orphan" inodes that are allocated but unreachable
        inode_free(new_file_ino);
        return -1;  // Error adding to directory
    }
    
    // Success! Return the new file's inode number
    return new_file_ino;
}


/**
 * fs_mkdir - Create a new directory
 * 
 * Like the Unix "mkdir" command.
 * Almost identical to fs_create, but creates a directory instead of a file.
 * 
 * @param dirname: Path for the new directory (e.g., "/home/newdir")
 * @return: Inode number of new directory, or -1 if error
 */
int fs_mkdir(char *dirname) {
    // NOTE: No check_rootSB() - filesystem operations depend on prior mount.
    // Mount already validates the superblock, so redundant checks are unnecessary.

    // Extract directory name from path
    char *file_name = get_filename(dirname);
    
    // If NULL, path ended with '/' (ambiguous)
    if (file_name == NULL)
    {
        return -1;  // Invalid path
    }

    // Find parent directory
    int parent_ino = get_parent_inode(dirname);
    if (parent_ino == -1)
    {
        return -1;  // Parent doesn't exist
    }

    // Load parent inode
    struct fs_inode parent_inode;

    if (inode_load(parent_ino, &parent_inode) == -1)
    {
        return -1;  // Error loading parent
    }

    // Check parent is a directory
    if (parent_inode.type != IFDIR)
    {
        return -1;  // Parent must be directory
    }

    // Check if name already exists
    if (dir_findname(&parent_inode, file_name) != -1)
    {
        return -1;  // Directory already exists
    }

    // Allocate new inode
    int new_dir_ino = inode_alloc();
    if (new_dir_ino == -1)
    {
        return -1;  // No free inodes
    }

    // Initialize new inode
    struct fs_inode new_dir_inode;
    memset(&new_dir_inode, 0, sizeof(new_dir_inode));

    // Set type to DIRECTORY (this is the key difference from fs_create)
    new_dir_inode.type = IFDIR;
    
    // One link (the name we're creating)
    new_dir_inode.nlinks = 1;
    
    // New directory is empty (0 bytes, no entries yet)
    new_dir_inode.size = 0;

    // Save inode to disk
    if (inode_save(new_dir_ino, &new_dir_inode) == -1)
    {
        return -1;  // Error saving
    }

    // Add entry to parent directory
    if (add_entry_to_directory(parent_ino, file_name, new_dir_ino) == -1)
    {
        // CLEANUP: Free inode if adding to directory fails
        inode_free(new_dir_ino);
        return -1;  // Error adding to directory
    }
    
    // Success! Return new directory's inode number
    return new_dir_ino;
}


/**
 * fs_unlink - Remove a file (delete one link to it)
 * 
 * Like the Unix "rm" command.
 * Removes one name/link to a file. If this was the last link,
 * the file's data is deleted and disk space is freed.
 * 
 * @param filename: Path to file to remove
 * @return: Inode number of removed file, or -1 if error
 */
int fs_unlink(char *filename) {
    // NOTE: Skipping check_rootSB() as mount ensures validity.
    // This is a safe optimization that reduces unnecessary checks.

    // Extract filename from path
    char *link_name = get_filename(filename);
    
    // Check path is valid
    if (link_name == NULL)
    {
        return -1;  // Invalid path
    }

    // Find parent directory
    int parent_ino = get_parent_inode(filename);
    
    // Variable for parent inode
    struct fs_inode parent_inode;

    // Check parent exists
    if (parent_ino == -1)
        return -1;  // Parent doesn't exist

    // Load parent inode
    if (inode_load(parent_ino, &parent_inode) == -1)
        return -1;  // Error loading parent

    // Check parent is directory
    if (parent_inode.type != IFDIR)
        return -1;  // Parent must be directory

    // Remove the entry from the parent directory
    // This marks the entry as FREE and returns the inode number
    int linked_entry_ino = dir_remove_entry(&parent_inode, link_name);

    // Check if entry was found
    if (linked_entry_ino == -1)
        return -1;  // File not found in directory

    // Load the file's inode
    struct fs_inode linked_entry_inode;
    if (inode_load(linked_entry_ino, &linked_entry_inode) == -1)
        return -1;  // Error loading file inode

    // Can only unlink regular files, not directories
    // (directories use a different command, rmdir)
    if (linked_entry_inode.type != IFREG)
        return -1;  // Not a regular file

    // Decrease the link count (one less name pointing to this file)
    linked_entry_inode.nlinks--;

    // Check if this was the LAST link to the file
    if (linked_entry_inode.nlinks == 0)
    {
        // No more links! Time to free all resources
        // Use the helper function delete_file to do a complete cleanup
        // This frees: direct blocks, indirect blocks, and the inode itself
        if (delete_file(linked_entry_ino, &linked_entry_inode) == -1)
            return -1;
    }
    else
    {
        // Still have other links to this file
        // Just save the decremented nlinks count
        // The file data remains on disk because other names still point to it
        if (inode_save(linked_entry_ino, &linked_entry_inode) == -1)
        {
            return -1;  // Error saving inode
        }
    }

    // Return the inode number that was unlinked
    return linked_entry_ino;
}
