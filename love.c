// love.c
#include <pthread.h>
#include <stdio.h>

pthread_mutex_t lock;
pthread_cond_t  cv;
int subaru = 0;

void* helper(void* arg) {
    pthread_mutex_lock(&lock);
    subaru += 1;                 // update the predicate under the lock
    pthread_cond_signal(&cv);    // signal AFTER changing the state
    pthread_mutex_unlock(&lock);
    return NULL;
}

int main(void) {
    pthread_t thread;

    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cv, NULL);

    // Lock before creating the thread to avoid any chance of a lost wakeup
    pthread_mutex_lock(&lock);
    pthread_create(&thread, NULL, helper, NULL);

    // Mesa semantics: wait in a WHILE loop guarding the predicate
    while (subaru != 1) {
        pthread_cond_wait(&cv, &lock);  // atomically releases & re-acquires the lock
    }

    printf("I love Emilia!\n");         // runs only after helper has incremented subaru
    pthread_mutex_unlock(&lock);

    pthread_join(thread, NULL);
    pthread_cond_destroy(&cv);
    pthread_mutex_destroy(&lock);
    return 0;
}
