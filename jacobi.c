//Adapted from Jacobi iteration using pthreads by Greg Andrews see
//
// http://www.cs.arizona.edu/people/greg/mpdbook/programs/jacobi.c
//
// Improved barrier implementation (addressing spurious wakeups) from
//
// https://stackoverflow.com/questions/73154724/creating-a-barrier-for-multi-thread
//
// Usage on DICE:
//    gcc -o jacobi jacobi.c -lpthread
//    ./jacobi gridSize numWorkers numIters
//

#define _REENTRANT
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/times.h>
#include <limits.h>
#define SHARED 1
#define MAXGRID 258   /* maximum grid size, including boundaries */
#define MAXWORKERS 4  /* maximum number of worker threads */

void *Worker(void *);
void InitializeGrids();
void barrier_init();
void barrier();

struct tms buffer;        /* used for timing */
clock_t start, finish;

int gridSize, numWorkers, numIters, stripSize;
double maxDiff[MAXWORKERS];
double grid1[MAXGRID][MAXGRID], grid2[MAXGRID][MAXGRID];


/* main() -- read command line, initialize grids, and create threads
             when the threads are done, print the results */

int main(int argc, char *argv[]) {
  /* thread ids and attributes */
  pthread_t workerid[MAXWORKERS];
  pthread_attr_t attr;
  long i, j;
  double maxdiff = 0.0;
  FILE *results;

  /* set global thread attributes */
  pthread_attr_init(&attr);
  pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

  /* read command line and initialize grids */
  gridSize = atoi(argv[1]);
  numWorkers = atoi(argv[2]);
  numIters = atoi(argv[3]);
  stripSize = gridSize/numWorkers;

  barrier_init();
  InitializeGrids();

  start = times(&buffer);
  /* create the workers, then wait for them to finish */
  for (i = 0; i < numWorkers; i++)
    pthread_create(&workerid[i], &attr, Worker, (void *) i);
  for (i = 0; i < numWorkers; i++)
    pthread_join(workerid[i], NULL);

  finish = times(&buffer);
  /* print the results */
  for (i = 0; i < numWorkers; i++)
    if (maxdiff < maxDiff[i])
      maxdiff = maxDiff[i];
  printf("number of iterations:  %d\nmaximum difference:  %e\n",
          numIters, maxdiff);
  printf("start:  %ld   finish:  %ld\n", start, finish);
  printf("elapsed time:  %ld\n", finish-start);
  results = fopen("results", "w");
  for (i = 1; i <= gridSize; i++) {
    for (j = 1; j <= gridSize; j++) {
      fprintf(results, "%f ", grid2[i][j]);
    }
    fprintf(results, "\n");
  }
}


/* Each Worker computes values in one strip of the grids.
   The main worker loop does two computations to avoid copying from
   one grid to the other.  */

void *Worker(void *arg) {
  long myid = (long) arg;
  double maxdiff, temp;
  int i, j, iters;
  int first, last;

  printf("worker %ld (pthread id %ld) has started\n", myid, pthread_self());

  /* determine first and last rows of my strip of the grids */
  first = myid*stripSize + 1;
  last = first + stripSize - 1;

  for (iters = 1; iters <= numIters; iters++) {
    /* update my points */
    for (i = first; i <= last; i++) {
      for (j = 1; j <= gridSize; j++) {
        grid2[i][j] = (grid1[i-1][j] + grid1[i+1][j] + 
                       grid1[i][j-1] + grid1[i][j+1]) * 0.25;
      }
    }
    barrier();
    /* update my points again */
    for (i = first; i <= last; i++) {
      for (j = 1; j <= gridSize; j++) {
        grid1[i][j] = (grid2[i-1][j] + grid2[i+1][j] +
               grid2[i][j-1] + grid2[i][j+1]) * 0.25;
      }
    }
    barrier();
  }
  /* compute the maximum difference in my strip and set global variable */
  maxdiff = 0.0;
  for (i = first; i <= last; i++) {
    for (j = 1; j <= gridSize; j++) {
      temp = grid1[i][j]-grid2[i][j];
      if (temp < 0)
        temp = -temp;
      if (maxdiff < temp)
        maxdiff = temp;
    }
  }
  maxDiff[myid] = maxdiff;
}

void InitializeGrids() {
  /* initialize the grids (grid1 and grid2)
     set boundaries to 1.0 and interior points to 0.0  */
  int i, j;
  for (i = 0; i <= gridSize+1; i++)
    for (j = 0; j <= gridSize+1; j++) {
      grid1[i][j] = 0.0;
      grid2[i][j] = 0.0;
    }
  for (i = 0; i <= gridSize+1; i++) {
    grid1[i][0] = 1.0;
    grid1[i][gridSize+1] = 1.0;
    grid2[i][0] = 1.0;
    grid2[i][gridSize+1] = 1.0;
  }
  for (j = 0; j <= gridSize+1; j++) {
    grid1[0][j] = 1.0;
    grid2[0][j] = 1.0;
    grid1[gridSize+1][j] = 1.0;
    grid2[gridSize+1][j] = 1.0;
  }
}

struct BarrierData {
  pthread_mutex_t barrier_mutex;
  pthread_cond_t barrier_cond;
  int nthread; // Number of threads that have reached this round of the barrier
  int round;   // Barrier round id
} bstate;

void barrier_init() {
  pthread_mutex_init(&bstate.barrier_mutex, NULL);
  pthread_cond_init(&bstate.barrier_cond, NULL);
  bstate.nthread = 0;
}

void barrier() {
    pthread_mutex_lock(&bstate.barrier_mutex);
    bstate.nthread++;
    if(bstate.nthread == numWorkers) {
        bstate.round++;
        bstate.nthread = 0;
        pthread_cond_broadcast(&bstate.barrier_cond);
    } else {
        int lround = bstate.round;
        do {
            pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
        } while(lround == bstate.round);
    }
    pthread_mutex_unlock(&bstate.barrier_mutex);
}
