#include <iostream>
#include <vector>
#include <cmath>   // For std::abs
#include <numeric> // For std::max
#include <cstdlib> // For malloc, free
#include <mpi.h>

// Function declarations
void read_problem(int &arr_size, float *&work);
// Changed signature for print_results
void print_results(int arr_size, const float *work);
void do_one_step(float *local_data, float *local_error, int size_per_process);

int main(int argc, char *argv[])
{
    int num_of_processes;
    int rank;
    int arr_size = 0; // Initialize arr_size
    int orchestrator = 0;
    float *work = nullptr; // Initialize work pointer

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &num_of_processes);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == orchestrator)
    {
        // read_problem now correctly allocates work based on the size it sets
        read_problem(arr_size, work);
        std::cout << "Problem size: " << arr_size << std::endl;
        std::cout << "Number of processes: " << num_of_processes << std::endl;

        // Basic check for divisibility (optional but good practice)
        if (arr_size % num_of_processes != 0)
        {
            std::cerr << "Error: Array size " << arr_size
                      << " is not divisible by the number of processes "
                      << num_of_processes << ". Exiting." << std::endl;
            // Free allocated memory before aborting
            delete[] work;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }

    // Broadcast the actual array size determined by rank 0
    MPI_Bcast(&arr_size, 1, MPI_INT, orchestrator, MPI_COMM_WORLD);

    // Ensure arr_size is positive and divisible before proceeding
    if (arr_size <= 0 || arr_size % num_of_processes != 0)
    {
        // Non-orchestrator ranks might hit this if orchestrator aborted
        // Or if read_problem failed silently (though it shouldn't here)
        if (rank != orchestrator)
        {
            std::cerr << "Rank " << rank << " received invalid arr_size " << arr_size
                      << " or non-divisible size. Exiting." << std::endl;
        }
        MPI_Finalize();
        return 1; // Exit if size is invalid
    }

    int size_per_process = arr_size / num_of_processes;
    // Allocate local buffer including ghost cells
    float *local = (float *)malloc(sizeof(float) * (size_per_process + 2));
    if (!local)
    {
        std::cerr << "Rank " << rank << " failed to allocate memory for local buffer. Exiting." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // Scatter the data from work (on rank 0) to the local buffers (starting at index 1)
    // Note: work pointer is only valid on rank 0 before Scatter
    MPI_Scatter(work, size_per_process, MPI_FLOAT,
                &local[1], size_per_process, MPI_FLOAT,
                orchestrator, MPI_COMM_WORLD);

    // Define neighbor ranks with wrap-around
    int left_process = (rank + num_of_processes - 1) % num_of_processes;
    int right_process = (rank + 1) % num_of_processes;

    float acceptable_error = 0.001f;              // Use a smaller error for convergence
    float global_error = acceptable_error + 1.0f; // Ensure loop runs at least once
    float local_error;
    MPI_Status status;
    int message_tag = 0;
    int iterations = 0; // Add iteration counter

    do
    {
        // Exchange ghost cells (boundary values) with neighbors
        // Send local[1] (leftmost data) to left neighbor, receive into local[size_per_process + 1] (right ghost cell) from right neighbor
        MPI_Sendrecv(&local[1], 1, MPI_FLOAT, left_process, message_tag,
                     &local[size_per_process + 1], 1, MPI_FLOAT, right_process, message_tag,
                     MPI_COMM_WORLD, &status);

        // Send local[size_per_process] (rightmost data) to right neighbor, receive into local[0] (left ghost cell) from left neighbor
        MPI_Sendrecv(&local[size_per_process], 1, MPI_FLOAT, right_process, message_tag,
                     &local[0], 1, MPI_FLOAT, left_process, message_tag,
                     MPI_COMM_WORLD, &status);

        // Perform one step of the Jacobi calculation and compute local error
        do_one_step(local, &local_error, size_per_process);

        // Reduce the maximum error across all processes
        MPI_Allreduce(&local_error, &global_error, 1, MPI_FLOAT, MPI_MAX, MPI_COMM_WORLD);

        iterations++;
        // Optional: Print progress occasionally
        // if (rank == orchestrator && iterations % 100 == 0) {
        //     std::cout << "Iteration " << iterations << ", Global Error: " << global_error << std::endl;
        // }
        if (iterations > 100000)
        { // Safety break for potential infinite loops
            if (rank == orchestrator)
            {
                std::cout << "Warning: Exceeded max iterations. Error: " << global_error << std::endl;
            }
            break;
        }

    } while (global_error > acceptable_error);

    // Gather the results (from index 1 of local buffers) back to work on rank 0
    // Note: work pointer is only valid on rank 0 after Gather
    MPI_Gather(&local[1], size_per_process, MPI_FLOAT,
               work, size_per_process, MPI_FLOAT,
               orchestrator, MPI_COMM_WORLD);

    // Print results and clean up
    if (rank == orchestrator)
    {
        std::cout << "Converged after " << iterations << " iterations with final error: " << global_error << std::endl;
        // Pass arr_size to print_results
        print_results(arr_size, work);
        // Free the memory allocated in read_problem
        delete[] work;
    }
    // Free the local buffer allocated with malloc
    free(local);

    MPI_Finalize();

    return 0;
}

// Corrected read_problem
void read_problem(int &arr_size, float *&work)
{
    // Define the size FIRST
    arr_size = 6; // Or read from file/input

    // Allocate memory using the now defined size
    work = new float[arr_size];

    // Check if allocation was successful (good practice)
    if (!work)
    {
        std::cerr << "Error: Failed to allocate memory in read_problem. Exiting." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
        return; // Should not be reached due to Abort
    }

    for (int i = 0; i < arr_size; ++i)
    {
        work[i] = (float)i * 20.0f; // Example initialization for smaller sizes
    }
}

// Corrected print_results
void print_results(int arr_size, const float *work)
{
    std::cout << "Final Results:" << std::endl;
    // Iterate up to arr_size
    for (int i = 0; i < arr_size; ++i)
    {
        // Add formatting for better readability
        std::cout << work[i] << (i == arr_size - 1 ? "" : " ");
    }
    std::cout << std::endl;
}

// do_one_step remains the same, but ensure correct headers included
void do_one_step(float *local_data, float *local_error, int size_per_process)
{
    float max_error = 0.0f;
    // Use std::vector for automatic memory management and safety
    std::vector<float> new_local(size_per_process + 2);

    // Copy ghost cells to new_local to avoid using uninitialized values
    // Though they are not directly used in the loop below, it's cleaner
    // if new_local is meant to represent the full state including ghosts.
    // Alternatively, size the vector to just size_per_process and adjust indexing.
    new_local[0] = local_data[0];
    new_local[size_per_process + 1] = local_data[size_per_process + 1];

    for (int i = 1; i <= size_per_process; ++i)
    {
        // Jacobi update formula: average of the left and right neighbours from the *old* data
        new_local[i] = 0.5f * (local_data[i - 1] + local_data[i + 1]);

        // Check the difference between the old value and the new value at the same index.
        // std::abs is preferred over abs for floats (from <cmath>)
        max_error = std::max(max_error, std::abs(new_local[i] - local_data[i]));
    }

    // Copy new values back to local_data's relevant portion
    for (int i = 1; i <= size_per_process; ++i)
    {
        local_data[i] = new_local[i];
    }

    *local_error = max_error;
}