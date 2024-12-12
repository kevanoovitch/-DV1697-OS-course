#include "fifo.h"
#include <stdlib.h>
#include <stdio.h>

void resize(FIFO *self, int new_capacity)
{
    int *new_buffer = (int *)malloc(sizeof(int) * new_capacity);

    for (int i = 0; i < self->size; i++)
    {
        new_buffer[i] = self->buffer[(self->front + i) % self->capacity];
    }

    free(self->buffer);
    self->buffer = new_buffer;
    self->capacity = new_capacity;
    self->front = 0;
    self->rear = self->size - 1;
}

bool enqueue(FIFO *self, int value)
{
    if (self->size == self->capacity)
    {
        /* Queue will be full resize first */
        int oldSize = self->capacity;

        resize(self, self->capacity * 2);
        if (self->capacity <= oldSize)
        {
            /* Failed to expand */
            return false;
        }
    }

    self->rear = (self->rear + 1) % self->capacity;
    self->buffer[self->rear] = value;
    self->size++;
    return true;
}

bool dequeue(FIFO *self, int *value)
{
    if (self->is_empty(self))
    {
        /* queue is empty */
        return false;
    }

    *value = self->buffer[self->front];
    self->front = (self->front + 1) % self->capacity;
    self->size--;
    return true;
}

int peek(FIFO *self)
{
    if (self->is_empty(self))
    {
        /* queue is empty */
        return -1;
    }

    return self->buffer[self->front];
}

bool is_empty(FIFO *self)
{
    if (self->size == 0)
    {
        return true;
    }

    return false;
}

static void fifo_destroy(FIFO *self)
{
    free(self->buffer);
    free(self);
}

FIFO *create_fifo(int initial_capacity)
{
    FIFO *fifo = (FIFO *)malloc(sizeof(FIFO));

    if (initial_capacity <= 0)
    {
        initial_capacity = 2;
    }

    fifo->buffer = (int *)malloc(sizeof(int) * initial_capacity);
    if (!fifo->buffer)
    {
        free(fifo);
        return NULL;
    }

    // Assign initiate member values

    fifo->capacity = initial_capacity;
    fifo->front = 0;
    fifo->rear = -1;
    fifo->size = 0;

    fifo->enqueue = enqueue;
    fifo->dequeue = dequeue;
    fifo->peek = peek;
    fifo->is_empty = is_empty;
    fifo->destroy = fifo_destroy;

    return fifo;
}
