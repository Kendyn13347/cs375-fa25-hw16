// priority_transfer.c
// Build: gcc -pthread priority_transfer.c -o priority_transfer
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

typedef struct {
    pthread_mutex_t m;
    pthread_t owner;
    int owner_base_prio;   // owner's original/base priority
    int owner_eff_prio;    // owner's effective (possibly boosted) priority
} prio_mutex_t;

static void prio_mutex_init(prio_mutex_t* pm) {
    pthread_mutex_init(&pm->m, NULL);
    pm->owner = 0;
    pm->owner_base_prio = pm->owner_eff_prio = 0;
}

static void prio_mutex_unlock(prio_mutex_t* pm) {
    // On unlock, drop effective priority back to base.
    pm->owner_eff_prio = pm->owner_base_prio;
    pm->owner = 0;
    pthread_mutex_unlock(&pm->m);
}

// Trylock; if busy, "donate" caller_prio to current owner, then block.
static void prio_mutex_lock(prio_mutex_t* pm, int caller_prio) {
    if (pthread_mutex_trylock(&pm->m) == 0) {
        pm->owner = pthread_self();
        pm->owner_base_prio = caller_prio;
        pm->owner_eff_prio  = caller_prio;
        return;
    }
    // Already locked: simulate priority donation
    // (We can't query the real owner's thread priority, so we store it.)
    if (caller_prio > pm->owner_eff_prio) {
        pm->owner_eff_prio = caller_prio;
        fprintf(stderr,
                "[donate] T%lu donates prio %d to owner T%lu (eff=%d)\n",
                (unsigned long)pthread_self(), caller_prio,
                (unsigned long)pm->owner, pm->owner_eff_prio);
    }
    // Block until available; when we acquire, we become the owner.
    pthread_mutex_lock(&pm->m);
    pm->owner = pthread_self();
    pm->owner_base_prio = caller_prio;
    pm->owner_eff_prio  = caller_prio;
}

// ---- Accounts & transfer ----
typedef struct account {
    prio_mutex_t lock;
    int   balance;
    long  uuid;
} account_t;

typedef struct {
    account_t* donor;
    account_t* recipient;
    int amount;
    int thread_prio;   // simulated priority (higher = more important)
    const char* name;
} transfer_args_t;

// Busy work to exaggerate inversion windows
static void burn_cpu_ms(int ms) {
    usleep(ms * 1000);
}

// Always lock in ascending-uuid order + simulate donation while waiting.
static void transfer(account_t* donor, account_t* recipient, int amount, int thr_prio, const char* who) {
    account_t* first  = (donor->uuid < recipient->uuid) ? donor     : recipient;
    account_t* second = (donor->uuid < recipient->uuid) ? recipient : donor;

    // Acquire first lock
    prio_mutex_lock(&first->lock, thr_prio);
    // Simulate some work while holding first lock to create contention
    burn_cpu_ms(30);

    // Acquire second lock (donation may occur inside prio_mutex_lock)
    prio_mutex_lock(&second->lock, thr_prio);

    if (donor->balance < amount) {
        printf("[%s] Insufficient funds.\n", who);
    } else {
        donor->balance    -= amount;
        recipient->balance += amount;
        printf("[%s] Transferred %d from %ld to %ld (balances: %d, %d)\n",
               who, amount, donor->uuid, recipient->uuid, donor->balance, recipient->balance);
    }

    prio_mutex_unlock(&second->lock);
    prio_mutex_unlock(&first->lock);
}

static void* transfer_thread(void* arg) {
    transfer_args_t* a = (transfer_args_t*)arg;

    // Each thread does two transfers to amplify interaction.
    transfer(a->donor, a->recipient, a->amount, a->thread_prio, a->name);
    burn_cpu_ms(20);
    transfer(a->recipient, a->donor, a->amount / 2, a->thread_prio, a->name);
    return NULL;
}

int main(void) {
    account_t A, B;
    prio_mutex_init(&A.lock);
    prio_mutex_init(&B.lock);
    A.balance = 1000; A.uuid = 1;
    B.balance =  500; B.uuid = 2;

    transfer_args_t hi = { &A, &B, 200, /*prio*/ 90, "HIGH" };
    transfer_args_t lo = { &B, &A, 100, /*prio*/ 10, "LOW"  };

    pthread_t th_hi, th_lo;

    // Start LOW first so it grabs one lock and then blocks on the other.
    pthread_create(&th_lo, NULL, transfer_thread, &lo);
    burn_cpu_ms(5);
    pthread_create(&th_hi, NULL, transfer_thread, &hi);

    pthread_join(th_hi, NULL);
    pthread_join(th_lo, NULL);

    printf("Final balances: A=%d B=%d\n", A.balance, B.balance);
    return 0;
}
