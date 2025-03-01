// PPLS Exercise 1

/**
 * @details
 * In this exercise, I am suppose to develop the prefix sum algorithm, where in phase 1 all the threads will perform some work on their chunk, phase 2 will only be performed by thread 0 on the entire array, and phase 3 will be performed by other threads(excluding thread 0).
 * 
 * Because the algorithm has different phases, I decided to follow a monitor-like structure for synchronization between phases.
 * 
 * I also split the algorithm logic into 3 functions: phase_1, phase_2, phase_3.
 * 
 * Threads were created using the pthread library and in the first approach, all threads are passed the same function.
 * In the second approach, thread 0 is passed with a slightly different function while the other threads have the same function (will be explained below).
 * For the arguments passed, the data array pointer is passed to each thread, but a start and end index is used to identify the chunk for the thread.
 * 
 * My first approach is that I can use barriers after phase 1 and phase 2 to ensure all threads are synchronized. (in the function parallelprefixsum_standard)
 * 
 * As the program prepares to enter phase 1, no check is required since all threads will be used to process their respective chunks.
 * 
 * As the program prepares to enter phase 2, it should check if the thread is id == 0 and then thread 0 should execute phase 2 logic and go to the next barrier, while other threads who failed the check should already be waiting in the next barrier.
 * 
 * The same goes for phase 3, where a check is applied on the thread whose id != 0 to allow other threads (excluding thread 0) to proceed with phase 3 logic. 
 * 
 * Upon further thought, I wonder if I can combine this logic so that only 1 modified barrier is required.
 * 
 * In this second approach (in function parallelprefixsum),
 * 
 * I want to allow thread 0 to perform the phase 2 task while all other threads are waiting in the condition queue. In this way, I only need to perform 1 synchronization for the whole algorithm, as thread 0 should be the one to initiate the broadcast to wake up all threads.
 * 
 * This approach made me realize that there is a potential flaw if I were to implement it as it is. I need to somehow guarantee that thread 0 is the last thread entering the monitor. For example, if thread 0 acquires the lock for the monitor before all other threads have been added to the wait condition queue, then once thread 0 completes and broadcasts to wake up all threads in the condition queue, some threads will get stucked in the wait condition queue, because they only got sent to the wait condition queue after thread 0 completes.
 * 
 * To resolve this issue, I modified the monitor to have 2 wait condition queues. If thread 0 arrives first, and some other threads have yet to arrive, thread 0 should be placed in a wait condition queue (worker0_must_be_last_thread_cond). 
 * 
 * If some other thread arrives (not the last thread), it will be placed in another wait condition queue (barrier_cond).
 * 
 * Then once the last thread arrives, it will broadcast and wake up thread 0 before also being placed in the wait condition queue (barrier_cond). This ensures that thread 0 is ready to execute once all other threads are in the condition queue.
 * 
 * Thread 0 is now able to execute phase 2, and once it completes it will broadcast and wakeup all other threads and have no more work to do.
 *  
 * The other threads will then execute phase 3 which completes the algorithm.
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

#define SHOWDATA 1
#define NITEMS 120
#define NTHREADS 25

void barrier_init();
void* other_threads(void* arg);
void* thread_0(void* arg);
void* thread_standard(void* arg);
void phase_1();
void phase_2();
void phase_3();
void barrier();

// Print a helpful message followed by the contents of an array
// Controlled by the value of SHOWDATA, which should be defined
// at compile time. Useful for debugging.
void showdata (char *message,  int *data,  int n) {
  int i; 

if (SHOWDATA) {
    printf ("%s", message);
    for (i=0; i<n; i++ ){
     printf (" %d", data[i]);
    }
    printf("\n");
  }
}

// Check that the contents of two integer arrays of the same length are equal
// and return a C-style boolean
int checkresult (int* correctresult,  int *data,  int n) {
  int i; 

  for (i=0; i<n; i++ ){
    if (data[i] != correctresult[i]) return 0;
  }
  return 1;
}

// Compute the prefix sum of an array **in place** sequentially
void sequentialprefixsum (int *data, int n) {
  int i;

  for (i=1; i<n; i++ ) {
    data[i] = data[i] + data[i-1];
  }
}

typedef struct worker_params {
  int worker_id;
  int start; // chunk start
  int end; // chunk end
  int size;
  int *data; // whole array
 } worker_params;

struct BarrierData {
  pthread_mutex_t barrier_mutex;
  pthread_cond_t barrier_cond;
  int nthread; // Number of threads that have reached this round of the barrier
  int round;   // Barrier round id
} bstate;

pthread_cond_t worker0_must_be_last_thread_cond;

void parallelprefixsum (int *data, int n) {
  // thread creation, size of array wouldn't change over time
  pthread_t threads[NTHREADS];
  worker_params args[NTHREADS];
  int chunk_slice = NITEMS / NTHREADS; // 10 / 4 = 2 
  int chunk_first_index = 0;

  barrier_init(); // initialize reusable barrier
  pthread_cond_init(&worker0_must_be_last_thread_cond, NULL);

  for (int id = 0; id < NTHREADS; id++) {
    // initialize worker params
    args[id].start = chunk_first_index; // 0
    int next_chunk_first_index = chunk_first_index + chunk_slice;

    // last thread takes the surplus
    if (id == NTHREADS - 1) {
      next_chunk_first_index = NITEMS;
    }
    args[id].end = next_chunk_first_index; // 1
    args[id].size = next_chunk_first_index - chunk_first_index;
    args[id].data = data;
    args[id].worker_id = id;
    chunk_first_index = next_chunk_first_index;

    if (id == 0) {
      pthread_create(&threads[id], NULL, thread_0, (void *) &args[id]);
    } else {
      pthread_create(&threads[id], NULL, other_threads, (void *) &args[id]);
    }

  }

  for (int id = 0; id < NTHREADS; id++) {
    pthread_join(threads[id], NULL);
  }

}

void parallelprefixsum_standard (int *data, int n) {
  // thread creation, size of array wouldn't change over time
  pthread_t threads[NTHREADS];
  worker_params args[NTHREADS];
  int chunk_slice = NITEMS / NTHREADS; // 10 / 4 = 2 
  int chunk_first_index = 0;

  barrier_init(); // initialize reusable barrier
  pthread_cond_init(&worker0_must_be_last_thread_cond, NULL);

  for (int id = 0; id < NTHREADS; id++) {
    // initialize worker params
    args[id].start = chunk_first_index; // 0
    int next_chunk_first_index = chunk_first_index + chunk_slice;

    // last thread takes the surplus
    if (id == NTHREADS - 1) {
      next_chunk_first_index = NITEMS;
    }
    args[id].end = next_chunk_first_index; // 1
    args[id].size = next_chunk_first_index - chunk_first_index;
    args[id].data = data;
    args[id].worker_id = id;
    chunk_first_index = next_chunk_first_index;

    pthread_create(&threads[id], NULL, thread_standard, (void *) &args[id]);
    

  }

  for (int id = 0; id < NTHREADS; id++) {
    pthread_join(threads[id], NULL);
  }

}

void* thread_standard(void* arg) {
  worker_params *worker_info = (worker_params *) arg;

  phase_1(worker_info);
  barrier();
  if (worker_info->worker_id == 0) {
    phase_2(worker_info);
  }
  barrier();
  if (worker_info->worker_id != 0) {
    phase_3(worker_info);
  }

}

void* thread_0(void* arg) {
  worker_params *worker_info = (worker_params *) arg;

  phase_1(worker_info);
  // phase 2, needs to be the last to hold the lock
  pthread_mutex_lock(&bstate.barrier_mutex);
  bstate.nthread += 1;
  if (bstate.nthread != NTHREADS) { // if this isn't the last thread to complete, it should wait 
    pthread_cond_wait(&worker0_must_be_last_thread_cond, &bstate.barrier_mutex);
  }

  phase_2(worker_info);

  bstate.round += 1; // to indicate that other threads can exit the while condition after waking up
  pthread_cond_broadcast(&bstate.barrier_cond); // wakes up rest of the threads once work is done
  pthread_mutex_unlock(&bstate.barrier_mutex); // worker 0 thread is done

}

void* other_threads(void* arg) {
  worker_params *worker_info = (worker_params *) arg;

  phase_1(worker_info);

  // custom barrier to also include waking up another condition queue containing worker_0
  pthread_mutex_lock(&bstate.barrier_mutex);
  bstate.nthread += 1;
  if (bstate.nthread == NTHREADS) { // this is the last thread to enter
    pthread_cond_broadcast(&worker0_must_be_last_thread_cond); // wake up worker 0

  }
  int current_round = bstate.round;
  do
  {
    pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
  } while (bstate.round == current_round);
  pthread_mutex_unlock(&bstate.barrier_mutex);

  // phase 3
  phase_3(worker_info);

}

// perform the prefix sum from the 2nd to last element of the chunk.
void phase_1(worker_params *worker_info) {
  for (int i = worker_info->start+1; i < worker_info->end; i++) {
    worker_info->data[i] = worker_info->data[i] + worker_info->data[i-1];
  }
}
void phase_2(worker_params *worker_info) {
  int slice = worker_info->size;
  int chunk_last_index = slice - 1;
  for (int i = 1; i < NTHREADS; i++) // NTHREADS - 1 chunks since first chunk doesn't need to update
  {
    int cur_chunk_value = (worker_info->data)[chunk_last_index]; // eg. chunk 0 value
    int next_chunk_index =chunk_last_index + slice;
    int next_chunk_value =  (worker_info->data)[next_chunk_index]; // eg. chunk 1 value

    if (i == NTHREADS - 1) { // last chunk
      next_chunk_index = NITEMS - 1;
      next_chunk_value = (worker_info->data)[next_chunk_index]; // last item in array
    }

    int new_next_chunk_value = cur_chunk_value + next_chunk_value; // eg. compute chunk 1 value with chunk 0
    worker_info->data[next_chunk_index] = new_next_chunk_value; // updates chunk 1
    chunk_last_index = next_chunk_index;
    // showdata("data array [during phase 2]: ", worker_info->data, NITEMS);
  }

}
void phase_3(worker_params *worker_info) {
  int cur_chunk_start_idx = worker_info->start;
  int prev_chunk_last_val = worker_info->data[cur_chunk_start_idx - 1];

  // all other threads need to compute from their first chunk index to the second last index.
  for (int i = cur_chunk_start_idx; i < worker_info->end - 1; i++) // omit the last indexed value as it was calculated in the second phase
  {
    worker_info->data[i] = worker_info->data[i] + prev_chunk_last_val;
  }
  // showdata("data array [during phase 3]: ", worker_info->data, NITEMS);
}

void barrier_init() {
  pthread_mutex_init(&bstate.barrier_mutex, NULL);
  pthread_cond_init(&bstate.barrier_cond, NULL);
  bstate.nthread = 0;
}


void barrier() {
  // only 1 thread can come into the barrier at a time
  // if not they will be blocked here
  pthread_mutex_lock(&bstate.barrier_mutex);
  bstate.nthread += 1;

  if (bstate.nthread == NTHREADS) {
    bstate.round += 1;
    bstate.nthread = 0;
    pthread_cond_broadcast(&bstate.barrier_cond); // wakes up all threads waiting
  } else {
    int current_round = bstate.round;

    do
    {
      pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
    } while (bstate.round == current_round); // prevents spurious wakeups
  }
  pthread_mutex_unlock(&bstate.barrier_mutex);
}




int main (int argc, char* argv[]) {

  int *arr1, *arr2, i;

  // Check that the compile time constants are sensible
  if ((NITEMS>10000000) || (NTHREADS>32)) {
    printf ("So much data or so many threads may not be a good idea! .... exiting\n");
    exit(EXIT_FAILURE);
  }

  // Create two copies of some random data
  arr1 = (int *) malloc(NITEMS*sizeof(int));
  arr2 = (int *) malloc(NITEMS*sizeof(int));
  srand((int)time(NULL));
  for (i=0; i<NITEMS; i++) {
     arr1[i] = arr2[i] = rand()%5;
  }
  showdata ("initial data          : ", arr1, NITEMS);

  // Calculate prefix sum sequentially, to check against later on
  sequentialprefixsum (arr1, NITEMS);
  showdata ("sequential prefix sum : ", arr1, NITEMS);

  // Calculate prefix sum in parallel on the other copy of the original data
  parallelprefixsum (arr2, NITEMS);
  // parallelprefixsum_standard(arr2, NITEMS);
  showdata ("parallel prefix sum   : ", arr2, NITEMS);

  // Check that the sequential and parallel results match
  if (checkresult(arr1, arr2, NITEMS))  {
    printf("Well done, the sequential and parallel prefix sum arrays match.\n");
  } else {
    printf("Error: The sequential and parallel prefix sum arrays don't match.\n");
  }

  free(arr1); free(arr2);
  return 0;
}
