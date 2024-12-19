#include <iostream>
#include "fs.h"
#include "disk.h"
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include <sstream>

FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
}

FS::~FS()
{
}

int myMin(int x, int y)
{
    if (x > y)
    {
        return y;
    }
    else
        return x;
}

std::string accessRightsToString(uint8_t access_rights)
{
    std::string result = "";
    result += (access_rights & READ) ? "r" : "-";    // Check READ bit
    result += (access_rights & WRITE) ? "w" : "-";   // Check WRITE bit
    result += (access_rights & EXECUTE) ? "x" : "-"; // Check EXECUTE bit
    return result;
}

std::vector<std::string> parsePath(const std::string &path)
{
    std::vector<std::string> components;
    std::stringstream ss(path);
    std::string part;
    while (std::getline(ss, part, '/'))
    {
        /*
        if (part == ".." && !components.empty())
        {
            components.pop_back(); // Navigate to the parent directory

        }
        */
        if (!part.empty() && part != ".")
        {
            components.push_back(part);
        }

        // components.push_back(part);
    }

    return components;
}

std::string extractLastPart(const std::string &path)
{
    // Find the position of the last '/' or '\'
    size_t lastSlash = path.find_last_of("/\\");

    if (lastSlash == std::string::npos)
    {
        // No slash found, the entire string is the name
        return path;
    }

    // Return the substring after the last slash
    return path.substr(lastSlash + 1);
}

std::string FS::extractFilename(const std::string &filepath)
{
    // Find the position of the last backslash or forward slash
    size_t lastSlashPos = filepath.find_last_of("/\\");

    // If no slash is found, return the original filepath (it's already a filename)
    if (lastSlashPos == std::string::npos)
    {
        return filepath;
    }

    // Otherwise, extract the substring after the last slash
    return filepath.substr(lastSlashPos + 1);
}

// formats the disk, i.e., creates an empty file system
int FS::format()
{

    this->fat[0] = ROOT_BLOCK;
    this->fat[1] = FAT_BLOCK;

    for (int i = 2; i < BLOCK_SIZE / 2; i++)
    {
        fat[i] = FAT_FREE;
    }

    // Step 2: Write the initialized FAT to the disk
    uint8_t fatBuffer[BLOCK_SIZE] = {0};
    memcpy(fatBuffer, this->fat, sizeof(this->fat));
    if (disk.write(FAT_BLOCK, fatBuffer) == -1)
    {
        std::cerr << "Error: Failed to write FAT to disk in format()." << std::endl;
        return -1;
    }

    // Step 3: Clear the root directory block
    uint8_t rootBuffer[BLOCK_SIZE] = {0}; // Set the entire block to zero
    if (disk.write(ROOT_BLOCK, rootBuffer) == -1)
    {
        std::cerr << "Error: Failed to clear root directory block in format()." << std::endl;
        return -1;
    }

    fat[ROOT_BLOCK] = FAT_EOF;

    std::cout << "FS::format()\n";
    return 0;
}

std::vector<uint16_t> FS::fatFinder()
{

    std::vector<uint16_t> freeBlks;
    for (int i = 2; i < BLOCK_SIZE / 2; i++)
    {
        if (this->fat[i] == FAT_FREE)
        {
            freeBlks.push_back(i);
        }
    }

    return freeBlks;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int FS::create(std::string filepath)
{
    // Step 0.5 checking so input is not longer than 56 and not filling the disk to much

    std::string fileName = filepath.c_str();

    if (fileName.size() > 55)
    {
        std::cerr << "ERROR in create() filename to long!" << std::endl;
        return -1;
    }

    int result = this->dirSize(current_dir);

    if (result > 63)
    {
        std::cerr << "Error: Directory is full. Cannot create more files." << std::endl;
        return -1;
    }
    // Step 1: Check if file already exists in the current directory
    // - Read the current directory block into memory.
    // - Iterate through the directory entries to check for a matching filename.
    // - If the file already exists, print an error message and return -1.

    u_int8_t blkBuffer[BLOCK_SIZE]; // Somewhere to store what we read

    if (disk.read(current_dir, blkBuffer) == -1)
    {
        std::cerr << "Error: failed to read file in create()." << std::endl;
        return -1;
    }

    // Interpret the block as an array of directory entries
    dir_entry *entries = reinterpret_cast<dir_entry *>(blkBuffer);

    for (int i = 2; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        if (entries[i].first_blk != FAT_FREE)
        {

            if (strcmp(entries[i].file_name, filepath.c_str()) == 0)
            {
                std::cerr << "Error: File with the same name already exists in the current directory." << std::endl;
                return -1; // Exit the function if a duplicate is found
            }
        }
    }

    /*--------- Step 2: Prompt the user for file content -----------*/

    /*
    - Read user input line by line until an empty line is entered.
    - Concatenate the input into a single string to represent the file content.
    */

    std::string line;
    std::string fileData;

    while (true)
    {
        std::getline(std::cin, line);

        if (line.empty())
        {
            break;
        }
        fileData += line + "\n";
    }

    /*--------- Step 3: Calculate the number of blocks needed -----------*/

    // - Compute the file size.
    // - Divide the file size by the block size (BLOCK_SIZE) to determine how many blocks are required.
    // - If the file size is not a multiple of BLOCK_SIZE, round up to the nearest whole number of blocks.

    double temp = (double)(fileData.size()) / BLOCK_SIZE;
    int neededBlks = std::ceil(temp);

    /*--------- Step 4: Find free blocks in the FAT -----------*/

    // - Iterate through the FAT to find free blocks (FAT_FREE).
    // - Keep track of the indices of these free blocks.
    // - If the number of free blocks found is less than the required number, print an error and return -1.

    std::vector<int> freeBlks;
    for (int i = 2; i < BLOCK_SIZE / 2; i++) // start from 2 to avoid the FAT and ROOT block
    {
        if (this->fat[i] == FAT_FREE)
        {
            freeBlks.push_back(i);
        }
        if (freeBlks.size() >= neededBlks)
        {
            // enough free space found
            break;
        }
    }

    if (freeBlks.size() < neededBlks)
    {
        std::cerr << "Error: in create(), free blocks less than needed blocks." << std::endl;
        return -1;
    }

    /*--------- Step 5: Allocate and chain the blocks -----------*/

    // - Link the allocated blocks in the FAT.
    // - Mark each allocated block in the FAT, with each block pointing to the next.
    // - Mark the last block with FAT_EOF to indicate the end of the chain.

    /* Example: freeBlks {5 , 8 , 12}
       then this will simply set fat[5] = 8, fat[8] = 12, fat[12] = EOF
    */
    for (int i = 0; i < freeBlks.size() - 1; i++)
    {
        this->fat[freeBlks[i]] = freeBlks[i + 1];
    }
    this->fat[freeBlks.back()] = EOF;

    /*--------- Step 6: Write the file content to the allocated blocks -----------*/
    // - Divide the file content into chunks that fit within a block (BLOCK_SIZE bytes).
    // - Write each chunk of data to the corresponding block on the disk using `Disk::write`.

    size_t fileSize = fileData.size();
    // int chunks = (fileSize + BLOCK_SIZE - 1) / BLOCK_SIZE; not used?

    uint8_t tempBuffer[BLOCK_SIZE] = {0};

    for (int i = 0; i < freeBlks.size(); i++)
    {
        size_t start = i * BLOCK_SIZE;
        size_t length = myMin(BLOCK_SIZE, fileSize - start);

        // Step 2: Extract the current chunk
        std::string chunk = fileData.substr(start, length); // Get a substring for the current chunk

        // Step 3: Convert the chunk to a format compatible with Disk::write
        std::vector<uint8_t> buffer(chunk.begin(), chunk.end()); // Copy string data to a byte buffer
        buffer.resize(BLOCK_SIZE, 0);                            // Ensure the buffer is exactly BLOCK_SIZE (zero-padded if needed)

        // Step 4: Write the buffer to the disk
        if (disk.write(freeBlks[i], buffer.data()) == -1)
        { // Use .data() to get the raw byte pointer
            std::cerr << "Error: Failed to write to block " << freeBlks[i] << std::endl;
            return -1; // Return on error
        }
    }

    /*--------- Step 7: Add a new directory entry for the file -----------*/

    // - Find an empty directory entry in the current directory block.
    // - Populate the entry with:
    //   - Filename (filepath).
    //   - File size.
    //   - First block index from the FAT.
    //   - Type (file).
    //   - Default access rights (e.g., read and write).
    // - Write the updated directory block back to the disk.

    dir_entry entry;

    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        if (entries[i].first_blk == FAT_FREE) // look for a free block
        {
            strncpy(entries[i].file_name, filepath.c_str(), sizeof(entries[i].file_name) - 1);
            entries[i].file_name[sizeof(entries[i].file_name) - 1] = '\0'; // Ensure null-termination
            entries[i].size = fileData.size();
            entries[i].first_blk = freeBlks[0];
            entries[i].type = TYPE_FILE;
            entries[i].access_rights = READ | WRITE;
            entry = entries[i];
            break;
        }
    }

    // Write the updated directory block back to the disk
    if (disk.write(current_dir, blkBuffer) == -1)
    {
        std::cerr << "Error: in create(), Failed to updated directory block struct variables." << std::endl;
        return -1;
    }

    /*--------- Step 8: Write the updated FAT to disk -----------*/
    // - Write the modified FAT table back to the disk (typically stored in block FAT_BLOCK).

    uint8_t fatBuffer[BLOCK_SIZE] = {0};
    memcpy(fatBuffer, this->fat, sizeof(this->fat));

    if (disk.write(FAT_BLOCK, fatBuffer) == -1)
    {
        std::cerr << "Error: in create(), failed to write FAT to disk" << std::endl;
        return -1;
    }

    /*--------- step 9: Return success -----------*/

    // - Print a success message or simply return 0 to indicate successful file creation.

    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int FS::cat(std::string filepath)
{

    // Step 1: Read from the disk

    u_int8_t blkBuffer[BLOCK_SIZE]; // Somewhere to store what we read

    if (disk.read(current_dir, blkBuffer) == -1)
    {
        std::cerr << "Error: failed to read file in create()." << std::endl;
        return -1;
    }

    // Interpret the block as an array of directory entries
    dir_entry *entries = reinterpret_cast<dir_entry *>(blkBuffer);

    // Step 2: find the file in the fat
    uint16_t chainStart;
    dir_entry file;
    bool foundFile = false;
    dir_entry *filePtr = nullptr;

    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        if (strcmp(entries[i].file_name, filepath.c_str()) == 0)
        {
            // found the file
            // std::cout << "the matched file name!: " << entries[i].file_name << "filepath: " << filepath << std::endl;
            chainStart = entries[i].first_blk;

            filePtr = &entries[i];
            file = entries[i];
            foundFile = true;
            break;
        }
    }

    if (foundFile == false)
    {
        std::cerr << "Error: in cat(), didnt find file" << std::endl;
        return -1;
    }
    if (file.access_rights == 0x01 || file.access_rights == 0x02 || file.access_rights == 0x03)
    {
        std::cerr << "Error: in cat(), not enough access" << std::endl;
        return -1;
    }

    if (filePtr->type == TYPE_DIR)
    {
        std::cerr << "Error: in cat(), param is directory" << std::endl;
        return -1;
    }

    // Step 3: print the file contents to screen
    std::string data = "";
    uint8_t blockData[BLOCK_SIZE]; // Buffer to store each block's content
    uint16_t currentBlock = chainStart;

    // use first_fileBlk from struct
    // follow the linked list untill EOF
    // add to a string/buffer

    while (currentBlock != FaT_EOF_unint16)
    {

        disk.read(currentBlock, blockData);

        // Append the content of the block to fileData
        data.append(reinterpret_cast<char *>(blockData), BLOCK_SIZE);

        // Move to the next block in the chain
        currentBlock = fat[currentBlock];
    }

    // step 5: print aformentioned buffer
    std::cout << data;

    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int FS::ls()
{
    u_int8_t blkBuffer[BLOCK_SIZE]; // Somewhere to store what we read

    if (disk.read(current_dir, blkBuffer) == -1)
    {
        std::cerr << "Error: failed to read file in create()." << std::endl;
        return -1;
    }

    // Interpret the block as an array of directory entries
    dir_entry *entries = reinterpret_cast<dir_entry *>(blkBuffer);

    std::cout << "name \t  type\t accessrights\t size\n";

    // Handle for case: empty FS

    // First print all dirs

    size_t nrOfEntries = BLOCK_SIZE / sizeof(dir_entry); // Number of entries in the block

    for (size_t i = 0; i < nrOfEntries; i++)
    {
        if (strcmp(entries[i].file_name, "..") != 0 && entries[i].first_blk != FAT_FREE && entries[i].type == TYPE_DIR)
        {
            // Print directory details if it's not ".."
            std::cout << entries[i].file_name << "\t"
                      << "dir" << "\t"
                      << accessRightsToString(entries[i].access_rights) << "\t"
                      << "-" << "\n"; // entries[i].size << "\n";
        }
    }

    for (size_t i = 0; i < nrOfEntries; i++)
    {
        if (entries[i].first_blk != FAT_FREE && entries[i].type == TYPE_FILE)
        {
            std::cout << entries[i].file_name << "\t" << "file" << "\t" << accessRightsToString(entries[i].access_rights) << "\t" << entries[i].size << "\n";
        }
    }

    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int FS::dirSize(uint16_t currenetDir)
{
    u_int8_t blkBuffer[BLOCK_SIZE]; // Somewhere to store what we read

    if (disk.read(current_dir, blkBuffer) == -1)
    {
        std::cerr << "Error: failed to read file in dirSize()." << std::endl;
        return -1;
    }

    // Interpret the block as an array of directory entries
    dir_entry *entries = (dir_entry *)(blkBuffer);

    // Handle for case: empty FS

    size_t nrOfEntries = BLOCK_SIZE / sizeof(dir_entry); // Number of entries in the block
    int countFiles = 0;
    for (size_t i = 0; i < nrOfEntries; i++)
    {
        if (entries[i].first_blk != FAT_FREE)
        {
            countFiles++;
        }
    }

    return countFiles;
}

int FS::cp(std::string sourcepath, std::string destpath)
{

    auto startDir = current_dir;

    dir_entry *dstEntryPtr = nullptr;
    dir_entry *destinationPtr = nullptr;
    u_int8_t afBlkBuffer[BLOCK_SIZE]; // Somewhere to store what we read after we have traversed if we traversed
    u_int8_t srcBlkPath[BLOCK_SIZE];  // Somewhere to store what we read
    dir_entry *sourcePtrPath = nullptr;
    bool PathFlag = false;
    // only traverse if there is a / in param
    // Read the current directory block into memory
    u_int8_t srcBlkBuffer[BLOCK_SIZE];

    // only traverse if there is a / in param

    if (destpath.find('/') != std::string::npos)
    {

        std::vector<std::string> components = parsePath(destpath);

        dir_entry *srcEntriesPath = (dir_entry *)(srcBlkPath);
        disk.read(current_dir, srcBlkPath);
        for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
        {
            if (strcmp(srcEntriesPath[i].file_name, sourcepath.c_str()) == 0)
            {
                // found the file that destination file already exists
                sourcePtrPath = &srcEntriesPath[i];
                PathFlag = true;
                break;
            }
        }
        if (!sourcePtrPath)
        {
            std::cerr << "couldnt find source file when working with paths in cp()" << std::endl;
        }

        // disk.read(current_dir, srcBlkPath);

        for (size_t i = 0; i < components.size(); ++i) // in order to land one over target dir
        {
            // const std::string &dirName = components[i];

            while (current_dir != 0)
            {
                if (cd("..") == -1)
                {
                    std::cerr << "Error: Could not navigate to the root directory." << std::endl;
                    return -1;
                }
            }

            if (cd(components[i]) == -1)
            {
                std::cerr << "error in cp()" << std::endl;
            }
        }

        destpath = extractLastPart(destpath);
    }

    if (disk.read(current_dir, srcBlkBuffer) == -1)
    {
        std::cerr << "Error: failed to read current directory in cp()." << std::endl;
        return -1;
    }

    dir_entry *entries = (dir_entry *)srcBlkBuffer;

    // Check if destination file already exists as a file in current directory
    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        if (entries[i].first_blk != FAT_FREE && strcmp(entries[i].file_name, destpath.c_str()) == 0 && entries[i].type == TYPE_FILE)
        {
            std::cerr << "Error: in cp(), destination file already exists." << std::endl;
            return -1;
        }
    }

    // Find the source file (must be TYPE_FILE)
    uint16_t srcChainStart = 0;
    dir_entry *srcFile = nullptr;
    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        if (entries[i].first_blk != FAT_FREE && strcmp(entries[i].file_name, sourcepath.c_str()) == 0 && entries[i].type == TYPE_FILE)
        {
            srcChainStart = entries[i].first_blk;
            srcFile = &entries[i];
            break;
        }
    }

    if (PathFlag == true)
    {

        srcFile = sourcePtrPath;
    }

    if (srcFile == nullptr)
    {
        std::cerr << "Error: in cp(), source file not found or not a file." << std::endl;
        return -1;
    }

    // Check if the destination is a directory
    dir_entry *dstDir = nullptr;
    bool dirFlag = false;
    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        if (entries[i].first_blk != FAT_FREE && strcmp(entries[i].file_name, destpath.c_str()) == 0 && entries[i].type == TYPE_DIR)
        {
            dstDir = &entries[i];
            dirFlag = true;
            break;
        }
    }

    // Calculate how many blocks we need
    double temp = (double)(srcFile->size) / BLOCK_SIZE;
    int neededBlks = (int)std::ceil(temp);
    if (neededBlks == 0 && srcFile->size > 0)
        neededBlks = 1;

    // Find free blocks for the copy
    std::vector<uint16_t> freeBlks = fatFinder();
    if ((int)freeBlks.size() < neededBlks)
    {
        std::cerr << "Error in cp(): not enough free blocks." << std::endl;
        return -1;
    }

    // Link new blocks in FAT
    for (int i = 0; i < neededBlks - 1; i++)
    {
        this->fat[freeBlks[i]] = freeBlks[i + 1];
    }
    this->fat[freeBlks[neededBlks - 1]] = FAT_EOF;

    // Copy the file contents from source to destination blocks
    uint8_t blockData[BLOCK_SIZE];
    uint16_t srcCurrent = srcChainStart;
    for (int i = 0; i < neededBlks; i++)
    {
        if (disk.read(srcCurrent, blockData) == -1)
        {
            std::cerr << "Error in cp(): failed to read source block." << std::endl;
            return -1;
        }

        if (disk.write(freeBlks[i], blockData) == -1)
        {
            std::cerr << "Error in cp(): failed to write to destination block." << std::endl;
            return -1;
        }

        srcCurrent = this->fat[srcCurrent];
    }

    // Now we need to create a directory entry for the copied file
    u_int8_t dstBlkBuffer[BLOCK_SIZE];
    dir_entry *dstEntries;
    if (dirFlag == true)
    {
        // Destination is a directory, so place the file inside that directory
        if (disk.read(dstDir->first_blk, dstBlkBuffer) == -1)
        {
            std::cerr << "Error in cp(): failed to read destination directory block." << std::endl;
            return -1;
        }

        dstEntries = (dir_entry *)dstBlkBuffer;

        // Start from index 1 to avoid overwriting the .. entry at [0]
        bool foundFreeEntry = false;
        for (int i = 1; i < BLOCK_SIZE / sizeof(dir_entry); i++)
        {

            if (dstEntries[i].first_blk == FAT_FREE && dstEntries[i].type != TYPE_DIR)
            {
                // Write new file entry here
                strncpy(dstEntries[i].file_name, srcFile->file_name, sizeof(dstEntries[i].file_name) - 1);
                dstEntries[i].file_name[sizeof(dstEntries[i].file_name) - 1] = '\0';
                dstEntries[i].size = srcFile->size;
                dstEntries[i].first_blk = freeBlks[0];
                dstEntries[i].type = TYPE_FILE;
                dstEntries[i].access_rights = srcFile->access_rights;
                foundFreeEntry = true;
                break;
            }
        }

        if (!foundFreeEntry)
        {
            std::cerr << "Error in cp(): destination directory is full." << std::endl;
            return -1;
        }

        // Write back the updated directory block
        if (disk.write(dstDir->first_blk, dstBlkBuffer) == -1)
        {
            std::cerr << "Error in cp(): failed to write updated destination directory block." << std::endl;
            return -1;
        }
    }

    else if (PathFlag == true)
    {
        // Case 3: Handle path traversal
        u_int8_t dstBlkBuffer[BLOCK_SIZE];
        if (disk.read(current_dir, dstBlkBuffer) == -1)
        {
            std::cerr << "Error in cp(): Failed to read destination directory.\n";
            current_dir = startDir;
            return -1;
        }

        dir_entry *dstEntries = reinterpret_cast<dir_entry *>(dstBlkBuffer);
        dir_entry *freeSpot = nullptr;

        for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
        {
            if (dstEntries[i].first_blk == FAT_FREE && dstEntries[i].type != TYPE_DIR)
            {
                freeSpot = &dstEntries[i];
                break;
            }
        }

        if (!freeSpot)
        {
            std::cerr << "Error in cp(): No space in destination directory.\n";
            current_dir = startDir;
            return -1;
        }

        strncpy(freeSpot->file_name, sourcePtrPath->file_name, sizeof(freeSpot->file_name) - 1);
        freeSpot->file_name[sizeof(freeSpot->file_name) - 1] = '\0';
        freeSpot->first_blk = sourcePtrPath->first_blk;
        freeSpot->size = sourcePtrPath->size;
        freeSpot->type = sourcePtrPath->type;
        freeSpot->access_rights = sourcePtrPath->access_rights;

        if (disk.write(current_dir, dstBlkBuffer) == -1)
        {
            std::cerr << "Error in cp(): Failed to write destination directory.\n";
            current_dir = startDir;
            return -1;
        }
    }
    else
    {
        // Destination is not a directory - create a new file in current directory
        dstEntries = (dir_entry *)srcBlkBuffer;

        bool foundFreeEntry = false;
        for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
        {
            // It's safe to start from 0 here because we're in the current directory, not a newly created directory
            // that must have .. at [0]. The root directory doesn't have .. at [0], and if you have a directory
            // structure where .. is at [0], you should also skip it in these cases. Adjust if needed.
            if (dstEntries[i].first_blk == FAT_FREE && dstEntries[i].type != TYPE_DIR)
            {
                strncpy(dstEntries[i].file_name, destpath.c_str(), sizeof(dstEntries[i].file_name) - 1);
                dstEntries[i].file_name[sizeof(dstEntries[i].file_name) - 1] = '\0';
                dstEntries[i].size = srcFile->size;
                dstEntries[i].first_blk = freeBlks[0];
                dstEntries[i].type = TYPE_FILE;
                dstEntries[i].access_rights = srcFile->access_rights;
                foundFreeEntry = true;
                break;
            }
        }

        if (!foundFreeEntry)
        {
            std::cerr << "Error in cp(): no free entry in current directory." << std::endl;
            return -1;
        }

        // Write back the updated directory block (current_dir)
        if (disk.write(current_dir, srcBlkBuffer) == -1)
        {
            std::cerr << "Error: Failed to write directory block in cp()." << std::endl;
            return -1;
        }
    }

    // Write the updated FAT to the disk
    uint8_t fatBuffer[BLOCK_SIZE] = {0};
    memcpy(fatBuffer, this->fat, sizeof(this->fat));
    if (disk.write(FAT_BLOCK, fatBuffer) == -1)
    {
        std::cerr << "Error: Failed to write FAT in cp()." << std::endl;
        return -1;
    }

    if (PathFlag == true)
    {
        current_dir = startDir;
    }

    std::cout << "FS::cp(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int FS::mv(std::string sourcepath, std::string destpath)
{
    auto startDir = current_dir;

    dir_entry *dstEntryPtr = nullptr;
    dir_entry *destinationPtr = nullptr;
    u_int8_t afBlkBuffer[BLOCK_SIZE]; // Somewhere to store what we read after we have traversed if we traversed
    u_int8_t srcBlkPath[BLOCK_SIZE];  // Somewhere to store what we read
    dir_entry *sourcePtrPath = nullptr;
    bool PathFlag = false;
    // only traverse if there is a / in param

    if (destpath.find('/') != std::string::npos)
    {

        std::vector<std::string> components = parsePath(destpath);

        dir_entry *srcEntriesPath = (dir_entry *)(srcBlkPath);
        disk.read(current_dir, srcBlkPath);
        for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
        {
            if (strcmp(srcEntriesPath[i].file_name, sourcepath.c_str()) == 0)
            {
                // found the file that destination file already exists
                sourcePtrPath = &srcEntriesPath[i];
                PathFlag = true;
                break;
            }
        }
        if (!sourcePtrPath)
        {
            std::cerr << "couldnt find source file when working with paths in mv()" << std::endl;
        }

        // disk.read(current_dir, srcBlkPath);

        for (size_t i = 0; i < components.size(); ++i) // in order to land one over target dir
        {
            // const std::string &dirName = components[i];

            while (current_dir != 0)
            {
                if (cd("..") == -1)
                {
                    std::cerr << "Error: Could not navigate to the root directory." << std::endl;
                    return -1;
                }
            }

            if (cd(components[i]) == -1)
            {
                std::cerr << "error in cp()" << std::endl;
            }
        }

        destpath = extractLastPart(destpath);
    }

    u_int8_t srcBlkBuffer[BLOCK_SIZE]; // Somewhere to store what we read
    auto readFrom = -1;
    auto readFromDst = -1;
    if (PathFlag == true)
    {
        readFrom = startDir;
        readFromDst = current_dir;
    }
    else
    {
        readFrom = current_dir;
    }

    if (disk.read(readFrom, srcBlkBuffer) == -1)
    {
        std::cerr << "Error: failed to read src file in mv()." << std::endl;
        return -1;
    }

    dir_entry *entries = (dir_entry *)(srcBlkBuffer);

    /* Case where source name is bad */
    bool foundFile = false;
    dir_entry *sourcePtr = nullptr;
    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        if (strcmp(entries[i].file_name, sourcepath.c_str()) == 0)
        {
            // found the file that destination file already exists
            sourcePtr = &entries[i];
            foundFile = true;
            break;
        }
    }
    if (foundFile == false)
    {
        /* Couldnt fint source filename */
        std::cerr << "error in mv(): couldnt find source file" << std::endl;
        return -1;
    }

    // Step 2: find the file in the fat or create tempfile here?

    bool foundDst = false;

    if (PathFlag == false)
    {

        for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
        {
            if (strcmp(entries[i].file_name, destpath.c_str()) == 0)
            {
                // found the file

                destinationPtr = &entries[i];

                foundDst = true;
                break;
            }
        }
    }

    /* Case where destpath-file already exists*/

    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {

        if (strcmp(entries[i].file_name, destpath.c_str()) == 0 && destinationPtr->type != TYPE_DIR)
        {
            // found the file that destination file already exists
            std::cerr << "Error: in mv(), Destination file already exists" << std::endl;
            return -1;
        }
    }

    if (!PathFlag && destinationPtr && destinationPtr->type != TYPE_DIR)
    {
        std::cerr << "Error: Destination file already exists in mv()." << std::endl;
        return -1;
    }

    if (!PathFlag && destinationPtr && destinationPtr->type == TYPE_DIR)
    {

        // Case 2

        // Read root block for source entry //Entries is source

        // Read dst block to add source entry

        readFromDst = destinationPtr->first_blk;

        u_int8_t dstBlkBuffer[BLOCK_SIZE]; // Somewhere to store what we read
        disk.read(readFromDst, dstBlkBuffer);
        dir_entry *dstEntries = (dir_entry *)(dstBlkBuffer);

        dir_entry *dstEntryPtr = nullptr;
        for (int i = 1; i < BLOCK_SIZE / sizeof(dir_entry); i++)
        {
            // Look for room in destination DIR
            if (dstEntries[i].first_blk == FAT_FREE && entries[i].type != TYPE_DIR)
            {
                // found a spot
                dstEntryPtr = &dstEntries[i];
                break;
            }
        }
        if (dstEntryPtr == nullptr)
        {
            std::cerr << "Error in mv(), when target was DIR there was no room in destination for a new file" << std::endl;
            return -1;
        }

        strncpy(dstEntryPtr->file_name, sourcePtr->file_name, sizeof(sourcePtr->file_name) - 1);
        dstEntryPtr->file_name[sizeof(sourcePtr->file_name) - 1] = '\0'; // Ensure null-termination
        dstEntryPtr->first_blk = sourcePtr->first_blk;
        dstEntryPtr->size = sourcePtr->size;
        dstEntryPtr->type = sourcePtr->type;
        dstEntryPtr->access_rights = sourcePtr->access_rights;

        // write both changes

        disk.write(readFromDst, dstBlkBuffer);

        // remove old file entry from src then write?
        memset(sourcePtr->file_name, 0, sizeof(sourcePtr->file_name));
        sourcePtr->first_blk = 0;
        sourcePtr->size = 0;
        sourcePtr->type = 0;
        sourcePtr->access_rights = READ | WRITE;

        disk.write(readFrom, srcBlkBuffer);
    }

    else if (PathFlag)
    {
        // Case 3: Handle path traversal
        u_int8_t dstBlkBuffer[BLOCK_SIZE];
        if (disk.read(current_dir, dstBlkBuffer) == -1)
        {
            std::cerr << "Error in mv(): Failed to read destination directory.\n";
            current_dir = startDir;
            return -1;
        }

        dir_entry *dstEntries = reinterpret_cast<dir_entry *>(dstBlkBuffer);
        dir_entry *freeSpot = nullptr;

        for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
        {
            if (dstEntries[i].first_blk == FAT_FREE && dstEntries[i].type != TYPE_DIR)
            {
                freeSpot = &dstEntries[i];
                break;
            }
        }

        if (!freeSpot)
        {
            std::cerr << "Error in mv(): No space in destination directory.\n";
            current_dir = startDir;
            return -1;
        }

        strncpy(freeSpot->file_name, sourcePtr->file_name, sizeof(freeSpot->file_name) - 1);
        freeSpot->file_name[sizeof(freeSpot->file_name) - 1] = '\0';
        freeSpot->first_blk = sourcePtr->first_blk;
        freeSpot->size = sourcePtr->size;
        freeSpot->type = sourcePtr->type;
        freeSpot->access_rights = sourcePtr->access_rights;

        if (disk.write(current_dir, dstBlkBuffer) == -1)
        {
            std::cerr << "Error in mv(): Failed to write destination directory.\n";
            current_dir = startDir;
            return -1;
        }

        // Remove old entry
        memset(sourcePtr, 0, sizeof(dir_entry));
        if (disk.write(readFrom, srcBlkBuffer) == -1)
        {
            std::cerr << "Error in mv(): Failed to write source directory block.\n";
            current_dir = startDir;
            return -1;
        }
    }

    else
    {

        // Case 1

        std::string dstName = extractFilename(destpath);

        // convert string to char[]
        char charArray[56]; // Ensure it has the same size as file_name in dir_entry
        for (size_t i = 0; i < dstName.length() && i < sizeof(charArray) - 1; ++i)
        {
            charArray[i] = dstName[i];
        }

        charArray[dstName.length()] = '\0'; // Null-terminate the array

        // Assign charArray to srcFile.file_name using strncpy (RENAME)
        strncpy(sourcePtr->file_name, charArray, sizeof(sourcePtr->file_name) - 1);
        sourcePtr->file_name[sizeof(sourcePtr->file_name) - 1] = '\0'; // Ensure null-termination

        disk.write(readFrom, srcBlkBuffer);
    }

    if (PathFlag)
    {
        current_dir = startDir; // Restore to the starting directory after operation
    }

    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int FS::rm(std::string filepath)
{

    u_int8_t blkBuffer[BLOCK_SIZE]; // Somewhere to store what we read

    dir_entry *entries = (dir_entry *)(blkBuffer);

    if (disk.read(current_dir, blkBuffer) == -1)
    {
        std::cerr << "error in rm() couldnt read disk" << std::endl;
        return -1;
    }

    bool foundFile = false;
    dir_entry *file = nullptr;
    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        if (entries[i].first_blk != FAT_FREE && entries[i].type != TYPE_DIR)
        {
            if (strcmp(entries[i].file_name, filepath.c_str()) == 0)
            {
                // found the file that destination file already exists
                foundFile = true;
                file = &entries[i];
                break;
            }
        }
    }
    if (foundFile == false)
    {
        std::cerr << "error in rm() couldnt find file" << std::endl;
        return -1;
    }

    if (file->type == TYPE_DIR)
    {
        // what we want to remove is a dir

        u_int8_t removeBuffer[BLOCK_SIZE]; // Somewhere to store what we read
        disk.read(file->first_blk, removeBuffer);
        dir_entry *removeEntries = (dir_entry *)(removeBuffer);

        for (int i = 1; i < BLOCK_SIZE / sizeof(dir_entry); i++)
        {
            if (removeEntries->first_blk != FAT_FREE)
            {
                std::cerr << "Error in rm(), tried to remove a dir but it wasn't empty!" << std::endl;
                return -1;
            }
        }

        // the dir is empty empty the dir entry
        file->first_blk = FAT_FREE;
        memset(file->file_name, 0, sizeof(file->file_name));
        file->first_blk = 0;
        file->size = 0;
        file->access_rights = READ | WRITE;

        this->fat[file->first_blk] = FAT_FREE;
        disk.write(current_dir, blkBuffer);
        return 0;
    }

    uint16_t currentBlk = file->first_blk;

    while (currentBlk != FaT_EOF_unint16)
    {

        auto temp = currentBlk;       // Save the current block
        currentBlk = this->fat[temp]; // Move to the next block in the chain
        this->fat[temp] = FAT_FREE;
    }

    // clear the args structure

    file->first_blk = FAT_FREE;     // Mark the file's first block as free
    memset(file->file_name, 0, 56); // Clear the filename
    file->size = 0;                 // Reset size
    file->type = 0;                 // Reset type
    file->access_rights = 0;        // Reset access rights

    if (disk.write(current_dir, blkBuffer) == -1)
    {
        std::cerr << "Error: Failed to write to disk in cp()." << std::endl;
        return -1;
    }

    uint8_t fatBuffer[BLOCK_SIZE] = {0};
    memcpy(fatBuffer, this->fat, sizeof(this->fat));
    if (disk.write(FAT_BLOCK, fatBuffer) == -1)
    {
        std::cerr << "Error: Failed to write FAT in cp()." << std::endl;
        return -1;
    }

    std::cout << "FS::rm(" << filepath << ")\n";
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int FS::append(std::string filepath1, std::string filepath2)
{
    // link the chains

    u_int8_t blkBuffer[BLOCK_SIZE]; // Somewhere to store what we read
    dir_entry *entries = (dir_entry *)(blkBuffer);
    if (disk.read(current_dir, blkBuffer) == -1)
    {
        std::cerr << "error in rm() couldnt read disk" << std::endl;
        return -1;
    }

    dir_entry *srcFile = nullptr;
    dir_entry *dstFile = nullptr;
    auto foundfileSrc = false;
    auto foundfileDst = false;
    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        if (entries[i].first_blk != FAT_FREE)
        {
            if (strcmp(entries[i].file_name, filepath1.c_str()) == 0)
            {
                // found the file that destination file already exists
                foundfileSrc = true;
                srcFile = &entries[i];
            }
            if (strcmp(entries[i].file_name, filepath2.c_str()) == 0)
            {
                // found the file that destination file already exists
                foundfileDst = true;
                dstFile = &entries[i];
            }
            if (foundfileDst == true && foundfileSrc == true)
            {
                /* found needed files */
                break;
            }
        }
    }
    if (foundfileDst != true)
    {
        std::cerr << "error in append: didnt find file2" << std::endl;
        return -1;
    }
    else if (foundfileSrc != true)
    {
        std::cerr << "error in append: didnt find file1" << std::endl;
        return -1;
    }

    if (srcFile->access_rights == 0x01 || srcFile->access_rights == 0x02 || srcFile->access_rights == (0x01 | 0x02))
    {
        std::cerr << "error in append: not enough rights for source file" << std::endl;
        return -1;
    }
    else if (dstFile->access_rights == EXECUTE || dstFile->access_rights == READ || dstFile->access_rights == (0x04 | 0x01))
    {
        std::cerr << "error in append: not enough rights for destination file" << std::endl;
        return -1;
    }

    // read data into temp container
    uint16_t srcCurrentBlock = srcFile->first_blk;
    u_int8_t dataBuffer[BLOCK_SIZE]; // Somewhere to store what we read

    while (srcCurrentBlock != FaT_EOF_unint16)
    {
        disk.read(srcCurrentBlock, dataBuffer);
        srcCurrentBlock = this->fat[srcCurrentBlock];
    }

    // dataBuffer = hej heja hejare hejast
    //  hej heja hejare hejast / blocks = st block
    double temp = (double)srcFile->size / BLOCK_SIZE;
    int nrNeededBlocks = std::ceil(temp);

    std::vector<uint16_t> freeblocks = fatFinder();

    if (freeblocks.size() < nrNeededBlocks)
    {
        std::cerr << "Error in append(): not enough free blocks" << std::endl;
        return -1;
    }

    // read/Write to update size
    // u_int8_t aBuffer[BLOCK_SIZE]; // Somewhere to store what we read
    // disk.read(current_dir, aBuffer);
    // disk.write(ROOT_BLOCK, aBuffer);

    // find last block of dst/file2
    uint16_t dstCurrentBlk = dstFile->first_blk;
    uint16_t lastBlock;

    srcCurrentBlock = srcFile->first_blk;

    while (dstCurrentBlk != FaT_EOF_unint16)
    {
        lastBlock = dstCurrentBlk;
        dstCurrentBlk = this->fat[dstCurrentBlk];
    }

    // append the data to dst/file2
    for (int i = 0; i < nrNeededBlocks; i++)
    {
        // updating FAT
        uint8_t buffer[BLOCK_SIZE] = {0};
        this->fat[lastBlock] = freeblocks[i];
        lastBlock = freeblocks[i];

        // Updating DATA
        disk.read(srcCurrentBlock, buffer);
        disk.write(freeblocks[i], buffer);
    }
    this->fat[lastBlock] = FaT_EOF_unint16;

    dstFile->size += srcFile->size;

    disk.write(ROOT_BLOCK, blkBuffer);

    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";
    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int FS::mkdir(std::string dirpath)
{

    auto startDir = current_dir;

    std::vector<std::string> components = parsePath(dirpath);

    // always start from root
    while (current_dir != 0)
    {
        if (cd("..") == -1)
        {
            std::cerr << "Error: Could not navigate to the root directory." << std::endl;
            return -1;
        }
    }

    u_int8_t blkBuffer[BLOCK_SIZE];
    if (disk.read(current_dir, blkBuffer) == -1)
    {
        std::cerr << "error in mkdir() couldn't read current directory block" << std::endl;
        return -1;
    }

    dir_entry *entries = (dir_entry *)blkBuffer;
    dir_entry *freeSpot = nullptr;

    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        if (entries[i].first_blk == FAT_FREE && entries[i].type != TYPE_DIR)
        {

            freeSpot = &entries[i];

            break;
        }
    }

    if (!freeSpot)
    {
        // No free entry in the current directory
        std::cerr << "error in mkdir(), current directory is full" << std::endl;
        return -1;
    }

    for (size_t i = 0; i < components.size(); ++i)
    {
        const std::string &dirName = components[i];

        // If it's the last component, create the directory
        // std::cout << "i | compSize " << i << " | " << components.size() << std::endl;

        // Edge Case
        if (0 == components.size() - 1)
        {
            if (dirpath[0] == '/')
            {

                dirpath = dirpath.erase(0, 1);
            }
            break;

            // continue normally
        }

        if (cd(components[i]) == -1)
        {

            // couldnt traverse so have to create
            dirpath = components[i];

            if (disk.read(current_dir, blkBuffer) == -1)
            {
                std::cerr << "failed to read in mkdir()" << std::endl;
                return -1;
            }
            entries = (dir_entry *)blkBuffer;
            // find new free spot in where we NOW stand
            for (int j = 0; j < BLOCK_SIZE / sizeof(dir_entry); j++)
            {
                if (entries[j].first_blk == FAT_FREE && entries[j].type != TYPE_DIR)
                {

                    freeSpot = &entries[j];
                    break;
                }
            }

            break;
        }
    }

    // Read current directory block

    // Check if dirpath already exists as a file
    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        if (entries[i].first_blk != FAT_FREE &&
            strcmp(entries[i].file_name, dirpath.c_str()) == 0 &&
            entries[i].type == TYPE_FILE)
        {
            std::cerr << "Error in mkdir(), param was a file" << std::endl;
            return -1;
        }
    }

    // Find a free block in FAT for the new directory
    std::vector<uint16_t> freeBlks = fatFinder();
    if (freeBlks.empty())
    {
        std::cerr << "error in mkdir(), no free block for new directory" << std::endl;
        return -1;
    }
    uint16_t firstFree = freeBlks[0];

    // Mark the directory block as EOF in FAT
    this->fat[firstFree] = FAT_EOF;

    // Initialize the new directory block (zero it out)
    uint8_t newDirBuffer[BLOCK_SIZE] = {0};
    dir_entry *newEntries = (dir_entry *)newDirBuffer;

    // Set up the ".." entry
    strncpy(newEntries[0].file_name, "..", sizeof(newEntries[0].file_name) - 1);
    newEntries[0].file_name[sizeof(newEntries[0].file_name) - 1] = '\0';
    newEntries[0].size = 0; // Directory size can be considered as 0 or 1, depending on your logic
    newEntries[0].first_blk = current_dir;
    newEntries[0].type = TYPE_DIR;
    newEntries[0].access_rights = READ | WRITE;

    // All other entries are already zeroed out with first_blk == 0 (which should be FAT_FREE)
    // but to be explicit, set them:

    for (int i = 1; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        newEntries[i].first_blk = FAT_FREE;
        memset(newEntries[i].file_name, 0, sizeof(newEntries[i].file_name));
        newEntries[i].size = 0;
        newEntries[i].type = 0;
        newEntries[i].access_rights = 0;
    }

    // Write the new directory block to disk
    if (disk.write(firstFree, newDirBuffer) == -1)
    {
        std::cerr << "Error in mkdir(), failed to write new directory block" << std::endl;
        return -1;
    }

    // Update the parent directory entry for this new directory
    strncpy(freeSpot->file_name, dirpath.c_str(), sizeof(freeSpot->file_name) - 1);
    freeSpot->file_name[sizeof(freeSpot->file_name) - 1] = '\0';
    freeSpot->size = 0; // directory size can be 0
    freeSpot->first_blk = firstFree;
    freeSpot->type = TYPE_DIR;
    freeSpot->access_rights = READ | WRITE;

    // Write the updated parent directory block
    if (disk.write(current_dir, blkBuffer) == -1)
    {
        std::cerr << "Error in mkdir(), failed to write updated current directory block" << std::endl;
        return -1;
    }

    // Update the FAT on disk
    uint8_t fatBuffer[BLOCK_SIZE] = {0};
    memcpy(fatBuffer, this->fat, sizeof(this->fat));
    if (disk.write(FAT_BLOCK, fatBuffer) == -1)
    {
        std::cerr << "Error in mkdir(), failed to write FAT" << std::endl;
        return -1;
    }

    current_dir = startDir;

    // std::cout << "FS::mkdir(" << dirpath << ")\n";
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int FS::cd(std::string dirpath)
{

    // search FAT for dirname that maches dirpath

    // only traverse if there is a / in param
    if (dirpath.find('/') != std::string::npos)
    {
        std::vector<std::string> components = parsePath(dirpath);

        for (size_t i = 0; i < components.size(); ++i) // in order to land one over target dir
        {
            // const std::string &dirName = components[i];

            while (current_dir != 0)
            {
                if (cd("..") == -1)
                {
                    std::cerr << "Error: Could not navigate to the root directory." << std::endl;
                    return -1;
                }
            }

            if (cd(components[i]) == -1)
            {
                std::cerr << "error in cp()" << std::endl;
            }
        }
        return 0;
    }

    uint8_t blkBuffer[BLOCK_SIZE];
    if (disk.read(current_dir, blkBuffer) == -1)
    {
        std::cerr << "error in cd(), on read " << std::endl;
    };

    dir_entry *entries = reinterpret_cast<dir_entry *>(blkBuffer);

    bool foundEntry = false;
    dir_entry *paramEntry;

    for (size_t i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        /* Find the dir that matches '..' name */

        if (strcmp(entries[i].file_name, dirpath.c_str()) == 0) //(entries[i].file_name == dirpath.c_str())
        {
            // temp_current_dir = entries[i].first_blk;
            foundEntry = true;
            paramEntry = &entries[i];
            break;
        }
    }

    if (foundEntry == false)
    {
        // std::cerr << "Error in cd() couldn't find directory" << std::endl;
        return -1;
    }

    //  Handle case where a file is in param
    if (paramEntry->type == TYPE_FILE)
    {
        std::cerr << "Error in cd() parameter is of the type file look:" << dirpath << std::endl;
        return -1;
    }

    // handle case when .. is dirpath
    if (dirpath == "..")
    {

        /* Find the dir that matches '..' name */

        current_dir = paramEntry->first_blk;
        return 0; // Return early to avoid processing further
    }

    current_dir = paramEntry->first_blk;

    // std::cout << "FS::cd(" << dirpath << ")\n";
    return 0;
}

int FS::pwd()
{

    std::string path = "";
    uint16_t travDir = current_dir; // Start from the current directory

    while (travDir != ROOT_BLOCK)
    {
        uint8_t blkBuffer[BLOCK_SIZE];

        // Step 1: Read the current directory to find the parent directory (..)
        if (disk.read(travDir, blkBuffer) == -1)
        {
            std::cerr << "Error: failed disk read in pwd()" << std::endl;
            return -1;
        }

        dir_entry *entries = reinterpret_cast<dir_entry *>(blkBuffer);

        uint16_t parent_dir = ROOT_BLOCK; // Default to root block

        // Find the parent directory block
        for (size_t i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
        {
            if (strcmp(entries[i].file_name, "..") == 0 && entries[i].type == TYPE_DIR)
            {
                parent_dir = entries[i].first_blk;
                break;
            }
        }

        // Step 2: Read the parent directory to find the current directory's name
        if (disk.read(parent_dir, blkBuffer) == -1)
        {
            std::cerr << "Error: failed disk read in pwd()" << std::endl;
            return -1;
        }

        entries = reinterpret_cast<dir_entry *>(blkBuffer);
        std::string current_dir_name = "";

        for (size_t i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
        {
            if (entries[i].first_blk == travDir && entries[i].type == TYPE_DIR)
            {
                // Found the name of the current directory
                current_dir_name = entries[i].file_name;
                break;
            }
        }

        // Prepend the current directory name to the path
        if (!current_dir_name.empty())
        {
            path = "/" + current_dir_name + path;
        }

        // Move to the parent directory
        travDir = parent_dir;
    }

    // If path is empty, we're at the root
    if (path.empty())
    {
        path = "/";
    }

    std::cout << path << std::endl;
    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int FS::chmod(std::string accessrights, std::string filepath)
{
    // 0x04 = read 0x02 = write, EXE = 0x01

    // open up the entry for filepath
    // read current block

    uint8_t blkBuffer[BLOCK_SIZE];
    if (disk.read(current_dir, blkBuffer) == -1)
    {
        std::cerr << "error in chmod(), on read " << std::endl;
    };

    dir_entry *entries = reinterpret_cast<dir_entry *>(blkBuffer);

    dir_entry *entryPtr = nullptr;
    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        if (strcmp(entries[i].file_name, filepath.c_str()) == 0)
        {
            entryPtr = &entries[i];
            break;
        }
    }
    if (entryPtr == nullptr)
    {
        std::cerr << "error in chmod()" << std::endl;
        return -1;
    }

    // find the file based on param

    if (accessrights == "1")
    {
        entryPtr->access_rights = 0x01; // EXECUTE
    }
    else if (accessrights == "2")
    {
        entryPtr->access_rights = 0x02; // WRITE
    }
    else if (accessrights == "3")
    {
        entryPtr->access_rights = 0x01 | 0x02; // EXECUTE + WRITE
    }
    else if (accessrights == "4")
    {
        entryPtr->access_rights = 0x04; // READ
    }
    else if (accessrights == "5")
    {
        entryPtr->access_rights = 0x04 | 0x01; // READ + EXECUTE
    }
    else if (accessrights == "6")
    {
        entryPtr->access_rights = 0x04 | 0x02; // READ + WRITE
    }
    else if (accessrights == "7")
    {
        entryPtr->access_rights = 0x04 | 0x02 | 0x01; // READ + WRITE + EXECUTE
    }
    else
    {
        std::cerr << "Error: Invalid access rights in chmod()." << std::endl;
        return -1;
    }

    if (disk.write(current_dir, blkBuffer) == -1)
    {
        std::cerr << "error in chmod(), on write " << std::endl;
    };

    std::cout << "FS::chmod(" << accessrights << "," << filepath << ")\n";
    return 0;
}
