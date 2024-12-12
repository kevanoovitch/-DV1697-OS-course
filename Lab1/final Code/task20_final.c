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

void changePrio(int index, int num_pages, int *arr)
{

    // Store the element to move
    int temp = arr[index];

    //[48,55,64,33] -->  -->[48,64,33,55], match 55
    /*
    [48,55,64,33]
    [48,64,64,33]
    [48,64,33,33]
    [48,64,33,33]
    after loop
    [48,64,33,55]

    */
    // Shift elements circularly from index to rear
    int i = index;
    while (i != rear)
    {
        arr[i] = arr[(i + 1) % num_pages];
        i = (i + 1) % num_pages; // Move to the next index circularly
    }
    // Place the stored element at the rear position
    arr[rear] = temp;
}

bool dequeue(int num_pages)
{
    if (nrOfElements == num_pages)
    {
        return false;
    }

    front = (front + 1) % num_pages;
    nrOfElements--; // Decrease the cob
    return true;
}

void enqueue(int page, int num_pages, int *arr)
{
    // check if full
    if (nrOfElements == num_pages)
    {
        dequeue(num_pages);
    }

    rear = (rear + 1) % num_pages; // Move rear to the next position
    arr[rear] = page;              // Insert the new page
    nrOfElements++;
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

    /* ---FIFO Data Structures--- */

    int *page_in_memory = (int *)malloc(num_pages * sizeof(int));

    /*  Tracks if a page is in memory:
    An array of false elements if a page exist, Arr[page_number] becomes true
    */

    while (fgets(buffer, sizeof(buffer), myFile) != NULL)
    {

        // Convert the line to an integer (memory reference)
        int memory_reference = atoi(buffer);

        // Compute the page number
        int page_number = memory_reference / page_size;

        /* Check if there is a page in memory eg. the FIFO-Queue */

        bool flag = false;

        for (int i = 0; i < num_pages; i++)
        {
            if (page_in_memory[i] == page_number)
            {

                /* The pages exist meaning no fault */

                flag = true;
                changePrio(i, num_pages, page_in_memory);

                break;
            }
        }

        if (flag == false)
        {

            /*Happens if page fault happens AND the memory is full*/
            if (full(num_pages) == true)
            {
                /* If it's also true purge the oldest page */
                dequeue(num_pages);
            }

            /* The pages does not exist meaning page fault, also add the page to memory */
            pageFaults += 1;
            enqueue(page_number, num_pages, page_in_memory);
        }
    }

    fclose(myFile);

    free(page_in_memory);
    printf("Number of pagefaults: %d \n", pageFaults);
}
