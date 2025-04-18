// A BROKEN simple producer/consumer program using semaphores and threads

/**
 * 1. producer thread writes to global buffer data = 1
 * 2. consumer thread writes to local var total with value of global buffer data
 * = 0 (becoz race condition)
 * 3. consumer reads global buffer data while producer is modifying it
 */

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define SHARED 1 // semaphore will shared between processes

void *Producer(void *); // the two threads prototype declaration
void *Consumer(void *);

sem_t empty, full; // the global semaphores
int data;          // shared buffer
int numIters;

// main() :    read command line and create threads, then
//             print result when the threads have quit

int main(int argc, char *argv[]) {
  // thread ids and attributes
  pthread_t pid, cid;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setscope(
      &attr, PTHREAD_SCOPE_SYSTEM); // all threads from different processes will
                                    // compete with each other

  numIters = atoi(argv[1]);
  sem_init(&empty, SHARED, 1); // sem empty = 1
  sem_init(&full, SHARED, 0);  // sem full = 0
  //
  printf("running for %d iterations\n", numIters);
  printf("main started\n");
  pthread_create(&pid, &attr, Producer, NULL); // creates producer thread
  pthread_create(&cid, &attr, Consumer, NULL); // creates consumer thread
  pthread_join(pid, NULL); // wait for producer thread to end
  pthread_join(cid, NULL); // wait for consumer thread to end
  printf("main done\n");
}

// deposit 1, ..., numIters into the data buffer
void *Producer(void *arg) {
  int produced;
  printf("Producer created\n");
  for (produced = 1; produced <= numIters; produced++) {
    data = produced;
  }
}

// fetch numIters items from the buffer and sum them
void *Consumer(void *arg) {
  int total = 0, consumed;
  printf("Consumer created\n");
  for (consumed = 0; consumed < numIters; consumed++) {
    total = total + data;
  }
  printf("after %d iterations, the total is %d (should be %d)\n", numIters,
         total, numIters * (numIters + 1) / 2);
}
