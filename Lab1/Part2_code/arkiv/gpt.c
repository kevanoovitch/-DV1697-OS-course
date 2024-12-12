#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

int nrOfElements = 0;
int pageFaults = 0;
int GlobalIndex = 0;
int pageReplace = 0;

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
        {
            return i; // Replace page not found in future references
        }

        if (hopCounter > longestHop)
        {
            longestHop = hopCounter;
            longestHopIndex = i;
        }
    }

    return longestHopIndex;
}

void add(int page, int num_pages, int *arr, int *FutArr, int SizeOfArray)
{
    for (int i = 0; i < num_pages; i++)
    {
        if (arr[i] == page)
        {
            return; // Page already in memory, no fault
        }
    }

    if (nrOfElements == num_pages)
    {
        int leastLikelyIndex = findLeastLikely(num_pages, arr, FutArr, SizeOfArray);
        pageReplace++;
        arr[leastLikelyIndex] = page;
    }
    else
    {
        arr[nrOfElements++] = page;
    }
    pageFaults++;
}

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        printf("Usage: %s <num_pages> <page_size> <trace_file>\n", argv[0]);
        return 1;
    }

    int num_pages = atoi(argv[1]);
    int page_size = atoi(argv[2]);
    char *trace_file = argv[3];

    FILE *myFile = fopen(trace_file, "r");
    if (!myFile)
    {
        perror("Error opening file");
        return 1;
    }

    int *page_in_memory = malloc(num_pages * sizeof(int));
    for (int i = 0; i < num_pages; i++)
    {
        page_in_memory[i] = -1; // Initialize memory
    }

    int futureAccessSize = 1000;
    int *futureAcess = malloc(futureAccessSize * sizeof(int));
    int index = 0;

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), myFile) != NULL)
    {
        int memory_reference = atoi(buffer);
        int page_number = memory_reference / page_size;

        if (index >= futureAccessSize)
        {
            int oldSize = futureAccessSize;
            futureAccessSize *= 2;
            futureAcess = realloc(futureAcess, futureAccessSize * sizeof(int));
            if (!futureAcess)
            {
                perror("Memory reallocation failed");
                exit(1);
            }
            for (int i = oldSize; i < futureAccessSize; i++)
            {
                futureAcess[i] = 0;
            }
        }
        futureAcess[index++] = page_number;
    }

    rewind(myFile);
    while (fgets(buffer, sizeof(buffer), myFile) != NULL)
    {
        int memory_reference = atoi(buffer);
        int page_number = memory_reference / page_size;

        add(page_number, num_pages, page_in_memory, futureAcess, index);
        GlobalIndex++;
    }

    fclose(myFile);
    free(page_in_memory);
    free(futureAcess);

    printf("Number of page faults: %d\n", pageFaults);
    printf("Number of page replacements: %d\n", pageReplace);

    return 0;
}
