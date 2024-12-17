#include <iostream>
#include "fs.h"
#include "disk.h"
#include <cmath>
#include <cstring>
#include <vector>

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
        std::cerr << "Error: in cat()" << std::endl;
        return -1;
    }
    if (filePtr->type == TYPE_DIR)
    {
        std::cerr << "Error: in cat()" << std::endl;
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

    std::cout << "name \t  type\t size\n";

    // Handle for case: empty FS

    // First print all dirs

    size_t nrOfEntries = BLOCK_SIZE / sizeof(dir_entry); // Number of entries in the block

    for (size_t i = 0; i < nrOfEntries; i++)
    {
        if (/*entries[i].first_blk != FAT_FREE &&*/ entries[i].type == TYPE_DIR)
        {
            std::cout << entries[i].file_name << "\t" << "dir" << "\t" << "-" << "\n"; // entries[i].size << "\n";
        }
    }

    for (size_t i = 0; i < nrOfEntries; i++)
    {
        if (entries[i].first_blk != FAT_FREE && entries[i].type == TYPE_FILE)
        {
            std::cout << entries[i].file_name << "\t" << "file" << "\t" << entries[i].size << "\n";
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

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int FS::cp(std::string sourcepath, std::string destpath)
{

    u_int8_t srcBlkBuffer[BLOCK_SIZE]; // Somewhere to store what we read
    u_int8_t dirBlkBuffer[BLOCK_SIZE]; // Buffer for reading directory block

    if (disk.read(current_dir, srcBlkBuffer) == -1)
    {
        std::cerr << "Error: failed to read src file in cp()." << std::endl;
        return -1;
    }

    if (disk.read(current_dir, dirBlkBuffer) == -1)
    {
        std::cerr << "Error: failed to read src file in cp()." << std::endl;
        return -1;
    }

    /* Case where destpath-file already exists*/

    dir_entry *entries = (dir_entry *)(srcBlkBuffer);
    dir_entry *dirEntries = (dir_entry *)(dirBlkBuffer);

    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        if (strcmp(entries[i].file_name, destpath.c_str()) == 0 && entries[i].type != TYPE_DIR)
        {
            // found the file that destination file already exists
            std::cerr << "Error: in cp(), Destination file already exists" << std::endl;
            return -1;
        }
    }

    dir_entry *dst = nullptr;
    bool foundFileDst = false;
    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        if (strcmp(dirEntries[i].file_name, destpath.c_str()) == 0 && dirEntries[i].type == TYPE_DIR)
        {
            // Found that target is a dir
            dst = &dirEntries[i];
            foundFileDst = true;
            break;
        }
    }

    // Step 2: find the file in the fat or create tempfile here?
    uint16_t srcChainStart;
    dir_entry *srcFile;
    bool foundFile = false;

    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        if (strcmp(entries[i].file_name, sourcepath.c_str()) == 0 && entries[i].type == TYPE_FILE)
        {
            // found the file
            // std::cout << "the matched file name!: " << entries[i].file_name << "filepath: " << filepath << std::endl;
            srcChainStart = entries[i].first_blk;
            srcFile = &entries[i];
            foundFile = true;
            break;
        }
    }

    if (!foundFile || !foundFileDst)
    {
        std::cerr << "Error: in cp(), couldn't find src or dst file" << std::endl;
        return -1;
    }

    // error check that we have enough blocks
    double temp = (double)(srcFile->size) / BLOCK_SIZE;
    int neededBlks = std::ceil(temp);

    std::vector<uint16_t> freeBlks = fatFinder();

    int nrOfFreeBlocks = freeBlks.size();

    if (nrOfFreeBlocks < neededBlks)
    {
        std::cerr << "ERROR in cp() not enough blocks" << std::endl;
        return -1;
    }

    bool dirFlag = false;

    if (dst->type == TYPE_DIR && foundFileDst == true)
    {
        dirFlag = true;
    }

    if (dirFlag != true)
    {
        // Link new blocks in FAT
        for (int i = 0; i < neededBlks - 1; i++)
        {
            this->fat[freeBlks[i]] = freeBlks[i + 1];
        }
        this->fat[freeBlks[neededBlks - 1]] = FAT_EOF;

        auto srcCurrent = srcChainStart;
        auto dstCurrent = freeBlks[0];
        for (int i = 0; i < neededBlks; i++)
        {
            disk.read(dst->first_blk, srcBlkBuffer);
            disk.write(dstCurrent, srcBlkBuffer);

            srcCurrent = this->fat[srcCurrent];
            dstCurrent = freeBlks[i];
        }
    }
    else
    {
        // copy data to taraget dir
        // Link new blocks in FAT

        u_int8_t dstBlkBuffer[BLOCK_SIZE]; // Buffer for reading directory block
        dir_entry *dstEntries = (dir_entry *)(dstBlkBuffer);

        for (int i = 0; i < neededBlks - 1; i++)
        {
            this->fat[freeBlks[i]] = freeBlks[i + 1];
        }
        this->fat[freeBlks[neededBlks - 1]] = FAT_EOF;

        auto srcCurrent = srcChainStart;
        auto dstCurrent = freeBlks[0];
        for (int i = 0; i < neededBlks; i++)
        {
            disk.read(dst->first_blk, dstBlkBuffer);
            disk.write(dstCurrent, srcBlkBuffer);

            srcCurrent = this->fat[srcCurrent]; // ?
            dstCurrent = freeBlks[i];
        }
    }

    if (dirFlag == true)
    {
        dir_entry *entries1 = reinterpret_cast<dir_entry *>(dirBlkBuffer);
        /* Dont overwrite dir args */
        if (disk.read(dst->first_blk, dirBlkBuffer) == -1)
        {
            std::cerr << "error in cp() read" << std::endl;
        };
        // Add a directory entry for the new file
        for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
        {
            // std::cout << "IN CP WE enterd the if a dir" << std::endl;
            // std::cout << "IN CP " << (int)srcFile.type << std::endl;

            if (entries1[i].first_blk == FAT_FREE)
            {

                strncpy(entries1[i].file_name, sourcepath.c_str(), sizeof(entries1[i].file_name) - 1);
                entries1[i].file_name[sizeof(entries1[i].file_name) - 1] = '\0'; // Ensure null-termination
                entries1[i].size = srcFile->size;                                // Set the size of the copied file
                entries1[i].first_blk = freeBlks[0];                             // Starting block
                entries1[i].type = srcFile->type;                                // File type
                entries1[i].access_rights = srcFile->access_rights;              // Access rights
                break;
            }
        }
    }
    else
    {

        // do normal dir update

        dir_entry *entries1 = reinterpret_cast<dir_entry *>(dirBlkBuffer);
        // auto entries1 = dirEntries;
        //  Add a directory entry for the new file
        for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
        {
            std::cout << "entries i block: " << (int)entries1[i].first_blk << std::endl;
            if (entries1[i].first_blk == FAT_FREE)
            { // Find a free directory entry

                strncpy(entries1[i].file_name, destpath.c_str(), sizeof(entries1[i].file_name) - 1);
                entries1[i].file_name[sizeof(entries1[i].file_name) - 1] = '\0'; // Ensure null-termination
                entries1[i].size = srcFile->size;                                // Set the size of the copied file
                entries1[i].first_blk = freeBlks[0];                             // Starting block
                entries1[i].type = srcFile->type;                                // File type
                entries1[i].access_rights = srcFile->access_rights;              // Access rights
                break;
            }
        }
    }

    if (dirFlag == true)
    {
        /* Destination is a directory */

        // write to destination block
        if (disk.write(dst->first_blk, dirBlkBuffer) == -1)
        {
            std::cerr << "Error: Failed to write directory block in cp()." << std::endl;
            return -1;
        }
    }
    else
    {
        // Write the updated directory block back to the disk
        if (disk.write(current_dir, dirBlkBuffer) == -1)
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

    std::cout << "FS::cp(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int FS::mv(std::string sourcepath, std::string destpath)
{
    //  /etc/usr/f1 /etc/usr/f2

    u_int8_t srcBlkBuffer[BLOCK_SIZE]; // Somewhere to store what we read

    if (disk.read(current_dir, srcBlkBuffer) == -1)
    {
        std::cerr << "Error: failed to read src file in mv()." << std::endl;
        return -1;
    }

    dir_entry *entries = (dir_entry *)(srcBlkBuffer);

    /* Case wher soruce name is bad */
    bool foundFile = false;
    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        if (strcmp(entries[i].file_name, sourcepath.c_str()) == 0)
        {
            // found the file that destination file already exists
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

    /* Case where destpath-file already exists*/

    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        if (strcmp(entries[i].file_name, destpath.c_str()) == 0)
        {
            // found the file that destination file already exists
            std::cerr << "Error: in mv(), Destination file already exists" << std::endl;
            return -1;
        }
    }

    // Step 2: find the file in the fat or create tempfile here?

    dir_entry *srcFile = nullptr;
    foundFile = false;

    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        if (strcmp(entries[i].file_name, sourcepath.c_str()) == 0)
        {
            // found the file
            srcFile = &entries[i];
            foundFile = true;
            break;
        }
    }

    if (foundFile == false)
    {
        /* couldnt find source file */
        std::cerr << "Error in mv() couldnt find source file" << std::endl;
    }

    std::string dstName = extractFilename(destpath);

    // convert string to char[]
    char charArray[56]; // Ensure it has the same size as file_name in dir_entry
    for (size_t i = 0; i < dstName.length() && i < sizeof(charArray) - 1; ++i)
    {
        charArray[i] = dstName[i];
    }
    charArray[dstName.length()] = '\0'; // Null-terminate the array

    // Assign charArray to srcFile.file_name using strncpy
    strncpy(srcFile->file_name, charArray, sizeof(srcFile->file_name) - 1);
    srcFile->file_name[sizeof(srcFile->file_name) - 1] = '\0'; // Ensure null-termination

    disk.write(current_dir, srcBlkBuffer);

    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int FS::rm(std::string filepath)
{
    /*
        rm <filename>
        Deletes the file filename, i.e., removes its directory entry and marks the corresponding disk blocks as free.
    */

    // auto filename = extractFilename(filepath);

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
        if (entries[i].first_blk != FAT_FREE)
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
    }
    else if (foundfileSrc != true)
    {
        std::cerr << "error in append: didnt find file1" << std::endl;
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

    u_int8_t blkBuffer[BLOCK_SIZE]; // Somewhere to store what we read
    dir_entry *entries = (dir_entry *)(blkBuffer);

    if (disk.read(current_dir, blkBuffer) == -1)
    {
        std::cerr << "error in mkdir() couldnt read disk" << std::endl;
        return -1;
    }

    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        if (entries[i].file_name == dirpath && entries[i].type == TYPE_FILE) // THIS IS BAD
        {
            /* Param is a file not ok */
            std::cerr << "Error in mkdir(), param was a file" << std::endl;
            return -1;
        }
    }

    dir_entry *freeSpot = nullptr;
    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        /* find free spot in 'Root' directory */
        if (entries[i].first_blk == FAT_FREE)
        {
            freeSpot = &entries[i];
            break;
        }
    }

    if (!freeSpot)
    {
        /* no room in origin dir */
        std::cerr << "error in mkdir()" << std::endl;
        return -1;
    }

    /* Hitta plats i FAT*/
    auto firstFree = fatFinder()[0]; // directoy block for new dir

    this->fat[firstFree] = FAT_EOF; // Mark the new directory block as allocated

    /* build FAT*/
    /*skriv metadata  */

    strncpy(freeSpot->file_name, dirpath.c_str(), sizeof(freeSpot->file_name) - 1);
    freeSpot->file_name[sizeof(freeSpot->file_name) - 1] = '\0';
    freeSpot->size = 1;
    freeSpot->first_blk = firstFree;
    freeSpot->type = TYPE_DIR;
    freeSpot->access_rights = READ | WRITE;

    /*skriv all till disk */
    disk.write(current_dir, blkBuffer);

    /*skriv metadata .. i dirX nya directory block */

    uint8_t newDirbuffer[BLOCK_SIZE] = {0}; // Somewhere to store what we read
    disk.read(firstFree, newDirbuffer);

    dir_entry *newEntries = (dir_entry *)(newDirbuffer);

    dir_entry *parentEntry = &newEntries[0];

    // Making .. entry

    std::string x = "..";
    strncpy(parentEntry->file_name, x.c_str(), sizeof(parentEntry->file_name) - 1);
    parentEntry->file_name[sizeof(parentEntry->file_name) - 1] = '\0';
    parentEntry->size = 1;
    parentEntry->first_blk = current_dir;
    parentEntry->type = TYPE_DIR;
    parentEntry->access_rights = READ | WRITE;

    disk.write(firstFree, newDirbuffer);

    this->fat[firstFree] = FaT_EOF_unint16;

    // Write the updated FAT to the disk
    uint8_t fatBuffer[BLOCK_SIZE] = {0};
    memcpy(fatBuffer, this->fat, sizeof(this->fat));
    if (disk.write(FAT_BLOCK, fatBuffer) == -1)
    {
        std::cerr << "Error: Failed to write FAT in cp()." << std::endl;
        return -1;
    }

    std::cout
        << "FS::mkdir(" << dirpath << ")\n";
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int FS::cd(std::string dirpath)
{

    // search FAT for dirname that maches dirpath

    uint8_t blkBuffer[BLOCK_SIZE];
    disk.read(current_dir, blkBuffer);

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
        std::cerr << "Error in cd() couldn't find directory" << std::endl;
        return -1;
    }

    // std::cout << "Found:" << paramEntry->file_name << std::endl;
    // std::cout << "Type:" << (int)paramEntry->type << std::endl;
    //  Handle case where a file is in param
    if (paramEntry->type == TYPE_FILE)
    {
        std::cerr << "Error in cd() parameter is of the type file" << std::endl;
        return -1;
    }

    // handle case when .. is dirpath
    if (dirpath == "..")
    {

        /* Find the dir that matches '..' name */

        current_dir = paramEntry->first_blk;
        return 0; // Return early to avoid processing further

        // write return
        /*
        if (disk.write(current_dir, blkBuffer) == -1)
        {
            std::cerr << "failed disk write in cd()" << std::endl;
            return 0;
        };
        */
    }

    /* Find the dir that matches param name */
    // std::cout << "OLD current dir" << current_dir << std::endl; // DEBUG
    current_dir = paramEntry->first_blk;

    // std::cout << "New current dir" << current_dir << std::endl; // DEBUG

    // write return
    /*
    if (disk.write(current_dir, blkBuffer) == -1)
    {
        std::cerr << "failed disk write in cd()" << std::endl;
    };
    */

    std::cout << "FS::cd(" << dirpath << ")\n";
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
    std::cout << "FS::chmod(" << accessrights << "," << filepath << ")\n";
    return 0;
}
