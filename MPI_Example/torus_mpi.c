// compile in OSX: mpicc -o test torus_mpi.c 
// Run (OSX): mpirun ./test

#include <stdio.h> 
#include <string.h> 
#include "mpi.h" 

#define BUFSIZE 1000000 
#define TAG 0
setup(int* source, int* dest, int myid, int np)
{
    *source = myid-1;
    *dest = myid+1; 
}

main(int argc, char **argv)
{
    double start, total_time;
    int myid, np, ierr, source, dest, i, check, buf[BUFSIZE];
    MPI_Status status;
    ierr = MPI_Init(&argc, &argv); 
    if ( ierr == MPI_SUCCESS ) {
        MPI_Comm_size(MPI_COMM_WORLD,&np); 
        MPI_Comm_rank(MPI_COMM_WORLD,&myid);

        for (i = 0; i < BUFSIZE; i++) 
        {
            buf[i] = myid; 
        }
        
        setup(&source, &dest, myid, np);
        start = MPI_Wtime();

        if ((myid > 0) && (myid <= np-1))
            MPI_Recv(&buf, BUFSIZE, MPI_INT, source, TAG, MPI_COMM_WORLD, &status);
        if ((myid >= 0) && (myid < np-1))
            MPI_Send(&buf, BUFSIZE, MPI_INT, dest, TAG, MPI_COMM_WORLD);

        total_time = MPI_Wtime()-start;
        printf("%d, %d, %d, %24.16f; \n",myid,source,dest,total_time);
        MPI_Finalize();
    }
}