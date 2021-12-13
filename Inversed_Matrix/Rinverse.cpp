//
// Sorts a list using multiple threads
//

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <new>
#include <omp.h>

using namespace std;

#define MAX_THREADS     65536
#define MAX_MATRIX_SIZE   65536
#define DELTA 1.0
#define TOL 1.0e-15


#define DEBUG 1

// Global variables
int matrix_size; 
int leaf_matrix_size;

// Define RMatrix - upper triangular matrix

class RMatrix {
    public:
        int  n;		// number of rows and columns 
        double **R;	// Matrix

        void initialize_matrix (int); 
        void compute_matrix_inverse_recursively (double **, int, int); 
        void invert_matrix_in_place (); 
        int  compare_inverse (); 
        void print_R (); 
        void print_Rinv (); 

    private:
        void invert_upper_triangular_matrix_block (double **, int, int);
        void compute_off_diagonal_block (double **, int, int);

        double **Rinv;	// Matrix inverse, for error checking 
        double **Rtemp;	// Matrix - temporary 
};

// Initialize upper triangular matrix R
//  - initialize R
//  - compute Rinv = inverse(R) for error checking
//  - allocate matrix Rtemp to be used as work martix

void RMatrix::initialize_matrix (int matrix_size) {
    double sum;
    // Allocate R
    n = matrix_size;
    R = new double *[n];
    double * array = new double[n*n];
    for (int i = 0; i < n; i++) R[i] = &(array[i*n]);

    // Initialize R
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < i; j++) R[i][j] = 0.0;
        sum = 0.0;
        for (int j = i+1; j < n; j++) {
	    R[i][j] = 1.0; // rand_r(); 
	    sum += R[i][j];
	}
	R[i][i] = sum + DELTA;
    }

    // Compute Rinv = inv(R)
    Rinv = new double *[n];
    array = new double[n*n];
    for (int i = 0; i < n; i++) Rinv[i] = &(array[i*n]);
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) Rinv[i][j] = R[i][j];
    }
    invert_upper_triangular_matrix_block (Rinv, 0, n); 

    // Allocate Rtemp
    Rtemp = new double *[n];
    array = new double[n*n];
    for (int i = 0; i < n; i++) Rtemp[i] = &(array[i*n]);
}

// Compute inverse of matrix block at location:
//    Rows:    [start, ..., start+size-1]
//    Columns: [start, ..., start+size-1]
void RMatrix::compute_matrix_inverse_recursively (double **R, int start, int size) {

    if (size <= leaf_matrix_size) { 
        // Set R = inv(R) using direct algorithm
        invert_upper_triangular_matrix_block (R, start, size);
    } else {
        // Set R11 = inv(R11)
        compute_matrix_inverse_recursively (R, start, size/2);
        // Set R22 = inv(R22)
        compute_matrix_inverse_recursively (R, start+size/2, size/2);
	// Set R12 = -inv(R11)*R12*inv(R22)
	compute_off_diagonal_block(R, start, size/2);
    }
}
// Compute off diagonal block R12 at location:
//    Rows:    start, ..., start+size-1
//    Cols: start+size, ..., start+2*size-1
//
// Set R12 = -inv(R11)*R12*inv(R22)
//
void RMatrix::compute_off_diagonal_block (double **R, int start, int size) {
    double sum;
    // Rtemp = -inv(R11)*R12
    for (int i = start; i < start+size; i++) {
        for (int j = start+size; j < start+2*size; j++) {
	    sum = 0.0;
	    for (int k = start; k < start+size; k++) 
	        sum += -R[i][k]*R[k][j];
	    Rtemp[i][j] = sum;
        }
    }
    // R12 = Rtemp*inv(R22)
    for (int i = start; i < start+size; i++) {
        for (int j = start+size; j < start+2*size; j++) {
	    sum = 0.0;
	    for (int k = start+size; k < start+2*size; k++) 
	        sum += Rtemp[i][k]*R[k][j];
	    R[i][j] = sum;
        }
    }
}

// Compute inverse of a diagonal block of upper triangular matrix R 
// "in place", i.e., contents of R are overwritten with inverse of R
void RMatrix::invert_upper_triangular_matrix_block (double **R, int start, int size) {
    double sum;
    for (int j = start+size-1; j >= start;  j--) {
        R[j][j] = 1.0/R[j][j];
	for (int i = j-1; i >= start; i--) {
	    sum = 0.0;
	    for (int k = i+1; k <= j; k++) {
	        sum += R[i][k]*R[k][j];
	    }
	    R[i][j] = -sum/R[i][i];
	}
    }
}

// Check if R = Rinv. This routine should be called only
// AFTER inverse of R has been computed "in place", i.e., 
// contents of R have been overwritten with inverse of R
int RMatrix::compare_inverse () {
    int error = 0;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) 
	    if (fabs(R[i][j] - Rinv[i][j]) > n*TOL) error = 1;
    }
    return error;
}

// Print Rinv
void RMatrix::print_Rinv () {
    printf("Printing Rinv -----------------------------------------\n"); 
    for (int i = 0; i < n; i++) {
        printf("Row %4d: ", i); 
        for (int j = 0; j < n; j++) 
	    printf("%f ", Rinv[i][j]); 
	printf("\n"); 
    }
    printf("----------------------------------------------------------\n"); 
}

// Print R
void RMatrix::print_R () {
    printf("Printing R ------------------------------------------\n"); 
    for (int i = 0; i < n; i++) {
        printf("Row %4d: ", i); 
        for (int j = 0; j < n; j++) 
	    printf("%f ", R[i][j]); 
	printf("\n");
    }
    printf("----------------------------------------------------------\n"); 
}

// Main program - set up an upper triangular matrix of random real
// numbers and computes the inverse of the matrix (in place) 
//
// Input: 
//	k = log_2(matrix_size), therefore matrix_size=2^k
//	q = log_2(leaf_matrix_size), therefore leaf_matrix_size=2^q
//
int main(int argc, char *argv[]) {

    double start_time, execution_time;
    int k, q, error;

    RMatrix R;		// Create matrix

    // Read input, validate
    if (argc != 3) {
	printf("Need two integers as input \n"); 
	printf("Use: <executable_name> <log_2(matrix_size)> <log_2(leaf_matrix_size)>\n"); 
	exit(0);
    }
    k = atoi(argv[argc-2]);
    matrix_size = (1 << k);
    if (matrix_size > MAX_MATRIX_SIZE) {
	printf("Maximum matrix size allowed: %d.\n", MAX_MATRIX_SIZE);
	exit(0);
    }; 
    q = atoi(argv[argc-1]);
    leaf_matrix_size = (1 << q);
    if (leaf_matrix_size > matrix_size) {
	printf("Leaf matrix size too large, setting to matrix size ...\n");
	leaf_matrix_size = matrix_size;
    }; 

    // Initialize R
    R.initialize_matrix(matrix_size);

    // Compute inverse of R - standard algorithm
//    R.compute_matrix_inverse();

    if (DEBUG > 2) R.print_R();
    if (DEBUG > 2) R.print_Rinv();

    start_time = omp_get_wtime();

    // Compute inverse of R - recursive algorithm
    R.compute_matrix_inverse_recursively(R.R, 0, matrix_size);

    execution_time = omp_get_wtime() - start_time;

    if (DEBUG > 2) R.print_R();

    if ((error = R.compare_inverse()) != 0) {
        printf("Houston, we have a problem! compare_inverse\n");
    }
    if (DEBUG > 2) R.print_R();

    printf("Matrix Size = %d,  Leaf Matrix Size = %d, Error = %d, Execution Time = %8.4f\n", matrix_size, leaf_matrix_size, error, execution_time);
}

// ------------------------------------------------------------
// Junk Code Follows
// ------------------------------------------------------------
/*
class RMatrix {
    public:
        void compute_matrix_inverse (); 
        int  check_inverse (); 

    private:
        void invert_upper_triangular_matrix (double **);
};

// ------------------------------------------------------------
// REMOVE EVENTUALLY
//
// Compute inverse of R and store in Rinv, for error checking
// R is copied in Rinv, then inverse of Rinv is computed "in place"
void RMatrix::compute_matrix_inverse () {
    // Allocate Rinv, initialize to R
    Rinv = new double *[n];
    double * array = new double[n*n];
    for (int i = 0; i < n; i++) Rinv[i] = &(array[i*n]);
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) Rinv[i][j] = R[i][j];
    }
    // Compute Rinv
    invert_upper_triangular_matrix(Rinv);
}

// ------------------------------------------------------------
// REMOVE EVENTUALLY
//
// Compute inverse of upper triangular matrix R "in place", i.e.,
// contents of R are overwritten with inverse of R
void RMatrix::invert_upper_triangular_matrix (double **R) {
    double sum;
    for (int j = n-1; j >= 0;  j--) {
        R[j][j] = 1.0/R[j][j];
	for (int i = j-1; i >= 0; i--) {
	    sum = 0.0;
	    for (int k = i+1; k <= j; k++) {
	        sum += R[i][k]*R[k][j];
	    }
	    R[i][j] = -sum/R[i][i];
	}
    }
}
// ------------------------------------------------------------
// REMOVE EVENTUALLY
//
// Check if Rinv * R = I (identity matrix)
int RMatrix::check_inverse () {
    int error = 0; 
    double sum;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
	    sum = 0.0;
	    for (int k = 0; k < n; k++) 
	        sum += Rinv[i][k] * R[k][j];
	    if (((i == j) && (fabs(sum-1.0) > n*TOL)) ||
	        ((i != j) && (fabs(sum) > n*TOL))) error = 1;
        }
    }
    return error;
}

*/
