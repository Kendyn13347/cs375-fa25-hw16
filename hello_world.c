// hello_sync.c
#include <pthread.h>
#include <stdio.h>

pthread_mutex_t lock;
pthread_cond_t  cv;
int hello = 0;

void* print_hello(void* arg) {
    pthread_mutex_lock(&lock);
    hello += 1;
    printf("First line (hello=%d)\n", hello);
    pthread_cond_signal(&cv);         // signal AFTER updating the state
    pthread_mutex_unlock(&lock);
    return NULL;
}

int main(void) {
    pthread_t thread;

    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cv, NULL);

    // Lock BEFORE creating the child to prevent a lost signal.
    pthread_mutex_lock(&lock);

    pthread_create(&thread, NULL, print_hello, NULL);

    // Wait in a loop to handle spurious wakeups.
    while (hello < 1) {
        pthread_cond_wait(&cv, &lock);  // atomically unlocks & waits, then re-locks
    }

    printf("Second line (hello=%d)\n", hello);
    pthread_mutex_unlock(&lock);

    pthread_join(thread, NULL);
    pthread_cond_destroy(&cv);
    pthread_mutex_destroy(&lock);
    return 0;
}
