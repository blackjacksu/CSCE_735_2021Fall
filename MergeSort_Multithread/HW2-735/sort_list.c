//
// Sorts a list using multiple threads
//

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <limits.h>

#define MAX_THREADS     65536
#define MAX_LIST_SIZE   100000000

#define DEBUG 1

// Thread variables
//
// VS: ... declare thread variables, mutexes, condition varables, etc.,
// VS: ... as needed for this assignment 
//



typedef struct thread_arg{
    int q;
    int level;
    int np;
    int my_id;
    int * ptr;
};

pthread_attr_t attr;
pthread_barrierattr_t b_attr;


pthread_cond_t cond;
pthread_mutex_t lock;
pthread_mutex_t sublist_sorted_count_mutex;
int sublist_sorted_count;
pthread_cond_t sorted_cond;
pthread_mutex_t sorted_cond_mutex;

typedef struct thread_node {
    int inserted_count;
    pthread_cond_t cond_count;
    pthread_t thread;
    pthread_mutex_t list_mutex;
    pthread_mutex_t work_mutex;
    pthread_mutex_t cond_mutex;
    pthread_mutex_t count_mutex;
};

struct thread_node thread_nodes[MAX_THREADS];

// Global variables
int num_threads;		// Number of threads to create - user input 
int list_size;			// List size
int *list;			// List of values
int *work;			// Work array
int *list_orig;			// Original list of values, used for error checking
// int ptr[MAX_THREADS+1];
// Print list - for debugging
void print_list(int *list, int list_size) {
    int i;
    for (i = 0; i < list_size; i++) {
        printf("[%d] \t %16d\n", i, list[i]); 
    }
    printf("--------------------------------------------------------------------\n"); 
}

// Comparison routine for qsort (stdlib.h) which is used to 
// a thread's sub-list at the start of the algorithm
int compare_int(const void *a0, const void *b0) {
    int a = *(int *)a0;
    int b = *(int *)b0;
    if (a < b) {
        return -1;
    } else if (a > b) {
        return 1;
    } else {
        return 0;
    }
}

// Return index of first element larger than or equal to v in sorted list
// ... return last if all elements are smaller than v
// ... elements in list[first], list[first+1], ... list[last-1]
//
//   int idx = first; while ((v > list[idx]) && (idx < last)) idx++;
//
int binary_search_lt(int v, int *list, int first, int last) {
   
    // Linear search code
    // int idx = first; while ((v > list[idx]) && (idx < last)) idx++; return idx;

    int left = first; 
    int right = last-1; 

    if (list[left] >= v) return left;
    if (list[right] < v) return right+1;
    int mid = (left+right)/2; 
    while (mid > left) {
        if (list[mid] < v) {
	    left = mid; 
	} else {
	    right = mid;
	}
	mid = (left+right)/2;
    }
    return right;
}
// Return index of first element larger than v in sorted list
// ... return last if all elements are smaller than or equal to v
// ... elements in list[first], list[first+1], ... list[last-1]
//
//   int idx = first; while ((v >= list[idx]) && (idx < last)) idx++;
//
int binary_search_le(int v, int *list, int first, int last) {

    // Linear search code
    // int idx = first; while ((v >= list[idx]) && (idx < last)) idx++; return idx;
 
    int left = first; 
    int right = last-1; 

    if (list[left] > v) return left; 
    if (list[right] <= v) return right+1;
    int mid = (left+right)/2; 
    while (mid > left) {
        if (list[mid] <= v) {
	    left = mid; 
	} else {
	    right = mid;
	}
	mid = (left+right)/2;
    }
    return right;
}

// Function for multi-threaded routine
void * sort_sublist(void * arg_ptr)
{
    struct thread_arg * arg = (struct thread_arg*)arg_ptr;
    int q = arg->q;
    int level = arg->level;
    int np = arg->np;
    int my_id = arg->my_id;
    int * ptr = arg->ptr;

    int my_own_blk, my_own_idx;
    int my_blk_size, my_search_blk, my_search_idx, my_search_idx_max;
    int my_write_blk, my_write_idx;
    int my_search_count = 0; 
    int i_write = 0;
    int idx = 0;
    int i = 0;
    int my_list_size = 0;     
    int wait_status = 0;   
    int thread_cancel = 0;


if (DEBUG)  print_list(list, list_size);
    while (level < q)
    {
        // Barrier thread
        my_blk_size = np * (1 << level); 

        my_own_blk = ((my_id >> level) << level);
	    my_own_idx = ptr[my_own_blk];

	    my_search_blk = ((my_id >> level) << level) ^ (1 << level);
	    my_search_idx = ptr[my_search_blk];
	    my_search_idx_max = my_search_idx+my_blk_size;

	    my_write_blk = ((my_id >> (level+1)) << (level+1));
	    my_write_idx = ptr[my_write_blk];

	    idx = my_search_idx;
    
	    my_search_count = 0;

        my_list_size = ptr[my_id+1]-ptr[my_id];

        // Binary search for 1st element
	    if (my_search_blk > my_own_blk) {
            idx = binary_search_lt(list[ptr[my_id]], list, my_search_idx, my_search_idx_max); 
	    } 
        else {
            idx = binary_search_le(list[ptr[my_id]], list, my_search_idx, my_search_idx_max); 
	    }
	    my_search_count = idx - my_search_idx;
	    i_write = my_write_idx + my_search_count + (ptr[my_id]-my_own_idx); 
        pthread_mutex_lock(&thread_nodes[i_write/my_list_size].work_mutex);
	    work[i_write] = list[ptr[my_id]];
        pthread_mutex_unlock(&thread_nodes[i_write/my_list_size].work_mutex);
        pthread_mutex_lock(&thread_nodes[i_write/my_blk_size/2].count_mutex);
        thread_nodes[i_write/my_blk_size/2].inserted_count++;
        pthread_mutex_unlock(&thread_nodes[i_write/my_blk_size/2].count_mutex);
	    // Linear search for 2nd element onwards
	    for (i = ptr[my_id]+1; i < ptr[my_id+1]; i++) {
	        if (my_search_blk > my_own_blk) {
	    	    while ((list[i] > list[idx]) && (idx < my_search_idx_max)) {
	    	        idx++; 
                    my_search_count++;
	    	    }
	    	} 
            else {
	    	    while ((list[i] >= list[idx]) && (idx < my_search_idx_max)) {
	    	        idx++; 
                    my_search_count++;
	    	    }
	    	}
	    	i_write = my_write_idx + my_search_count + (i-my_own_idx); 
            pthread_mutex_lock(&thread_nodes[i_write/my_list_size].work_mutex);
	        work[i_write] = list[i];
            pthread_mutex_unlock(&thread_nodes[i_write/my_list_size].work_mutex);
            pthread_mutex_lock(&thread_nodes[i_write/my_blk_size/2].count_mutex);
            thread_nodes[i_write/my_blk_size/2].inserted_count++;
            pthread_mutex_unlock(&thread_nodes[i_write/my_blk_size/2].count_mutex);
	    }

        // finished inserting for the sublist to work[]
        if (thread_nodes[my_id/2].inserted_count == my_blk_size*2)
        {
            pthread_mutex_lock(&thread_nodes[my_id/2].count_mutex);
            thread_nodes[my_id/2].inserted_count = 0;
            pthread_mutex_unlock(&thread_nodes[my_id/2].count_mutex);
            pthread_cond_signal(&thread_nodes[my_id/2].cond_count);
        }
        else
        {
            // mutex lock
            pthread_mutex_lock(&thread_nodes[my_id/2].cond_mutex);
            pthread_cond_wait(&thread_nodes[my_id/2].cond_count, &thread_nodes[my_id/2].cond_mutex);
            pthread_mutex_unlock(&thread_nodes[my_id/2].cond_mutex);
        }
        // pthread signal
        // Copy work into list for next itertion
        // use another barrier to make sure the work list is inserted already complete 
	    for (i = ptr[my_id]; i < ptr[my_id+1]; i++) {
            pthread_mutex_lock(&thread_nodes[i/my_list_size].list_mutex);
	        list[i] = work[i];
            pthread_mutex_unlock(&thread_nodes[i/my_list_size].list_mutex);
	    }
        pthread_mutex_lock(&sublist_sorted_count_mutex);
	    sublist_sorted_count++;
        pthread_mutex_unlock(&sublist_sorted_count_mutex);
        // Make sure all thread are done with sorting
        // Destroy the thread that is no longer needed
        // Lead else thread to the next level
        if (sublist_sorted_count == list_size / my_blk_size)
        {
            // Good to go to the next level
            sublist_sorted_count = 0;
            pthread_cond_signal(&sorted_cond);
        }
        else
        {
            // wait until others finished
            pthread_mutex_lock(&sorted_cond_mutex);
            pthread_cond_wait(&sorted_cond, &sorted_cond_mutex);
            pthread_mutex_unlock(&sorted_cond_mutex);
        }

        if (my_id >= list_size / my_blk_size)
        {
            pthread_exit(0);
            printf("Thread %d exit\n", my_id);
        }
        level++;
        printf("Thread pass:%d, level:%d\n", wait_status, level);



if (DEBUG)  print_list(list, list_size);
    }

    if (level == q)
    {
        // the sorting is completed
        pthread_cond_signal(&cond);
    }
}


// Sort list via parallel merge sort
//
// VS: ... to be parallelized using threads ...
//
void sort_list(int q) {

    int level, my_id, np, my_list_size; 

    int thread_status;

    struct thread_arg *args = (struct thread_arg *) malloc(sizeof( struct thread_arg ));

    np = list_size/num_threads; 	// Sub list size 
    int ptr[num_threads+1];
    level = 0;
    // Initialize starting position for each sublist
    for (my_id = 0; my_id < num_threads; my_id++) {
        ptr[my_id] = my_id * np;
    }
    ptr[num_threads] = list_size;

    // Sort local lists
    for (my_id = 0; my_id < num_threads; my_id++) {
        my_list_size = ptr[my_id+1]-ptr[my_id];
        qsort(&list[ptr[my_id]], my_list_size, sizeof(int), compare_int);
    }

// if (DEBUG) print_list(list, list_size); 

    // Sort list
    // Each thread scatters its sub_list into work array
	for (my_id = 0; my_id < num_threads; my_id++) {

        // Passing arguments to each thread
        args->q = q;
        args->level = level;
        args->np = np;
        args->my_id = my_id;
        args->ptr = ptr;

        thread_status = pthread_create(&thread_nodes[my_id].thread, NULL, sort_sublist, (void *) args);

        if (thread_status)
        {
            // thread creation failed
            printf("Thread created failed!");
        }
    }

    // block until the sorting is complete
    pthread_mutex_lock(&lock);

    pthread_cond_wait(&cond, &lock);

    pthread_mutex_unlock(&lock);

	//     my_blk_size = np * (1 << level); 

	//     my_own_blk = ((my_id >> level) << level);
	//     my_own_idx = ptr[my_own_blk];

	//     my_search_blk = ((my_id >> level) << level) ^ (1 << level);
	//     my_search_idx = ptr[my_search_blk];
	//     my_search_idx_max = my_search_idx+my_blk_size;

	//     my_write_blk = ((my_id >> (level+1)) << (level+1));
	//     my_write_idx = ptr[my_write_blk];

	//     idx = my_search_idx;
	    
	//     my_search_count = 0;


	//     // Binary search for 1st element
	//     if (my_search_blk > my_own_blk) {
    //         idx = binary_search_lt(list[ptr[my_id]], list, my_search_idx, my_search_idx_max); 
	//     } 
    //     else {
    //         idx = binary_search_le(list[ptr[my_id]], list, my_search_idx, my_search_idx_max); 
	//     }
	//     my_search_count = idx - my_search_idx;
	//     i_write = my_write_idx + my_search_count + (ptr[my_id]-my_own_idx); 
	//     work[i_write] = list[ptr[my_id]];

	//     // Linear search for 2nd element onwards
	//     for (i = ptr[my_id]+1; i < ptr[my_id+1]; i++) {
	//         if (my_search_blk > my_own_blk) {
	// 	        while ((list[i] > list[idx]) && (idx < my_search_idx_max)) {
	// 	            idx++; my_search_count++;
	// 	        }
	// 	    } 
    //         else {
	// 	        while ((list[i] >= list[idx]) && (idx < my_search_idx_max)) {
	// 	            idx++; my_search_count++;
	// 	        }
	// 	    }
	// 	    i_write = my_write_idx + my_search_count + (i-my_own_idx); 
	// 	    work[i_write] = list[i];
	//     }
	// }
    //     // Copy work into list for next itertion
	// for (my_id = 0; my_id < num_threads; my_id++) {
	//     for (i = ptr[my_id]; i < ptr[my_id+1]; i++) {
	//         list[i] = work[i];
	//     } 

// if (DEBUG) print_list(list, list_size); 
}


// Main program - set up list of random integers and use threads to sort the list
//
// Input: 
//	k = log_2(list size), therefore list_size = 2^k
//	q = log_2(num_threads), therefore num_threads = 2^q
//
int main(int argc, char *argv[]) {

    struct timespec start, stop, stop_qsort;
    double total_time, time_res, total_time_qsort;
    int k, q, j, error; 
    int thread_init_status;
    int i;

    // Read input, validate
    if (argc != 3) {
	    printf("Need two integers as input \n"); 
	    printf("Use: <executable_name> <log_2(list_size)> <log_2(num_threads)>\n"); 
	    exit(0);
    }
    k = atoi(argv[argc-2]);
    if ((list_size = (1 << k)) > MAX_LIST_SIZE) {
	    printf("Maximum list size allowed: %d.\n", MAX_LIST_SIZE);
	    exit(0);
    }; 
    q = atoi(argv[argc-1]);
    if ((num_threads = (1 << q)) > MAX_THREADS) {
	    printf("Maximum number of threads allowed: %d.\n", MAX_THREADS);
	    exit(0);
    }; 
    if (num_threads > list_size) {
	    printf("Number of threads (%d) < list_size (%d) not allowed.\n", 
	    num_threads, list_size);
	    exit(0);
    }; 

    // Allocate list, list_orig, and work

    list = (int *) malloc(list_size * sizeof(int));
    list_orig = (int *) malloc(list_size * sizeof(int));
    work = (int *) malloc(list_size * sizeof(int));

//
// VS: ... May need to initialize mutexes, condition variables, 
// VS: ... and their attributes
//
    for (i = 0; i < MAX_THREADS; i++) {
        thread_init_status = pthread_mutex_init(&thread_nodes[i].list_mutex, NULL);        
        if (thread_init_status != 0)
        {
            printf("Init barrier fail:%d\n", i);
            exit(0);
        }
        thread_init_status = pthread_mutex_init(&thread_nodes[i].work_mutex, NULL);        
        if (thread_init_status != 0)
        {
            printf("Init barrier fail:%d\n", i);
            exit(0);
        }
        thread_init_status = pthread_cond_init(&thread_nodes[i].cond_count, NULL);        
        if (thread_init_status != 0)
        {
            printf("Init barrier fail:%d\n", i);
            exit(0);
        }
        thread_init_status = pthread_mutex_init(&thread_nodes[i].cond_mutex, NULL);        
        if (thread_init_status != 0)
        {
            printf("Init cond_lock fail:%d\n", i);
            exit(0);
        }
        thread_init_status = pthread_mutex_init(&thread_nodes[i].count_mutex, NULL);        
        if (thread_init_status != 0)
        {
            printf("Init count lock fail:%d\n", i);
            exit(0);
        }
        thread_nodes[i].inserted_count = 0;
    }
    thread_init_status = pthread_mutex_init(&lock, NULL);
    if (thread_init_status != 0)
    {
        printf("Init mutex fail:%d\n", i);
        exit(0);
    }    
    thread_init_status = pthread_mutex_init(&sublist_sorted_count_mutex, NULL);
    if (thread_init_status != 0)
    {
        printf("Init sublist count mutex fail:%d\n", i);
        exit(0);
    }
    thread_init_status = pthread_cond_init(&cond, NULL);
    if (thread_init_status != 0)
    {
        printf("Init cond fail:%d\n", i);
        exit(0);
    }
    sublist_sorted_count = 0;

    // Initialize list of random integers; list will be sorted by 
    // multi-threaded parallel merge sort
    // Copy list to list_orig; list_orig will be sorted by qsort and used
    // to check correctness of multi-threaded parallel merge sort
    srand48(0); 	// seed the random number generator
    for (j = 0; j < list_size; j++) {
	list[j] = (int) lrand48();
	list_orig[j] = list[j];
    }
    // duplicate first value at last location to test for repeated values
    list[list_size-1] = list[0]; list_orig[list_size-1] = list_orig[0];

    // Create threads; each thread executes find_minimum
    clock_gettime(CLOCK_REALTIME, &start);

//
// VS: ... may need to initialize mutexes, condition variables, and their attributes
//

// Serial merge sort 
// VS: ... replace this call with multi-threaded parallel routine for merge sort
// VS: ... need to create threads and execute thread routine that implements 
// VS: ... parallel merge sort
    printf("Old list\n");
if (DEBUG) print_list(list, list_size); 

    sort_list(q);

    printf("New list\n");

if (DEBUG) print_list(list, list_size); 

    // Compute time taken
    clock_gettime(CLOCK_REALTIME, &stop);
    total_time = (stop.tv_sec-start.tv_sec)
	+0.000000001*(stop.tv_nsec-start.tv_nsec);

    // Check answer
    qsort(list_orig, list_size, sizeof(int), compare_int);
    clock_gettime(CLOCK_REALTIME, &stop_qsort);
    total_time_qsort = (stop_qsort.tv_sec-stop.tv_sec)
	+0.000000001*(stop_qsort.tv_nsec-stop.tv_nsec);

    error = 0; 
    for (j = 1; j < list_size; j++) {
	    if (list[j] != list_orig[j]) error = 1; 
    }

    if (error != 0) {
	    printf("Houston, we have a problem!\n"); 
    }

    // Print time taken
    printf("List Size = %d, Threads = %d, error = %d, time (sec) = %8.4f, qsort_time = %8.4f\n", 
	    list_size, num_threads, error, total_time, total_time_qsort);

// VS: ... destroy mutex, condition variables, etc.

    free(list); free(work); free(list_orig); 

}

