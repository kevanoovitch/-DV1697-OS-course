#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdbool.h>
#include "fifo.h"

#define maxPages 120000

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
    int line = 0;     // Line counter

    /* ---FIFO Data Structures--- */

    int pageFaults = 0;
    FIFO *myQueue = create_fifo(maxPages);

    /*  Tracks if a page is in memory:
        An array of false elements if a page exist, Arr[page_number] becomes true
    */

    // bool *page_in_memory = (bool *)malloc(maxPages * sizeof(bool));

    // 6800/256 = 33
    while (fgets(buffer, sizeof(buffer), myFile) != NULL)
    {
        line++;

        // Convert the line to an integer (memory reference)
        int memory_reference = atoi(buffer);

        // Compute the page number
        int page_number = memory_reference / page_size;

        /* Check if there is a page in memory eg. the FIFO-Queue */

        if (page_in_memory[page_number] != false)
        {
            /* The pages exist meaning no fault */
        }
        else
        {
            /* The pages does not exist meaning page fault, also add the page to memory */
            pageFaults += 1;
            myQueue->enqueue(myQueue, page_number);
            page_in_memory[page_number] = true;
        }
    }

    fclose(myFile);
    myQueue->destroy(myQueue);

    printf("Number of pagefaults: %d \n", pageFaults);
    // free(page_in_memory);
}
