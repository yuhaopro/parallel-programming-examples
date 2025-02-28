// This version of "Hello World" doesn't behave! The problem
// is that the "id" parameter is passed to the thread by 
// reference (ie as a pointer to its true location. But 
// that location is shared, and is being updated all the
// time by the loops in the main function. Thus
// the value actually picked up by sayhello depends upon 
// the interleaving of these events. The sleep() just
// encourages the effect.
//
// Compile: gcc nasty.c -o nasty -lpthread
// Run: ./nasty

/**
 * 1. main thread create thread 0
 * 2. address of id is passed to thread 0
 * 3. thread 0 sleeps
 * 4. id increment in outer loop
 * 5. thread 0 wakes up
 * 6. thread 0 accesses id value which will be 1.
 * 
 * # another situation
 * 1. thread 0 about to read id
 * 2. main thread modifies id while thread 0 is reading
 * 3. undefined behaviour -> ends up printing 0 -> cpu caching
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define P 10

void *sayhello (void *id)
{
  sleep(rand()%5);
  printf("Hello from thread %ld\n", *(long *)id);
}


int main (int argc, char *argv[]) { 
  long i;
  time_t t;
  pthread_t thread[P];

  t = time(NULL);  // seed the random number
  srand((int) t);  // geberator from outside

  printf("Hello from the main thread\n");
  for (i=0; i<P; i++) {
    pthread_create(&thread[i], NULL, sayhello, &i); // last parameter contains arg to sayhello
  }

  for (i=0; i<P; i++) {
    pthread_join(thread[i], NULL);
  }
  printf("Goodbye from the main thread\n");
  exit(0);
}
  