// Just counting again, but with protected access to the counter.
// N is smaller than for "badcounter", because it gets too slow
// with the N == 2000000 (and there is nothing interesting to see - it works)
//
//
// Compile: gcc goodcounter.c -o goodcounter -lpthread
// Run: ./goodcounter

/**
 * 1. semaphore is initialized to 1 (when it reach 0, it will lock)
 * 2. thread 0 created and goes to sem_wait
 * 3. checks that there is a slot available (sem = 1)
 * 4. subtracts the slot (sem = 0)
 * 5. thread 0 allowed to modify target.
 * 6. thread 1 created and goes to sem_wait (sem = 0)
 * 7. thread 1 will wait (sleep/suspend) until there is a slot. 
 */

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#define P 4
#define N 200000

// make sure that the compiler doesn't
// optimise this into a register
volatile int target;

sem_t lock;

void *adderthread (void *arg)
{
  int i;

  for (i=0; i<N; i++) {
    sem_wait(&lock);
    target = target+1;
    sem_post(&lock);
  }
}

int main (int argc, char *argv[]) { 
  int i;
  pthread_t thread[P];

  target = 0;
  printf("Used %d threads, each making %d increments to a counter \n", P, N);

  sem_init(&lock, 0, 1); // semaphore_obj, share_with_other_processes, semaphore value
  
  // semaphore value set to 1

  for (i=0; i<P; i++) {
    pthread_create(&thread[i], NULL, adderthread, NULL);
  }

  for (i=0; i<P; i++) {
    pthread_join(thread[i], NULL);
  }


  printf("Final counter value was %d (should be %d)\n", target, P*N);

  exit(0);
}
  