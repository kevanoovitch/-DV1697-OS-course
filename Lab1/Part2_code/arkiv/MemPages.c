#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
/* fifo.c

function fifo_page_replacement(num_pages, page_size, trace_file):
    Initialize page_queue as empty queue
    Initialize pages_in_memory as empty set
    Initialize page_faults = 0

    Open trace_file
    For each memory_reference in trace_file:
        page_number = memory_reference / page_size

        if page_number not in pages_in_memory:
            page_faults += 1
            if length(page_queue) == num_pages:
                evicted_page = page_queue.dequeue()
                pages_in_memory.remove(evicted_page)
            page_queue.enqueue(page_number)
            pages_in_memory.add(page_number)

    Print "Number of page faults:", page_faults



*/

void fifo()
{
    /*
    function fifo_page_replacement(num_pages, page_size, trace_file):
    Initialize page_queue as empty queue
    Initialize pages_in_memory as empty set
    Initialize page_faults = 0

    */
}

int main(int argc, char **argv)
{
    /*
    Open trace_file
      For each memory_reference in trace_file:
          page_number = memory_reference / page_size


              if length(page_queue) == num_pages:
                  evicted_page = page_queue.dequeue()
                  pages_in_memory.remove(evicted_page)
              page_queue.enqueue(page_number)
              pages_in_memory.add(page_number)

      Print "Number of page faults:", page_faults
    */

    // Parse command-line arguments eg. interpret inputted args
    int num_pages = atoi(argv[1]); // Convert number of pages to integer
    int page_size = atoi(argv[2]); // Convert page size to integer
    char *trace_file = argv[3];    // Trace file name as a string
    int SIZE = num_pages * page_size;

    // open file in read mode
    FILE *myFile = fopen(trace_file, "r");

    // for each row in txt

    // For each row in the text file
    char buffer[256]; // Buffer to hold a single line
    int line = 0;     // Line counter

    while (fgets(buffer, sizeof(buffer), myFile) != NULL)
    {
        line++;

        // Convert the line to an integer (memory reference)
        int memory_reference = atoi(buffer);

        // Compute the page number
        int page_number = memory_reference / page_size;

        /*

        if page_number not in pages_in_memory:
            page_faults += 1
            if length(page_queue) == num_pages:
                evicted_page = page_queue.dequeue()
                pages_in_memory.remove(evicted_page)
            page_queue.enqueue(page_number)
            pages_in_memory.add(page_number)

        */

        /*

DEBUG BLOAT CODE

printf("Line %d: Memory reference = %d, Page number = %d\n",
       line, memory_reference, page_number);
*/
    }
}
