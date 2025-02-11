#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <math.h>
#include <pthread.h>

typedef struct {
    int p;          // Number of threads
    int stages;     // Number of stages = log2(p)
    // 1D array representing a 2D array: each thread has 'stages' flags.
    atomic_int *arrive;  
} barrier_t;

void barrier_init(barrier_t *bar, int p) {
    bar->p = p;
    bar->stages = (int)log2(p);  // Assumes p is a power of 2.
    bar->arrive = malloc(p * bar->stages * sizeof(atomic_int));
    for (int i = 0; i < p * bar->stages; i++) {
        atomic_init(&bar->arrive[i], 0);
    }
}

void barrier_wait(barrier_t *bar, int myid) {
    for (int s = 0; s < bar->stages; s++) {
        // Index into the 1D array: element corresponding to arrive[myid][s]
        int index = myid * bar->stages + s;
        
        // Wait until our flag for stage s is 0
        while (atomic_load_explicit(&bar->arrive[index], memory_order_acquire) != 0) {
            // Busy-wait; optionally, call sched_yield() to be CPU-friendly.
        }
        
        // Signal our arrival at stage s
        atomic_store_explicit(&bar->arrive[index], 1, memory_order_release);
        
        // Compute friend using XOR: myid XOR (1 << s)
        int friend_id = myid ^ (1 << s);
        int friend_index = friend_id * bar->stages + s;
        
        // Wait until the friend’s flag becomes 1
        while (atomic_load_explicit(&bar->arrive[friend_index], memory_order_acquire) != 1) {
            // Busy-wait
        }
        
        // Reset the friend's flag for the next barrier cycle
        atomic_store_explicit(&bar->arrive[friend_index], 0, memory_order_release);
    }
}

typedef struct {
    int myid;
    barrier_t *bar;
} thread_arg_t;

void* thread_func(void *arg) {
    thread_arg_t *targ = (thread_arg_t*)arg;
    int id = targ->myid;
    barrier_t *bar = targ->bar;
    
    printf("Thread %d before barrier\n", id);
    barrier_wait(bar, id);
    printf("Thread %d after barrier\n", id);
    
    return NULL;
}

int main(void) {
    int p = 8; // Number of threads (must be a power of 2)
    barrier_t bar; // shared variable
    barrier_init(&bar, p);
    
    pthread_t threads[p];
    thread_arg_t thread_args[p];
    
    // Create threads with unique IDs.
    for (int i = 0; i < p; i++) {
        thread_args[i].myid = i;
        thread_args[i].bar = &bar;
        pthread_create(&threads[i], NULL, thread_func, &thread_args[i]);
    }
    
    // Wait for all threads to complete.
    for (int i = 0; i < p; i++) {
        pthread_join(threads[i], NULL);
    }
    
    free(bar.arrive);
    return 0;
}

