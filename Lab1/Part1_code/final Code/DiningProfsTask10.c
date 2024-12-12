#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>

// Shared Variables

pthread_mutex_t chopssticks[5] = PTHREAD_MUTEX_INITIALIZER;

struct threadArgs
{
    unsigned int id;
    unsigned int seat;
};

void waitRandom(int option)
{

    if (option == 1) // wait 1 - 5 s
    {
        unsigned int seed = (unsigned int)(time(NULL) ^ pthread_self()); // To avoid threadss using the same RNG and not getting diffrent wait times
        int wait_time_ms = (rand_r(&seed) % 4001) + 1000;
        usleep(wait_time_ms * 1000); // Sleep for the random duration
    }
    else if (option == 2)
    {
        // wait 2 - 8
        unsigned int seed = (unsigned int)(time(NULL) ^ pthread_self()); // To avoid threadss using the same RNG and not getting diffrent wait times
        int wait_time_ms = (rand_r(&seed) % 7001) + 2000;
        usleep(wait_time_ms * 1000);
    }
    else
    {
        // wait 5 - 10s
        unsigned int seed = (unsigned int)(time(NULL) ^ pthread_self()); // To avoid threadss using the same RNG and not getting diffrent wait times
        int wait_time_ms = (rand_r(&seed) % 9001) + 5000;
        usleep(wait_time_ms * 1000);
    }
}

bool Chopstick(int seat, int id)
{
    // Try left return false if fail

    int left = seat % 5;
    int right = seat - 1;

    waitRandom(1); // wait 1-5s
    printf("Thread#%u Waiting and ready to grab left, professor at seat %u\n", id, seat);

    if (pthread_mutex_lock(&chopssticks[left]) != 0)
    {
        printf("Thread#%u Didn't left chopstick, professor at seat %u\n", id, seat);
        // failed to grabe left
        return false;
    }
    else
    {
        // found left chopstick
        //  wait and Try right return false if fail

        printf("Thread#%u got left chopstick starting wait, professor at seat %u\n", id, seat);
        waitRandom(2); // wait 2-8s

        if (pthread_mutex_lock(&chopssticks[right]) != 0)
        {
            printf("Thread#%u Didn't get right chopstick, professor at seat %u\n", id, seat);
            // failed to grab right
            pthread_mutex_unlock(&chopssticks[left]); // release left
            return false;
        }

        // if ok, wait/eat then return true

        printf("Thread#%u Got both starting to eat nom nom nom, professor at seat %u\n", id, seat);
        waitRandom(3); // both mutexes locked wait about 5-10s

        pthread_mutex_unlock(&chopssticks[left]);
        pthread_mutex_unlock(&chopssticks[right]);

        return true;
    }
}

void *professor(void *params)
{
    struct threadArgs *args = (struct threadArgs *)params;

    unsigned int seat = args->seat;
    unsigned int id = args->id;

    bool flag = false;

    while (flag != true)
    {
        if (Chopstick(seat, id) == true)
        {

            break;
        }
    }

    printf("Professor Ate, seat : %u\n", seat);

    return NULL;
}

int main(int argc, char **argv)
{
    pthread_t professors[5];
    struct threadArgs args[5];
    unsigned int numProfs = 0;
    // get desired # of threads
    if (argc > 1)
    {
        numProfs = atoi(argv[1]);
    }

    for (unsigned int index = 0; index < numProfs; index++)
    {

        // create threads
        args[index].id = index;
        args[index].seat = index + 1;

        pthread_create(&(professors[index]), // our handle for the child
                       NULL,                 // attributes of the child
                       professor,            // the function it should run
                       (void *)&args[index]);
    }

    for (unsigned int index = 0; index < numProfs; index++)
    {
        pthread_join(professors[index], NULL);
    }

    pthread_mutex_destroy(chopssticks);

    return 0;
}
