// baking.c
#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

int  numBatterInBowl = 0;
int  numEggInBowl    = 0;
bool readyToEat      = false;

pthread_mutex_t lock;
pthread_cond_t  needIngredients;  // wake ingredient threads to add more
pthread_cond_t  readyToBake;      // wake heater when bowl has 1 batter + 2 eggs
pthread_cond_t  startEating;      // wake eater when cake is ready

static void addBatter(void) { numBatterInBowl += 1; }
static void addEgg(void)    { numEggInBowl    += 1; }
static void heatBowl(void)  { readyToEat = true; numBatterInBowl = 0; numEggInBowl = 0; }
static void eatCake(void)   { readyToEat = false; }

void* batterAdder(void* arg) {
    pthread_mutex_lock(&lock);
    while (1) {
        // only one batter per cake, and don't add during/after baking
        while (numBatterInBowl != 0 || readyToEat) {
            pthread_cond_wait(&needIngredients, &lock);
        }
        addBatter();
        printf("[Batter] +1 (batter=%d, eggs=%d)\n", numBatterInBowl, numEggInBowl);
        pthread_cond_signal(&readyToBake);  // heater may proceed if eggs ready
        pthread_mutex_unlock(&lock);
        pthread_mutex_lock(&lock);
    }
}

void* eggBreaker(void* arg) {
    pthread_mutex_lock(&lock);
    while (1) {
        // need exactly two eggs; don't add during/after baking
        while (numEggInBowl >= 2 || readyToEat) {
            pthread_cond_wait(&needIngredients, &lock);
        }
        addEgg();
        printf("[Egg] +1 (batter=%d, eggs=%d)\n", numBatterInBowl, numEggInBowl);
        pthread_cond_signal(&readyToBake);  // heater may proceed if batter present
        pthread_mutex_unlock(&lock);
        pthread_mutex_lock(&lock);
    }
}

void* bowlHeater(void* arg) {
    pthread_mutex_lock(&lock);
    while (1) {
        // bake only when ingredients are ready and not already eating
        while (numBatterInBowl < 1 || numEggInBowl < 2 || readyToEat) {
            pthread_cond_wait(&readyToBake, &lock);
        }
        printf("[Heater] Baking...\n");
        heatBowl();
        printf("[Heater] Cake ready! (batter=%d, eggs=%d)\n", numBatterInBowl, numEggInBowl);
        pthread_cond_signal(&startEating);  // let eater proceed
        pthread_mutex_unlock(&lock);
        pthread_mutex_lock(&lock);
    }
}

void* cakeEater(void* arg) {
    pthread_mutex_lock(&lock);
    while (1) {
        while (!readyToEat) {
            pthread_cond_wait(&startEating, &lock);
        }
        printf("[Eater] Eating cake!\n");
        eatCake();
        printf("[Eater] Done. Requesting more ingredients.\n");
        // wake ALL ingredient threads to start the next cycle
        pthread_cond_broadcast(&needIngredients);
        pthread_mutex_unlock(&lock);
        pthread_mutex_lock(&lock);
    }
}

int main(void) {
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&needIngredients, NULL);
    pthread_cond_init(&readyToBake, NULL);
    pthread_cond_init(&startEating, NULL);

    pthread_t batter, egg1, egg2, heater, eater;
    pthread_create(&batter, NULL, batterAdder, NULL);
    pthread_create(&egg1,  NULL, eggBreaker, NULL);
    pthread_create(&egg2,  NULL, eggBreaker, NULL);
    pthread_create(&heater,NULL, bowlHeater,  NULL);
    pthread_create(&eater, NULL, cakeEater,   NULL);

    // keep the process alive
    while (1) sleep(1);
    return 0;
}
