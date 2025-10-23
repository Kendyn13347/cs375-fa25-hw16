// spacex_countdown.c
#include <pthread.h>
#include <stdio.h>

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cv   = PTHREAD_COND_INITIALIZER;
int n = 3;

void* counter(void* arg) {
    pthread_mutex_lock(&lock);
    while (n > 0) {
        printf("%d\n", n);
        n--;
        pthread_cond_signal(&cv);   // wake announcer in case it's waiting
        // keep the lock so prints are serialized; loop will re-check n
    }
    pthread_mutex_unlock(&lock);
    return NULL;
}

void* announcer(void* arg) {
    pthread_mutex_lock(&lock);
    while (n != 0) {                // wait until countdown reaches zero
        pthread_cond_wait(&cv, &lock);
    }
    printf("FALCON HEAVY TOUCH DOWN!\n");
    pthread_mutex_unlock(&lock);
    return NULL;
}

int main(void) {
    pthread_t t_counter, t_ann;

    // Start announcer first so it likely waits before first signal
    pthread_create(&t_ann,     NULL, announcer, NULL);
    pthread_create(&t_counter, NULL, counter,   NULL);

    pthread_join(t_counter, NULL);
    pthread_join(t_ann, NULL);
    return 0;
}
