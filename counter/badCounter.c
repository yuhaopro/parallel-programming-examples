
// Just counting, but with no protection on the concurrent
// additions. We need a big N for the effect to become apparent.
//
// Compile: gcc badcounter.c -o badcounter -lpthread
// Run: ./badcounter

/**
 * 1. Thread 0 increments target from 0 to 1
 * 2. While thread 0 increments, thread 1 reads the target value (still 0) also increments by 1
 * 3. thread 1 output target as 1
 * 4. thread 0 also output target as 1
 */


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#define P 4
#define N 20000000

// make sure that the compiler doesn't
// optimise this into a register -> which means it doesn't get cached.
volatile int target;

void *adderthread (void *arg)
{
  int i;

  for (i=0; i<N; i++) {
    target = target+1;
  }
}

int main (int argc, char *argv[]) { 
  int i;
  pthread_t thread[P];

  target = 0;
  printf("Used %d threads, each making %d increments to a counter \n", P, N);
  for (i=0; i<P; i++) {
    pthread_create(&thread[i], NULL, adderthread, NULL);
  }

  for (i=0; i<P; i++) {
    pthread_join(thread[i], NULL);
  }

  printf("Final counter value was %d (should be %d)\n", target, P*N);
  exit(0);
}
 