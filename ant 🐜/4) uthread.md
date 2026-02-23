# Task 4: uthread - 20 Alternate Versions

## Original Task Summary

The original **uthread** task requires:
- Implement user-level thread context switching in xv6
- Create a `thread_context` struct holding callee-saved registers (ra, sp, s0-s11)
- Complete `thread_create()` to set up a new thread's stack pointer and return address
- Complete `thread_schedule()` to call `thread_switch` with the correct arguments
- Implement `thread_switch` in `uthread_switch.S` to save/restore callee-saved registers using `sd`/`ld` instructions
- Three test threads (a, b, c) each loop 100 times, yield, and exit

**Key data structures:**
```c
struct thread_context {
    uint64 ra;
    uint64 sp;
    uint64 s0;
    uint64 s1;
    uint64 s2;
    uint64 s3;
    uint64 s4;
    uint64 s5;
    uint64 s6;
    uint64 s7;
    uint64 s8;
    uint64 s9;
    uint64 s10;
    uint64 s11;
};

struct thread {
    char stack[STACK_SIZE];
    int state;
    struct thread_context context;
};
```

---

## Alternate 1: Four Threads Instead of Three

**What changed:** Instead of three test threads (a, b, c), there are four threads (a, b, c, d). Thread D loops 50 times instead of 100.

```c
// user/uthread.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define STACK_SIZE 8192
#define MAX_THREAD 8  // CHANGED: increased from 4 to 8 for headroom
#define FREE    0
#define RUNNING 1
#define RUNNABLE 2

struct thread_context {
    uint64 ra;
    uint64 sp;
    uint64 s0;
    uint64 s1;
    uint64 s2;
    uint64 s3;
    uint64 s4;
    uint64 s5;
    uint64 s6;
    uint64 s7;
    uint64 s8;
    uint64 s9;
    uint64 s10;
    uint64 s11;
};

struct thread {
    char stack[STACK_SIZE];
    int state;
    struct thread_context context;
};

struct thread all_thread[MAX_THREAD];
struct thread *current_thread;

extern void thread_switch(struct thread_context *old, struct thread_context *new);

void thread_init(void)
{
    current_thread = &all_thread[0];
    current_thread->state = RUNNING;
}

void thread_create(void (*func)())
{
    struct thread *t;
    for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
        if (t->state == FREE) break;
    }
    if (t >= all_thread + MAX_THREAD) {
        printf("thread_create: no free threads\n");
        return;
    }
    t->state = RUNNABLE;
    t->context.ra = (uint64)func;
    t->context.sp = (uint64)(t->stack + STACK_SIZE);
}

void thread_yield(void)
{
    current_thread->state = RUNNABLE;
    thread_schedule();
}

void thread_schedule(void)
{
    struct thread *t, *next_thread = 0;
    t = current_thread + 1;
    for (int i = 0; i < MAX_THREAD; i++) {
        if (t >= all_thread + MAX_THREAD)
            t = all_thread;
        if (t->state == RUNNABLE) {
            next_thread = t;
            break;
        }
        t++;
    }
    if (next_thread == 0) {
        if (current_thread->state == RUNNING) return;
        printf("thread_schedule: no runnable threads\n");
        exit(0);
    }
    if (current_thread != next_thread) {
        next_thread->state = RUNNING;
        struct thread *prev = current_thread;
        current_thread = next_thread;
        thread_switch(&prev->context, &next_thread->context);
    }
}

void thread_a(void)
{
    int i;
    printf("thread_a started\n");
    for (i = 0; i < 100; i++) {
        printf("thread_a %d\n", i);
        thread_yield();
    }
    printf("thread_a: exit after 100\n");
    current_thread->state = FREE;
    thread_schedule();
}

void thread_b(void)
{
    int i;
    printf("thread_b started\n");
    for (i = 0; i < 100; i++) {
        printf("thread_b %d\n", i);
        thread_yield();
    }
    printf("thread_b: exit after 100\n");
    current_thread->state = FREE;
    thread_schedule();
}

void thread_c(void)
{
    int i;
    printf("thread_c started\n");
    for (i = 0; i < 100; i++) {
        printf("thread_c %d\n", i);
        thread_yield();
    }
    printf("thread_c: exit after 100\n");
    current_thread->state = FREE;
    thread_schedule();
}

// --- CHANGED: Added a fourth thread that loops 50 times ---
void thread_d(void)
{
    int i;
    printf("thread_d started\n");
    for (i = 0; i < 50; i++) {
        printf("thread_d %d\n", i);
        thread_yield();
    }
    printf("thread_d: exit after 50\n");
    current_thread->state = FREE;
    thread_schedule();
}
// --- END CHANGED ---

int main(int argc, char *argv[])
{
    thread_init();
    thread_create(thread_a);
    thread_create(thread_b);
    thread_create(thread_c);
    thread_create(thread_d); // CHANGED: create fourth thread
    thread_schedule();
    exit(0);
}
```

```asm
# user/uthread_switch.S (unchanged from original solution)
.globl thread_switch
thread_switch:
    sd ra, 0(a0)
    sd sp, 8(a0)
    sd s0, 16(a0)
    sd s1, 24(a0)
    sd s2, 32(a0)
    sd s3, 40(a0)
    sd s4, 48(a0)
    sd s5, 56(a0)
    sd s6, 64(a0)
    sd s7, 72(a0)
    sd s8, 80(a0)
    sd s9, 88(a0)
    sd s10, 96(a0)
    sd s11, 104(a0)

    ld ra, 0(a1)
    ld sp, 8(a1)
    ld s0, 16(a1)
    ld s1, 24(a1)
    ld s2, 32(a1)
    ld s3, 40(a1)
    ld s4, 48(a1)
    ld s5, 56(a1)
    ld s6, 64(a1)
    ld s7, 72(a1)
    ld s8, 80(a1)
    ld s9, 88(a1)
    ld s10, 96(a1)
    ld s11, 104(a1)

    ret
```

---

## Alternate 2: Threads With Priority

**What changed:** Each thread has a priority level (0=low, 1=medium, 2=high). The scheduler always picks the highest-priority runnable thread instead of round-robin.

```c
// user/uthread.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define STACK_SIZE 8192
#define MAX_THREAD 4
#define FREE    0
#define RUNNING 1
#define RUNNABLE 2

struct thread_context {
    uint64 ra; uint64 sp;
    uint64 s0; uint64 s1; uint64 s2; uint64 s3;
    uint64 s4; uint64 s5; uint64 s6; uint64 s7;
    uint64 s8; uint64 s9; uint64 s10; uint64 s11;
};

struct thread {
    char stack[STACK_SIZE];
    int state;
    struct thread_context context;
    int priority; // --- CHANGED: Added priority field ---
};

struct thread all_thread[MAX_THREAD];
struct thread *current_thread;

extern void thread_switch(struct thread_context *old, struct thread_context *new);

void thread_init(void)
{
    current_thread = &all_thread[0];
    current_thread->state = RUNNING;
    current_thread->priority = 0;
}

// --- CHANGED: thread_create takes a priority argument ---
void thread_create(void (*func)(), int priority)
{
    struct thread *t;
    for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
        if (t->state == FREE) break;
    }
    if (t >= all_thread + MAX_THREAD) return;
    t->state = RUNNABLE;
    t->context.ra = (uint64)func;
    t->context.sp = (uint64)(t->stack + STACK_SIZE);
    t->priority = priority; // CHANGED: set priority
}
// --- END CHANGED ---

void thread_yield(void)
{
    current_thread->state = RUNNABLE;
    thread_schedule();
}

void thread_schedule(void)
{
    // --- CHANGED: Pick highest-priority runnable thread ---
    struct thread *next_thread = 0;
    int best_priority = -1;

    for (int i = 0; i < MAX_THREAD; i++) {
        if (all_thread[i].state == RUNNABLE && all_thread[i].priority > best_priority) {
            best_priority = all_thread[i].priority;
            next_thread = &all_thread[i];
        }
    }
    // --- END CHANGED ---

    if (next_thread == 0) {
        if (current_thread->state == RUNNING) return;
        printf("thread_schedule: no runnable threads\n");
        exit(0);
    }
    if (current_thread != next_thread) {
        next_thread->state = RUNNING;
        struct thread *prev = current_thread;
        current_thread = next_thread;
        thread_switch(&prev->context, &next_thread->context);
    }
}

void thread_a(void)
{
    printf("thread_a (LOW priority) started\n");
    for (int i = 0; i < 100; i++) {
        printf("thread_a %d\n", i);
        thread_yield();
    }
    printf("thread_a: exit\n");
    current_thread->state = FREE;
    thread_schedule();
}

void thread_b(void)
{
    printf("thread_b (HIGH priority) started\n");
    for (int i = 0; i < 100; i++) {
        printf("thread_b %d\n", i);
        thread_yield();
    }
    printf("thread_b: exit\n");
    current_thread->state = FREE;
    thread_schedule();
}

void thread_c(void)
{
    printf("thread_c (MED priority) started\n");
    for (int i = 0; i < 100; i++) {
        printf("thread_c %d\n", i);
        thread_yield();
    }
    printf("thread_c: exit\n");
    current_thread->state = FREE;
    thread_schedule();
}

int main(int argc, char *argv[])
{
    thread_init();
    // --- CHANGED: Pass priority to thread_create ---
    thread_create(thread_a, 0); // low
    thread_create(thread_b, 2); // high
    thread_create(thread_c, 1); // medium
    // --- END CHANGED ---
    thread_schedule();
    exit(0);
}
```

Assembly: Same as Alternate 1 (unchanged `uthread_switch.S`).

---

## Alternate 3: Thread With Join (Wait for Completion)

**What changed:** Added `thread_join(int tid)` that blocks the calling thread until the target thread exits. Threads have an integer ID.

```c
// user/uthread.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define STACK_SIZE 8192
#define MAX_THREAD 4
#define FREE    0
#define RUNNING 1
#define RUNNABLE 2
#define BLOCKED 3  // --- CHANGED: New state for blocked threads ---

struct thread_context {
    uint64 ra; uint64 sp;
    uint64 s0; uint64 s1; uint64 s2; uint64 s3;
    uint64 s4; uint64 s5; uint64 s6; uint64 s7;
    uint64 s8; uint64 s9; uint64 s10; uint64 s11;
};

struct thread {
    char stack[STACK_SIZE];
    int state;
    struct thread_context context;
    int tid;           // --- CHANGED: thread ID ---
    int join_target;   // --- CHANGED: tid we are waiting for (-1 if none) ---
};

struct thread all_thread[MAX_THREAD];
struct thread *current_thread;
int next_tid = 0; // CHANGED: global tid counter

extern void thread_switch(struct thread_context *old, struct thread_context *new);

void thread_init(void)
{
    current_thread = &all_thread[0];
    current_thread->state = RUNNING;
    current_thread->tid = next_tid++;
    current_thread->join_target = -1;
}

int thread_create(void (*func)())
{
    struct thread *t;
    for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
        if (t->state == FREE) break;
    }
    if (t >= all_thread + MAX_THREAD) return -1;
    t->state = RUNNABLE;
    t->context.ra = (uint64)func;
    t->context.sp = (uint64)(t->stack + STACK_SIZE);
    t->tid = next_tid++; // CHANGED: assign tid
    t->join_target = -1;
    return t->tid; // CHANGED: return the tid
}

void thread_yield(void)
{
    current_thread->state = RUNNABLE;
    thread_schedule();
}

// --- CHANGED: thread_join blocks until target tid exits ---
void thread_join(int tid)
{
    current_thread->join_target = tid;
    current_thread->state = BLOCKED;
    thread_schedule();
}
// --- END CHANGED ---

void thread_schedule(void)
{
    // --- CHANGED: Wake up threads whose join target is now FREE ---
    for (int i = 0; i < MAX_THREAD; i++) {
        if (all_thread[i].state == BLOCKED && all_thread[i].join_target >= 0) {
            int found = 0;
            for (int j = 0; j < MAX_THREAD; j++) {
                if (all_thread[j].tid == all_thread[i].join_target &&
                    all_thread[j].state != FREE) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                // target thread has exited, unblock
                all_thread[i].state = RUNNABLE;
                all_thread[i].join_target = -1;
            }
        }
    }
    // --- END CHANGED ---

    struct thread *t, *next_thread = 0;
    t = current_thread + 1;
    for (int i = 0; i < MAX_THREAD; i++) {
        if (t >= all_thread + MAX_THREAD) t = all_thread;
        if (t->state == RUNNABLE) { next_thread = t; break; }
        t++;
    }
    if (next_thread == 0) {
        if (current_thread->state == RUNNING) return;
        printf("thread_schedule: no runnable threads\n");
        exit(0);
    }
    if (current_thread != next_thread) {
        next_thread->state = RUNNING;
        struct thread *prev = current_thread;
        current_thread = next_thread;
        thread_switch(&prev->context, &next_thread->context);
    }
}

void thread_a(void)
{
    printf("thread_a started\n");
    for (int i = 0; i < 100; i++) {
        printf("thread_a %d\n", i);
        thread_yield();
    }
    printf("thread_a: exit\n");
    current_thread->state = FREE;
    thread_schedule();
}

void thread_b(void)
{
    printf("thread_b started\n");
    for (int i = 0; i < 100; i++) {
        printf("thread_b %d\n", i);
        thread_yield();
    }
    printf("thread_b: exit\n");
    current_thread->state = FREE;
    thread_schedule();
}

void thread_c(void)
{
    printf("thread_c started\n");
    for (int i = 0; i < 100; i++) {
        printf("thread_c %d\n", i);
        thread_yield();
    }
    printf("thread_c: exit\n");
    current_thread->state = FREE;
    thread_schedule();
}

int main(int argc, char *argv[])
{
    thread_init();
    int ta = thread_create(thread_a);
    int tb = thread_create(thread_b);
    thread_create(thread_c);

    // --- CHANGED: Main thread joins on thread_a, then thread_b ---
    printf("main: waiting for thread_a (tid %d)\n", ta);
    thread_join(ta);
    printf("main: thread_a done, waiting for thread_b (tid %d)\n", tb);
    thread_join(tb);
    printf("main: all joined\n");
    // --- END CHANGED ---

    thread_schedule();
    exit(0);
}
```

Assembly: Same as Alternate 1.

---

## Alternate 4: Configurable Loop Count via Argument

**What changed:** Each thread function takes a loop count from a global variable set by a command-line argument, instead of hardcoded 100.

```c
// user/uthread.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define STACK_SIZE 8192
#define MAX_THREAD 4
#define FREE 0
#define RUNNING 1
#define RUNNABLE 2

// --- CHANGED: Global loop count from argv ---
int loop_count = 100; // default
// --- END CHANGED ---

struct thread_context {
    uint64 ra; uint64 sp;
    uint64 s0; uint64 s1; uint64 s2; uint64 s3;
    uint64 s4; uint64 s5; uint64 s6; uint64 s7;
    uint64 s8; uint64 s9; uint64 s10; uint64 s11;
};

struct thread {
    char stack[STACK_SIZE];
    int state;
    struct thread_context context;
};

struct thread all_thread[MAX_THREAD];
struct thread *current_thread;
extern void thread_switch(struct thread_context *old, struct thread_context *new);

void thread_init(void) {
    current_thread = &all_thread[0];
    current_thread->state = RUNNING;
}

void thread_create(void (*func)()) {
    struct thread *t;
    for (t = all_thread; t < all_thread + MAX_THREAD; t++)
        if (t->state == FREE) break;
    if (t >= all_thread + MAX_THREAD) return;
    t->state = RUNNABLE;
    t->context.ra = (uint64)func;
    t->context.sp = (uint64)(t->stack + STACK_SIZE);
}

void thread_yield(void) {
    current_thread->state = RUNNABLE;
    thread_schedule();
}

void thread_schedule(void) {
    struct thread *t, *next_thread = 0;
    t = current_thread + 1;
    for (int i = 0; i < MAX_THREAD; i++) {
        if (t >= all_thread + MAX_THREAD) t = all_thread;
        if (t->state == RUNNABLE) { next_thread = t; break; }
        t++;
    }
    if (next_thread == 0) {
        if (current_thread->state == RUNNING) return;
        printf("thread_schedule: no runnable threads\n");
        exit(0);
    }
    if (current_thread != next_thread) {
        next_thread->state = RUNNING;
        struct thread *prev = current_thread;
        current_thread = next_thread;
        thread_switch(&prev->context, &next_thread->context);
    }
}

// --- CHANGED: Threads use global loop_count instead of hardcoded 100 ---
void thread_a(void) {
    printf("thread_a started (loops: %d)\n", loop_count);
    for (int i = 0; i < loop_count; i++) {
        printf("thread_a %d\n", i);
        thread_yield();
    }
    printf("thread_a: exit after %d\n", loop_count);
    current_thread->state = FREE;
    thread_schedule();
}

void thread_b(void) {
    printf("thread_b started (loops: %d)\n", loop_count);
    for (int i = 0; i < loop_count; i++) {
        printf("thread_b %d\n", i);
        thread_yield();
    }
    printf("thread_b: exit after %d\n", loop_count);
    current_thread->state = FREE;
    thread_schedule();
}

void thread_c(void) {
    printf("thread_c started (loops: %d)\n", loop_count);
    for (int i = 0; i < loop_count; i++) {
        printf("thread_c %d\n", i);
        thread_yield();
    }
    printf("thread_c: exit after %d\n", loop_count);
    current_thread->state = FREE;
    thread_schedule();
}
// --- END CHANGED ---

int main(int argc, char *argv[]) {
    // --- CHANGED: Parse loop count from command line ---
    if (argc >= 2)
        loop_count = atoi(argv[1]);
    // --- END CHANGED ---
    thread_init();
    thread_create(thread_a);
    thread_create(thread_b);
    thread_create(thread_c);
    thread_schedule();
    exit(0);
}
```

Assembly: Same as Alternate 1.

---

## Alternate 5: Thread Yield Counter

**What changed:** Each thread tracks how many times it has yielded. On exit, it prints the total yield count.

```c
// user/uthread.c (showing only changed parts, rest is standard)
// ... standard includes, defines, context, thread struct ...

struct thread {
    char stack[STACK_SIZE];
    int state;
    struct thread_context context;
    int yield_count; // --- CHANGED: Track yields per thread ---
};

// ... thread_init, thread_create, thread_schedule same as original ...

// --- CHANGED: thread_yield increments counter ---
void thread_yield(void) {
    current_thread->yield_count++;
    current_thread->state = RUNNABLE;
    thread_schedule();
}
// --- END CHANGED ---

void thread_a(void) {
    printf("thread_a started\n");
    for (int i = 0; i < 100; i++) {
        printf("thread_a %d\n", i);
        thread_yield();
    }
    // --- CHANGED: Print yield count on exit ---
    printf("thread_a: exit after 100 (yielded %d times)\n", current_thread->yield_count);
    // --- END CHANGED ---
    current_thread->state = FREE;
    thread_schedule();
}

void thread_b(void) {
    printf("thread_b started\n");
    for (int i = 0; i < 100; i++) {
        printf("thread_b %d\n", i);
        thread_yield();
    }
    printf("thread_b: exit after 100 (yielded %d times)\n", current_thread->yield_count);
    current_thread->state = FREE;
    thread_schedule();
}

void thread_c(void) {
    printf("thread_c started\n");
    for (int i = 0; i < 100; i++) {
        printf("thread_c %d\n", i);
        thread_yield();
    }
    printf("thread_c: exit after 100 (yielded %d times)\n", current_thread->yield_count);
    current_thread->state = FREE;
    thread_schedule();
}

// main same as original
```

Assembly: Same as Alternate 1.

---

## Alternate 6: Different Loop Counts Per Thread

**What changed:** Thread A loops 50 times, Thread B loops 100 times, Thread C loops 200 times. Each thread has a different workload.

```c
// user/uthread.c (showing only changed thread functions)

// --- CHANGED: Each thread has a different loop count ---
void thread_a(void) {
    printf("thread_a started\n");
    for (int i = 0; i < 50; i++) { // CHANGED: 50 iterations
        printf("thread_a %d\n", i);
        thread_yield();
    }
    printf("thread_a: exit after 50\n");
    current_thread->state = FREE;
    thread_schedule();
}

void thread_b(void) {
    printf("thread_b started\n");
    for (int i = 0; i < 100; i++) { // CHANGED: 100 iterations
        printf("thread_b %d\n", i);
        thread_yield();
    }
    printf("thread_b: exit after 100\n");
    current_thread->state = FREE;
    thread_schedule();
}

void thread_c(void) {
    printf("thread_c started\n");
    for (int i = 0; i < 200; i++) { // CHANGED: 200 iterations
        printf("thread_c %d\n", i);
        thread_yield();
    }
    printf("thread_c: exit after 200\n");
    current_thread->state = FREE;
    thread_schedule();
}
// --- END CHANGED ---

// Everything else (context, thread_create, thread_schedule, switch) same as original
```

Assembly: Same as Alternate 1.

---

## Alternate 7: Round-Robin With Time Slice Counter

**What changed:** Instead of yielding voluntarily, each thread gets a "time slice" of N iterations before being forced to yield. A global counter tracks this.

```c
// user/uthread.c (key changes)

// --- CHANGED: Time slice mechanism ---
#define TIME_SLICE 5 // force yield every 5 iterations

struct thread {
    char stack[STACK_SIZE];
    int state;
    struct thread_context context;
    int slice_remaining; // CHANGED: ticks left in current slice
};
// --- END CHANGED ---

void thread_create(void (*func)()) {
    struct thread *t;
    for (t = all_thread; t < all_thread + MAX_THREAD; t++)
        if (t->state == FREE) break;
    if (t >= all_thread + MAX_THREAD) return;
    t->state = RUNNABLE;
    t->context.ra = (uint64)func;
    t->context.sp = (uint64)(t->stack + STACK_SIZE);
    t->slice_remaining = TIME_SLICE; // CHANGED: initialize slice
}

// --- CHANGED: thread_tick decrements slice, auto-yields at 0 ---
void thread_tick(void) {
    current_thread->slice_remaining--;
    if (current_thread->slice_remaining <= 0) {
        current_thread->slice_remaining = TIME_SLICE; // reset
        thread_yield();
    }
}
// --- END CHANGED ---

void thread_a(void) {
    printf("thread_a started\n");
    for (int i = 0; i < 100; i++) {
        printf("thread_a %d\n", i);
        thread_tick(); // CHANGED: tick instead of yield
    }
    printf("thread_a: exit after 100\n");
    current_thread->state = FREE;
    thread_schedule();
}

void thread_b(void) {
    printf("thread_b started\n");
    for (int i = 0; i < 100; i++) {
        printf("thread_b %d\n", i);
        thread_tick(); // CHANGED
    }
    printf("thread_b: exit after 100\n");
    current_thread->state = FREE;
    thread_schedule();
}

void thread_c(void) {
    printf("thread_c started\n");
    for (int i = 0; i < 100; i++) {
        printf("thread_c %d\n", i);
        thread_tick(); // CHANGED
    }
    printf("thread_c: exit after 100\n");
    current_thread->state = FREE;
    thread_schedule();
}
```

Assembly: Same as Alternate 1.

---

## Alternate 8: Thread Names

**What changed:** Each thread has a name string. The scheduler prints which thread it switches to.

```c
// user/uthread.c (key changes)

struct thread {
    char stack[STACK_SIZE];
    int state;
    struct thread_context context;
    char name[16]; // --- CHANGED: Thread name ---
};

// --- CHANGED: thread_create takes a name ---
void thread_create(void (*func)(), char *name) {
    struct thread *t;
    for (t = all_thread; t < all_thread + MAX_THREAD; t++)
        if (t->state == FREE) break;
    if (t >= all_thread + MAX_THREAD) return;
    t->state = RUNNABLE;
    t->context.ra = (uint64)func;
    t->context.sp = (uint64)(t->stack + STACK_SIZE);
    // CHANGED: copy name
    int i;
    for (i = 0; name[i] && i < 15; i++)
        t->name[i] = name[i];
    t->name[i] = 0;
}
// --- END CHANGED ---

void thread_schedule(void) {
    struct thread *t, *next_thread = 0;
    t = current_thread + 1;
    for (int i = 0; i < MAX_THREAD; i++) {
        if (t >= all_thread + MAX_THREAD) t = all_thread;
        if (t->state == RUNNABLE) { next_thread = t; break; }
        t++;
    }
    if (next_thread == 0) {
        if (current_thread->state == RUNNING) return;
        printf("thread_schedule: no runnable threads\n");
        exit(0);
    }
    if (current_thread != next_thread) {
        // --- CHANGED: Print switch info with names ---
        printf("switch: %s -> %s\n", current_thread->name, next_thread->name);
        // --- END CHANGED ---
        next_thread->state = RUNNING;
        struct thread *prev = current_thread;
        current_thread = next_thread;
        thread_switch(&prev->context, &next_thread->context);
    }
}

int main(int argc, char *argv[]) {
    thread_init();
    strcpy(current_thread->name, "main"); // CHANGED: name for main
    thread_create(thread_a, "alpha");     // CHANGED
    thread_create(thread_b, "beta");      // CHANGED
    thread_create(thread_c, "gamma");     // CHANGED
    thread_schedule();
    exit(0);
}
```

Assembly: Same as Alternate 1.

---

## Alternate 9: Fibonacci Thread

**What changed:** Instead of a simple counter, thread_a computes Fibonacci numbers up to the Nth term, printing each one and yielding between computations.

```c
// user/uthread.c (showing only changed thread_a, rest is standard)

// --- CHANGED: thread_a computes Fibonacci sequence ---
void thread_a(void) {
    printf("thread_a (fibonacci) started\n");
    int a = 0, b = 1;
    for (int i = 0; i < 20; i++) { // first 20 Fibonacci numbers
        printf("thread_a fib(%d) = %d\n", i, a);
        int temp = a + b;
        a = b;
        b = temp;
        thread_yield();
    }
    printf("thread_a: exit\n");
    current_thread->state = FREE;
    thread_schedule();
}
// --- END CHANGED ---

// thread_b and thread_c remain the standard 100-iteration counters
```

Assembly: Same as Alternate 1.

---

## Alternate 10: Producer-Consumer Threads

**What changed:** Thread A is a producer that writes values to a shared buffer. Thread B is a consumer that reads from the buffer. Thread C prints status. Simple cooperative producer-consumer without locks (works because of cooperative scheduling).

```c
// user/uthread.c (key changes)

// --- CHANGED: Shared buffer for producer-consumer ---
#define BUF_SIZE 8
int buffer[BUF_SIZE];
int buf_count = 0;
int buf_read_idx = 0;
int buf_write_idx = 0;
// --- END CHANGED ---

void thread_a(void) {
    // --- CHANGED: Producer thread ---
    printf("producer started\n");
    for (int i = 0; i < 100; i++) {
        // Wait for space (cooperative: just yield until space available)
        while (buf_count >= BUF_SIZE)
            thread_yield();
        buffer[buf_write_idx] = i;
        buf_write_idx = (buf_write_idx + 1) % BUF_SIZE;
        buf_count++;
        printf("produced: %d\n", i);
        thread_yield();
    }
    printf("producer: exit\n");
    current_thread->state = FREE;
    thread_schedule();
    // --- END CHANGED ---
}

void thread_b(void) {
    // --- CHANGED: Consumer thread ---
    printf("consumer started\n");
    for (int i = 0; i < 100; i++) {
        // Wait for data
        while (buf_count <= 0)
            thread_yield();
        int val = buffer[buf_read_idx];
        buf_read_idx = (buf_read_idx + 1) % BUF_SIZE;
        buf_count--;
        printf("consumed: %d\n", val);
        thread_yield();
    }
    printf("consumer: exit\n");
    current_thread->state = FREE;
    thread_schedule();
    // --- END CHANGED ---
}

void thread_c(void) {
    // --- CHANGED: Status monitor thread ---
    printf("monitor started\n");
    for (int i = 0; i < 50; i++) {
        printf("buffer status: %d/%d items\n", buf_count, BUF_SIZE);
        thread_yield();
    }
    printf("monitor: exit\n");
    current_thread->state = FREE;
    thread_schedule();
    // --- END CHANGED ---
}
```

Assembly: Same as Alternate 1.

---

## Alternate 11: Countdown Threads (Reverse Loop)

**What changed:** All threads count DOWN from 99 to 0 instead of counting up from 0 to 99.

```c
// --- CHANGED: Threads count down instead of up ---
void thread_a(void) {
    printf("thread_a started\n");
    for (int i = 99; i >= 0; i--) { // CHANGED: count down
        printf("thread_a %d\n", i);
        thread_yield();
    }
    printf("thread_a: exit\n");
    current_thread->state = FREE;
    thread_schedule();
}

void thread_b(void) {
    printf("thread_b started\n");
    for (int i = 99; i >= 0; i--) { // CHANGED: count down
        printf("thread_b %d\n", i);
        thread_yield();
    }
    printf("thread_b: exit\n");
    current_thread->state = FREE;
    thread_schedule();
}

void thread_c(void) {
    printf("thread_c started\n");
    for (int i = 99; i >= 0; i--) { // CHANGED: count down
        printf("thread_c %d\n", i);
        thread_yield();
    }
    printf("thread_c: exit\n");
    current_thread->state = FREE;
    thread_schedule();
}
// --- END CHANGED ---
```

Assembly: Same as Alternate 1.

---

## Alternate 12: Thread With Sleep (Yield N Times)

**What changed:** Added `thread_sleep(int ticks)` that yields N times before returning, simulating a sleep.

```c
// --- CHANGED: thread_sleep yields N times ---
void thread_sleep(int ticks) {
    for (int i = 0; i < ticks; i++)
        thread_yield();
}
// --- END CHANGED ---

void thread_a(void) {
    printf("thread_a started\n");
    for (int i = 0; i < 100; i++) {
        printf("thread_a %d\n", i);
        thread_sleep(1); // CHANGED: sleep for 1 tick
    }
    printf("thread_a: exit\n");
    current_thread->state = FREE;
    thread_schedule();
}

void thread_b(void) {
    printf("thread_b started\n");
    for (int i = 0; i < 100; i++) {
        printf("thread_b %d\n", i);
        thread_sleep(2); // CHANGED: sleep for 2 ticks (runs slower)
    }
    printf("thread_b: exit\n");
    current_thread->state = FREE;
    thread_schedule();
}

void thread_c(void) {
    printf("thread_c started\n");
    for (int i = 0; i < 100; i++) {
        printf("thread_c %d\n", i);
        thread_sleep(3); // CHANGED: sleep for 3 ticks (runs slowest)
    }
    printf("thread_c: exit\n");
    current_thread->state = FREE;
    thread_schedule();
}
```

Assembly: Same as Alternate 1.

---

## Alternate 13: Shared Counter (Cooperation Test)

**What changed:** All three threads increment a single shared global counter. Each thread increments it 100 times. After all threads exit, main prints the final value (should be 300 with cooperative scheduling).

```c
// --- CHANGED: Shared counter ---
int shared_counter = 0;
// --- END CHANGED ---

void thread_a(void) {
    printf("thread_a started\n");
    for (int i = 0; i < 100; i++) {
        shared_counter++; // CHANGED: increment shared counter
        printf("thread_a inc -> %d\n", shared_counter);
        thread_yield();
    }
    printf("thread_a: exit\n");
    current_thread->state = FREE;
    thread_schedule();
}

void thread_b(void) {
    printf("thread_b started\n");
    for (int i = 0; i < 100; i++) {
        shared_counter++;
        printf("thread_b inc -> %d\n", shared_counter);
        thread_yield();
    }
    printf("thread_b: exit\n");
    current_thread->state = FREE;
    thread_schedule();
}

void thread_c(void) {
    printf("thread_c started\n");
    for (int i = 0; i < 100; i++) {
        shared_counter++;
        printf("thread_c inc -> %d\n", shared_counter);
        thread_yield();
    }
    printf("thread_c: exit\n");
    current_thread->state = FREE;
    thread_schedule();
}

int main(int argc, char *argv[]) {
    thread_init();
    thread_create(thread_a);
    thread_create(thread_b);
    thread_create(thread_c);
    thread_schedule();
    // --- CHANGED: Print final shared counter ---
    printf("final shared_counter = %d (expected 300)\n", shared_counter);
    // --- END CHANGED ---
    exit(0);
}
```

Assembly: Same as Alternate 1.

---

## Alternate 14: Thread With Return Value

**What changed:** Threads can set a return value before exiting. A new `thread_exit(int retval)` function stores it. The main thread can read it.

```c
struct thread {
    char stack[STACK_SIZE];
    int state;
    struct thread_context context;
    int retval; // --- CHANGED: return value ---
};

// --- CHANGED: thread_exit stores return value ---
void thread_exit(int retval) {
    current_thread->retval = retval;
    current_thread->state = FREE;
    thread_schedule();
}
// --- END CHANGED ---

void thread_a(void) {
    printf("thread_a started\n");
    int sum = 0;
    for (int i = 0; i < 100; i++) {
        sum += i;
        thread_yield();
    }
    printf("thread_a: exit with sum %d\n", sum);
    thread_exit(sum); // CHANGED: exit with return value
}

void thread_b(void) {
    printf("thread_b started\n");
    int product = 1;
    for (int i = 1; i <= 10; i++) {
        product *= i;
        thread_yield();
    }
    printf("thread_b: exit with factorial %d\n", product);
    thread_exit(product); // CHANGED
}

void thread_c(void) {
    printf("thread_c started\n");
    for (int i = 0; i < 100; i++) {
        thread_yield();
    }
    printf("thread_c: exit with 42\n");
    thread_exit(42); // CHANGED
}
```

Assembly: Same as Alternate 1.

---

## Alternate 15: Dynamic Thread Creation

**What changed:** Thread A dynamically creates Thread D during its execution (after iteration 50). Thread D runs its own loop.

```c
void thread_d(void) {
    // --- CHANGED: Dynamically created thread ---
    printf("thread_d (dynamic) started\n");
    for (int i = 0; i < 30; i++) {
        printf("thread_d %d\n", i);
        thread_yield();
    }
    printf("thread_d: exit\n");
    current_thread->state = FREE;
    thread_schedule();
}
// --- END CHANGED ---

void thread_a(void) {
    printf("thread_a started\n");
    for (int i = 0; i < 100; i++) {
        printf("thread_a %d\n", i);
        // --- CHANGED: Create thread_d dynamically at iteration 50 ---
        if (i == 50) {
            printf("thread_a: creating thread_d dynamically\n");
            thread_create(thread_d);
        }
        // --- END CHANGED ---
        thread_yield();
    }
    printf("thread_a: exit\n");
    current_thread->state = FREE;
    thread_schedule();
}

// thread_b and thread_c unchanged from original
```

Assembly: Same as Alternate 1.

---

## Alternate 16: Thread That Prints Squares

**What changed:** Thread A prints squares (i*i), Thread B prints cubes (i*i*i), Thread C prints triangular numbers (i*(i+1)/2).

```c
// --- CHANGED: Threads compute different mathematical sequences ---
void thread_a(void) {
    printf("thread_a (squares) started\n");
    for (int i = 0; i < 20; i++) {
        printf("thread_a square(%d) = %d\n", i, i * i);
        thread_yield();
    }
    printf("thread_a: exit\n");
    current_thread->state = FREE;
    thread_schedule();
}

void thread_b(void) {
    printf("thread_b (cubes) started\n");
    for (int i = 0; i < 20; i++) {
        printf("thread_b cube(%d) = %d\n", i, i * i * i);
        thread_yield();
    }
    printf("thread_b: exit\n");
    current_thread->state = FREE;
    thread_schedule();
}

void thread_c(void) {
    printf("thread_c (triangular) started\n");
    for (int i = 0; i < 20; i++) {
        printf("thread_c tri(%d) = %d\n", i, i * (i + 1) / 2);
        thread_yield();
    }
    printf("thread_c: exit\n");
    current_thread->state = FREE;
    thread_schedule();
}
// --- END CHANGED ---
```

Assembly: Same as Alternate 1.

---

## Alternate 17: LIFO Scheduling (Stack Order)

**What changed:** Instead of round-robin, the scheduler picks the most recently created runnable thread (LIFO/stack order), always checking from the end of the thread table.

```c
void thread_schedule(void) {
    // --- CHANGED: LIFO scheduling - search from end of array ---
    struct thread *next_thread = 0;
    for (int i = MAX_THREAD - 1; i >= 0; i--) {
        if (all_thread[i].state == RUNNABLE) {
            next_thread = &all_thread[i];
            break;
        }
    }
    // --- END CHANGED ---

    if (next_thread == 0) {
        if (current_thread->state == RUNNING) return;
        printf("thread_schedule: no runnable threads\n");
        exit(0);
    }
    if (current_thread != next_thread) {
        next_thread->state = RUNNING;
        struct thread *prev = current_thread;
        current_thread = next_thread;
        thread_switch(&prev->context, &next_thread->context);
    }
}
```

Assembly: Same as Alternate 1.

---

## Alternate 18: Thread State Display

**What changed:** The scheduler prints the state of all threads every time it runs, showing a table of thread states.

```c
void thread_schedule(void) {
    // --- CHANGED: Print thread state table ---
    printf("[ ");
    for (int i = 0; i < MAX_THREAD; i++) {
        char *s = "?";
        if (all_thread[i].state == FREE) s = "FREE";
        else if (all_thread[i].state == RUNNING) s = "RUN";
        else if (all_thread[i].state == RUNNABLE) s = "READY";
        printf("T%d:%s ", i, s);
    }
    printf("]\n");
    // --- END CHANGED ---

    struct thread *t, *next_thread = 0;
    t = current_thread + 1;
    for (int i = 0; i < MAX_THREAD; i++) {
        if (t >= all_thread + MAX_THREAD) t = all_thread;
        if (t->state == RUNNABLE) { next_thread = t; break; }
        t++;
    }
    if (next_thread == 0) {
        if (current_thread->state == RUNNING) return;
        printf("thread_schedule: no runnable threads\n");
        exit(0);
    }
    if (current_thread != next_thread) {
        next_thread->state = RUNNING;
        struct thread *prev = current_thread;
        current_thread = next_thread;
        thread_switch(&prev->context, &next_thread->context);
    }
}
```

Assembly: Same as Alternate 1.

---

## Alternate 19: Thread With Accumulator (Pass Data Between Threads)

**What changed:** A global accumulator is passed between threads. Each thread adds its own value and yields. The final accumulated result is printed.

```c
// --- CHANGED: Global accumulator ---
int accumulator = 0;
// --- END CHANGED ---

void thread_a(void) {
    printf("thread_a started\n");
    for (int i = 0; i < 100; i++) {
        accumulator += 1; // CHANGED: adds 1 each iteration
        thread_yield();
    }
    printf("thread_a: exit (accumulator = %d)\n", accumulator);
    current_thread->state = FREE;
    thread_schedule();
}

void thread_b(void) {
    printf("thread_b started\n");
    for (int i = 0; i < 100; i++) {
        accumulator += 2; // CHANGED: adds 2 each iteration
        thread_yield();
    }
    printf("thread_b: exit (accumulator = %d)\n", accumulator);
    current_thread->state = FREE;
    thread_schedule();
}

void thread_c(void) {
    printf("thread_c started\n");
    for (int i = 0; i < 100; i++) {
        accumulator += 3; // CHANGED: adds 3 each iteration
        thread_yield();
    }
    // CHANGED: Final value should be 100*(1+2+3) = 600
    printf("thread_c: exit (accumulator = %d, expected 600)\n", accumulator);
    current_thread->state = FREE;
    thread_schedule();
}
```

Assembly: Same as Alternate 1.

---

## Alternate 20: Thread Ping-Pong (Two Threads Only)

**What changed:** Only two threads. They "ping-pong" back and forth, printing PING and PONG alternately.

```c
void thread_ping(void) {
    // --- CHANGED: Ping thread ---
    for (int i = 0; i < 50; i++) {
        printf("PING %d\n", i);
        thread_yield();
    }
    printf("ping: exit\n");
    current_thread->state = FREE;
    thread_schedule();
}

void thread_pong(void) {
    // --- CHANGED: Pong thread ---
    for (int i = 0; i < 50; i++) {
        printf("PONG %d\n", i);
        thread_yield();
    }
    printf("pong: exit\n");
    current_thread->state = FREE;
    thread_schedule();
}
// --- END CHANGED ---

int main(int argc, char *argv[]) {
    thread_init();
    // --- CHANGED: Only two threads ---
    thread_create(thread_ping);
    thread_create(thread_pong);
    // --- END CHANGED ---
    thread_schedule();
    exit(0);
}
```

Assembly: Same as Alternate 1.

---

## The Assembly (Shared Across All Alternates)

All 20 alternates use the same `uthread_switch.S`:

```asm
# user/uthread_switch.S
.globl thread_switch
thread_switch:
    # Save callee-saved registers of current thread (a0 = old context)
    sd ra, 0(a0)
    sd sp, 8(a0)
    sd s0, 16(a0)
    sd s1, 24(a0)
    sd s2, 32(a0)
    sd s3, 40(a0)
    sd s4, 48(a0)
    sd s5, 56(a0)
    sd s6, 64(a0)
    sd s7, 72(a0)
    sd s8, 80(a0)
    sd s9, 88(a0)
    sd s10, 96(a0)
    sd s11, 104(a0)

    # Restore callee-saved registers of next thread (a1 = new context)
    ld ra, 0(a1)
    ld sp, 8(a1)
    ld s0, 16(a1)
    ld s1, 24(a1)
    ld s2, 32(a1)
    ld s3, 40(a1)
    ld s4, 48(a1)
    ld s5, 56(a1)
    ld s6, 64(a1)
    ld s7, 72(a1)
    ld s8, 80(a1)
    ld s9, 88(a1)
    ld s10, 96(a1)
    ld s11, 104(a1)

    ret
```

---

## Quick Reference: All 20 Alternates

| # | Name | Key Change |
|---|------|------------|
| 1 | Four Threads | Added thread_d that loops 50 times |
| 2 | Priority Scheduling | Threads have priority; scheduler picks highest |
| 3 | Thread Join | `thread_join(tid)` blocks until target exits |
| 4 | Configurable Loop | Loop count from command-line argument |
| 5 | Yield Counter | Track and print per-thread yield counts |
| 6 | Different Loops | A=50, B=100, C=200 iterations |
| 7 | Time Slice | Auto-yield every N iterations |
| 8 | Thread Names | Named threads, scheduler prints switches |
| 9 | Fibonacci Thread | Thread A computes Fibonacci sequence |
| 10 | Producer-Consumer | Thread A produces, B consumes, C monitors |
| 11 | Countdown | Threads count 99 down to 0 |
| 12 | Thread Sleep | `thread_sleep(n)` yields N times |
| 13 | Shared Counter | All threads increment a shared global |
| 14 | Return Value | `thread_exit(retval)` stores a return value |
| 15 | Dynamic Creation | Thread A creates Thread D at iteration 50 |
| 16 | Math Sequences | Squares, cubes, triangular numbers |
| 17 | LIFO Scheduling | Scheduler picks most recently created thread |
| 18 | State Display | Scheduler prints thread state table |
| 19 | Accumulator | Threads add different values to shared accumulator |
| 20 | Ping-Pong | Two threads alternate PING/PONG |

---

> **Note:** The `uthread_switch.S` assembly is the same for all alternates. The context switch logic never changes because the callee-saved register set is fixed by the RISC-V calling convention. All variations are in the C code (scheduling policy, thread behavior, data structures). No need to add to `UPROGS` as stated in the original task.
