// PPLS Exercise 1


/**
 * @bug
 * This code intends to synchronize phase 1 and phase 2 of the prefix sum algorithm using a monitor implementation with 2 condition queues. The code intends to have thread 0 be put in it's own condition queue if it is not the last thread to arrive after phase 1. If the other thread arrives last, it will wake up thread 0 to perform phase 2. The problem is each condition queue needs a spin lock to ensure it will not be affected by spurious wakeups. This is not possible to achieve with using 1 shared round variable to ensure both thread 0 and other threads are kept in their respective spin locks. Thus the code will fail if a spurious wakeup occurs.
 */



#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>

#define SHOWDATA 1
#define NITEMS 10000000
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

pthread_cond_t worker0_must_be_last_thread_cond;

void barrier_init();
void *thread(void *arg);
void phase_1(worker_params *worker_info);
void phase_2(worker_params *worker_info);
void phase_3(worker_params *worker_info);

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

  barrier_init(); // initialize reusable barrier
  pthread_cond_init(&worker0_must_be_last_thread_cond, NULL);

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

    // if (id == 0) {
    //   pthread_create(&threads[id], NULL, thread_0, (void *)&args[id]);
    // } else {
    //   pthread_create(&threads[id], NULL, other_threads, (void *)&args[id]);
    // }

    pthread_create(&threads[id], NULL, thread, (void *)&args[id]);
  }

  for (int id = 0; id < NTHREADS; id++) {
    pthread_join(threads[id], NULL);
  }
}

void *thread(void *arg) {
  worker_params *worker_info = (worker_params *)arg;
  phase_1(worker_info);

  pthread_mutex_lock(&bstate.barrier_mutex);
  bstate.nthread += 1;

  // if this is thread 0
  if (worker_info->worker_id == 0) {
    // if this is not the last thread
    if (bstate.nthread != NTHREADS) {
      // thread 0 gets put in condition queue 1
      int current_round = bstate.round;
      do
      {
        pthread_cond_wait(&worker0_must_be_last_thread_cond, &bstate.barrier_mutex); 
      } while (bstate.round == current_round);
    }
    // thread 0 gets woken up and perform phase 2 work.
    phase_2(worker_info);

    // increment bstate round so that other threads can be woken up.
    bstate.round += 1;
    pthread_cond_broadcast(&bstate.barrier_cond);

  } else {
    if (bstate.nthread == NTHREADS) {
      // thread x arrives and is last thread.
      // wakes up thread 0 to start phase 2.
      // adds to round so that thread 0 can break out of while loop.
      bstate.round += 1;
      pthread_cond_broadcast(
          &worker0_must_be_last_thread_cond);
    }

    // BIG PROBLEM --> the round variable no longer works, because now it must control the spurious wakeup for both thread 0 and other threads.
    int current_round = bstate.round;
    do {
      pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
    } while (bstate.round == current_round);
  }
  pthread_mutex_unlock(&bstate.barrier_mutex);

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
  for (int i = 1; i < NTHREADS;
       i++) // NTHREADS - 1 chunks since first chunk doesn't need to update
  {
    int cur_chunk_value =
        (worker_info->data)[chunk_last_index]; // eg. chunk 0 value
    int next_chunk_index = chunk_last_index + slice;
    int next_chunk_value =
        (worker_info->data)[next_chunk_index]; // eg. chunk 1 value

    if (i == NTHREADS - 1) { // last chunk
      next_chunk_index = NITEMS - 1;
      next_chunk_value =
          (worker_info->data)[next_chunk_index]; // last item in array
    }

    int new_next_chunk_value =
        cur_chunk_value +
        next_chunk_value; // eg. compute chunk 1 value with chunk 0
    worker_info->data[next_chunk_index] =
        new_next_chunk_value; // updates chunk 1
    chunk_last_index = next_chunk_index;
    // showdata("data array [during phase 2]: ", worker_info->data, NITEMS);
  }
}
void phase_3(worker_params *worker_info) {
  int cur_chunk_start_idx = worker_info->start;
  int prev_chunk_last_val = worker_info->data[cur_chunk_start_idx - 1];

  // all other threads need to compute from their first chunk index to the
  // second last index.
  for (int i = cur_chunk_start_idx; i < worker_info->end - 1;
       i++) // omit the last indexed value as it was calculated in the second
            // phase
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
    pthread_cond_broadcast(
        &bstate.barrier_cond); // wakes up all threads waiting
  } else {
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
