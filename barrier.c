// barrier.c
#include <pthread.h>
#include <stdio.h>

#define NUM_THREADS 4

pthread_mutex_t lock;
pthread_cond_t  cv;
int count = 0;
int generation = 0;   // increments each time the barrier opens

void barrier(void) {
    pthread_mutex_lock(&lock);

    int my_gen = generation;      // snapshot the current “sense”
    if (++count == NUM_THREADS) {
        // Last thread arrives: open the barrier for everyone
        count = 0;                // reset for next use
        generation++;             // flip sense
        pthread_cond_broadcast(&cv);
    } else {
        // Wait until the generation changes
        while (my_gen == generation) {
            pthread_cond_wait(&cv, &lock);
        }
    }

    pthread_mutex_unlock(&lock);
}

void* worker(void* arg) {
    int id = *(int*)arg;

    printf("Thread %d: Before barrier 1\n", id);
    barrier();
    printf("Thread %d: After  barrier 1\n", id);

    // Demonstrate reusability:
    printf("Thread %d: Before barrier 2\n", id);
    barrier();
    printf("Thread %d: After  barrier 2\n", id);

    return NULL;
}

int main(void) {
    pthread_t threads[NUM_THREADS];
    int ids[NUM_THREADS];

    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cv, NULL);

    for (int i = 0; i < NUM_THREADS; ++i) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, worker, &ids[i]);
    }
    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], NULL);
    }

    pthread_cond_destroy(&cv);
    pthread_mutex_destroy(&lock);
    return 0;
}
