// thread_pool.c
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NUM_THREADS 4

typedef struct {
    void (*task)(int);
    int arg;
} Task;

typedef struct {
    Task* queue;
    int head, tail, count, size;
    int closed;                      // set when we stop accepting work
    pthread_mutex_t lock;
    pthread_cond_t  not_empty, not_full;
} ThreadSafeQueue;

/* ------------ Queue --------------- */
void queue_init(ThreadSafeQueue* q, int size) {
    q->queue = (Task*)malloc(sizeof(Task) * size);
    q->head = q->tail = q->count = 0;
    q->size = size;
    q->closed = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

void queue_destroy(ThreadSafeQueue* q) {
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
    free(q->queue);
}

void queue_close(ThreadSafeQueue* q) {
    pthread_mutex_lock(&q->lock);
    q->closed = 1;
    pthread_cond_broadcast(&q->not_empty);  // wake waiting workers
    pthread_cond_broadcast(&q->not_full);   // wake producers, if any
    pthread_mutex_unlock(&q->lock);
}

int queue_push(ThreadSafeQueue* q, Task task) {
    pthread_mutex_lock(&q->lock);
    while (!q->closed && q->count == q->size) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }
    if (q->closed) {                       // no longer accepting tasks
        pthread_mutex_unlock(&q->lock);
        return 0;
    }
    q->queue[q->tail] = task;
    q->tail = (q->tail + 1) % q->size;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
    return 1;
}

/* returns 1 if a task was popped, 0 if queue is closed and empty */
int queue_pop(ThreadSafeQueue* q, Task* task) {
    pthread_mutex_lock(&q->lock);
    while (q->count == 0 && !q->closed) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }
    if (q->count == 0 && q->closed) {
        pthread_mutex_unlock(&q->lock);
        return 0;                           // shutdown signal
    }
    *task = q->queue[q->head];
    q->head = (q->head + 1) % q->size;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return 1;
}

/* ------------ Thread pool workers --------------- */
void sample_task(int arg) {
    printf("Task executed with arg: %d\n", arg);
    usleep(50000); // simulate some work (~50ms)
}

void* worker(void* arg) {
    ThreadSafeQueue* q = (ThreadSafeQueue*)arg;
    Task t;
    while (queue_pop(q, &t)) {
        t.task(t.arg);
    }
    return NULL;
}

int main(void) {
    ThreadSafeQueue q;
    queue_init(&q, 16);

    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_create(&threads[i], NULL, worker, &q);
    }

    // Enqueue some work
    for (int i = 0; i < 20; ++i) {
        queue_push(&q, (Task){ sample_task, i });
    }

    // No more tasks: close the queue and join workers
    queue_close(&q);
    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], NULL);
    }
    queue_destroy(&q);
    return 0;
}
