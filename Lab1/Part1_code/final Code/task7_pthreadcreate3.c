#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

struct threadArgs
{
	unsigned int id;
	unsigned int numThreads;
	unsigned int squareId;
};

void *child(void *params)
{
	struct threadArgs *args = (struct threadArgs *)params;
	unsigned int childID = args->id;
	unsigned int numThreads = args->numThreads;

	printf("Greetings from child #%u of %u\n", childID, numThreads);

	// Step 2
	args->squareId = childID * childID;
}

int main(int argc, char **argv)
{
	pthread_t *children;	 // dynamic array of child threads
	struct threadArgs *args; // argument buffer
	unsigned int numThreads = 0;
	// get desired # of threads
	if (argc > 1)
		numThreads = atoi(argv[1]);

	children = malloc(numThreads * sizeof(pthread_t));	   // allocate array of handles
	args = malloc(numThreads * sizeof(struct threadArgs)); // args vector

	for (unsigned int id = 0; id < numThreads; id++)
	{

		// create threads
		args[id].id = id;
		args[id].numThreads = numThreads;
		if (pthread_create(&(children[id]), // our handle for the child
						   NULL,			// attributes of the child
						   child,			// the function it should run
						   (void *)&args[id]) != 0)
		{
			return 10; // Added error handling/debug returns 10 if it could not create a thread
		}; // args to that function
	}
	printf("I am the parent (main) thread.\n");

	for (unsigned int id = 0; id < numThreads; id++)
	{

		pthread_join(children[id], NULL);
	}

	for (unsigned int id = 0; id < numThreads; id++)
	{
		printf("Thread #%u squared ID: %u\n", id, args[id].squareId);
	}

	free(args);		// deallocate args vector
	free(children); // deallocate array
	return 0;
}
