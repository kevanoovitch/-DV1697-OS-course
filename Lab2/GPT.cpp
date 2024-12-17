int FS::mkdir(std::string dirpath)
{
    // Read the current directory block
    uint8_t blkBuffer[BLOCK_SIZE];
    if (disk.read(current_dir, blkBuffer) == -1)
    {
        std::cerr << "Error in mkdir() couldn't read current directory block.\n";
        return -1;
    }

    dir_entry *entries = reinterpret_cast<dir_entry *>(blkBuffer);

    // Check if a file or directory with the same name already exists
    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        if (entries[i].first_blk != FAT_FREE && strcmp(entries[i].file_name, dirpath.c_str()) == 0)
        {
            if (entries[i].type == TYPE_FILE)
            {
                std::cerr << "Error in mkdir(), param was a file\n";
            }
            else
            {
                std::cerr << "Error in mkdir(), directory already exists\n";
            }
            return -1;
        }
    }

    // Find a free entry in the current directory
    dir_entry *freePtr = nullptr;
    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        if (entries[i].first_blk == FAT_FREE)
        {
            freePtr = &entries[i];
            break;
        }
    }
    if (!freePtr)
    {
        std::cerr << "Error in mkdir, no free directory entries\n";
        return -1;
    }

    // Find a free block for the new directory
    std::vector<uint16_t> freeBlocks = fatFinder();
    if (freeBlocks.empty())
    {
        std::cerr << "Error in mkdir(), no free blocks available.\n";
        return -1;
    }

    uint16_t dirBlock = freeBlocks[0];
    this->fat[dirBlock] = FAT_EOF; // single-block directory

    // Initialize the new directory block
    dir_entry newDirEntries[BLOCK_SIZE / sizeof(dir_entry)];
    memset(newDirEntries, 0, sizeof(newDirEntries));

    // Only ".." entry is created, pointing to the parent directory
    strncpy(newDirEntries[0].file_name, "..", sizeof(newDirEntries[0].file_name) - 1);
    newDirEntries[0].file_name[sizeof(newDirEntries[0].file_name) - 1] = '\0';
    newDirEntries[0].first_blk = current_dir; // parent directory block
    newDirEntries[0].size = 0;
    newDirEntries[0].type = TYPE_DIR;
    newDirEntries[0].access_rights = READ | WRITE;

    // Write the new directory block to the disk
    if (disk.write(dirBlock, reinterpret_cast<uint8_t *>(newDirEntries)) == -1)
    {
        std::cerr << "Error in mkdir(): failed to write new directory block.\n";
        return -1;
    }

    // Add the directory entry to the parent directory
    strncpy(freePtr->file_name, dirpath.c_str(), sizeof(freePtr->file_name) - 1);
    freePtr->file_name[sizeof(freePtr->file_name) - 1] = '\0';
    freePtr->first_blk = dirBlock;
    freePtr->size = 0; // empty directory initially
    freePtr->type = TYPE_DIR;
    freePtr->access_rights = READ | WRITE;

    // Write the updated parent directory block
    if (disk.write(current_dir, blkBuffer) == -1)
    {
        std::cerr << "Error in mkdir(): failed to update parent directory block.\n";
        return -1;
    }

    // Update FAT on disk
    uint8_t fatBuffer[BLOCK_SIZE] = {0};
    memcpy(fatBuffer, this->fat, sizeof(this->fat));
    if (disk.write(FAT_BLOCK, fatBuffer) == -1)
    {
        std::cerr << "Error: Failed to write FAT in mkdir().\n";
        return -1;
    }

    std::cout << "FS::mkdir(" << dirpath << ")\n";
    return 0;
}
