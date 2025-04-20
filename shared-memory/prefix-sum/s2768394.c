// PPLS Exercise 1

/**
 * @author Yuhao Zhu
 * 
 * # Overview
 * This program uses a monitor to achieve synchronization betweeen threads for performing the prefix sum algorithm.
 * A global struct variable is created representing the monitor.
 * The entry point to the monitor is represented by the barrier function.
 * I chose the monitor for condition synchronization here because I feel that it is much more intuitive due to the clear transition between states.
 * 
 * # Phase 1
 * In this phase, all threads are to perform the prefix sum on their own corresponding chunks.
 * The chunks are represented by a start and end index.
 * Since these threads will not interfere with each other chunks, mutual exclusion is not required on the shared prefix sum array.
 * 
 * Once a thread finishes processing their chunk, the thread will first attempt to acquire the mutex to execute code within the monitor.
 * If the thread is not the last thread to arrive, it will be instead placed in the condition queue.
 * If the last thread arrives, it will wake up all other threads in the condition queue.
 * This ensures all threads have arrived, and can proceed with phase 2.
 * 
 * # Phase 2 
 * In this phase, only thread 0 needs to perform the prefix sum calculation for the highest index in each chunk.
 * This means other threads should ignore this task and wait at the next synchronization point.
 * So I re-used the monitor to act as the next synchronization point. 
 * 
 * # Phase 3
 * In this phase, thread 0 has no more work left to do, while other threads will need to recompute their chunk by referencing the last value of the previous chunk. Therefore, there is no more synchronization required in this phase.
 */

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>

#define SHOWDATA 1
#define NITEMS 10000
#define NTHREADS 25

typedef struct worker_params {
  int worker_id;
  int start; // chunk start
  int end;   // chunk end
  int size;
  int *data; // whole array
} worker_params;

struct BarrierData {
  pthread_mutex_t barrier_mutex;
  pthread_cond_t barrier_cond;
  int nthread; // volatile not required as mutex acts as memory barrier
  int round;   // volatile not required as mutex acts as memory barrier
} bstate;

void barrier_init();
void *thread(void *arg);
void phase_1(worker_params *worker_info);
void phase_2(worker_params *worker_info);
void phase_3(worker_params *worker_info);
void barrier();

// Print a helpful message followed by the contents of an array
// Controlled by the value of SHOWDATA, which should be defined
// at compile time. Useful for debugging.
void showdata(char *message, int *data, int n) {
  int i;

  if (SHOWDATA) {
    printf("%s", message);
    for (i = 0; i < n; i++) {
      printf(" %d", data[i]);
    }
    printf("\n");
  }
}

// Check that the contents of two integer arrays of the same length are equal
// and return a C-style boolean
int checkresult(int *correctresult, int *data, int n) {
  int i;

  for (i = 0; i < n; i++) {
    if (data[i] != correctresult[i])
      return 0;
  }
  return 1;
}

// Compute the prefix sum of an array **in place** sequentially
void sequentialprefixsum(int *data, int n) {
  int i;

  for (i = 1; i < n; i++) {
    data[i] = data[i] + data[i - 1];
  }
}

void parallelprefixsum(int *data, int n) {
  pthread_t threads[NTHREADS];
  worker_params args[NTHREADS];
  int chunk_slice = NITEMS / NTHREADS;
  int chunk_first_index = 0;

  // initialize barrier mutex and condition variables.
  barrier_init();

  for (int id = 0; id < NTHREADS; id++) {
    // initialize worker params
    args[id].start = chunk_first_index;
    int next_chunk_first_index = chunk_first_index + chunk_slice;

    // last thread takes the surplus
    if (id == NTHREADS - 1) {
      next_chunk_first_index = NITEMS;
    }
    args[id].end = next_chunk_first_index;
    args[id].size = next_chunk_first_index - chunk_first_index;
    args[id].data = data;
    args[id].worker_id = id;
    chunk_first_index = next_chunk_first_index;

    pthread_create(&threads[id], NULL, thread, (void *)&args[id]);
  }

  for (int id = 0; id < NTHREADS; id++) {
    pthread_join(threads[id], NULL);
  }
}

void *thread(void *arg) {
  worker_params *worker_info = (worker_params *)arg;

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

// perform the prefix sum from the 2nd to last element of the chunk.
void phase_1(worker_params *worker_info) {
  for (int i = worker_info->start + 1; i < worker_info->end; i++) {
    worker_info->data[i] = worker_info->data[i] + worker_info->data[i - 1];
  }
}
void phase_2(worker_params *worker_info) {
  int slice = worker_info->size;
  int chunk_last_index = slice - 1;
  for (int i = 1; i < NTHREADS; i++) {
    int cur_chunk_value = (worker_info->data)[chunk_last_index];
    int next_chunk_index = chunk_last_index + slice;
    int next_chunk_value = (worker_info->data)[next_chunk_index];

    if (i == NTHREADS - 1) { // last chunk
      next_chunk_index = NITEMS - 1;
      next_chunk_value = (worker_info->data)[next_chunk_index];
    }

    int new_next_chunk_value = cur_chunk_value + next_chunk_value;
    worker_info->data[next_chunk_index] = new_next_chunk_value;
    chunk_last_index = next_chunk_index;
  }
}

void phase_3(worker_params *worker_info) {
  int cur_chunk_start_idx = worker_info->start;
  int prev_chunk_last_val = worker_info->data[cur_chunk_start_idx - 1];

  for (int i = cur_chunk_start_idx; i < worker_info->end - 1; i++) {
    worker_info->data[i] = worker_info->data[i] + prev_chunk_last_val;
  }
}

void barrier_init() {
  pthread_mutex_init(&bstate.barrier_mutex, NULL);
  pthread_cond_init(&bstate.barrier_cond, NULL);
  bstate.nthread = 0;
}

void barrier() {
  // only 1 thread can come into the barrier at a time.
  // if not they will be blocked here.
  pthread_mutex_lock(&bstate.barrier_mutex);
  bstate.nthread += 1;

  // check if this is the last thread.
  // if it is, all threads have arrived.
  if (bstate.nthread == NTHREADS) {
    bstate.round += 1;
    bstate.nthread = 0;
    pthread_cond_broadcast(
        &bstate.barrier_cond); // wakes up all threads waiting.
  } else {
    // this is not the last thread.
    // thread should join the condition queue.
    int current_round = bstate.round;

    do {
      pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
    } while (bstate.round == current_round); // prevents spurious wakeups
  }

  pthread_mutex_unlock(&bstate.barrier_mutex);
}

int main(int argc, char *argv[]) {

  int *arr1, *arr2, i;

  // Check that the compile time constants are sensible
  if ((NITEMS > 10000000) || (NTHREADS > 32)) {
    printf("So much data or so many threads may not be a good idea! .... "
           "exiting\n");
    exit(EXIT_FAILURE);
  }

  // Create two copies of some random data
  arr1 = (int *)malloc(NITEMS * sizeof(int));
  arr2 = (int *)malloc(NITEMS * sizeof(int));
  srand((int)time(NULL));
  for (i = 0; i < NITEMS; i++) {
    arr1[i] = arr2[i] = rand() % 5;
  }
  showdata("initial data          : ", arr1, NITEMS);

  // Calculate prefix sum sequentially, to check against later on
  sequentialprefixsum(arr1, NITEMS);
  showdata("sequential prefix sum : ", arr1, NITEMS);

  // Calculate prefix sum in parallel on the other copy of the original data
  parallelprefixsum(arr2, NITEMS);
  // parallelprefixsum_standard(arr2, NITEMS);
  showdata("parallel prefix sum   : ", arr2, NITEMS);

  // Check that the sequential and parallel results match
  if (checkresult(arr1, arr2, NITEMS)) {
    printf("Well done, the sequential and parallel prefix sum arrays match.\n");
  } else {
    printf(
        "Error: The sequential and parallel prefix sum arrays don't match.\n");
  }

  free(arr1);
  free(arr2);
  return 0;
}
