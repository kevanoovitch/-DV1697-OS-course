#ifndef FIFO_H
#define FIFO_H

#include <stdbool.h>
#include <stdlib.h>

// FIFO structure declaration
typedef struct FIFO
{
    int *buffer;
    int capacity;
    int front;
    int rear;
    int size;

    // Member functions
    bool (*enqueue)(struct FIFO *self, int value);
    bool (*dequeue)(struct FIFO *self, int *value);
    int (*peek)(struct FIFO *self);
    bool (*is_empty)(struct FIFO *self);
    void (*destroy)(struct FIFO *self);
} FIFO;

// Function prototypes
FIFO *create_fifo(int initial_capacity);

#endif // FIFO_H