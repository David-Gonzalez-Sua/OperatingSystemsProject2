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

#include <stdarg.h>

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

// ======= Mutexes and other vars for a buffered sparse output =======
static pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;

// buffer for sparse output : 50 parents -> 7 lines per -> "started", "created children"*4, "joined children", "completed"
// max of 50 characters per line 
static char output_dict[50][7][50];

// 20 parents -> 3 children each -> 82 grandchildren each = 5000 threads total
// buffer for sparse output : 2 lines per parent -> "started", "completed"
// buffer for sparse output : 7 lines per child -> "created child", "created grandchildren"*4, "joined grandchildren", " child completed"
// max of 128 characters per line 
static char initials_dict[20][2][128];
static char children_dict[20][3][7][128];

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
    die_pthread((atomic_load(&g_created) == N_TOTAL && atomic_load(&g_destroyed) == N_TOTAL && atomic_load(&minimal_work_var) > 0) ? 0 : -1, "pthread_count_mismatch");

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

        // sparse printing for children creation
        if ((i + 1) % 25 == 0 || i + 1 == B_CHILDREN_PER_PARENT) {
            pthread_mutex_lock(&print_lock);
            int start = i + 1 - ((i + 1) % 25 == 0 ? 24 : ((i + 1) % 25) - 1);

            snprintf(output_dict[pa->parent_id - 1][(i + 1) / 25], 50, "Parent %d created children: %d-%d ... %d-%d\n",
                    pa->parent_id,
                    pa->parent_id, start,
                    pa->parent_id, i + 1);

            pthread_mutex_unlock(&print_lock);
        }
    }

    // join all children
    for (int i = B_CHILDREN_PER_PARENT - 1; i >= 0; i--) {
        int rc = pthread_join(this[i], NULL);
        die_pthread(rc, "pthread_join");
    }

    // sparse print for parent joined children
    pthread_mutex_lock(&print_lock);
    snprintf(output_dict[pa->parent_id - 1][5], 50, "Parent %d joined   children: %d-99 ... %d-1\n",
        pa->parent_id,
        pa->parent_id,
        pa->parent_id);
    pthread_mutex_unlock(&print_lock);
        
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
    printf("Start time: %lld ns\n\n", start);

    // allocate pthread_t parents[B_PARENTS]
    pthread_t *parents = malloc(sizeof(*parents) * B_PARENTS);

    // loop for parent_id in 1..B_PARENTS:
    for (int i=0; i < B_PARENTS; i++) {
        pthread_mutex_lock(&print_lock);
        snprintf(output_dict[i][0], 50, "Parent %d started\n", i + 1);
        pthread_mutex_unlock(&print_lock);

        // allocate parent_arg_t on heap or static storage
        // allocating on heap
        parent_arg_t *pa = malloc(sizeof(*pa));
        pa->parent_id = i + 1;
        
        atomic_fetch_add(&g_created, 1); // parent

        // pthread_create parent thread -> parent_worker_2b_no_batching
        int rc = pthread_create(&parents[i], NULL, parent_worker_2b_no_batching, pa);
        die_pthread(rc, "pthread_create");

        // printf("Parent %d completed\n", i);
        pthread_mutex_lock(&print_lock);
        snprintf(output_dict[i][6], 50, "Parent %d completed\n", i + 1);
        pthread_mutex_unlock(&print_lock);
    }

    // join all parents
    for (int i = B_PARENTS - 1; i >= 0; i--) {
        int rc = pthread_join(parents[i], NULL);
        die_pthread(rc, "pthread_join");
    }

    free(parents);

    long long end = now_ns();

    // print all buffered output in order
    for (int i = 0; i < B_PARENTS; i++) {
        for (int j = 0; j < 7; j++) {
            if (output_dict[i][j][0] != '\0') {
                printf("%s", output_dict[i][j]);
            }
        }
        printf("\n");
    }

    // RESET output_dict for next experiment
    memset(output_dict, 0, sizeof(output_dict));

    // print_summary("2.b", start, end);
    printf("End time: %lld ns\n", end);
    print_summary("", start, end);

    // verify created == destroyed == N_TOTAL
    die_pthread((atomic_load(&g_created) == N_TOTAL && atomic_load(&g_destroyed) == N_TOTAL && atomic_load(&minimal_work_var) > 0) ? 0 : -1, "pthread_count_mismatch");

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

typedef struct {
    int initial_id;
    int child_id;
    int grandchild_id;
} grandchild_arg_t;

static void *grandchild_worker_2c(void *arg) {
    (void)arg;

    // minimal work
    volatile int x = 1;
    if (atomic_load(&minimal_work_var) % 2 == 0) x++;
    atomic_fetch_add(&minimal_work_var, x);

    atomic_fetch_add(&g_destroyed, 1);
    return NULL;
}

static void *child_worker_2c_no_batching(void *arg) {
    child_arg_t *ca = (child_arg_t *)arg;

    // create C_GRANDCHILDREN_PER_CHILD grandchild threads
    pthread_t *grandchildren = malloc(sizeof(*grandchildren) * C_GRANDCHILDREN_PER_CHILD);
    grandchild_arg_t *args = malloc(sizeof(*args) * C_GRANDCHILDREN_PER_CHILD);

    for (int i = 0; i < C_GRANDCHILDREN_PER_CHILD; i++) {
        args[i].initial_id = ca->initial_id;
        args[i].child_id = ca->child_id;
        args[i].grandchild_id = i + 1;
        atomic_fetch_add(&g_created, 1);

        int rc = pthread_create(&grandchildren[i], NULL, grandchild_worker_2c, &args[i]);
        die_pthread(rc, "pthread_create grandchild");

        // sparse printing for grandchildren creation
        if ((i + 1) % 25 == 0 || i + 1 == C_GRANDCHILDREN_PER_CHILD) {
            pthread_mutex_lock(&print_lock);
            int start = i + 1 - ((i + 1) % 25 == 0 ? 24 : ((i + 1) % 25) - 1);

            snprintf(children_dict[(ca->initial_id - 1)][(ca->child_id - 1)][(i + 1) / 25], 128,
                    "Initial %d Child %d created grandchildren: %d-%d-%d ... %d-%d-%d\n",
                    ca->initial_id, ca->child_id,
                    ca->initial_id, ca->child_id, start,
                    ca->initial_id, ca->child_id, i + 1);
        
            pthread_mutex_unlock(&print_lock);
        }
    }

    // join all grandchildren
    for (int i = C_GRANDCHILDREN_PER_CHILD - 1; i >= 0; i--) {
        int rc = pthread_join(grandchildren[i], NULL);
        die_pthread(rc, "pthread_join grandchild");
    }

    // sparse print for child joined grandchildren
    pthread_mutex_lock(&print_lock);
    snprintf(children_dict[(ca->initial_id - 1)][(ca->child_id - 1)][5], 128,
            "Initial %d Child %d joined   grandchildren: %d-%d-%d ... %d-%d-%d\n",
            ca->initial_id, ca->child_id,
            ca->initial_id, ca->child_id, 1,
            ca->initial_id, ca->child_id, C_GRANDCHILDREN_PER_CHILD);
    pthread_mutex_unlock(&print_lock);

    // free ca if heap-allocated
    free(ca);
    free(grandchildren);
    free(args);

    atomic_fetch_add(&g_destroyed, 1); // child destroyed
    return NULL;
}

static void *initial_worker_2c_no_batching(void *arg) {
    initial_arg_t *ia = (initial_arg_t *)arg;

    // create C_CHILDREN_PER_INITIAL child threads
    //   - each child runs child_worker_2c_no_batching
    pthread_t *children = malloc(sizeof(*children) * C_CHILDREN_PER_INITIAL);
    child_arg_t *args = malloc(sizeof(*args) * C_CHILDREN_PER_INITIAL);

    for (int i = 0; i < C_CHILDREN_PER_INITIAL; i++) {
        args[i].initial_id = ia->initial_id;
        args[i].child_id = i + 1;
        atomic_fetch_add(&g_created, 1);

        int rc = pthread_create(&children[i], NULL, child_worker_2c_no_batching, &args[i]);
        die_pthread(rc, "pthread_create child");

        // sparse printing for child creation
        pthread_mutex_lock(&print_lock);
        snprintf(initials_dict[ia->initial_id - 1][0], 128,
                "Initial %d started and created child %d\n",
                ia->initial_id, i + 1);
        pthread_mutex_unlock(&print_lock);
    }

    // join all children
    for (int i = C_CHILDREN_PER_INITIAL - 1; i >= 0; i--) {
        int rc = pthread_join(children[i], NULL);
        die_pthread(rc, "pthread_join child");
    }

    // sparse print for initial joined children
    pthread_mutex_lock(&print_lock);
    snprintf(initials_dict[ia->initial_id - 1][1], 128,
            "Initial %d completed and joined children 1-%d\n",
            ia->initial_id, C_CHILDREN_PER_INITIAL);
    pthread_mutex_unlock(&print_lock);

    // free ia if heap-allocated
    free(ia);
    free(children);
    free(args);

    atomic_fetch_add(&g_destroyed, 1); // initial destroyed
    return NULL;
}

static long long run2c_three_level_no_batching(void) {
    printf("\n=== C. Three-level (UNBATCHED) ===\n");
    printf("Initial threads: %d\n", C_INITIALS);
    printf("Children per initial: %d\n", C_CHILDREN_PER_INITIAL);
    printf("Grandchildren per child: %d\n", C_GRANDCHILDREN_PER_CHILD);
    printf("Output grouping: 25 threads\n");
    printf("Total threads: 5000\n");

    long long start = now_ns();
    printf("Start time: %lld ns\n\n", start);

    // allocate pthread_t initials[C_INITIALS]
    pthread_t *initials = malloc(sizeof(*initials) * C_INITIALS);

    // for initial_id in 1..C_INITIALS:
    //   - atomic_fetch_add(&g_created, 1) // initial
    //   - allocate initial_arg_t
    //   - pthread_create -> initial_worker_2c_no_batching
    for (int i = 0; i < C_INITIALS; i++) {
        pthread_mutex_lock(&print_lock);
        snprintf(initials_dict[i][0], 128, "Initial %d started\n", i + 1);
        pthread_mutex_unlock(&print_lock);

        initial_arg_t *ia = malloc(sizeof(*ia));
        ia->initial_id = i + 1;
        atomic_fetch_add(&g_created, 1);

        int rc = pthread_create(&initials[i], NULL, initial_worker_2c_no_batching, ia);
        die_pthread(rc, "pthread_create initial");

        pthread_mutex_lock(&print_lock);
        snprintf(initials_dict[i][0], 128, "Initial %d Completed\n", i + 1);
        pthread_mutex_unlock(&print_lock);
    }
    
    // join all initials
    for (int i = C_INITIALS - 1; i >= 0; i--) {
        int rc = pthread_join(initials[i], NULL);
        die_pthread(rc, "pthread_join initial");
    }

    free(initials);

    long long end = now_ns();

    // print all buffered output in order
    // for (int i = 0; i < C_INITIALS; i++) {
    //     for (int j = 0; j < 2; j++) {
    //         if (initials_dict[i][j][0] != '\0') {
    //             printf("%s", initials_dict[i][j]);
    //         }
    //     }
    //     for (int c = 0; c < C_CHILDREN_PER_INITIAL; c++) {
    //         for (int j = 0; j < 7; j++) {
    //             if (children_dict[i * C_CHILDREN_PER_INITIAL + c][j][0] != '\0') {
    //                 printf("%s", children_dict[i * C_CHILDREN_PER_INITIAL + c][j]);
    //             }
    //         }
    //     }
    //     printf("\n");
    // }

    for (int i = 0; i < C_INITIALS; i++) {
        printf("%s", initials_dict[i][0]);
        for (int c = 0; c < C_CHILDREN_PER_INITIAL; c++) {
            for (int j = 0; j < 7; j++) {
                printf("%s", children_dict[i][c][j]);
            }
        }
    }

    // RESET initials_dict and children_dict for next experiment
    memset(initials_dict, 0, sizeof(initials_dict));
    memset(children_dict, 0, sizeof(children_dict));

    // print_summary("2.c", start, end);
    printf("End time: %lld ns\n", end);
    print_summary("", start, end);

    // verify created == destroyed == N_TOTAL
    die_pthread((atomic_load(&g_created) == N_TOTAL && atomic_load(&g_destroyed) == N_TOTAL && atomic_load(&minimal_work_var) > 0) ? 0 : -1, "pthread_count_mismatch");

    return end - start;
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
        // avg_times_2c[i] = 0;
        avg_times_2c[i] = run2c_three_level_no_batching();
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