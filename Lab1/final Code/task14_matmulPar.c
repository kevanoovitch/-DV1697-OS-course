/***************************************************************************
 *
 * Sequential version of Matrix-Matrix multiplication
 *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define SIZE 16

// declare functions
static void *matmul_par(void *params);

struct threadArgs
{
    unsigned int id;
};

static double a[SIZE][SIZE];
static double b[SIZE][SIZE];
static double c[SIZE][SIZE];

static void *init_matrix(void *params)
{
    struct threadArgs *args = (struct threadArgs *)params;

    unsigned int row = args->id;

    int j;
    for (j = 0; j < SIZE; j++)
    {
        /* Simple initialization, which enables us to easy check
         * the correct answer. Each element in c will have the same
         * value as SIZE after the matmul operation.
         */
        a[row][j] = 1.0;
        b[row][j] = 1.0;
    }
}

static void *matmul_par(void *params)
{
    int i, j, k;
    struct threadArgs *args = (struct threadArgs *)params;

    unsigned int row = args->id;

    // j is column number, k is next element in column (?)
    for (int j = 0; j < SIZE; j++) // FOR each column j in matrix C DO
    {
        c[row][j] = 0;

        for (int k = 0; k < SIZE; k++) // FOR each element k in the row of A and column of B DO
        {
            /* Add the product A[row][k] * B[k][j] to C[row][j] */
            c[row][j] = (a[row][k] * b[k][j]) + c[row][j];
        }
    }

    printf("Thread# %u did the calculations: \n", row);

    return NULL;
}

static void Threader()
{
    pthread_t *threads;      // dynamic array of child threads
    struct threadArgs *args; // argument buffer

    threads = malloc(SIZE * sizeof(pthread_t));      // allocate array of handles
    args = malloc(SIZE * sizeof(struct threadArgs)); // args vector

    /* Added create loop */

    for (unsigned int id = 0; id < SIZE; id++)
    {

        // create threads
        args[id].id = id;
        pthread_create(&(threads[id]), // our handle for the child
                       NULL,           // attributes of the child
                       init_matrix,    // the function it should run
                       (void *)&args[id]);
    }

    for (unsigned int id = 0; id < SIZE; id++)
    {

        // create threads
        args[id].id = id;
        pthread_create(&(threads[id]), // our handle for the child
                       NULL,           // attributes of the child
                       matmul_par,     // the function it should run
                       (void *)&args[id]);
    }

    for (unsigned int id = 0; id < SIZE; id++)
    {
        pthread_join(threads[id], NULL);
    }

    free(args);    // deallocate args vector
    free(threads); // deallocate array
}

static void print_matrix(void)
{

    int i, j;

    for (i = 0; i < SIZE; i++)
    {
        for (j = 0; j < SIZE; j++)
            printf(" %7.2f", c[i][j]);
        printf("\n");
    }
}

int main(int argc, char **argv)
{
    Threader();
    print_matrix();
}
