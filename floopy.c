// floopy.c
#include <pthread.h>
#include <stdio.h>

typedef struct account {
    pthread_mutex_t lock;
    int   balance;
    long  uuid;
} account_t;

typedef struct {
    account_t* donor;
    account_t* recipient;
    int amount;
} transfer_args_t;

/* Lock-ordering transfer: always acquire locks in ascending uuid order. */
void transfer(account_t* donor, account_t* recipient, int amount) {
    account_t* first  = (donor->uuid < recipient->uuid) ? donor     : recipient;
    account_t* second = (donor->uuid < recipient->uuid) ? recipient : donor;

    pthread_mutex_lock(&first->lock);
    pthread_mutex_lock(&second->lock);

    if (donor->balance < amount) {
        printf("Insufficient funds.\n");
    } else {
        donor->balance    -= amount;
        recipient->balance += amount;
        printf("Transferred %d from account %ld to %ld\n",
               amount, donor->uuid, recipient->uuid);
    }

    pthread_mutex_unlock(&second->lock);
    pthread_mutex_unlock(&first->lock);
}

void* transfer_thread(void* arg) {
    transfer_args_t* a = (transfer_args_t*)arg;
    transfer(a->donor, a->recipient, a->amount);
    return NULL;
}

int main(void) {
    account_t acc1 = { PTHREAD_MUTEX_INITIALIZER, 1000, 1 };
    account_t acc2 = { PTHREAD_MUTEX_INITIALIZER,  500, 2 };

    transfer_args_t t1 = { &acc1, &acc2, 200 };  // A -> B
    transfer_args_t t2 = { &acc2, &acc1, 100 };  // B -> A

    pthread_t th1, th2;
    pthread_create(&th1, NULL, transfer_thread, &t1);
    pthread_create(&th2, NULL, transfer_thread, &t2);

    pthread_join(th1, NULL);
    pthread_join(th2, NULL);
    return 0;
}
