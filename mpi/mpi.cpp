#include <iostream>
#include <vector> // Use vector for easier management if needed, but arrays are fine
#include <mpi.h>

#define MAX_TASKS 10
// Make NO_MORE_TASKS distinct from any valid task ID (1 to MAX_TASKS)
#define NO_MORE_TASKS (MAX_TASKS + 1)
#define FARMER 0
#define MESSAGE_COUNT 1 // Sending 1 integer at a time

// Function Prototypes
void farmer(int);
void worker(int);
int compute(int);

int main(int argc, char *argv[])
{
    int np, rank;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &np);

    if (np < 2)
    {
        if (rank == FARMER)
        {
            std::cerr << "Error: This program requires at least 2 processes (1 farmer, 1+ workers)." << std::endl;
        }
        MPI_Finalize();
        return 1;
    }

    if (rank == FARMER)
    {
        // Number of workers is total processes minus 1 (the farmer)
        farmer(np - 1);
    }
    else
    {
        worker(rank);
    }

    MPI_Finalize();
    return 0; // Added return statement
}

void farmer(int num_of_workers)
{
    // Use vectors or arrays, ensure indexing is correct. Index should follow worker's rank.
    int task_data[MAX_TASKS + 1];           
    int result_data[MAX_TASKS + 1];         
    int worker_results[num_of_workers + 1];

    int next_task_id; // Use this consistently
    int temp_result;
    MPI_Status status;
    int completed_task_count = 0; // Keep track of finished tasks

    // Initialize tasks with some data (e.g., task ID itself)
    for (int i = 1; i <= MAX_TASKS; ++i)
    {
        task_data[i] = i; // Task 'i' has data value 'i'
    }

    // --- PHASE 1 ---
    // Send one initial task to each worker.
    std::cout << "Farmer: Starting Phase 1 (Initial Task Distribution)..." << std::endl;
    next_task_id = 1; // Start with task 1
    for (int worker_rank = 1; worker_rank <= num_of_workers; ++worker_rank)
    {
        // Ensure we don't assign more tasks than available if MAX_TASKS < num_of_workers
        if (next_task_id <= MAX_TASKS)
        {
            int current_task_id = next_task_id; // The task ID we are sending
            int message_dest = worker_rank;     // The worker's rank
            int message_tag = current_task_id;  // Tag the message with the task ID

            std::cout << "Farmer: Sending Task " << current_task_id << " (Value: " << task_data[current_task_id] << ") to Worker Rank " << message_dest << std::endl;
            MPI_Send(&task_data[current_task_id], MESSAGE_COUNT, MPI_INT, message_dest, message_tag, MPI_COMM_WORLD);
            next_task_id++; // Move to the next task ID for the next worker/assignment
        }
        else
        {
            // If no more tasks initially, maybe send NO_MORE_TASKS? Or handle later.
            // For simplicity, assume MAX_TASKS >= num_of_workers for initial phase.
            int message_dest = worker_rank;
            int dummy_data = 0; // Value doesn't matter
            int message_tag = NO_MORE_TASKS;
            std::cout << "Farmer: No initial task for Worker Rank " << message_dest << ". Sending termination signal." << std::endl;
            MPI_Send(&dummy_data, MESSAGE_COUNT, MPI_INT, message_dest, message_tag, MPI_COMM_WORLD);
        }
    }
    std::cout << "Farmer: Finished Phase 1." << std::endl;

    // --- PHASE 2 ---
    // Receive results and send new tasks until all tasks are assigned.
    std::cout << "Farmer: Starting Phase 2 (Dynamic Task Assignment)..." << std::endl;
    // Loop while there are completed tasks to receive OR tasks left to assign
    // We expect MAX_TASKS results in total.
    while (completed_task_count < MAX_TASKS)
    {
        // Receive a result from *any* worker. The tag indicates which task was completed.
        MPI_Recv(&temp_result, MESSAGE_COUNT, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        int message_src = status.MPI_SOURCE;     // Rank of the worker sending the result
        int completed_task_tag = status.MPI_TAG; // Tag = ID of the task the worker just finished

        std::cout << "Farmer: Received Result " << temp_result << " for Task " << completed_task_tag << " from Worker Rank " << message_src << std::endl;
        result_data[completed_task_tag] = temp_result; // Store result based on task ID
        completed_task_count++;

        // Assign a new task if available
        if (next_task_id <= MAX_TASKS)
        {
            int task_to_assign = next_task_id;
            int message_tag = task_to_assign; // Tag with new task ID

            std::cout << "Farmer: Sending Task " << task_to_assign << " (Value: " << task_data[task_to_assign] << ") to Worker Rank " << message_src << std::endl;
            MPI_Send(&task_data[task_to_assign], MESSAGE_COUNT, MPI_INT, message_src, message_tag, MPI_COMM_WORLD);
            next_task_id++;
        }
        // If no more tasks left to assign, we'll send termination signal in the next phase
    }
    std::cout << "Farmer: Finished Phase 2." << std::endl;

    // --- Phase 3 ---
    // All tasks have been assigned. Send termination signal to workers
    // as they report their last result (which we already received in Phase 2).
    std::cout << "Farmer: Starting Phase 3 (Termination)..." << std::endl;
    for (int worker_rank = 1; worker_rank <= num_of_workers; ++worker_rank)
    {

        // Send the termination signal. The worker rank is `worker_rank`.
        int dummy_data = 0; // Value doesn't matter
        int message_tag = NO_MORE_TASKS;
        int message_dest = worker_rank;
        std::cout << "Farmer: Sending Terminate Signal to Worker " << worker_rank << std::endl;
        MPI_Send(&dummy_data, MESSAGE_COUNT, MPI_INT, message_dest, message_tag, MPI_COMM_WORLD);
    } 
}

void worker(int rank)
{
    int received_task_value, result, message_tag, message_src;
    MPI_Status status;

    // Initial receive
    MPI_Recv(&received_task_value, MESSAGE_COUNT, MPI_INT, FARMER, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    message_src = status.MPI_SOURCE; // Should be FARMER (0)
    message_tag = status.MPI_TAG;    // This is the ID of the task received

    std::cout << " Worker " << rank << ": Received initial Task ID " << message_tag << " (Value: " << received_task_value << ") from Rank " << message_src << std::endl;

    while (message_tag != NO_MORE_TASKS)
    {
        // Process the task
        result = compute(received_task_value);
        std::cout << " Worker " << rank << ": Computed result " << result << " for Task ID " << message_tag << std::endl;

        // Send the result back, tagged with the ID of the task just completed
        MPI_Send(&result, MESSAGE_COUNT, MPI_INT, FARMER, message_tag, MPI_COMM_WORLD);
        std::cout << " Worker " << rank << ": Sent result for Task ID " << message_tag << " to Farmer" << std::endl;

        // Receive the next task assignment or termination signal
        MPI_Recv(&received_task_value, MESSAGE_COUNT, MPI_INT, FARMER, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        message_src = status.MPI_SOURCE;
        message_tag = status.MPI_TAG; // ID of the *next* task, or NO_MORE_TASKS

        if (message_tag != NO_MORE_TASKS)
        {
            std::cout << " Worker " << rank << ": Received next Task ID " << message_tag << " (Value: " << received_task_value << ") from Rank " << message_src << std::endl;
        }
        else
        {
            std::cout << " Worker " << rank << ": Received NO_MORE_TASKS signal from Rank " << message_src << std::endl;
        }
    }

    std::cout << " Worker " << rank << ": Exiting." << std::endl;

} // End Worker

int compute(int task_value)
{
    // Simulate some work
    // Make sure this computation is correct based on task_value not task_id if they differ
    if (task_value > 0)
    {
        int wait_time = task_value % 5; // Simple wait based on task value
        for (volatile int i = 0; i < wait_time * 1000000; ++i)
            ; // Simulate work
    }
    return task_value * 10; // Example computation
}