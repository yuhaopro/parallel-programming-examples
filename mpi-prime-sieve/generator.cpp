#include <iostream>
#include <mpi.h>

#define MESSAGE_COUNT 1

int main(int argc, char *argv[])
{
    MPI_Comm nextComm;
    int candidate = 2, N = atoi(argv[1]), siever_rank = 0, siever_tag = 0;
    MPI_Init(&argc, &argv);

    // will execute siever process who belongs in the nextComm group.
    std::cout << "Generator: Spawning Siever" << std::endl;
    MPI_Comm_spawn("siever", argv, 1, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &nextComm, MPI_ERRCODES_IGNORE);
    while (candidate < N)
    {
        // first candidate = 2, known prime number
        std::cout << "Generator: Sending Candidate " << candidate << std::endl;
        MPI_Send(&candidate, MESSAGE_COUNT, MPI_INT, siever_rank, siever_tag, nextComm);
        candidate++;
    }
    candidate = -1;

    // To signal end of program
    std::cout << "Generator: Sending Terminal Signal" << std::endl;
    MPI_Send(&candidate, MESSAGE_COUNT, MPI_INT, siever_rank, siever_tag, nextComm);
    MPI_Finalize();
}
