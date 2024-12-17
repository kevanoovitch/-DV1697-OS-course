// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int FS::pwd()
{

    // Step 1: Check if current directory is the root
    if (this->current_dir == 0)
    {
        std::cout << "/" << std::endl;
        return 0;
    }
    // Step 2: variable to hold the path
    std::string path = "";

    // Temporary variable for traversing directories
    uint16_t travDir = current_dir;

    // Read the directory block
    while (travDir != static_cast<uint16_t>(ROOT_BLOCK))
    {
        uint8_t blkBuffer[BLOCK_SIZE];

        if (disk.read(travDir, blkBuffer) == -1)
        {
            std::cout << "Error: failed disk read in pwd()" << std::endl;
            return -1;
        }

        // Interpret the block as directory entries
        dir_entry *entries = reinterpret_cast<dir_entry *>(blkBuffer);

        // Step 3: Find the parent directory and current directory's name

        uint16_t parent_dir = ROOT_BLOCK; // Assume parent is root if not found
        std::string current_dir_name = "";

        for (size_t i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
        {
            if (entries[i].first_blk == travDir)
            {
                /* Current directory entry in parent */
                current_dir_name = entries[i].file_name;
            }
            // DONT KNOW IF THIS COMPARE WITH .. WORKS IN LABB ENVIRONMENT
            else if (strcmp(entries[i].file_name, "..") == 0) // strcmp: compare to string if eq ret --> 0
            {
                parent_dir = entries[i].first_blk;
            }
        }

        // Prepend the current directory name to the path
        path = "/" + current_dir_name + path;
        // Move up to the parent directory
        travDir = parent_dir;
    }

    // Step 4: Print the full path
    std::cout << path << std::endl;
    return 0;
}
