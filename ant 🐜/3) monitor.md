# Task 3: Monitor - 20 Alternate Versions

## Original Task Summary

The original **monitor** task requires:
- Add a new `monitor` system call to xv6 that takes an integer bitmask
- The bitmask specifies which system calls to trace (e.g., `1 << SYS_fork` traces fork)
- When a monitored syscall is about to return, print: `<pid>: syscall <name> -> <return_value>`
- The monitor mask is inherited by child processes via `fork()`
- Requires kernel modifications: new syscall number, `monitor_mask` field in `struct proc`, changes to `syscall()` dispatch, `kfork()` copy, and the user-space program `monitor.c`

**Kernel files modified:** `kernel/syscall.h`, `kernel/syscall.c`, `kernel/sysproc.c`, `kernel/proc.h`, `kernel/proc.c`, `user/user.h`, `user/usys.pl`

---

## Alternate 1: Monitor With Call Count

**What changed:** Instead of printing every monitored syscall invocation, the kernel tracks a per-process counter for each syscall. The user program `monitor` prints the total count of each monitored syscall after the child exits.

### Kernel changes

```c
// kernel/proc.h
// --- CHANGED: Added syscall counter array instead of just a mask ---
struct proc {
    // ... existing fields ...
    uint32 monitor_mask;
    int syscall_counts[32]; // CHANGED: counter for each syscall number
};
// --- END CHANGED ---
```

```c
// kernel/sysproc.c
// --- CHANGED: sys_monitor also zeroes the counter array ---
uint64
sys_monitor(void)
{
    int mask;
    argint(0, &mask);
    struct proc *p = myproc();
    p->monitor_mask = (uint32)mask;

    // CHANGED: Reset all counters when monitor is set
    for (int i = 0; i < 32; i++)
        p->syscall_counts[i] = 0;

    return 0;
}
// --- END CHANGED ---
```

```c
// kernel/syscall.c - inside syscall() function
// --- CHANGED: Increment counter instead of printing every call ---
void
syscall(void)
{
    int num;
    struct proc *p = myproc();

    num = p->trapframe->a7;
    if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
        p->trapframe->a0 = syscalls[num]();
        // CHANGED: Increment counter for monitored syscalls
        if (p->monitor_mask & (1 << num)) {
            p->syscall_counts[num]++;
        }
    } else {
        printf("%d %s: unknown sys call %d\n", p->pid, p->name, num);
        p->trapframe->a0 = -1;
    }
}
// --- END CHANGED ---
```

```c
// kernel/sysproc.c
// --- CHANGED: New sys_getmonitor syscall to retrieve counts ---
// Also need to add SYS_getmonitor to syscall.h, syscalls array, etc.
uint64
sys_getmonitor(void)
{
    uint64 addr;
    argaddr(0, &addr);
    struct proc *p = myproc();
    // Copy the counts array to user space
    if (copyout(p->pagetable, addr, (char *)p->syscall_counts,
                sizeof(p->syscall_counts)) < 0)
        return -1;
    return 0;
}
// --- END CHANGED ---
```

### User program

```c
// user/monitor.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/syscall.h"

// --- CHANGED: syscall name table in user space for printing counts ---
static const char *syscall_names[] = {
    [SYS_fork]    "fork",
    [SYS_exit]    "exit",
    [SYS_wait]    "wait",
    [SYS_pipe]    "pipe",
    [SYS_read]    "read",
    [SYS_kill]    "kill",
    [SYS_exec]    "exec",
    [SYS_fstat]   "fstat",
    [SYS_chdir]   "chdir",
    [SYS_dup]     "dup",
    [SYS_getpid]  "getpid",
    [SYS_sbrk]    "sbrk",
    [SYS_pause]   "pause",
    [SYS_uptime]  "uptime",
    [SYS_open]    "open",
    [SYS_write]   "write",
    [SYS_mknod]   "mknod",
    [SYS_unlink]  "unlink",
    [SYS_link]    "link",
    [SYS_mkdir]   "mkdir",
    [SYS_close]   "close",
    [SYS_monitor] "monitor",
};
// --- END CHANGED ---

int
main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(2, "Usage: monitor <mask> <command> [args]\n");
        exit(1);
    }

    int mask = atoi(argv[1]);
    monitor(mask);

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        exec(argv[2], &argv[2]);
        fprintf(2, "exec %s failed\n", argv[2]);
        exit(1);
    }

    wait(0);

    // --- CHANGED: After child exits, retrieve and print syscall counts ---
    int counts[32];
    getmonitor((uint64)counts); // new syscall to get counts

    printf("\nSyscall counts:\n");
    for (int i = 1; i < 32; i++) {
        if ((mask & (1 << i)) && counts[i] > 0) {
            printf("  %s: %d\n", syscall_names[i], counts[i]);
        }
    }
    // --- END CHANGED ---

    exit(0);
}
```

---

## Alternate 2: Monitor With Timestamp

**What changed:** When printing a monitored syscall, include the system uptime (ticks) alongside the PID, name, and return value.

### Kernel changes

```c
// kernel/syscall.c - inside syscall() function
// --- CHANGED: Print uptime ticks alongside syscall info ---
void
syscall(void)
{
    int num;
    struct proc *p = myproc();

    num = p->trapframe->a7;
    if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
        p->trapframe->a0 = syscalls[num]();
        if (p->monitor_mask & (1 << num)) {
            // CHANGED: Include ticks (uptime) in output
            extern uint ticks;
            printf("%d: [tick %d] syscall %s -> %d\n",
                   p->pid, ticks, syscall_names[num], p->trapframe->a0);
        }
    } else {
        printf("%d %s: unknown sys call %d\n", p->pid, p->name, num);
        p->trapframe->a0 = -1;
    }
}
// --- END CHANGED ---
```

### User program (unchanged from original)

```c
// user/monitor.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(2, "Usage: monitor <mask> <command> [args]\n");
        exit(1);
    }

    monitor(atoi(argv[1]));

    // The kernel change handles the timestamp printing
    // User program is identical to original
    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }
    if (pid == 0) {
        exec(argv[2], &argv[2]);
        fprintf(2, "exec %s failed\n", argv[2]);
        exit(1);
    }
    wait(0);
    exit(0);
}
```

**Example output:** `4: [tick 127] syscall read -> 1023`

---

## Alternate 3: Monitor Only Failed Syscalls

**What changed:** Instead of printing every monitored syscall, only print syscalls that returned a negative value (i.e., failed).

### Kernel changes

```c
// kernel/syscall.c - inside syscall() function
// --- CHANGED: Only print monitored syscalls that FAILED (return < 0) ---
void
syscall(void)
{
    int num;
    struct proc *p = myproc();

    num = p->trapframe->a7;
    if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
        p->trapframe->a0 = syscalls[num]();
        if (p->monitor_mask & (1 << num)) {
            // CHANGED: Only print if syscall returned negative (error)
            if ((int)p->trapframe->a0 < 0) {
                printf("%d: syscall %s FAILED -> %d\n",
                       p->pid, syscall_names[num], p->trapframe->a0);
            }
        }
    } else {
        printf("%d %s: unknown sys call %d\n", p->pid, p->name, num);
        p->trapframe->a0 = -1;
    }
}
// --- END CHANGED ---
```

### User program

```c
// user/monitor.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(2, "Usage: monitor <mask> <command> [args]\n");
        exit(1);
    }

    // --- CHANGED: Prints a note that only failures are shown ---
    printf("monitoring failures only for mask %d\n", atoi(argv[1]));
    // --- END CHANGED ---

    monitor(atoi(argv[1]));

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }
    if (pid == 0) {
        exec(argv[2], &argv[2]);
        fprintf(2, "exec %s failed\n", argv[2]);
        exit(1);
    }
    wait(0);
    exit(0);
}
```

---

## Alternate 4: Monitor With Process Name

**What changed:** Print the process name (from `p->name`) alongside the PID in the trace output.

### Kernel changes

```c
// kernel/syscall.c - inside syscall() function
// --- CHANGED: Include process name in monitor output ---
void
syscall(void)
{
    int num;
    struct proc *p = myproc();

    num = p->trapframe->a7;
    if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
        p->trapframe->a0 = syscalls[num]();
        if (p->monitor_mask & (1 << num)) {
            // CHANGED: Print p->name alongside pid
            printf("%d (%s): syscall %s -> %d\n",
                   p->pid, p->name, syscall_names[num], p->trapframe->a0);
        }
    } else {
        printf("%d %s: unknown sys call %d\n", p->pid, p->name, num);
        p->trapframe->a0 = -1;
    }
}
// --- END CHANGED ---
```

### User program (unchanged from original)

```c
// user/monitor.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(2, "Usage: monitor <mask> <command> [args]\n");
        exit(1);
    }

    monitor(atoi(argv[1]));

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }
    if (pid == 0) {
        exec(argv[2], &argv[2]);
        fprintf(2, "exec %s failed\n", argv[2]);
        exit(1);
    }
    wait(0);
    exit(0);
}
```

**Example output:** `4 (grep): syscall read -> 1023`

---

## Alternate 5: Monitor With Syscall Number

**What changed:** Print the raw syscall number alongside the name in trace output.

### Kernel changes

```c
// kernel/syscall.c - inside syscall() function
// --- CHANGED: Include syscall number in output ---
void
syscall(void)
{
    int num;
    struct proc *p = myproc();

    num = p->trapframe->a7;
    if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
        p->trapframe->a0 = syscalls[num]();
        if (p->monitor_mask & (1 << num)) {
            // CHANGED: Print syscall number alongside name
            printf("%d: syscall[%d] %s -> %d\n",
                   p->pid, num, syscall_names[num], p->trapframe->a0);
        }
    } else {
        printf("%d %s: unknown sys call %d\n", p->pid, p->name, num);
        p->trapframe->a0 = -1;
    }
}
// --- END CHANGED ---
```

### User program (unchanged from original)

```c
// user/monitor.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(2, "Usage: monitor <mask> <command> [args]\n");
        exit(1);
    }
    monitor(atoi(argv[1]));
    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }
    if (pid == 0) {
        exec(argv[2], &argv[2]);
        fprintf(2, "exec %s failed\n", argv[2]);
        exit(1);
    }
    wait(0);
    exit(0);
}
```

**Example output:** `4: syscall[5] read -> 1023`

---

## Alternate 6: Monitor Do-Not-Inherit

**What changed:** The monitor mask is NOT inherited by child processes. Only the process that calls `monitor()` is traced. (Opposite of the original which inherits via fork.)

### Kernel changes

```c
// kernel/proc.c - inside kfork()
// --- CHANGED: Do NOT copy monitor_mask to child ---
// The original has: np->monitor_mask = p->monitor_mask;
// CHANGED: This line is removed, so children don't inherit the mask.
// np->monitor_mask is already 0 from allocproc.
// --- END CHANGED ---
```

### User program

```c
// user/monitor.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(2, "Usage: monitor <mask> <command> [args]\n");
        exit(1);
    }

    monitor(atoi(argv[1]));

    // --- CHANGED: Note to user that children are NOT traced ---
    printf("monitoring PID %d only (children NOT traced)\n", getpid());
    // --- END CHANGED ---

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }
    if (pid == 0) {
        exec(argv[2], &argv[2]);
        fprintf(2, "exec %s failed\n", argv[2]);
        exit(1);
    }
    wait(0);
    exit(0);
}
```

---

## Alternate 7: Monitor Toggle (Enable/Disable)

**What changed:** Calling `monitor(mask)` toggles monitoring. If the mask is already set to the same value, it turns monitoring off (sets mask to 0). This lets a program enable and disable tracing mid-execution.

### Kernel changes

```c
// kernel/sysproc.c
// --- CHANGED: Toggle behavior for monitor ---
uint64
sys_monitor(void)
{
    int mask;
    argint(0, &mask);
    struct proc *p = myproc();

    // CHANGED: If same mask is already set, turn it off (toggle)
    if (p->monitor_mask == (uint32)mask) {
        p->monitor_mask = 0; // disable
    } else {
        p->monitor_mask = (uint32)mask; // enable
    }

    return 0;
}
// --- END CHANGED ---
```

### User program

```c
// user/monitor.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(2, "Usage: monitor <mask> <command> [args]\n");
        exit(1);
    }

    // --- CHANGED: Demonstrate toggle ---
    int mask = atoi(argv[1]);
    monitor(mask);  // Enable
    printf("monitoring enabled with mask %d\n", mask);
    // --- END CHANGED ---

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }
    if (pid == 0) {
        exec(argv[2], &argv[2]);
        fprintf(2, "exec %s failed\n", argv[2]);
        exit(1);
    }
    wait(0);

    // --- CHANGED: Toggle off ---
    monitor(mask);  // same mask again = disable
    printf("monitoring disabled\n");
    // --- END CHANGED ---

    exit(0);
}
```

---

## Alternate 8: Monitor With Max Trace Limit

**What changed:** The `monitor` syscall takes a second argument: a maximum number of trace lines to print. After that many lines are printed, tracing automatically stops.

### Kernel changes

```c
// kernel/proc.h
// --- CHANGED: Added trace limit and counter ---
struct proc {
    // ... existing fields ...
    uint32 monitor_mask;
    int monitor_limit;   // CHANGED: max traces to print
    int monitor_printed;  // CHANGED: how many have been printed
};
// --- END CHANGED ---
```

```c
// kernel/sysproc.c
// --- CHANGED: sys_monitor takes two arguments ---
uint64
sys_monitor(void)
{
    int mask, limit;
    argint(0, &mask);
    argint(1, &limit); // CHANGED: second arg is the limit
    struct proc *p = myproc();
    p->monitor_mask = (uint32)mask;
    p->monitor_limit = limit;
    p->monitor_printed = 0;

    return 0;
}
// --- END CHANGED ---
```

```c
// kernel/syscall.c - inside syscall() function
// --- CHANGED: Respect the trace limit ---
void
syscall(void)
{
    int num;
    struct proc *p = myproc();

    num = p->trapframe->a7;
    if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
        p->trapframe->a0 = syscalls[num]();
        if (p->monitor_mask & (1 << num)) {
            // CHANGED: Only print if under the limit
            if (p->monitor_limit == 0 || p->monitor_printed < p->monitor_limit) {
                printf("%d: syscall %s -> %d\n",
                       p->pid, syscall_names[num], p->trapframe->a0);
                p->monitor_printed++;
            }
        }
    } else {
        printf("%d %s: unknown sys call %d\n", p->pid, p->name, num);
        p->trapframe->a0 = -1;
    }
}
// --- END CHANGED ---
```

### User program

```c
// user/monitor.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    // --- CHANGED: Takes mask AND limit ---
    if (argc < 4) {
        fprintf(2, "Usage: monitor <mask> <limit> <command> [args]\n");
        exit(1);
    }

    int mask = atoi(argv[1]);
    int limit = atoi(argv[2]); // CHANGED: max traces
    monitor(mask, limit);
    // --- END CHANGED ---

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }
    if (pid == 0) {
        exec(argv[3], &argv[3]); // CHANGED: command is argv[3] now
        fprintf(2, "exec %s failed\n", argv[3]);
        exit(1);
    }
    wait(0);
    exit(0);
}
```

---

## Alternate 9: Monitor With Inverted Mask

**What changed:** The mask specifies which syscalls to EXCLUDE from tracing (inverted logic). All syscalls are traced EXCEPT those whose bits are set.

### Kernel changes

```c
// kernel/syscall.c - inside syscall() function
// --- CHANGED: Inverted mask logic ---
void
syscall(void)
{
    int num;
    struct proc *p = myproc();

    num = p->trapframe->a7;
    if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
        p->trapframe->a0 = syscalls[num]();
        // CHANGED: Print if bit is NOT set (inverted)
        // mask of 0 = trace everything; mask with bit set = exclude that syscall
        if (p->monitor_mask != 0 && !(p->monitor_mask & (1 << num))) {
            printf("%d: syscall %s -> %d\n",
                   p->pid, syscall_names[num], p->trapframe->a0);
        }
    } else {
        printf("%d %s: unknown sys call %d\n", p->pid, p->name, num);
        p->trapframe->a0 = -1;
    }
}
// --- END CHANGED ---
```

### User program

```c
// user/monitor.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(2, "Usage: monitor <exclude_mask> <command> [args]\n");
        exit(1);
    }

    // --- CHANGED: Mask now EXCLUDES syscalls ---
    printf("excluding syscalls matching mask %d\n", atoi(argv[1]));
    // --- END CHANGED ---

    monitor(atoi(argv[1]));

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }
    if (pid == 0) {
        exec(argv[2], &argv[2]);
        fprintf(2, "exec %s failed\n", argv[2]);
        exit(1);
    }
    wait(0);
    exit(0);
}
```

---

## Alternate 10: Monitor Syscall Duration

**What changed:** The kernel records the tick count before and after each monitored syscall and prints the duration (in ticks) that the syscall took.

### Kernel changes

```c
// kernel/syscall.c - inside syscall() function
// --- CHANGED: Measure and print syscall duration in ticks ---
void
syscall(void)
{
    int num;
    struct proc *p = myproc();

    num = p->trapframe->a7;
    if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
        // CHANGED: Record start time
        extern uint ticks;
        uint start_tick = 0;
        int monitored = (p->monitor_mask & (1 << num)) != 0;
        if (monitored)
            start_tick = ticks;

        p->trapframe->a0 = syscalls[num]();

        if (monitored) {
            // CHANGED: Print duration
            uint end_tick = ticks;
            printf("%d: syscall %s -> %d (duration: %d ticks)\n",
                   p->pid, syscall_names[num], p->trapframe->a0,
                   end_tick - start_tick);
        }
    } else {
        printf("%d %s: unknown sys call %d\n", p->pid, p->name, num);
        p->trapframe->a0 = -1;
    }
}
// --- END CHANGED ---
```

### User program (unchanged from original)

```c
// user/monitor.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(2, "Usage: monitor <mask> <command> [args]\n");
        exit(1);
    }
    monitor(atoi(argv[1]));
    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }
    if (pid == 0) {
        exec(argv[2], &argv[2]);
        fprintf(2, "exec %s failed\n", argv[2]);
        exit(1);
    }
    wait(0);
    exit(0);
}
```

**Example output:** `4: syscall read -> 1023 (duration: 2 ticks)`

---

## Alternate 11: Monitor Log to Buffer (Ring Buffer)

**What changed:** Instead of printing immediately, the kernel stores trace events in a per-process ring buffer. A new `getlog` syscall retrieves the buffer contents for the user program to print.

### Kernel changes

```c
// kernel/proc.h
// --- CHANGED: Per-process ring buffer for trace events ---
#define MONITOR_LOG_SIZE 1024

struct proc {
    // ... existing fields ...
    uint32 monitor_mask;
    char monitor_log[MONITOR_LOG_SIZE]; // CHANGED: ring buffer
    int monitor_log_pos;                // CHANGED: write position
};
// --- END CHANGED ---
```

```c
// kernel/syscall.c - inside syscall() function
// --- CHANGED: Write to ring buffer instead of printing ---
// Helper to append to log
static void
log_append(struct proc *p, char *s)
{
    while (*s && p->monitor_log_pos < MONITOR_LOG_SIZE - 1) {
        p->monitor_log[p->monitor_log_pos++] = *s++;
    }
}

void
syscall(void)
{
    int num;
    struct proc *p = myproc();

    num = p->trapframe->a7;
    if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
        p->trapframe->a0 = syscalls[num]();
        if (p->monitor_mask & (1 << num)) {
            // CHANGED: Append to log buffer instead of printf
            // Format a simple log entry manually
            char buf[64];
            // Simplified: just store syscall name and return value
            char *name = (char *)syscall_names[num];
            log_append(p, name);
            log_append(p, ":");
            // Convert return value to string (simplified)
            char numbuf[12];
            int val = (int)p->trapframe->a0;
            int neg = 0;
            if (val < 0) { neg = 1; val = -val; }
            int idx = 11;
            numbuf[idx] = 0;
            do {
                numbuf[--idx] = '0' + (val % 10);
                val /= 10;
            } while (val > 0);
            if (neg) numbuf[--idx] = '-';
            log_append(p, &numbuf[idx]);
            log_append(p, "\n");
        }
    } else {
        printf("%d %s: unknown sys call %d\n", p->pid, p->name, num);
        p->trapframe->a0 = -1;
    }
}
// --- END CHANGED ---
```

```c
// kernel/sysproc.c
// --- CHANGED: New sys_getlog to copy log buffer to user space ---
uint64
sys_getlog(void)
{
    uint64 addr;
    argaddr(0, &addr);
    struct proc *p = myproc();
    p->monitor_log[p->monitor_log_pos] = 0; // null terminate
    if (copyout(p->pagetable, addr, p->monitor_log, p->monitor_log_pos + 1) < 0)
        return -1;
    return p->monitor_log_pos;
}
// --- END CHANGED ---
```

### User program

```c
// user/monitor.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(2, "Usage: monitor <mask> <command> [args]\n");
        exit(1);
    }

    monitor(atoi(argv[1]));

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }
    if (pid == 0) {
        exec(argv[2], &argv[2]);
        fprintf(2, "exec %s failed\n", argv[2]);
        exit(1);
    }
    wait(0);

    // --- CHANGED: Retrieve and print the log buffer ---
    char log[1024];
    int len = getlog((uint64)log);
    if (len > 0) {
        printf("=== Monitor Log ===\n");
        write(1, log, len);
        printf("=== End Log ===\n");
    }
    // --- END CHANGED ---

    exit(0);
}
```

---

## Alternate 12: Monitor Specific PID Only

**What changed:** The `monitor` syscall takes a second argument: a target PID. Only syscalls from that specific PID are traced (not the calling process or other children).

### Kernel changes

```c
// kernel/proc.h
// --- CHANGED: Added target PID field ---
struct proc {
    // ... existing fields ...
    uint32 monitor_mask;
    int monitor_target_pid; // CHANGED: only trace this PID
};
// --- END CHANGED ---
```

```c
// kernel/sysproc.c
// --- CHANGED: sys_monitor takes mask and target PID ---
uint64
sys_monitor(void)
{
    int mask, target;
    argint(0, &mask);
    argint(1, &target); // CHANGED: second arg is target PID
    struct proc *p = myproc();
    p->monitor_mask = (uint32)mask;
    p->monitor_target_pid = target;
    return 0;
}
// --- END CHANGED ---
```

```c
// kernel/syscall.c - inside syscall() function
// --- CHANGED: Only trace if PID matches target ---
void
syscall(void)
{
    int num;
    struct proc *p = myproc();

    num = p->trapframe->a7;
    if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
        p->trapframe->a0 = syscalls[num]();
        if (p->monitor_mask & (1 << num)) {
            // CHANGED: Check if current PID matches the target
            if (p->monitor_target_pid == 0 || p->pid == p->monitor_target_pid) {
                printf("%d: syscall %s -> %d\n",
                       p->pid, syscall_names[num], p->trapframe->a0);
            }
        }
    } else {
        printf("%d %s: unknown sys call %d\n", p->pid, p->name, num);
        p->trapframe->a0 = -1;
    }
}
// --- END CHANGED ---
```

### User program

```c
// user/monitor.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    // --- CHANGED: Takes mask, target PID, and command ---
    if (argc < 4) {
        fprintf(2, "Usage: monitor <mask> <target_pid> <command> [args]\n");
        exit(1);
    }

    int mask = atoi(argv[1]);
    int target = atoi(argv[2]); // CHANGED: target PID
    monitor(mask, target);
    // --- END CHANGED ---

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }
    if (pid == 0) {
        exec(argv[3], &argv[3]); // CHANGED: command is argv[3]
        fprintf(2, "exec %s failed\n", argv[3]);
        exit(1);
    }
    wait(0);
    exit(0);
}
```

---

## Alternate 13: Monitor With Enter/Exit Printing

**What changed:** Print two lines per monitored syscall: one when the syscall is entered (before execution) and one when it returns (after execution).

### Kernel changes

```c
// kernel/syscall.c - inside syscall() function
// --- CHANGED: Print both ENTER and EXIT for monitored syscalls ---
void
syscall(void)
{
    int num;
    struct proc *p = myproc();

    num = p->trapframe->a7;
    if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
        int monitored = (p->monitor_mask & (1 << num)) != 0;

        // CHANGED: Print ENTER before executing syscall
        if (monitored)
            printf("%d: syscall %s ENTER\n", p->pid, syscall_names[num]);

        p->trapframe->a0 = syscalls[num]();

        // CHANGED: Print EXIT after executing syscall
        if (monitored)
            printf("%d: syscall %s EXIT -> %d\n",
                   p->pid, syscall_names[num], p->trapframe->a0);
    } else {
        printf("%d %s: unknown sys call %d\n", p->pid, p->name, num);
        p->trapframe->a0 = -1;
    }
}
// --- END CHANGED ---
```

### User program (unchanged from original)

```c
// user/monitor.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(2, "Usage: monitor <mask> <command> [args]\n");
        exit(1);
    }
    monitor(atoi(argv[1]));
    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }
    if (pid == 0) {
        exec(argv[2], &argv[2]);
        fprintf(2, "exec %s failed\n", argv[2]);
        exit(1);
    }
    wait(0);
    exit(0);
}
```

**Example output:**
```
4: syscall read ENTER
4: syscall read EXIT -> 1023
```

---

## Alternate 14: Monitor by Syscall Name (String Argument)

**What changed:** Instead of a numeric bitmask, the user program takes a syscall name as a string (e.g., "read", "fork") and the program computes the correct mask automatically.

### Kernel changes (unchanged from original)

The kernel side is identical to the original since it still uses a bitmask internally.

### User program

```c
// user/monitor.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/syscall.h"

// --- CHANGED: Map syscall names to numbers ---
struct {
    char *name;
    int num;
} syscall_table[] = {
    {"fork",    SYS_fork},
    {"exit",    SYS_exit},
    {"wait",    SYS_wait},
    {"pipe",    SYS_pipe},
    {"read",    SYS_read},
    {"kill",    SYS_kill},
    {"exec",    SYS_exec},
    {"fstat",   SYS_fstat},
    {"chdir",   SYS_chdir},
    {"dup",     SYS_dup},
    {"getpid",  SYS_getpid},
    {"sbrk",    SYS_sbrk},
    {"uptime",  SYS_uptime},
    {"open",    SYS_open},
    {"write",   SYS_write},
    {"mknod",   SYS_mknod},
    {"unlink",  SYS_unlink},
    {"link",    SYS_link},
    {"mkdir",   SYS_mkdir},
    {"close",   SYS_close},
    {"monitor", SYS_monitor},
    {0, 0},
};

int
name_to_mask(char *name)
{
    for (int i = 0; syscall_table[i].name != 0; i++) {
        if (strcmp(name, syscall_table[i].name) == 0)
            return 1 << syscall_table[i].num;
    }
    return 0;
}
// --- END CHANGED ---

int
main(int argc, char *argv[])
{
    // --- CHANGED: Takes syscall name string instead of numeric mask ---
    if (argc < 3) {
        fprintf(2, "Usage: monitor <syscall_name> <command> [args]\n");
        exit(1);
    }

    int mask = name_to_mask(argv[1]);
    if (mask == 0) {
        fprintf(2, "unknown syscall: %s\n", argv[1]);
        exit(1);
    }

    printf("monitoring %s (mask=%d)\n", argv[1], mask);
    // --- END CHANGED ---

    monitor(mask);

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }
    if (pid == 0) {
        exec(argv[2], &argv[2]);
        fprintf(2, "exec %s failed\n", argv[2]);
        exit(1);
    }
    wait(0);
    exit(0);
}
```

**Usage:** `$ monitor read grep hello README` instead of `$ monitor 32 grep hello README`

---

## Alternate 15: Monitor With Depth Limit (No Grandchildren)

**What changed:** The mask is inherited by direct children but NOT by grandchildren. A `monitor_depth` field tracks inheritance depth and stops at 1.

### Kernel changes

```c
// kernel/proc.h
// --- CHANGED: Added depth counter ---
struct proc {
    // ... existing fields ...
    uint32 monitor_mask;
    int monitor_depth; // CHANGED: 0 = originator, increments on fork
};
// --- END CHANGED ---
```

```c
// kernel/proc.c - inside kfork()
// --- CHANGED: Copy mask only if depth allows ---
if (p->monitor_mask && p->monitor_depth < 1) {
    np->monitor_mask = p->monitor_mask;
    np->monitor_depth = p->monitor_depth + 1; // CHANGED: increment depth
} else {
    np->monitor_mask = 0; // CHANGED: grandchildren not traced
    np->monitor_depth = 0;
}
// --- END CHANGED ---
```

### User program (unchanged from original)

```c
// user/monitor.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(2, "Usage: monitor <mask> <command> [args]\n");
        exit(1);
    }
    monitor(atoi(argv[1]));
    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }
    if (pid == 0) {
        exec(argv[2], &argv[2]);
        fprintf(2, "exec %s failed\n", argv[2]);
        exit(1);
    }
    wait(0);
    exit(0);
}
```

---

## Alternate 16: Monitor Output to Pipe

**What changed:** The user program creates a pipe, forks, and routes the child's monitor output through the pipe. The parent reads from the pipe and prints the trace with a `[TRACE]` prefix.

### Kernel changes (unchanged from original)

### User program

```c
// user/monitor.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(2, "Usage: monitor <mask> <command> [args]\n");
        exit(1);
    }

    monitor(atoi(argv[1]));

    // --- CHANGED: Create pipe, redirect child stderr to pipe ---
    int p[2];
    pipe(p);

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        // Child: redirect fd 2 (stderr) to pipe write end
        close(p[0]);
        close(2);
        dup(p[1]);
        close(p[1]);

        exec(argv[2], &argv[2]);
        fprintf(2, "exec %s failed\n", argv[2]);
        exit(1);
    }

    // Parent: read from pipe and prefix each line
    close(p[1]);
    char buf[512];
    int n;
    // CHANGED: Read trace output from pipe and add prefix
    while ((n = read(p[0], buf, sizeof(buf) - 1)) > 0) {
        buf[n] = 0;
        printf("[TRACE] %s", buf);
    }
    close(p[0]);
    // --- END CHANGED ---

    wait(0);
    exit(0);
}
```

---

## Alternate 17: Monitor Multiple Masks (OR-combined)

**What changed:** The user program accepts multiple mask values as arguments and OR-combines them before passing to the kernel.

### Kernel changes (unchanged from original)

### User program

```c
// user/monitor.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    // --- CHANGED: Accept multiple masks, OR them together ---
    if (argc < 3) {
        fprintf(2, "Usage: monitor <mask1> [mask2] ... <command> [args]\n");
        exit(1);
    }

    // Find where the command starts (first non-numeric argument after mask(s))
    int combined_mask = 0;
    int cmd_start = 1;

    for (int i = 1; i < argc; i++) {
        // Check if argv[i] looks like a number
        int is_num = 1;
        for (int j = 0; argv[i][j]; j++) {
            if (argv[i][j] < '0' || argv[i][j] > '9') {
                is_num = 0;
                break;
            }
        }
        if (is_num) {
            // CHANGED: OR this mask into the combined mask
            combined_mask |= atoi(argv[i]);
        } else {
            cmd_start = i;
            break;
        }
    }

    if (cmd_start == 1) {
        fprintf(2, "no command specified\n");
        exit(1);
    }

    printf("combined mask: %d\n", combined_mask);
    // --- END CHANGED ---

    monitor(combined_mask);

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }
    if (pid == 0) {
        exec(argv[cmd_start], &argv[cmd_start]);
        fprintf(2, "exec %s failed\n", argv[cmd_start]);
        exit(1);
    }
    wait(0);
    exit(0);
}
```

**Usage:** `$ monitor 2 32 128 grep hello README` (ORs masks for fork, read, exec)

---

## Alternate 18: Monitor With Return Value Filter

**What changed:** The `monitor` syscall takes a second argument: a return value threshold. Only syscalls whose return value exceeds this threshold are printed.

### Kernel changes

```c
// kernel/proc.h
// --- CHANGED: Added threshold field ---
struct proc {
    // ... existing fields ...
    uint32 monitor_mask;
    int monitor_threshold; // CHANGED: only print if return > threshold
};
// --- END CHANGED ---
```

```c
// kernel/sysproc.c
// --- CHANGED: sys_monitor takes mask and threshold ---
uint64
sys_monitor(void)
{
    int mask, threshold;
    argint(0, &mask);
    argint(1, &threshold); // CHANGED: second arg is threshold
    struct proc *p = myproc();
    p->monitor_mask = (uint32)mask;
    p->monitor_threshold = threshold;
    return 0;
}
// --- END CHANGED ---
```

```c
// kernel/syscall.c - inside syscall() function
// --- CHANGED: Filter by return value threshold ---
void
syscall(void)
{
    int num;
    struct proc *p = myproc();

    num = p->trapframe->a7;
    if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
        p->trapframe->a0 = syscalls[num]();
        if (p->monitor_mask & (1 << num)) {
            // CHANGED: Only print if return value > threshold
            int retval = (int)p->trapframe->a0;
            if (retval > p->monitor_threshold) {
                printf("%d: syscall %s -> %d (above threshold %d)\n",
                       p->pid, syscall_names[num], retval, p->monitor_threshold);
            }
        }
    } else {
        printf("%d %s: unknown sys call %d\n", p->pid, p->name, num);
        p->trapframe->a0 = -1;
    }
}
// --- END CHANGED ---
```

### User program

```c
// user/monitor.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    // --- CHANGED: Takes mask, threshold, and command ---
    if (argc < 4) {
        fprintf(2, "Usage: monitor <mask> <threshold> <command> [args]\n");
        exit(1);
    }

    int mask = atoi(argv[1]);
    int threshold = atoi(argv[2]); // CHANGED
    monitor(mask, threshold);
    // --- END CHANGED ---

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }
    if (pid == 0) {
        exec(argv[3], &argv[3]);
        fprintf(2, "exec %s failed\n", argv[3]);
        exit(1);
    }
    wait(0);
    exit(0);
}
```

---

## Alternate 19: Monitor Sequence Number

**What changed:** Each trace line includes a global sequence number that increments across all monitored syscalls for the process, showing the order of calls.

### Kernel changes

```c
// kernel/proc.h
// --- CHANGED: Added sequence counter ---
struct proc {
    // ... existing fields ...
    uint32 monitor_mask;
    int monitor_seq; // CHANGED: sequence number counter
};
// --- END CHANGED ---
```

```c
// kernel/syscall.c - inside syscall() function
// --- CHANGED: Print sequence number with each trace line ---
void
syscall(void)
{
    int num;
    struct proc *p = myproc();

    num = p->trapframe->a7;
    if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
        p->trapframe->a0 = syscalls[num]();
        if (p->monitor_mask & (1 << num)) {
            // CHANGED: Include incrementing sequence number
            p->monitor_seq++;
            printf("%d: [#%d] syscall %s -> %d\n",
                   p->pid, p->monitor_seq, syscall_names[num], p->trapframe->a0);
        }
    } else {
        printf("%d %s: unknown sys call %d\n", p->pid, p->name, num);
        p->trapframe->a0 = -1;
    }
}
// --- END CHANGED ---
```

### User program (unchanged from original)

```c
// user/monitor.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(2, "Usage: monitor <mask> <command> [args]\n");
        exit(1);
    }
    monitor(atoi(argv[1]));
    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }
    if (pid == 0) {
        exec(argv[2], &argv[2]);
        fprintf(2, "exec %s failed\n", argv[2]);
        exit(1);
    }
    wait(0);
    exit(0);
}
```

**Example output:** `4: [#3] syscall read -> 1023`

---

## Alternate 20: Monitor With Summary at Exit

**What changed:** Monitoring works normally (prints each syscall as it happens), but when the monitored process exits, a summary of total syscall counts is automatically printed by the kernel.

### Kernel changes

```c
// kernel/proc.h
// --- CHANGED: Added per-syscall counters alongside the mask ---
struct proc {
    // ... existing fields ...
    uint32 monitor_mask;
    int monitor_counts[32]; // CHANGED: count each syscall
};
// --- END CHANGED ---
```

```c
// kernel/syscall.c - inside syscall() function
// --- CHANGED: Print and also count ---
void
syscall(void)
{
    int num;
    struct proc *p = myproc();

    num = p->trapframe->a7;
    if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
        p->trapframe->a0 = syscalls[num]();
        if (p->monitor_mask & (1 << num)) {
            printf("%d: syscall %s -> %d\n",
                   p->pid, syscall_names[num], p->trapframe->a0);
            p->monitor_counts[num]++; // CHANGED: increment counter
        }
    } else {
        printf("%d %s: unknown sys call %d\n", p->pid, p->name, num);
        p->trapframe->a0 = -1;
    }
}
// --- END CHANGED ---
```

```c
// kernel/proc.c - inside exit() or freeproc()
// --- CHANGED: Print summary when a monitored process exits ---
// Add this before the process is fully freed:
if (p->monitor_mask) {
    printf("=== Monitor summary for PID %d ===\n", p->pid);
    for (int i = 1; i < 32; i++) {
        if (p->monitor_counts[i] > 0) {
            printf("  %s: %d calls\n", syscall_names[i], p->monitor_counts[i]);
        }
    }
    printf("=================================\n");
}
// --- END CHANGED ---
```

### User program (unchanged from original)

```c
// user/monitor.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(2, "Usage: monitor <mask> <command> [args]\n");
        exit(1);
    }
    monitor(atoi(argv[1]));
    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }
    if (pid == 0) {
        exec(argv[2], &argv[2]);
        fprintf(2, "exec %s failed\n", argv[2]);
        exit(1);
    }
    wait(0);
    exit(0);
}
```

---

## Quick Reference: All 20 Alternates

| # | Name | Key Change | Kernel Mod? |
|---|------|------------|-------------|
| 1 | Call Count | Count syscalls, print totals via new `getmonitor` syscall | Yes |
| 2 | Timestamp | Include `ticks` (uptime) in trace output | Yes |
| 3 | Failed Only | Only print syscalls that returned negative | Yes |
| 4 | Process Name | Include `p->name` in trace output | Yes |
| 5 | Syscall Number | Print raw syscall number alongside name | Yes |
| 6 | No Inherit | Mask is NOT inherited by children (skip fork copy) | Yes |
| 7 | Toggle | Same mask twice turns monitoring off | Yes |
| 8 | Max Trace Limit | Second arg limits how many lines print | Yes |
| 9 | Inverted Mask | Mask EXCLUDES syscalls instead of including | Yes |
| 10 | Duration | Print tick duration of each syscall | Yes |
| 11 | Ring Buffer Log | Store traces in buffer, retrieve via `getlog` syscall | Yes |
| 12 | Specific PID | Only trace a target PID (second arg) | Yes |
| 13 | Enter/Exit | Print both ENTER and EXIT lines per syscall | Yes |
| 14 | Name Argument | User passes "read" instead of "32" | No (user only) |
| 15 | Depth Limit | Mask inherited by children but not grandchildren | Yes |
| 16 | Output to Pipe | Route trace output through a pipe with prefix | No (user only) |
| 17 | Multiple Masks | OR-combine multiple mask args | No (user only) |
| 18 | Return Threshold | Only print if return value exceeds a threshold | Yes |
| 19 | Sequence Number | Each trace line gets an incrementing sequence number | Yes |
| 20 | Summary at Exit | Kernel prints syscall count summary when process exits | Yes |

---

## Common Setup for All Alternates

For every alternate version, you need to:

1. **Add syscall number** in `kernel/syscall.h`:
   ```c
   #define SYS_monitor 22
   ```
   (Add `SYS_getmonitor` or `SYS_getlog` if the alternate adds new syscalls)

2. **Add prototype** in `user/user.h`:
   ```c
   int monitor(int);
   ```

3. **Add stub** in `user/usys.pl`:
   ```perl
   entry("monitor");
   ```

4. **Add `monitor_mask`** (and any other fields) to `struct proc` in `kernel/proc.h`

5. **Add `sys_monitor`** to `kernel/sysproc.c`

6. **Register** in syscalls array in `kernel/syscall.c`

7. **Add names** to `syscall_names` array in `kernel/syscall.c`

8. **Copy mask** in `kfork()` in `kernel/proc.c` (unless alternate 6)

9. **Add printing logic** in `syscall()` in `kernel/syscall.c`

10. **Add to UPROGS** in `Makefile`:
    ```
    $U/_monitor\
    ```

> **Note:** Run `make clean && make qemu` for a clean file system after changes.
