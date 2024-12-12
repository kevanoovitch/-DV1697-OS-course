#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdbool.h>

int nrOfElements = 0;
int front = 0;
int rear = 0;
int pageFaults = 0;

bool full(int num_pages)
{
    if (nrOfElements == num_pages)
    {
        return true;
    }
    return false;
}

int findLeastLikely(int num_pages, int *arr, int *FutArr)
{
    int smallestYet = FutArr[arr[0]];
    int smallestIndex = 0;

    for (int i = 1; i < num_pages; i++)
    {
        if (smallestYet > FutArr[arr[i]])
        {
            smallestYet = FutArr[arr[i]];
            smallestIndex = i;
        }
    }

    return smallestIndex;
}

// Load a page into memory, if it is not already there
void add(int page, int num_pages, int *arr, int *FutArr)
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
        int index = findLeastLikely(num_pages, arr, FutArr);
        arr[index] = page;
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

    // open file in read mode
    FILE *myFile = fopen(trace_file, "r");

    // For each row in the text file
    char buffer[256]; // Buffer to hold a single line

    int *page_in_memory = (int *)malloc(num_pages * sizeof(int));

    /* Analys the file */
    int futureAccessSize = 100000;
    int *futureAcess = (int *)calloc(futureAccessSize, sizeof(int));
    while (fgets(buffer, sizeof(buffer), myFile) != NULL)
    {

        // Convert the line to an integer (memory reference)
        int memory_reference = atoi(buffer);

        // Compute the page number
        int page_number = memory_reference / page_size;

        if (page_number > futureAccessSize)
        {
            int *newFutureAccess = calloc(page_number, sizeof(int));

            for (int i = 0; i < futureAccessSize; i++)
            {
                newFutureAccess[i] = futureAcess[i];
            }

            for (int i = futureAccessSize; i < page_number; i++)
            {
                newFutureAccess[i] = 0;
            }

            futureAccessSize = page_number;
            free(futureAcess);
            futureAcess = newFutureAccess;
        }

        futureAcess[page_number]++; // Each index is a page number and each element is it's frequency of occurance
    }
    printf("HELLO WORLD %d\n", futureAccessSize);

    fseek(myFile, 0, SEEK_SET); // Moves seeker to top of file
    while (fgets(buffer, sizeof(buffer), myFile) != NULL)
    {

        // Convert the line to an integer (memory reference)
        int memory_reference = atoi(buffer);

        // Compute the page number
        int page_number = memory_reference / page_size;

        add(page_number, num_pages, page_in_memory, futureAcess);

        futureAcess[page_number]--;
    }

    fclose(myFile);

    free(page_in_memory);
    free(futureAcess);
    printf("Number of pagefaults: %d \n", pageFaults);
}
