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

    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        if (strcmp(entries[i].file_name, filepath.c_str()) == 0)
        {
            // found the file
            // std::cout << "the matched file name!: " << entries[i].file_name << "filepath: " << filepath << std::endl;
            chainStart = entries[i].first_blk;

            file = entries[i];
            foundFile = true;
            break;
        }
    }

    if (foundFile == false)
    {
        std::cerr << "Error: in cat(), couldn't find file" << std::endl;
        return -1;
    }

    // Step 3: print the file contents to screen
    std::string data = "";
    uint8_t blockData[BLOCK_SIZE]; // Buffer to store each block's content
    uint16_t currentBlock = chainStart;

    // use first_fileBlk from struct
    // follow the linked list untill EOF
    // add to a string/buffer

    int localEOF = 65535;

    while (currentBlock != localEOF)
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

    std::cout << "name \t size\n";

    // Handle for case: empty FS

    size_t nrOfEntries = BLOCK_SIZE / sizeof(dir_entry); // Number of entries in the block

    for (size_t i = 0; i < nrOfEntries; i++)
    {
        if (entries[i].first_blk != FAT_FREE)
        {
            std::cout << entries[i].file_name << "\t" << entries[i].size << " bytes\n";
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
    // scenario CP is only called in same dir...

    u_int8_t srcBlkBuffer[BLOCK_SIZE]; // Somewhere to store what we read
    u_int8_t dirBlkBuffer[BLOCK_SIZE]; // Buffer for reading directory block

    if (disk.read(current_dir, srcBlkBuffer) == -1)
    {
        std::cerr << "Error: failed to read src file in cp()." << std::endl;
        return -1;
    }

    dir_entry *entries = (dir_entry *)(srcBlkBuffer);

    // Step 2: find the file in the fat or create tempfile here?
    uint16_t srcChainStart;
    dir_entry srcFile;
    bool foundFile = false;

    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {
        if (strcmp(entries[i].file_name, sourcepath.c_str()) == 0)
        {
            // found the file
            // std::cout << "the matched file name!: " << entries[i].file_name << "filepath: " << filepath << std::endl;
            srcChainStart = entries[i].first_blk;
            srcFile = entries[i];
            foundFile = true;
            break;
        }
    }

    if (!foundFile)
    {
        std::cerr << "Error: in cp(), couldn't file file" << std::endl;
        return -1;
    }

    // error check that we have enough blocks
    double temp = (double)(srcFile.size) / BLOCK_SIZE;
    int neededBlks = std::ceil(temp);

    std::vector<uint16_t> freeBlks = fatFinder();

    int nrOfFreeBlocks = freeBlks.size();

    if (nrOfFreeBlocks < neededBlks)
    {
        std::cerr << "ERROR in cp() not enough blocks" << std::endl;
        return -1;
    }

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
        disk.read(srcCurrent, srcBlkBuffer);
        disk.write(dstCurrent, srcBlkBuffer);

        srcCurrent = this->fat[srcCurrent];
        dstCurrent = freeBlks[i];
    }

    disk.read(current_dir, dirBlkBuffer);

    dir_entry *entries1 = reinterpret_cast<dir_entry *>(dirBlkBuffer);

    // Add a directory entry for the new file
    for (int i = 0; i < BLOCK_SIZE / sizeof(dir_entry); i++)
    {

        if (entries1[i].first_blk == FAT_FREE)
        { // Find a free directory entry

            strncpy(entries1[i].file_name, destpath.c_str(), sizeof(entries1[i].file_name) - 1);
            entries1[i].file_name[sizeof(entries1[i].file_name) - 1] = '\0'; // Ensure null-termination
            entries1[i].size = srcFile.size;                                 // Set the size of the copied file
            entries1[i].first_blk = freeBlks[0];                             // Starting block
            entries1[i].type = srcFile.type;                                 // File type
            entries1[i].access_rights = srcFile.access_rights;               // Access rights
            break;
        }
    }

    // Write the updated directory block back to the disk
    if (disk.write(current_dir, dirBlkBuffer) == -1)
    {
        std::cerr << "Error: Failed to write directory block in cp()." << std::endl;
        return -1;
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
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int FS::rm(std::string filepath)
{
    std::cout << "FS::rm(" << filepath << ")\n";
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int FS::append(std::string filepath1, std::string filepath2)
{
    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";
    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int FS::mkdir(std::string dirpath)
{
    std::cout << "FS::mkdir(" << dirpath << ")\n";
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int FS::cd(std::string dirpath)
{
    std::cout << "FS::cd(" << dirpath << ")\n";
    return 0;
}

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
    while (travDir != ROOT_BLOCK)
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
    }

    // Step 4: Print the full path
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
