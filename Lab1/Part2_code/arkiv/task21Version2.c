#include <stdio.h>
#include <stdlib.h>

int nrOfElements = 0;
int pageFaults = 0;

// The add function now uses the precomputed next use positions
void add(int currentIndex, int num_pages, int *arr, int *memoryReferences, int totalReferences, int *nextUse, int *page_in_memory_nextUse)
{
    int page = memoryReferences[currentIndex];

    // Check if page is already in memory
    for (int i = 0; i < nrOfElements; i++)
    {
        if (arr[i] == page)
        {
            // Update the next use position for this page
            page_in_memory_nextUse[i] = nextUse[currentIndex];
            return; // Page is already in memory, no page fault
        }
    }

    // Page fault occurs
    pageFaults++;

    // If memory is not full, add page to memory
    if (nrOfElements < num_pages)
    {
        arr[nrOfElements] = page;
        page_in_memory_nextUse[nrOfElements] = nextUse[currentIndex];
        nrOfElements++;
    }
    else
    {
        // Memory is full, find the page with farthest next use
        int replaceIndex = -1;
        int farthestNextUse = -1;

        for (int i = 0; i < num_pages; i++)
        {
            int nextUsePosition = page_in_memory_nextUse[i];

            if (nextUsePosition == -1)
            {
                // Page is not used again; select it for replacement
                replaceIndex = i;
                break;
            }
            else if (nextUsePosition > farthestNextUse)
            {
                farthestNextUse = nextUsePosition;
                replaceIndex = i;
            }
        }

        // Replace the page
        arr[replaceIndex] = page;
        page_in_memory_nextUse[replaceIndex] = nextUse[currentIndex];
    }
}

int main(int argc, char **argv)
{
    // Check command-line arguments
    if (argc < 4)
    {
        fprintf(stderr, "Usage: %s <num_pages> <page_size> <trace_file>\n", argv[0]);
        return 1;
    }

    // Parse command-line arguments
    int num_pages = atoi(argv[1]); // Number of page frames
    int page_size = atoi(argv[2]); // Page size
    char *trace_file = argv[3];    // Trace file name

    // Open the trace file
    FILE *myFile = fopen(trace_file, "r");
    if (!myFile)
    {
        perror("Error opening file");
        return 1;
    }

    // Read the entire file into an array
    int maxReferences = 1000000; // Initial size for memoryReferences array
    int *memoryReferences = malloc(maxReferences * sizeof(int));
    char buffer[256];
    int totalReferences = 0;
    int maxPageNumber = 0;

    while (fgets(buffer, sizeof(buffer), myFile) != NULL)
    {
        int memory_reference = atoi(buffer);
        int page_number = memory_reference / page_size;
        memoryReferences[totalReferences++] = page_number;

        if (page_number > maxPageNumber)
        {
            maxPageNumber = page_number;
        }

        if (totalReferences >= maxReferences)
        {
            // Reallocate more space if needed
            maxReferences *= 2;
            int *temp = realloc(memoryReferences, maxReferences * sizeof(int));
            if (!temp)
            {
                perror("Error reallocating memory");
                free(memoryReferences);
                fclose(myFile);
                return 1;
            }
            memoryReferences = temp;
        }
    }
    fclose(myFile);

    // Build next use information
    int *nextUse = malloc(totalReferences * sizeof(int));
    int *lastSeen = malloc((maxPageNumber + 1) * sizeof(int));

    if (!nextUse || !lastSeen)
    {
        perror("Error allocating memory for next use information");
        free(memoryReferences);
        return 1;
    }

    // Initialize lastSeen array
    for (int i = 0; i <= maxPageNumber; i++)
    {
        lastSeen[i] = -1;
    }

    // Build nextUse array
    for (int i = totalReferences - 1; i >= 0; i--)
    {
        int page = memoryReferences[i];
        nextUse[i] = lastSeen[page];
        lastSeen[page] = i;
    }
    free(lastSeen);

    // Initialize page_in_memory array and their next use positions
    int *page_in_memory = malloc(num_pages * sizeof(int));
    int *page_in_memory_nextUse = malloc(num_pages * sizeof(int));

    if (!page_in_memory || !page_in_memory_nextUse)
    {
        perror("Error allocating memory for page frames");
        free(memoryReferences);
        free(nextUse);
        return 1;
    }

    // Reset nrOfElements and pageFaults
    nrOfElements = 0;
    pageFaults = 0;

    // Loop over all memory references
    for (int currentIndex = 0; currentIndex < totalReferences; currentIndex++)
    {
        add(currentIndex, num_pages, page_in_memory, memoryReferences, totalReferences, nextUse, page_in_memory_nextUse);
    }

    // Clean up
    free(memoryReferences);
    free(nextUse);
    free(page_in_memory);
    free(page_in_memory_nextUse);

    printf("Number of page faults: %d\n", pageFaults);

    return 0;
}
