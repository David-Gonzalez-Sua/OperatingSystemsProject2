/*
 * CS 440 Project 2 � POSIX (pthreads) TEMPLATE
 * Name(s):
 * Date:
 *
 * Goal: Implement 2.a / 2.b / 2.c so that EACH experiment creates/destroys
 * exactly N_TOTAL threads (including all parent/initial/child/grandchild threads).
 *
 * Includes:
 *  - skeleton runners for 2.a, 2.b, 2.c (non-batched)
 *  - skeleton runners for batching fallback
 *
 * Students: Fill in TODO blocks. Keep printing sparse.
 *
 * Build:
 *   cc -O2 -Wall -Wextra -pedantic -pthread project2_posix_template.c -o project2
 */

#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <time.h>
#include <errno.h>
#include <string.h>

// ======= Fixed baseline (A, B, C must match) =======
enum { N_TOTAL = 5000 };

// ======= 2.b parameters (must total exactly 5000) =======
// verify math: parents + parents*children_per_parent == 5000  --> 50+50*99 = 5000
enum { B_PARENTS = 50, B_CHILDREN_PER_PARENT = 99 };

// ======= 2.c parameters (must total exactly 5000) =======
// verify math: 
// initials + initials*children_per_initial + initials*children_per_initial*grandchildren_per_child == 5000  --> 20+20*3+20*3*82 = 5000
enum { C_INITIALS = 20, C_CHILDREN_PER_INITIAL = 3, C_GRANDCHILDREN_PER_CHILD = 82 };

// ======= Batching knobs (reduce concurrency if needed) =======
enum { A_BATCH_SIZE = 25, B_CHILD_BATCH_SIZE = 25, C_GRANDCHILD_BATCH_SIZE = 25 };

// ======= Counters =======
static atomic_int g_created = 0;
static atomic_int g_destroyed = 0;

static atomic_int minimal_work_var = 0;

// ------------------------------------------------------------
// Timing (POSIX monotonic clock)
// ------------------------------------------------------------
static long long now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    if (ts.tv_sec < 0 || ts.tv_nsec < 0) {
        fprintf(stderr, "ERROR: clock_gettime failed\n");
        exit(EXIT_FAILURE);
    }
    return (long long)ts.tv_sec * 1000000000LL + (long long)ts.tv_nsec;
}

static void reset_counts(void) {
    atomic_store(&g_created, 0);
    atomic_store(&g_destroyed, 0);

    atomic_store(&minimal_work_var, 0);
}

static void print_summary(const char *label, long long start_ns, long long end_ns) {
    double elapsed_ms = (end_ns - start_ns) / 1e6;
    printf("%sElapsed:    %.3f ms\n\n", label, elapsed_ms);
    printf("Threads created:   %d\n", atomic_load(&g_created));
    printf("Threads destroyed: %d\n", atomic_load(&g_destroyed));
}

// ------------------------------------------------------------
// Error helper
// ------------------------------------------------------------
static void die_pthread(int rc, const char *where) {
    if (rc == 0) return;
    fprintf(stderr, "ERROR: %s: %s\n", where, strerror(rc));
    exit(EXIT_FAILURE);
}

// ============================================================
// 2.a � Flat workers
// ============================================================
typedef struct {
    int id; // optional
} flat_arg_t;

static void *flat_worker(void *arg) {
    (void)arg;
    
    // optional minimal work
    volatile int x = 0;
    x = 1;
    x++;
    atomic_fetch_add(&minimal_work_var, x);

    // optional sparse print using id
    // format: Created threads:   1–100
    flat_arg_t *fa = (flat_arg_t *)arg;
    if (fa->id % 100 == 0) {
        printf("Created threads:    %d-%d\n", fa->id - 99, fa->id);
    }

    atomic_fetch_add(&g_destroyed, 1);
    return NULL;
}

// ============================================================
// 2.a � Flat (no batching)
// ============================================================
static long long run2a_flat_no_batching(void) {
    printf("\n=== A. Flat (UNBATCHED) ===\n");
    printf("N_TOTAL: %d\n", N_TOTAL);
    printf("Output grouping: 100 threads\n");

    long long start = now_ns();
    printf("Start time: %lld ns\n", start);

    // allocate pthread_t array of size N_TOTAL
    pthread_t *this = malloc(sizeof(*this) * N_TOTAL);
    
    // optionally allocate args array or reuse one per thread
    // allocate args array
    flat_arg_t *args = malloc(sizeof(*args) * N_TOTAL);

    // loop i = 0..N_TOTAL-1
    //   - atomic_fetch_add(&g_created, 1)
    //   - pthread_create(&ths[i], NULL, flat_worker, argptr)
    //   - handle rc with die_pthread
    for (int i = 0; i < N_TOTAL; i++) {
        args[i].id = i + 1;
        atomic_fetch_add(&g_created, 1);
        
        int rc = pthread_create(&this[i], NULL, flat_worker, &args[i]);
        die_pthread(rc, "pthread_create");
    }

    // join all threads (reverse order)
    //   - pthread_join(ths[i], NULL)
    //   - handle rc with die_pthread
    for (int i = N_TOTAL - 1; i >= 0; i--) {
        int rc = pthread_join(this[i], NULL);
        die_pthread(rc, "pthread_join");
    }
    
    // free allocations
    free(this);
    free(args);

    long long end = now_ns();
    // struct tm *info = localtime(&seconds);
    // // 3. Format the date/time string and the timezone offset
    // char time_str[20]; // Space for "YYYY-MM-DD HH:MM:SS"
    // char tz_str[10];   // Space for "-0700"
    // strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", info);
    // strftime(tz_str, sizeof(tz_str), "%z", info);

    // print_summary("2.a", start, end);
    printf("End time: %lld ns\n", end);
    print_summary("", start, end);

    // verify created == destroyed == N_TOTAL
    die_pthread((atomic_load(&g_created) == N_TOTAL && atomic_load(&g_destroyed) == N_TOTAL && atomic_load(&minimal_work_var)) > 0 ? 0 : -1, "pthread_count_mismatch");

    return end - start;
}

// ============================================================
// 2.a � Flat (batched)
// ============================================================
static void run2a_flat_batched(int batch_size) {
    printf("\n=== 2.a Flat (BATCHED), batch_size=%d ===\n", batch_size);
    long long start = now_ns();

    // TODO: create N_TOTAL threads in batches:
    // next = 0
    // while next < N_TOTAL:
    //   - batch_count = min(batch_size, N_TOTAL - next)
    //   - allocate pthread_t batch[batch_count] (heap or VLA if allowed)
    //   - create batch_count threads, start them
    //   - join the batch
    //   - next += batch_count
    // end while

    long long end = now_ns();
    print_summary("2.a(batched)", start, end);

    // TODO: verify created == destroyed == N_TOTAL
}

// ============================================================
// 2.b � Two-level hierarchy (parent -> children)
// ============================================================
typedef struct {
    int parent_id;
    // optional fields for sparse printing
} parent_arg_t;

typedef struct {
    int parent_id;
    int child_id;
} child_2b_arg_t;

static void *child_worker_2b(void *arg) {
    (void)arg;
    // minimal work
    volatile int x = 1;
    if (atomic_load(&minimal_work_var) % 2 == 0) x++;
    atomic_fetch_add(&minimal_work_var, x);

    // sparse printing
    // format: Parent 1 created children: 1-1 ... 1-25
    child_2b_arg_t *fa = (child_2b_arg_t *)arg;
    if (fa->child_id % 25 == 0) {
        printf("Parent %d created children: %d-%d ... %d-%d\n",
               fa->parent_id,
               fa->parent_id, fa->child_id - 24,
               fa->parent_id, fa->child_id);
    }

    atomic_fetch_add(&g_destroyed, 1);
    return NULL;
}

static void *parent_worker_2b_no_batching(void *arg) {
    // create B_CHILDREN_PER_PARENT child threads
    
    parent_arg_t *pa = (parent_arg_t *)arg;

    // allocate pthread_t children[B_CHILDREN_PER_PARENT]
    pthread_t *this = malloc(sizeof(*this) * B_CHILDREN_PER_PARENT);

    // allocate args array for threads
    child_2b_arg_t *args = malloc(sizeof(*args) * B_CHILDREN_PER_PARENT);

    // loop child_id: atomic_fetch_add(&g_created, 1); pthread_create(...)
    for (int i = 0; i < B_CHILDREN_PER_PARENT; i++) {
        args[i].parent_id = pa->parent_id;
        args[i].child_id  = i + 1;
        atomic_fetch_add(&g_created, 1);

        int rc = pthread_create(&this[i], NULL, child_worker_2b, &args[i]);
        die_pthread(rc, "pthread_create");
    }

    // join all children
    for (int i = B_CHILDREN_PER_PARENT - 1; i >= 0; i--) {
        int rc = pthread_join(this[i], NULL);
        die_pthread(rc, "pthread_join");
    }
    printf("\nParent %d joined   children: %d-99 ... %d-1\n", pa->parent_id, pa->parent_id, pa->parent_id);
    
    // free allocations
    free(pa);
    free(this);
    free(args);

    atomic_fetch_add(&g_destroyed, 1); // parent destroyed
    return NULL;
}

static long long run2b_two_level_no_batching(void) {
    printf("\n=== B. Two-level (UNBATCHED) ===\n");
    printf("Parents: %d\n", B_PARENTS);
    printf("Children per parent: %d\n", B_CHILDREN_PER_PARENT);
    printf("Output grouping: 25 threads\n");
    printf("Total threads: 5000\n");

    long long start = now_ns();
    printf("Start time: %lld ns\n", start);

    // allocate pthread_t parents[B_PARENTS]
    pthread_t *parents = malloc(sizeof(*parents) * B_PARENTS);

    // loop for parent_id in 1..B_PARENTS:
    for (int i=0; i < B_PARENTS; i++) {
        printf("Parent %d started\n", i);

        // allocate parent_arg_t on heap or static storage
        // allocating on heap
        parent_arg_t *pa = malloc(sizeof(*pa));
        pa->parent_id = i + 1;
        
        atomic_fetch_add(&g_created, 1); // parent

        // pthread_create parent thread -> parent_worker_2b_no_batching
        int rc = pthread_create(&parents[i], NULL, parent_worker_2b_no_batching, pa);
        die_pthread(rc, "pthread_create");

        printf("Parent %d completed\n", i);
    }

    // join all parents
    for (int i = B_PARENTS - 1; i >= 0; i--) {
        int rc = pthread_join(parents[i], NULL);
        die_pthread(rc, "pthread_join");
    }

    free(parents);

    long long end = now_ns();

    // print_summary("2.b", start, end);
    printf("End time: %lld ns\n", end);
    print_summary("", start, end);

    // verify created == destroyed == N_TOTAL
    die_pthread((atomic_load(&g_created) == N_TOTAL && atomic_load(&g_destroyed) == N_TOTAL && atomic_load(&minimal_work_var)) ? 0 : -1, "pthread_count_mismatch");

    return end - start;
}

// ============================================================
// 2.b � Two-level hierarchy (batched children, if needed)
// ============================================================
typedef struct {
    int parent_id;
    int child_batch_size;
} parent_batch_arg_t;

static void *parent_worker_2b_batched(void *arg) {
    parent_batch_arg_t *pa = (parent_batch_arg_t *)arg;

    // TODO: inside each parent:
    // nextChild = 1
    // while nextChild <= B_CHILDREN_PER_PARENT:
    //   - create up to child_batch_size children
    //   - join that batch
    // end while
    // TODO: free pa if heap-allocated

    atomic_fetch_add(&g_destroyed, 1); // parent destroyed
    return NULL;
}

static void run2b_two_level_batched(int child_batch_size) {
    printf("\n=== 2.b Two-level (BATCHED children), child_batch_size=%d ===\n", child_batch_size);
    long long start = now_ns();

    // TODO: same as run2b_two_level_no_batching, but parent uses parent_worker_2b_batched
    // and child_batch_size is passed in via parent_batch_arg_t.

    long long end = now_ns();
    print_summary("2.b(batched)", start, end);

    // TODO: verify created == destroyed == N_TOTAL
}

// ============================================================
// 2.c � Three-level hierarchy (initial -> child -> grandchild)
// ============================================================
typedef struct {
    int initial_id;
} initial_arg_t;

typedef struct {
    int initial_id;
    int child_id;
    int grand_batch_size; // used for batched version
} child_arg_t;

static void *grandchild_worker_2c(void *arg) {
    (void)arg;
    // TODO: minimal work
    atomic_fetch_add(&g_destroyed, 1);
    return NULL;
}

static void *child_worker_2c_no_batching(void *arg) {
    child_arg_t *ca = (child_arg_t *)arg;

    // TODO: create C_GRANDCHILDREN_PER_CHILD grandchild threads
    // TODO: join all grandchildren
    // TODO: free ca if heap-allocated

    atomic_fetch_add(&g_destroyed, 1); // child destroyed
    return NULL;
}

static void *initial_worker_2c_no_batching(void *arg) {
    initial_arg_t *ia = (initial_arg_t *)arg;

    // TODO: create C_CHILDREN_PER_INITIAL child threads
    //   - each child runs child_worker_2c_no_batching
    // TODO: join all children
    // TODO: free ia if heap-allocated

    atomic_fetch_add(&g_destroyed, 1); // initial destroyed
    return NULL;
}

static void run2c_three_level_no_batching(void) {
    printf("\n=== 2.c Three-level (no batching) ===\n");
    long long start = now_ns();

    // TODO: allocate pthread_t initials[C_INITIALS]
    // TODO: for initial_id in 1..C_INITIALS:
    //   - atomic_fetch_add(&g_created, 1) // initial
    //   - allocate initial_arg_t
    //   - pthread_create -> initial_worker_2c_no_batching
    // TODO: join all initials

    long long end = now_ns();
    print_summary("2.c", start, end);

    // TODO: verify created == destroyed == N_TOTAL
}

// ============================================================
// 2.c � Three-level hierarchy (batched grandchildren, if needed)
// ============================================================
static void *child_worker_2c_batched(void *arg) {
    child_arg_t *ca = (child_arg_t *)arg;

    // TODO: inside each child:
    // nextGrand = 1
    // while nextGrand <= C_GRANDCHILDREN_PER_CHILD:
    //   - create up to ca->grand_batch_size grandchildren
    //   - join that batch
    // end while
    // TODO: free ca if heap-allocated

    atomic_fetch_add(&g_destroyed, 1); // child destroyed
    return NULL;
}

static void *initial_worker_2c_batched(void *arg) {
    initial_arg_t *ia = (initial_arg_t *)arg;

    // TODO: create C_CHILDREN_PER_INITIAL children
    //   - each child runs child_worker_2c_batched with grand_batch_size
    // TODO: join all children
    // TODO: free ia if heap-allocated

    atomic_fetch_add(&g_destroyed, 1); // initial destroyed
    return NULL;
}

static void run2c_three_level_batched(int grand_batch_size) {
    printf("\n=== 2.c Three-level (BATCHED grandchildren), grand_batch_size=%d ===\n", grand_batch_size);
    long long start = now_ns();

    // TODO: same as run2c_three_level_no_batching, but initial uses initial_worker_2c_batched
    // and grand_batch_size is passed down to children via child_arg_t.

    long long end = now_ns();
    print_summary("2.c(batched)", start, end);

    // TODO: verify created == destroyed == N_TOTAL
}

// ============================================================
// main
// ============================================================
int main(void) {
    // allocate arrays to store trial times for each experiment
    long long *avg_times_2a = malloc(sizeof(long long) * 3);
    long long *avg_times_2b = malloc(sizeof(long long) * 3);
    long long *avg_times_2c = malloc(sizeof(long long) * 3);
    
    int total_threads_created = 0, total_threads_destroyed = 0;
    
    // run 3 trials each and compute averages in your report.
    for (int i = 0; i < 3; i++) {
        printf("\n\n[Trial %d]\n", i + 1);

        reset_counts();
        avg_times_2a[i] = run2a_flat_no_batching();
        // avg_times_2a[i] = run2a_flat_batched(A_BATCH_SIZE);
        total_threads_created += atomic_load(&g_created);
        total_threads_destroyed += atomic_load(&g_destroyed);

        reset_counts();
        // avg_times_2b[i] = 0;
        avg_times_2b[i] = run2b_two_level_no_batching();
        // run2b_two_level_batched(B_CHILD_BATCH_SIZE);
        total_threads_created += atomic_load(&g_created);
        total_threads_destroyed += atomic_load(&g_destroyed);

        reset_counts();
        avg_times_2c[i] = 0;
        // avg_times_2c[i] = run2c_three_level_no_batching();
        // run2c_three_level_batched(C_GRANDCHILD_BATCH_SIZE);
        total_threads_created += atomic_load(&g_created);
        total_threads_destroyed += atomic_load(&g_destroyed);

    }

    printf("\nTotal threads created across all experiments: %d\n", total_threads_created);
    printf("Total threads destroyed across all experiments: %d\n", total_threads_destroyed);

    long long avg_2a = 0, avg_2b = 0, avg_2c = 0;
    for (int i = 0; i < 3; i++) {
        avg_2a += avg_times_2a[i];
        avg_2b += avg_times_2b[i];
        avg_2c += avg_times_2c[i];
    }
    avg_2a /= 3;
    avg_2b /= 3;
    avg_2c /= 3;

    // print report summary in the following format
    // Experiment    Trial 1 (ms)  Trial 2 (ms)  Trial 3 (ms)  Average (ms)
    // 2.a Flat        123.456       234.567       345.678       234.567
    // 2.b Two-level   123.456       234.567       345.678       234.567
    // 2.c Three-level 123.456       234.567       345.678       234.567
    // Average (ms)    123.456       234.567       345.678       234.567

    printf("\nExperiment    Trial 1 (ms)  Trial 2 (ms)  Trial 3 (ms)  Average (ms)\n");
    printf("2.a Flat        %07.3f       %07.3f       %07.3f       %07.3f\n", avg_times_2a[0] / 1e6, avg_times_2a[1] / 1e6, avg_times_2a[2] / 1e6, avg_2a / 1e6);
    printf("2.b Two-level   %07.3f       %07.3f       %07.3f       %07.3f\n", avg_times_2b[0] / 1e6, avg_times_2b[1] / 1e6, avg_times_2b[2] / 1e6, avg_2b / 1e6);
    printf("2.c Three-level %07.3f       %07.3f       %07.3f       %07.3f\n", avg_times_2c[0] / 1e6, avg_times_2c[1] / 1e6, avg_times_2c[2] / 1e6, avg_2c / 1e6);
    printf("Average (ms)    %07.3f       %07.3f       %07.3f       %07.3f\n", avg_2a / 1e6, avg_2b / 1e6, avg_2c / 1e6, (avg_2a + avg_2b + avg_2c) / 3 / 1e6);

    free(avg_times_2a);
    free(avg_times_2b);
    free(avg_times_2c);
    return 0;
}