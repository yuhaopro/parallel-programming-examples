#include <iostream>
#include <mpi.h>

#define MESSAGE_COUNT 1
#define SPAWN_PROCESS_COUNT 1

int main(int argc, char *argv[]) {
    MPI_Comm predComm, succComm;
    MPI_Status status;
    int prime, candidate, generator_rank = 0, generator_tag = 0, siever_rank = 0, siever_tag = 0;

    int first_output = 1;
    MPI_Init(&argc, &argv);

    MPI_Comm_get_parent(&predComm);
    MPI_Recv(&prime, MESSAGE_COUNT, MPI_INT, generator_rank, generator_tag, predComm, &status);
    std::cout << "Siever: " << prime << " is a prime number." << std::endl;

    MPI_Recv(&candidate, MESSAGE_COUNT, MPI_INT, generator_rank, generator_tag, predComm, &status);
    // std::cout << "Siever: " << candidate << " is a candidate." << std::endl;

    // Terminate signal = -1
    // Candidates are repeatedly passed through the sievers, until they reach the last child siever, signifying it is prime.
    while (candidate != -1) {
        // candidate not divisible by prime, could be another prime.
        if (candidate % prime != 0) {

            // creates a new siever which takes the candidate and treats it as it's prime.
            if (first_output) {
                //std::cout << "Siever: " << "Spawning first successor" << std::endl;

                MPI_Comm_spawn("siever", argv, SPAWN_PROCESS_COUNT, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &succComm, MPI_ERRCODES_IGNORE);
                first_output = 0;            
            }
            //std::cout << "Siever: " << "Sending " << candidate << " to next successor" << std::endl;
            MPI_Send(&candidate, MESSAGE_COUNT, MPI_INT, siever_rank, siever_tag, succComm);
        }

        // receives next candidate provided by the candidate generator
        MPI_Recv(&candidate, MESSAGE_COUNT, MPI_INT, generator_rank, generator_tag, predComm, &status);
        //std::cout << "Siever: " << "Receiving " << candidate << " from generator" << std::endl;

    }

    // candidate is terminate signal, send it to the successor siever.
    if (!first_output) {
        //std::cout << "Siever: " << "Sending terminate signal..." << std::endl;
        MPI_Send(&candidate, MESSAGE_COUNT, MPI_INT, siever_rank, siever_tag, succComm);
    }
    MPI_Finalize();
    
}