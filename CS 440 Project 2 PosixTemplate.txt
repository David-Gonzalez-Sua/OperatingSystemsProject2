/*
 * CS 440 Project 2 — POSIX (pthreads) TEMPLATE
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
// TODO: verify math: parents + parents*children_per_parent == 5000
enum { B_PARENTS = 50, B_CHILDREN_PER_PARENT = 99 };

// ======= 2.c parameters (must total exactly 5000) =======
// TODO: verify math:
// initials + initials*children_per_initial + initials*children_per_initial*grandchildren_per_child == 5000
enum { C_INITIALS = 20, C_CHILDREN_PER_INITIAL = 3, C_GRANDCHILDREN_PER_CHILD = 82 };

// ======= Batching knobs (reduce concurrency if needed) =======
enum { A_BATCH_SIZE = 25, B_CHILD_BATCH_SIZE = 25, C_GRANDCHILD_BATCH_SIZE = 25 };

// ======= Counters =======
static atomic_int g_created = 0;
static atomic_int g_destroyed = 0;

// ------------------------------------------------------------
// Timing (POSIX monotonic clock)
// ------------------------------------------------------------
static long long now_ns(void) {
    struct timespec ts;
    // TODO: handle error if clock_gettime fails (rare)
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + (long long)ts.tv_nsec;
}

static void reset_counts(void) {
    atomic_store(&g_created, 0);
    atomic_store(&g_destroyed, 0);
}

static void print_summary(const char *label, long long start_ns, long long end_ns) {
    double elapsed_ms = (end_ns - start_ns) / 1e6;
    printf("%s elapsed: %.3f ms\n", label, elapsed_ms);
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
// 2.a — Flat workers
// ============================================================
typedef struct {
    int id; // optional
} flat_arg_t;

static void *flat_worker(void *arg) {
    (void)arg;
    // TODO: optional minimal work
    // TODO: optional sparse print using id
    atomic_fetch_add(&g_destroyed, 1);
    return NULL;
}

// ============================================================
// 2.a — Flat (no batching)
// ============================================================
static void run2a_flat_no_batching(void) {
    printf("\n=== 2.a Flat (no batching) ===\n");
    long long start = now_ns();

    // TODO: allocate pthread_t array of size N_TOTAL
    // pthread_t *ths = malloc(sizeof(*ths) * N_TOTAL);
    // TODO: optionally allocate args array or reuse one per thread

    // TODO: loop i = 0..N_TOTAL-1
    //   - atomic_fetch_add(&g_created, 1)
    //   - pthread_create(&ths[i], NULL, flat_worker, argptr)
    //   - handle rc with die_pthread

    // TODO: join all threads (optionally reverse order)
    //   - pthread_join(ths[i], NULL)

    // TODO: free allocations

    long long end = now_ns();
    print_summary("2.a", start, end);

    // TODO: verify created == destroyed == N_TOTAL
}

// ============================================================
// 2.a — Flat (batched)
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
// 2.b — Two-level hierarchy (parent -> children)
// ============================================================
typedef struct {
    int parent_id;
    // TODO: optional fields for sparse printing
} parent_arg_t;

static void *child_worker_2b(void *arg) {
    (void)arg;
    // TODO: minimal work
    atomic_fetch_add(&g_destroyed, 1);
    return NULL;
}

static void *parent_worker_2b_no_batching(void *arg) {
    parent_arg_t *pa = (parent_arg_t *)arg;

    // TODO: create B_CHILDREN_PER_PARENT child threads
    //   - allocate pthread_t children[B_CHILDREN_PER_PARENT]
    //   - loop child_id: atomic_fetch_add(&g_created, 1); pthread_create(...)
    // TODO: join all children
    // TODO: free pa if heap-allocated

    atomic_fetch_add(&g_destroyed, 1); // parent destroyed
    return NULL;
}

static void run2b_two_level_no_batching(void) {
    printf("\n=== 2.b Two-level (no batching) ===\n");
    long long start = now_ns();

    // TODO: allocate pthread_t parents[B_PARENTS]
    // TODO: for parent_id in 1..B_PARENTS:
    //   - atomic_fetch_add(&g_created, 1) // parent
    //   - allocate parent_arg_t on heap or static storage
    //   - pthread_create parent thread -> parent_worker_2b_no_batching
    // TODO: join all parents

    long long end = now_ns();
    print_summary("2.b", start, end);

    // TODO: verify created == destroyed == N_TOTAL
}

// ============================================================
// 2.b — Two-level hierarchy (batched children, if needed)
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
// 2.c — Three-level hierarchy (initial -> child -> grandchild)
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
// 2.c — Three-level hierarchy (batched grandchildren, if needed)
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
    // TODO: run 3 trials each and compute averages in your report.

    reset_counts();
    run2a_flat_no_batching();
    // reset_counts();
    // run2a_flat_batched(A_BATCH_SIZE);

    reset_counts();
    run2b_two_level_no_batching();
    // reset_counts();
    // run2b_two_level_batched(B_CHILD_BATCH_SIZE);

    reset_counts();
    run2c_three_level_no_batching();
    // reset_counts();
    // run2c_three_level_batched(C_GRANDCHILD_BATCH_SIZE);

    return 0;
}
