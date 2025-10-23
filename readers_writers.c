// readers_writers.c
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

pthread_mutex_t lock;
pthread_cond_t  reader_cv, writer_cv;

int reader_count   = 0;  // number of active readers
int writer_waiting = 0;  // number of writers waiting
int writer_active  = 0;  // 1 if a writer holds the resource
int shared_data    = 0;

void* reader(void* arg) {
    // ENTRY
    pthread_mutex_lock(&lock);
    while (writer_waiting > 0 || writer_active) {   // writers have priority
        pthread_cond_wait(&reader_cv, &lock);
    }
    reader_count++;
    pthread_mutex_unlock(&lock);

    // CRITICAL SECTION (read)
    printf("Reader reads: %d\n", shared_data);
    usleep(50 * 1000);

    // EXIT
    pthread_mutex_lock(&lock);
    reader_count--;
    if (reader_count == 0)
        pthread_cond_signal(&writer_cv);           // let a waiting writer go
    pthread_mutex_unlock(&lock);
    return NULL;
}

void* writer(void* arg) {
    // ENTRY
    pthread_mutex_lock(&lock);
    writer_waiting++;
    while (reader_count > 0 || writer_active) {
        pthread_cond_wait(&writer_cv, &lock);
    }
    writer_waiting--;
    writer_active = 1;
    pthread_mutex_unlock(&lock);

    // CRITICAL SECTION (write)
    shared_data++;
    printf("Writer writes: %d\n", shared_data);
    usleep(60 * 1000);

    // EXIT
    pthread_mutex_lock(&lock);
    writer_active = 0;
    if (writer_waiting > 0) {
        pthread_cond_signal(&writer_cv);           // next writer first (priority)
    } else {
        pthread_cond_broadcast(&reader_cv);        // otherwise free all readers
    }
    pthread_mutex_unlock(&lock);
    return NULL;
}

int main(void) {
    pthread_t readers[3], writers[2];

    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&reader_cv, NULL);
    pthread_cond_init(&writer_cv, NULL);

    for (int i = 0; i < 3; ++i) pthread_create(&readers[i], NULL, reader, NULL);
    for (int i = 0; i < 2; ++i) pthread_create(&writers[i], NULL, writer, NULL);
    for (int i = 0; i < 3; ++i) pthread_join(readers[i], NULL);
    for (int i = 0; i < 2; ++i) pthread_join(writers[i], NULL);

    pthread_cond_destroy(&reader_cv);
    pthread_cond_destroy(&writer_cv);
    pthread_mutex_destroy(&lock);
    return 0;
}
