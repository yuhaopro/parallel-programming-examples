#include <iostream>
#include <mpi.h>

#define MESSAGE_COUNT 1
#define SPAWN_PROCESS_COUNT 1

int main(int argc, char *argv[]) {
    MPI_Comm predComm, succComm;
    MPI_Status status;
    int prime, candidate, generator_rank = 0, generator_tag = 0;

    int first_output = 1;
    MPI_Init(&argc, &argv);

    MPI_Comm_get_parent(&predComm);
    MPI_Recv(&prime, MESSAGE_COUNT, MPI_INT, generator_rank, generator_tag, predComm, &status);
    std::cout << "Seiver: " << prime << " is a prime number." << std::endl;

    MPI_Recv(&candidate, MESSAGE_COUNT, MPI_INT, generator_rank, generator_tag, predComm, &status);
    
    // Terminate signal = -1
    while (candidate != -1) {
        // candidate not divisible by prime, is another prime.
        if (candidate % prime != 0) {
            if (first_output) {
                MPI_Comm_spawn("siever", argv, SPAWN_PROCESS_COUNT, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &succComm, MPI_ERRCODES_IGNORE);
                first_output = 0;            
            }
            MPI_Send(&candidate, MESSAGE_COUNT, MPI_INT, 0, 0, succComm);
        }

        // receives next candidate provided by the candidate generator
        MPI_Recv(&candidate, MESSAGE_COUNT, MPI_INT, 0, 0, predComm, &status);
    }
    if (!first_output) {
        MPI_Send(&candidate, MESSAGE_COUNT, MPI_INT, 0, 0, succComm);
    }
    MPI_Finalize();
    
}