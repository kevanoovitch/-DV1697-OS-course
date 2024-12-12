#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdbool.h>

int nrOfElements = 0;
int pageFaults = 0;
int GlobalIndex = 0;

bool full(int num_pages)
{
    return nrOfElements == num_pages;
}

int findLeastLikely(int num_pages, int *arr, int *FutArr, int SizeOfArray)
{
    int longestHopIndex = 0;
    int longestHop = -1;

    for (int i = 0; i < num_pages; i++)
    {

        int hopCounter = 0;
        int found = 0;
        for (int j = GlobalIndex; j < SizeOfArray; j++)
        {
            if (arr[i] == FutArr[j])
            {
                found = 1;
                break;
            }

            hopCounter++;
        }

        if (!found)
        {             // Page not found in future references
            return i; // Replace this page immediately
        }

        if (hopCounter > longestHop)
        {
            longestHop = hopCounter;

            longestHopIndex = i;
        }
    }

    return longestHopIndex;
}

// Load a page into memory, if it is not already there
void add(int page, int num_pages, int *arr, int *FutArr, int SizeOfArray)
{
    for (int i = 0; i < num_pages; i++)
    {
        if (arr[i] == page)
        {
            /* The pages exist meaning no fault */
            return;
        }
    }

    // check if full
    if (nrOfElements == num_pages)
    {

        int leastLikleyIndex = findLeastLikely(num_pages, arr, FutArr, SizeOfArray);

        arr[leastLikleyIndex] = page;
    }
    else
    {
        // Not full then just add in next best place no need to think about algoritm.
        arr[nrOfElements] = page;
        nrOfElements++;
    }
    pageFaults++;
}

int main(int argc, char **argv)
{
    // Parse command-line arguments eg. interpret inputted args
    int num_pages = atoi(argv[1]); // Convert number of pages to integer
    int page_size = atoi(argv[2]); // Convert page size to integer
    char *trace_file = argv[3];    // Trace file name as a string

    // change globalIndex
    // GlobalIndex = num_pages;

    // open file in read mode
    FILE *myFile = fopen(trace_file, "r");

    // For each row in the text file
    char buffer[256]; // Buffer to hold a single line

    int *page_in_memory = (int *)malloc(num_pages * sizeof(int));

    for (int i = 0; i < num_pages; i++)
    {
        page_in_memory[i] = -1;
    }

    /* Analys the file */
    int futureAccessSize = 1000;
    int *futureAcess = (int *)calloc(futureAccessSize, sizeof(int));

    int index = 0;

    while (fgets(buffer, sizeof(buffer), myFile) != NULL)
    {

        // Convert the line to an integer (memory reference)
        int memory_reference = atoi(buffer);

        // Compute the page number
        int page_number = memory_reference / page_size;

        if (index >= futureAccessSize)
        {
            int oldSize = futureAccessSize;
            futureAccessSize *= 2; // Double the size for efficiency
            futureAcess = realloc(futureAcess, futureAccessSize * sizeof(int));
            if (!futureAcess)
            {
                perror("Memory reallocation failed");
                exit(1); // Exit if realloc fails to prevent undefined behavior
            }

            // Initialize new memory
            for (int i = oldSize; i < futureAccessSize; i++)
            {
                futureAcess[i] = 0;
            }
        }

        futureAcess[index++] = page_number;
    }

    rewind(myFile); // Moves seeker to top of file

    while (fgets(buffer, sizeof(buffer), myFile) != NULL)
    {

        // Convert the line to an integer (memory reference)
        int memory_reference = atoi(buffer);

        // Compute the page number
        int page_number = memory_reference / page_size;

        add(page_number, num_pages, page_in_memory, futureAcess, index);
        GlobalIndex++;
    }

    fclose(myFile);

    free(page_in_memory);
    free(futureAcess);
    printf("Number of pagefaults: %d \n", pageFaults);
}
