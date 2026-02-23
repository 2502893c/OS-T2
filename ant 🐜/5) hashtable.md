# Task 5: Multi-Threaded Hash Table & Synchronization - 20 Alternate Versions

## Original Task Summary

The original task requires:
- Start with `ph-without-locks.c`: a hash table with 5 buckets (NBUCKET) and 100,000 keys (NKEYS) that loses keys under concurrent access
- Copy to `ph-with-mutex-locks.c` and add per-bucket mutex locks to eliminate missing keys
- Use `pthread_mutex_t`, one lock per bucket (not one global lock)
- Lock only the bucket being accessed in `put()` so threads operating on different buckets can run in parallel
- `get()` does not need locks (read-only phase after all puts complete)
- Goal: 0 missing keys AND parallel speedup > 1.25x with 2 threads

**Core solution pattern:**
```c
pthread_mutex_t locks[NBUCKET]; // one lock per bucket

// In put():
int i = key % NBUCKET;
pthread_mutex_lock(&locks[i]);
insert(key, value, &table[i], table[i]);
pthread_mutex_unlock(&locks[i]);
```

---

## Alternate 1: Read-Write Locks Instead of Mutexes

**What changed:** Instead of `pthread_mutex_t`, use `pthread_rwlock_t` per bucket. `put()` takes a write lock, `get()` takes a read lock. This allows multiple concurrent readers on the same bucket.

```c
// notxv6/ph-with-rwlocks.c
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>

#define NBUCKET 5
#define NKEYS 100000

struct entry {
    int key;
    int value;
    struct entry *next;
};

struct entry *table[NBUCKET];

// --- CHANGED: rwlock per bucket instead of mutex ---
pthread_rwlock_t locks[NBUCKET];
// --- END CHANGED ---

int keys[NKEYS];
int nthread = 1;

double now()
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

static void insert(int key, int value, struct entry **p, struct entry *n)
{
    struct entry *e = malloc(sizeof(struct entry));
    e->key = key;
    e->value = value;
    e->next = n;
    *p = e;
}

static void put(int key, int value)
{
    int i = key % NBUCKET;

    // --- CHANGED: Write lock for put ---
    pthread_rwlock_wrlock(&locks[i]);
    // --- END CHANGED ---

    struct entry *e = 0;
    for (e = table[i]; e != 0; e = e->next) {
        if (e->key == key) break;
    }
    if (e) {
        e->value = value;
    } else {
        insert(key, value, &table[i], table[i]);
    }

    // --- CHANGED: Unlock write lock ---
    pthread_rwlock_unlock(&locks[i]);
    // --- END CHANGED ---
}

static struct entry* get(int key)
{
    int i = key % NBUCKET;

    // --- CHANGED: Read lock for get (allows concurrent readers) ---
    pthread_rwlock_rdlock(&locks[i]);
    // --- END CHANGED ---

    struct entry *e = 0;
    for (e = table[i]; e != 0; e = e->next) {
        if (e->key == key) break;
    }

    // --- CHANGED: Unlock read lock ---
    pthread_rwlock_unlock(&locks[i]);
    // --- END CHANGED ---

    return e;
}

static void *put_thread(void *xa)
{
    int n = (int)(long)xa;
    int b = NKEYS / nthread;
    int k = 0;
    for (int i = 0; i < b; i++) {
        put(keys[b * n + i], n);
        k++;
    }
    return (void *)(long)k;
}

static void *get_thread(void *xa)
{
    int n = (int)(long)xa;
    int missing = 0;
    for (int i = 0; i < NKEYS; i++) {
        struct entry *e = get(keys[i]);
        if (e == 0) missing++;
    }
    printf("%d: %d keys missing\n", n, missing);
    return (void *)(long)missing;
}

int main(int argc, char *argv[])
{
    pthread_t *tha, *thb;
    void *value;
    double t1, t0;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s nthreads\n", argv[0]);
        exit(-1);
    }
    nthread = atoi(argv[1]);
    tha = malloc(sizeof(pthread_t) * nthread);
    thb = malloc(sizeof(pthread_t) * nthread);

    assert(NKEYS % nthread == 0);
    for (int i = 0; i < NKEYS; i++)
        keys[i] = random();

    // --- CHANGED: Init rwlocks ---
    for (int i = 0; i < NBUCKET; i++)
        pthread_rwlock_init(&locks[i], NULL);
    // --- END CHANGED ---

    t0 = now();
    for (int i = 0; i < nthread; i++)
        assert(pthread_create(&tha[i], NULL, put_thread, (void *)(long)i) == 0);
    for (int i = 0; i < nthread; i++)
        assert(pthread_join(tha[i], &value) == 0);
    t1 = now();
    printf("%d puts, %.3f seconds, %.0f puts/second\n",
           NKEYS, t1 - t0, NKEYS / (t1 - t0));

    t0 = now();
    for (int i = 0; i < nthread; i++)
        assert(pthread_create(&thb[i], NULL, get_thread, (void *)(long)i) == 0);
    for (int i = 0; i < nthread; i++)
        assert(pthread_join(thb[i], &value) == 0);
    t1 = now();
    printf("%d gets, %.3f seconds, %.0f gets/second\n",
           nthread * NKEYS, t1 - t0, (nthread * NKEYS) / (t1 - t0));

    // --- CHANGED: Destroy rwlocks ---
    for (int i = 0; i < NBUCKET; i++)
        pthread_rwlock_destroy(&locks[i]);
    // --- END CHANGED ---

    free(tha);
    free(thb);
    return 0;
}
```

---

## Alternate 2: Single Global Lock (Coarse-Grained)

**What changed:** Instead of per-bucket locks, use ONE global mutex for the entire hash table. Safe but slow (no parallel speedup). Demonstrates the tradeoff.

```c
// notxv6/ph-with-global-lock.c
// ... same includes, structs, keys, table as original ...

// --- CHANGED: Single global lock instead of per-bucket ---
pthread_mutex_t global_lock;
// --- END CHANGED ---

static void put(int key, int value)
{
    int i = key % NBUCKET;

    // --- CHANGED: Lock the entire table ---
    pthread_mutex_lock(&global_lock);
    // --- END CHANGED ---

    struct entry *e = 0;
    for (e = table[i]; e != 0; e = e->next) {
        if (e->key == key) break;
    }
    if (e) {
        e->value = value;
    } else {
        insert(key, value, &table[i], table[i]);
    }

    // --- CHANGED: Unlock global ---
    pthread_mutex_unlock(&global_lock);
    // --- END CHANGED ---
}

// get() unchanged (no locks needed, read-only phase)

int main(int argc, char *argv[])
{
    // ... same setup ...

    // --- CHANGED: Init single global lock ---
    pthread_mutex_init(&global_lock, NULL);
    // --- END CHANGED ---

    // ... run put threads, get threads ...

    // --- CHANGED: Destroy single global lock ---
    pthread_mutex_destroy(&global_lock);
    // --- END CHANGED ---

    return 0;
}
```

---

## Alternate 3: Spinlock Instead of Mutex

**What changed:** Use `pthread_spinlock_t` per bucket instead of `pthread_mutex_t`. Spinlocks busy-wait instead of sleeping, which can be faster for very short critical sections.

```c
// notxv6/ph-with-spinlocks.c
// ... same includes, structs ...

// --- CHANGED: Spinlock per bucket ---
pthread_spinlock_t locks[NBUCKET];
// --- END CHANGED ---

static void put(int key, int value)
{
    int i = key % NBUCKET;

    // --- CHANGED: Spin lock/unlock ---
    pthread_spin_lock(&locks[i]);
    // --- END CHANGED ---

    struct entry *e = 0;
    for (e = table[i]; e != 0; e = e->next) {
        if (e->key == key) break;
    }
    if (e) {
        e->value = value;
    } else {
        insert(key, value, &table[i], table[i]);
    }

    // --- CHANGED ---
    pthread_spin_unlock(&locks[i]);
    // --- END CHANGED ---
}

int main(int argc, char *argv[])
{
    // ... setup ...

    // --- CHANGED: Init spinlocks ---
    for (int i = 0; i < NBUCKET; i++)
        pthread_spin_init(&locks[i], PTHREAD_PROCESS_PRIVATE);
    // --- END CHANGED ---

    // ... run threads ...

    // --- CHANGED: Destroy spinlocks ---
    for (int i = 0; i < NBUCKET; i++)
        pthread_spin_destroy(&locks[i]);
    // --- END CHANGED ---

    return 0;
}
```

---

## Alternate 4: More Buckets (Reduced Contention)

**What changed:** Increase NBUCKET from 5 to 101 (a prime number). More buckets means shorter chains and lower lock contention, demonstrating how data structure design affects concurrency.

```c
// notxv6/ph-more-buckets.c
// ... same includes ...

// --- CHANGED: 101 buckets instead of 5 ---
#define NBUCKET 101
// --- END CHANGED ---
#define NKEYS 100000

struct entry {
    int key;
    int value;
    struct entry *next;
};

struct entry *table[NBUCKET];
pthread_mutex_t locks[NBUCKET]; // CHANGED: 101 locks

// ... put, get, insert same as original per-bucket lock solution ...
// (key % NBUCKET now distributes across 101 buckets)

int main(int argc, char *argv[])
{
    // ... setup ...

    // CHANGED: Init 101 locks
    for (int i = 0; i < NBUCKET; i++)
        pthread_mutex_init(&locks[i], NULL);

    // ... run threads ...

    for (int i = 0; i < NBUCKET; i++)
        pthread_mutex_destroy(&locks[i]);

    return 0;
}
```

---

## Alternate 5: Lock-Free Insert Using __sync_bool_compare_and_swap

**What changed:** Instead of mutexes, use GCC's atomic compare-and-swap (`__sync_bool_compare_and_swap`) for lock-free insertion into the linked list.

```c
// notxv6/ph-lockfree.c
// ... same includes, structs ...

// --- CHANGED: No locks at all, use atomic CAS ---
static void put(int key, int value)
{
    int i = key % NBUCKET;

    struct entry *e = 0;
    for (e = table[i]; e != 0; e = e->next) {
        if (e->key == key) break;
    }
    if (e) {
        e->value = value;
    } else {
        // CHANGED: Lock-free insert with CAS retry loop
        struct entry *new_entry = malloc(sizeof(struct entry));
        new_entry->key = key;
        new_entry->value = value;
        struct entry *old_head;
        do {
            old_head = table[i];
            new_entry->next = old_head;
        } while (!__sync_bool_compare_and_swap(&table[i], old_head, new_entry));
    }
}
// --- END CHANGED ---

// get() unchanged
// main() unchanged (no lock init/destroy needed)
```

---

## Alternate 6: Condition Variable for Phased Execution

**What changed:** Added a pthread barrier between the put phase and get phase, and a condition variable that signals when all puts are done. Demonstrates synchronization beyond just mutexes.

```c
// notxv6/ph-with-barrier.c
// ... same includes ...

pthread_mutex_t locks[NBUCKET];

// --- CHANGED: Barrier between put and get phases ---
pthread_barrier_t phase_barrier;
// --- END CHANGED ---

// put() and get() same as original per-bucket solution

static void *combined_thread(void *xa)
{
    int n = (int)(long)xa;

    // Put phase
    int b = NKEYS / nthread;
    for (int i = 0; i < b; i++)
        put(keys[b * n + i], n);

    // --- CHANGED: Wait for all threads to finish puts before gets ---
    pthread_barrier_wait(&phase_barrier);
    // --- END CHANGED ---

    // Get phase
    int missing = 0;
    for (int i = 0; i < NKEYS; i++) {
        struct entry *e = get(keys[i]);
        if (e == 0) missing++;
    }
    printf("%d: %d keys missing\n", n, missing);

    return NULL;
}

int main(int argc, char *argv[])
{
    // ... setup, parse nthread ...

    for (int i = 0; i < NBUCKET; i++)
        pthread_mutex_init(&locks[i], NULL);

    // --- CHANGED: Init barrier for nthread threads ---
    pthread_barrier_init(&phase_barrier, NULL, nthread);
    // --- END CHANGED ---

    pthread_t *th = malloc(sizeof(pthread_t) * nthread);
    double t0 = now();
    for (int i = 0; i < nthread; i++)
        pthread_create(&th[i], NULL, combined_thread, (void *)(long)i);
    for (int i = 0; i < nthread; i++)
        pthread_join(th[i], NULL);
    double t1 = now();

    printf("total time: %.3f seconds\n", t1 - t0);

    // --- CHANGED: Destroy barrier ---
    pthread_barrier_destroy(&phase_barrier);
    // --- END CHANGED ---

    for (int i = 0; i < NBUCKET; i++)
        pthread_mutex_destroy(&locks[i]);

    free(th);
    return 0;
}
```

---

## Alternate 7: Delete Operation With Locking

**What changed:** Added a `delete(int key)` function that removes a key from the hash table with proper per-bucket locking. A third phase deletes half the keys after gets.

```c
// notxv6/ph-with-delete.c
// ... same includes, structs, per-bucket locks ...

// --- CHANGED: New delete function ---
static int delete(int key)
{
    int i = key % NBUCKET;
    pthread_mutex_lock(&locks[i]);

    struct entry **pp = &table[i];
    struct entry *e;
    for (e = *pp; e != 0; pp = &e->next, e = e->next) {
        if (e->key == key) {
            *pp = e->next; // unlink
            free(e);
            pthread_mutex_unlock(&locks[i]);
            return 1; // found and deleted
        }
    }

    pthread_mutex_unlock(&locks[i]);
    return 0; // not found
}
// --- END CHANGED ---

// --- CHANGED: Delete thread function ---
static void *delete_thread(void *xa)
{
    int n = (int)(long)xa;
    int b = NKEYS / nthread;
    int deleted = 0;
    // Delete every other key in this thread's range
    for (int i = 0; i < b; i += 2) {
        if (delete(keys[b * n + i]))
            deleted++;
    }
    printf("%d: deleted %d keys\n", n, deleted);
    return NULL;
}
// --- END CHANGED ---

int main(int argc, char *argv[])
{
    // ... setup, put phase, get phase same as original ...

    // --- CHANGED: Third phase: delete half the keys ---
    printf("\nDelete phase:\n");
    pthread_t *thd = malloc(sizeof(pthread_t) * nthread);
    double t0 = now();
    for (int i = 0; i < nthread; i++)
        pthread_create(&thd[i], NULL, delete_thread, (void *)(long)i);
    for (int i = 0; i < nthread; i++)
        pthread_join(thd[i], NULL);
    double t1 = now();
    printf("delete phase: %.3f seconds\n", t1 - t0);
    free(thd);
    // --- END CHANGED ---

    // cleanup ...
    return 0;
}
```

---

## Alternate 8: 10 Buckets and 200,000 Keys

**What changed:** Doubled the key count to 200,000 and increased buckets to 10. Tests scalability under heavier load.

```c
// notxv6/ph-heavy-load.c

// --- CHANGED: More buckets and more keys ---
#define NBUCKET 10
#define NKEYS 200000
// --- END CHANGED ---

// Everything else (per-bucket locks, put, get, main) identical to original solution
// The only difference is the scale: 200K keys across 10 buckets
```

---

## Alternate 9: Trylock With Retry

**What changed:** Instead of blocking on `pthread_mutex_lock`, use `pthread_mutex_trylock` in a loop. If the lock is busy, skip to the next key and retry later. Demonstrates non-blocking locking.

```c
// notxv6/ph-trylock.c
// ... same includes, structs, per-bucket locks ...

// --- CHANGED: put_with_trylock uses non-blocking lock attempts ---
#define MAX_DEFERRED 1024

static void put_batch(int *deferred_keys, int *deferred_values, int count)
{
    // Retry deferred keys
    for (int d = 0; d < count; d++) {
        int key = deferred_keys[d];
        int value = deferred_values[d];
        int i = key % NBUCKET;
        pthread_mutex_lock(&locks[i]); // blocking this time
        struct entry *e = 0;
        for (e = table[i]; e != 0; e = e->next)
            if (e->key == key) break;
        if (e)
            e->value = value;
        else
            insert(key, value, &table[i], table[i]);
        pthread_mutex_unlock(&locks[i]);
    }
}

static void *put_thread(void *xa)
{
    int n = (int)(long)xa;
    int b = NKEYS / nthread;
    int deferred_keys[MAX_DEFERRED];
    int deferred_values[MAX_DEFERRED];
    int deferred_count = 0;

    for (int idx = 0; idx < b; idx++) {
        int key = keys[b * n + idx];
        int i = key % NBUCKET;

        // CHANGED: Try to acquire lock without blocking
        if (pthread_mutex_trylock(&locks[i]) == 0) {
            struct entry *e = 0;
            for (e = table[i]; e != 0; e = e->next)
                if (e->key == key) break;
            if (e)
                e->value = n;
            else
                insert(key, n, &table[i], table[i]);
            pthread_mutex_unlock(&locks[i]);
        } else {
            // CHANGED: Defer this key for later retry
            if (deferred_count < MAX_DEFERRED) {
                deferred_keys[deferred_count] = key;
                deferred_values[deferred_count] = n;
                deferred_count++;
            }
            if (deferred_count >= MAX_DEFERRED) {
                put_batch(deferred_keys, deferred_values, deferred_count);
                deferred_count = 0;
            }
        }
    }
    // Flush remaining deferred
    if (deferred_count > 0)
        put_batch(deferred_keys, deferred_values, deferred_count);

    return NULL;
}
// --- END CHANGED ---
```

---

## Alternate 10: Thread-Local Insertion Counts

**What changed:** Each thread tracks how many insertions and updates it performed using thread-local storage, and prints a per-thread report.

```c
// notxv6/ph-with-stats.c
// ... same per-bucket lock solution ...

// --- CHANGED: Thread-local counters ---
__thread int local_inserts = 0;
__thread int local_updates = 0;
// --- END CHANGED ---

static void put(int key, int value)
{
    int i = key % NBUCKET;
    pthread_mutex_lock(&locks[i]);

    struct entry *e = 0;
    for (e = table[i]; e != 0; e = e->next) {
        if (e->key == key) break;
    }
    if (e) {
        e->value = value;
        local_updates++; // CHANGED: track updates
    } else {
        insert(key, value, &table[i], table[i]);
        local_inserts++; // CHANGED: track inserts
    }

    pthread_mutex_unlock(&locks[i]);
}

static void *put_thread(void *xa)
{
    int n = (int)(long)xa;
    int b = NKEYS / nthread;
    local_inserts = 0;
    local_updates = 0;

    for (int i = 0; i < b; i++)
        put(keys[b * n + i], n);

    // --- CHANGED: Print per-thread stats ---
    printf("thread %d: %d inserts, %d updates\n", n, local_inserts, local_updates);
    // --- END CHANGED ---

    return NULL;
}
```

---

## Alternate 11: Resize Hash Table (Double Buckets When Full)

**What changed:** When any bucket exceeds 10,000 entries, the table is resized (doubled). Requires a global write lock during resize.

```c
// notxv6/ph-resize.c
// ... same includes ...

// --- CHANGED: Dynamic bucket count ---
int nbucket = 5;
struct entry **table;
pthread_mutex_t *locks;
pthread_mutex_t resize_lock = PTHREAD_MUTEX_INITIALIZER;
int *bucket_sizes; // track chain length
// --- END CHANGED ---

#define RESIZE_THRESHOLD 10000

static void resize_table(void)
{
    // --- CHANGED: Double the bucket count, rehash all entries ---
    pthread_mutex_lock(&resize_lock);

    int old_nbucket = nbucket;
    int new_nbucket = nbucket * 2;
    struct entry **new_table = calloc(new_nbucket, sizeof(struct entry *));
    pthread_mutex_t *new_locks = malloc(new_nbucket * sizeof(pthread_mutex_t));
    int *new_sizes = calloc(new_nbucket, sizeof(int));

    for (int i = 0; i < new_nbucket; i++)
        pthread_mutex_init(&new_locks[i], NULL);

    // Lock all old buckets
    for (int i = 0; i < old_nbucket; i++)
        pthread_mutex_lock(&locks[i]);

    // Rehash
    for (int i = 0; i < old_nbucket; i++) {
        struct entry *e = table[i];
        while (e) {
            struct entry *next = e->next;
            int ni = e->key % new_nbucket;
            e->next = new_table[ni];
            new_table[ni] = e;
            new_sizes[ni]++;
            e = next;
        }
    }

    // Swap
    for (int i = 0; i < old_nbucket; i++) {
        pthread_mutex_unlock(&locks[i]);
        pthread_mutex_destroy(&locks[i]);
    }
    free(table);
    free(locks);
    free(bucket_sizes);

    table = new_table;
    locks = new_locks;
    bucket_sizes = new_sizes;
    nbucket = new_nbucket;

    printf("resized to %d buckets\n", nbucket);

    pthread_mutex_unlock(&resize_lock);
    // --- END CHANGED ---
}

static void put(int key, int value)
{
    int i = key % nbucket;
    pthread_mutex_lock(&locks[i]);

    struct entry *e = 0;
    for (e = table[i]; e != 0; e = e->next)
        if (e->key == key) break;
    if (e) {
        e->value = value;
    } else {
        insert(key, value, &table[i], table[i]);
        bucket_sizes[i]++;
        // CHANGED: Check if resize needed
        if (bucket_sizes[i] > RESIZE_THRESHOLD) {
            pthread_mutex_unlock(&locks[i]);
            resize_table();
            return;
        }
    }

    pthread_mutex_unlock(&locks[i]);
}
```

---

## Alternate 12: Chaining With Sorted Insert

**What changed:** Instead of inserting at the head of the linked list, insert in sorted order (by key). This makes get() faster on average but put() slower.

```c
// notxv6/ph-sorted.c
// ... same per-bucket lock structure ...

// --- CHANGED: Sorted insert instead of head insert ---
static void put(int key, int value)
{
    int i = key % NBUCKET;
    pthread_mutex_lock(&locks[i]);

    struct entry *e = 0;
    for (e = table[i]; e != 0; e = e->next) {
        if (e->key == key) break;
    }
    if (e) {
        e->value = value;
    } else {
        // CHANGED: Find correct sorted position
        struct entry *new_entry = malloc(sizeof(struct entry));
        new_entry->key = key;
        new_entry->value = value;

        struct entry **pp = &table[i];
        while (*pp != 0 && (*pp)->key < key)
            pp = &(*pp)->next;
        new_entry->next = *pp;
        *pp = new_entry;
    }

    pthread_mutex_unlock(&locks[i]);
}
// --- END CHANGED ---

// get() can now do early termination:
// --- CHANGED: get with early exit on sorted chain ---
static struct entry* get(int key)
{
    int i = key % NBUCKET;
    struct entry *e;
    for (e = table[i]; e != 0; e = e->next) {
        if (e->key == key) return e;
        if (e->key > key) return 0; // CHANGED: early termination
    }
    return 0;
}
// --- END CHANGED ---
```

---

## Alternate 13: Per-Thread Hash Tables (No Sharing)

**What changed:** Each thread has its own separate hash table. After the put phase, tables are merged. Eliminates contention entirely during puts.

```c
// notxv6/ph-per-thread.c
// ... same includes ...

#define NBUCKET 5
#define NKEYS 100000
#define MAX_THREADS 16

struct entry {
    int key;
    int value;
    struct entry *next;
};

// --- CHANGED: Per-thread tables ---
struct entry *per_thread_table[MAX_THREADS][NBUCKET];
struct entry *final_table[NBUCKET]; // merged result
pthread_mutex_t merge_locks[NBUCKET];
// --- END CHANGED ---

int keys[NKEYS];
int nthread = 1;

static void insert_to(int key, int value, struct entry **bucket)
{
    struct entry *e = malloc(sizeof(struct entry));
    e->key = key;
    e->value = value;
    e->next = *bucket;
    *bucket = e;
}

static void *put_thread(void *xa)
{
    int n = (int)(long)xa;
    int b = NKEYS / nthread;

    // --- CHANGED: Insert into thread-local table (no locks needed!) ---
    for (int i = 0; i < b; i++) {
        int key = keys[b * n + i];
        int bucket = key % NBUCKET;
        insert_to(key, n, &per_thread_table[n][bucket]);
    }
    // --- END CHANGED ---

    return NULL;
}

// --- CHANGED: Merge all per-thread tables into final table ---
static void merge_tables(void)
{
    for (int t = 0; t < nthread; t++) {
        for (int i = 0; i < NBUCKET; i++) {
            pthread_mutex_lock(&merge_locks[i]);
            struct entry *e = per_thread_table[t][i];
            while (e) {
                struct entry *next = e->next;
                e->next = final_table[i];
                final_table[i] = e;
                e = next;
            }
            per_thread_table[t][i] = 0;
            pthread_mutex_unlock(&merge_locks[i]);
        }
    }
}
// --- END CHANGED ---

// get searches final_table
static struct entry* get(int key)
{
    int i = key % NBUCKET;
    for (struct entry *e = final_table[i]; e != 0; e = e->next)
        if (e->key == key) return e;
    return 0;
}

int main(int argc, char *argv[])
{
    // ... parse nthread, init keys ...

    for (int i = 0; i < NBUCKET; i++)
        pthread_mutex_init(&merge_locks[i], NULL);

    // Put phase (zero contention)
    // ... create/join put_threads ...

    // Merge phase
    merge_tables();

    // Get phase
    // ... create/join get_threads ...

    for (int i = 0; i < NBUCKET; i++)
        pthread_mutex_destroy(&merge_locks[i]);

    return 0;
}
```

---

## Alternate 14: Locked Get (Both Phases Locked)

**What changed:** Unlike the original where get() is unlocked, this version locks get() too. Demonstrates the performance cost of unnecessary locking.

```c
// notxv6/ph-locked-get.c
// ... same per-bucket lock solution ...

// --- CHANGED: get() also acquires the bucket lock ---
static struct entry* get(int key)
{
    int i = key % NBUCKET;

    // CHANGED: Lock even for reads
    pthread_mutex_lock(&locks[i]);

    struct entry *e = 0;
    for (e = table[i]; e != 0; e = e->next) {
        if (e->key == key) break;
    }

    pthread_mutex_unlock(&locks[i]);
    return e;
}
// --- END CHANGED ---

// put(), main() same as original per-bucket solution
// This version is SLOWER for gets but demonstrates over-locking
```

---

## Alternate 15: Concurrent Put and Get (Mixed Phase)

**What changed:** Instead of separate put and get phases, threads do puts and gets simultaneously in a single mixed phase. Requires locks on both put and get.

```c
// notxv6/ph-mixed.c
// ... same includes, per-bucket locks ...

// --- CHANGED: Combined put+get thread ---
static void *mixed_thread(void *xa)
{
    int n = (int)(long)xa;
    int b = NKEYS / nthread;
    int missing = 0;

    // CHANGED: Interleave puts and gets
    for (int i = 0; i < b; i++) {
        // Put
        int key = keys[b * n + i];
        int bucket = key % NBUCKET;
        pthread_mutex_lock(&locks[bucket]);
        struct entry *e = 0;
        for (e = table[bucket]; e != 0; e = e->next)
            if (e->key == key) break;
        if (e)
            e->value = n;
        else
            insert(key, n, &table[bucket], table[bucket]);
        pthread_mutex_unlock(&locks[bucket]);

        // Immediately try to get a random key
        int rkey = keys[random() % NKEYS];
        int rb = rkey % NBUCKET;
        pthread_mutex_lock(&locks[rb]); // CHANGED: must lock gets in mixed mode
        e = 0;
        for (e = table[rb]; e != 0; e = e->next)
            if (e->key == rkey) break;
        if (e == 0) missing++;
        pthread_mutex_unlock(&locks[rb]);
    }

    printf("thread %d: %d misses during mixed phase\n", n, missing);
    return NULL;
}
// --- END CHANGED ---
```

---

## Alternate 16: Bucket Chain Length Reporter

**What changed:** After the put phase, prints the chain length (number of entries) in each bucket. Demonstrates how well the hash function distributes keys.

```c
// notxv6/ph-chain-stats.c
// ... same per-bucket lock solution ...

// --- CHANGED: Report chain lengths ---
static void print_chain_stats(void)
{
    printf("\nBucket chain lengths:\n");
    int total = 0;
    int max_len = 0;
    for (int i = 0; i < NBUCKET; i++) {
        int len = 0;
        for (struct entry *e = table[i]; e != 0; e = e->next)
            len++;
        printf("  bucket %d: %d entries\n", i, len);
        total += len;
        if (len > max_len) max_len = len;
    }
    printf("  total: %d, avg: %d, max: %d\n", total, total / NBUCKET, max_len);
}
// --- END CHANGED ---

int main(int argc, char *argv[])
{
    // ... setup, put phase ...

    // --- CHANGED: Print stats after puts ---
    print_chain_stats();
    // --- END CHANGED ---

    // ... get phase ...
    return 0;
}
```

---

## Alternate 17: Custom Hash Function

**What changed:** Instead of `key % NBUCKET`, use a better hash function (FNV-1a inspired) to improve distribution across buckets.

```c
// notxv6/ph-custom-hash.c
// ... same per-bucket lock structure ...

// --- CHANGED: Custom hash function instead of simple modulo ---
static int hash(int key)
{
    unsigned int h = 2166136261u; // FNV offset basis
    unsigned char *bytes = (unsigned char *)&key;
    for (int i = 0; i < sizeof(int); i++) {
        h ^= bytes[i];
        h *= 16777619u; // FNV prime
    }
    return h % NBUCKET;
}
// --- END CHANGED ---

static void put(int key, int value)
{
    int i = hash(key); // CHANGED: use custom hash
    pthread_mutex_lock(&locks[i]);

    struct entry *e = 0;
    for (e = table[i]; e != 0; e = e->next) {
        if (e->key == key) break;
    }
    if (e) {
        e->value = value;
    } else {
        insert(key, value, &table[i], table[i]);
    }

    pthread_mutex_unlock(&locks[i]);
}

static struct entry* get(int key)
{
    int i = hash(key); // CHANGED: use custom hash
    struct entry *e = 0;
    for (e = table[i]; e != 0; e = e->next) {
        if (e->key == key) break;
    }
    return e;
}
```

---

## Alternate 18: Timed Lock With Timeout

**What changed:** Use `pthread_mutex_timedlock` with a timeout. If a thread can't acquire a lock within 1ms, it skips that key and retries later.

```c
// notxv6/ph-timedlock.c
// ... same includes, add <time.h> ...
#include <time.h>
#include <errno.h>

// ... same per-bucket locks ...

// --- CHANGED: put with timed lock ---
static int put_timed(int key, int value)
{
    int i = key % NBUCKET;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += 1000000; // 1ms timeout
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }

    // CHANGED: Try with timeout
    if (pthread_mutex_timedlock(&locks[i], &ts) != 0) {
        return -1; // timed out, caller should retry
    }

    struct entry *e = 0;
    for (e = table[i]; e != 0; e = e->next)
        if (e->key == key) break;
    if (e)
        e->value = value;
    else
        insert(key, value, &table[i], table[i]);

    pthread_mutex_unlock(&locks[i]);
    return 0; // success
}
// --- END CHANGED ---

static void *put_thread(void *xa)
{
    int n = (int)(long)xa;
    int b = NKEYS / nthread;
    // --- CHANGED: Retry loop for timed-out keys ---
    int *retry = malloc(b * sizeof(int));
    int retry_count = 0;

    for (int i = 0; i < b; i++) {
        if (put_timed(keys[b * n + i], n) < 0)
            retry[retry_count++] = b * n + i;
    }
    // Retry failures with blocking lock
    for (int i = 0; i < retry_count; i++) {
        int key = keys[retry[i]];
        int bucket = key % NBUCKET;
        pthread_mutex_lock(&locks[bucket]);
        struct entry *e = 0;
        for (e = table[bucket]; e != 0; e = e->next)
            if (e->key == key) break;
        if (e)
            e->value = n;
        else
            insert(key, n, &table[bucket], table[bucket]);
        pthread_mutex_unlock(&locks[bucket]);
    }
    printf("thread %d: %d retries\n", n, retry_count);
    free(retry);
    // --- END CHANGED ---
    return NULL;
}
```

---

## Alternate 19: Atomic Counter for Total Insertions

**What changed:** Added an atomic global counter that tracks the total number of insertions across all threads, using `__sync_fetch_and_add`.

```c
// notxv6/ph-atomic-count.c
// ... same per-bucket lock solution ...

// --- CHANGED: Atomic global insertion counter ---
volatile int total_inserts = 0;
// --- END CHANGED ---

static void put(int key, int value)
{
    int i = key % NBUCKET;
    pthread_mutex_lock(&locks[i]);

    struct entry *e = 0;
    for (e = table[i]; e != 0; e = e->next)
        if (e->key == key) break;
    if (e) {
        e->value = value;
    } else {
        insert(key, value, &table[i], table[i]);
        // --- CHANGED: Atomically increment global counter ---
        __sync_fetch_and_add(&total_inserts, 1);
        // --- END CHANGED ---
    }

    pthread_mutex_unlock(&locks[i]);
}

int main(int argc, char *argv[])
{
    // ... setup, run put threads ...

    // --- CHANGED: Print total insertions ---
    printf("total insertions (atomic count): %d\n", total_inserts);
    // --- END CHANGED ---

    // ... run get threads ...
    return 0;
}
```

---

## Alternate 20: Benchmark Comparison Mode

**What changed:** The program runs the hash table test three times: first with no locks, then with a global lock, then with per-bucket locks, and prints a comparison table.

```c
// notxv6/ph-benchmark.c
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>
#include <string.h>

#define NBUCKET 5
#define NKEYS 100000

struct entry {
    int key;
    int value;
    struct entry *next;
};

struct entry *table[NBUCKET];
int keys[NKEYS];
int nthread = 1;

pthread_mutex_t global_lock;
pthread_mutex_t bucket_locks[NBUCKET];

// --- CHANGED: Lock mode enum ---
enum lock_mode { NO_LOCK, GLOBAL_LOCK, BUCKET_LOCK };
enum lock_mode current_mode;
// --- END CHANGED ---

double now() { struct timeval tv; gettimeofday(&tv, 0); return tv.tv_sec + tv.tv_usec / 1e6; }

static void insert(int key, int value, struct entry **p, struct entry *n)
{
    struct entry *e = malloc(sizeof(struct entry));
    e->key = key; e->value = value; e->next = n; *p = e;
}

static void put(int key, int value)
{
    int i = key % NBUCKET;

    // --- CHANGED: Lock based on current mode ---
    if (current_mode == GLOBAL_LOCK)
        pthread_mutex_lock(&global_lock);
    else if (current_mode == BUCKET_LOCK)
        pthread_mutex_lock(&bucket_locks[i]);
    // --- END CHANGED ---

    struct entry *e = 0;
    for (e = table[i]; e != 0; e = e->next)
        if (e->key == key) break;
    if (e) e->value = value;
    else insert(key, value, &table[i], table[i]);

    if (current_mode == GLOBAL_LOCK)
        pthread_mutex_unlock(&global_lock);
    else if (current_mode == BUCKET_LOCK)
        pthread_mutex_unlock(&bucket_locks[i]);
}

static void *put_thread(void *xa) {
    int n = (int)(long)xa;
    int b = NKEYS / nthread;
    for (int i = 0; i < b; i++) put(keys[b * n + i], n);
    return NULL;
}

static void *get_thread(void *xa) {
    int n = (int)(long)xa;
    int missing = 0;
    for (int i = 0; i < NKEYS; i++) {
        int bucket = keys[i] % NBUCKET;
        struct entry *e;
        for (e = table[bucket]; e != 0; e = e->next)
            if (e->key == keys[i]) break;
        if (e == 0) missing++;
    }
    return (void *)(long)missing;
}

static void clear_table(void) {
    for (int i = 0; i < NBUCKET; i++) {
        struct entry *e = table[i];
        while (e) { struct entry *n = e->next; free(e); e = n; }
        table[i] = 0;
    }
}

// --- CHANGED: Run one benchmark ---
static void run_benchmark(const char *label, enum lock_mode mode)
{
    clear_table();
    current_mode = mode;

    pthread_t *th = malloc(sizeof(pthread_t) * nthread);
    void *val;

    double t0 = now();
    for (int i = 0; i < nthread; i++)
        pthread_create(&th[i], NULL, put_thread, (void *)(long)i);
    for (int i = 0; i < nthread; i++)
        pthread_join(th[i], NULL);
    double t1 = now();
    double put_rate = NKEYS / (t1 - t0);

    t0 = now();
    int total_missing = 0;
    for (int i = 0; i < nthread; i++)
        pthread_create(&th[i], NULL, get_thread, (void *)(long)i);
    for (int i = 0; i < nthread; i++) {
        pthread_join(th[i], &val);
        total_missing += (int)(long)val;
    }
    t1 = now();
    double get_rate = (nthread * NKEYS) / (t1 - t0);

    printf("%-15s | %d threads | %6d missing | %8.0f puts/s | %8.0f gets/s\n",
           label, nthread, total_missing, put_rate, get_rate);

    free(th);
}
// --- END CHANGED ---

int main(int argc, char *argv[])
{
    if (argc < 2) { fprintf(stderr, "Usage: %s nthreads\n", argv[0]); exit(1); }
    nthread = atoi(argv[1]);
    for (int i = 0; i < NKEYS; i++) keys[i] = random();

    pthread_mutex_init(&global_lock, NULL);
    for (int i = 0; i < NBUCKET; i++)
        pthread_mutex_init(&bucket_locks[i], NULL);

    // --- CHANGED: Run all three configurations ---
    printf("%-15s | %-9s | %14s | %11s | %11s\n",
           "Config", "Threads", "Missing", "Put Rate", "Get Rate");
    printf("--------------------------------------------------------------\n");
    run_benchmark("No Lock", NO_LOCK);
    run_benchmark("Global Lock", GLOBAL_LOCK);
    run_benchmark("Bucket Lock", BUCKET_LOCK);
    // --- END CHANGED ---

    pthread_mutex_destroy(&global_lock);
    for (int i = 0; i < NBUCKET; i++)
        pthread_mutex_destroy(&bucket_locks[i]);

    return 0;
}
```

---

## Quick Reference: All 20 Alternates

| # | Name | Key Change |
|---|------|------------|
| 1 | Read-Write Locks | `pthread_rwlock_t` per bucket (concurrent readers) |
| 2 | Single Global Lock | One mutex for entire table (safe but no speedup) |
| 3 | Spinlocks | `pthread_spinlock_t` per bucket (busy-wait) |
| 4 | More Buckets | NBUCKET=101 for reduced contention |
| 5 | Lock-Free CAS | Atomic compare-and-swap for insert (no locks) |
| 6 | Barrier Between Phases | `pthread_barrier_t` synchronizes put/get phases |
| 7 | Delete Operation | New `delete()` function with proper locking |
| 8 | Heavy Load | 200K keys, 10 buckets |
| 9 | Trylock With Retry | Non-blocking `trylock`, defer and retry |
| 10 | Thread-Local Stats | Per-thread insert/update counters |
| 11 | Dynamic Resize | Double buckets when chain exceeds threshold |
| 12 | Sorted Insert | Insert in sorted order, early-exit get |
| 13 | Per-Thread Tables | Thread-local tables merged after puts |
| 14 | Locked Get | Lock get() too (demonstrates over-locking cost) |
| 15 | Mixed Phase | Interleaved puts and gets simultaneously |
| 16 | Chain Length Stats | Print bucket chain length distribution |
| 17 | Custom Hash | FNV-1a hash instead of simple modulo |
| 18 | Timed Lock | `pthread_mutex_timedlock` with 1ms timeout |
| 19 | Atomic Counter | `__sync_fetch_and_add` global insert counter |
| 20 | Benchmark Comparison | Runs no-lock, global-lock, bucket-lock and prints table |

---

## Build Instructions

All alternates compile on Linux/macOS with:
```bash
gcc -o <output_name> -g -O2 notxv6/<source_file>.c -pthread
```

For example:
```bash
gcc -o ph-with-rwlocks -g -O2 notxv6/ph-with-rwlocks.c -pthread
./ph-with-rwlocks 2
```

> **Note:** These run on the host OS (Linux/macOS), NOT inside xv6/qemu. You need a multicore machine to observe real parallel speedup and race conditions.
