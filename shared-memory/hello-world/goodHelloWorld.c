// A well behaved version of "Hello World".
// The random sleep() just causes different 
// interleavings, but each thread always gets
// a distinct value of i.
//
// Compile: gcc hello.c -o hello -lpthread
// Run: ./hello

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define P 10

void *sayhello (void *id)
{  
  sleep(rand()%5);
  printf("Hello from thread %ld\n", (long) id);
}

int main (int argc, char *argv[]) { 
  long i;
  pthread_t thread[P];
  time_t t;

  t = time(NULL);  // seed the random number
  srand((int) t);  // generator from outside

  printf("Hello from the main thread\n");
  for (i=0; i<P; i++) {
    pthread_create(&thread[i], NULL, sayhello, (void *)i); // i's value is passed to arg but casted to void*
  }

  for (i=0; i<P; i++) {
    pthread_join(thread[i], NULL);
  }
  printf("Goodbye from the main thread\n");
  exit(0);
}
