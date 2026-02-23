# Task 1: Handshake - 20 Alternate Versions

## Original Task Summary

The original **handshake** task requires:
- Parent sends a byte (from command line arg) to child via pipe
- Child prints `<pid>: received <x> from parent`, sends byte back to parent via second pipe, exits
- Parent reads byte from child, prints `<pid>: received <x> from child`, exits
- Uses: `pipe`, `fork`, `write`, `read`, `getpid`

---

## Alternate 1: Double Handshake

**What changed:** Instead of sending the byte once in each direction, the parent sends the byte to the child, the child increments it by 1 and sends it back, then the parent sends the incremented byte back again. Two full round trips instead of one.

```c
// user/doublehandshake.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int p2c[2]; // parent to child pipe
    int c2p[2]; // child to parent pipe
    char byte;

    if (argc != 2) {
        fprintf(2, "Usage: doublehandshake <byte>\n");
        exit(1);
    }

    byte = atoi(argv[1]);

    pipe(p2c);
    pipe(c2p);

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        // Child process
        close(p2c[1]);
        close(c2p[0]);

        // --- CHANGED: Two reads instead of one ---
        // First receive
        read(p2c[0], &byte, 1);
        printf("%d: received %d from parent (round 1)\n", getpid(), byte);
        byte = byte + 1; // CHANGED: increment the byte before sending back
        write(c2p[1], &byte, 1);

        // Second receive
        read(p2c[0], &byte, 1);
        printf("%d: received %d from parent (round 2)\n", getpid(), byte);
        write(c2p[1], &byte, 1);
        // --- END CHANGED ---

        close(p2c[0]);
        close(c2p[1]);
        exit(0);
    } else {
        // Parent process
        close(p2c[0]);
        close(c2p[1]);

        // --- CHANGED: Two sends instead of one ---
        // First send
        write(p2c[1], &byte, 1);
        read(c2p[0], &byte, 1);
        printf("%d: received %d from child (round 1)\n", getpid(), byte);

        // Second send: send the incremented byte back
        write(p2c[1], &byte, 1);
        read(c2p[0], &byte, 1);
        printf("%d: received %d from child (round 2)\n", getpid(), byte);
        // --- END CHANGED ---

        close(p2c[1]);
        close(c2p[0]);
        wait(0);
        exit(0);
    }
}
```

---

## Alternate 2: Reverse Handshake

**What changed:** Instead of the parent initiating the send, the child initiates by sending the byte first, and the parent echoes it back.

```c
// user/reversehandshake.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int p2c[2];
    int c2p[2];
    char byte;

    if (argc != 2) {
        fprintf(2, "Usage: reversehandshake <byte>\n");
        exit(1);
    }

    byte = atoi(argv[1]);

    pipe(p2c);
    pipe(c2p);

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        // Child process
        close(p2c[1]);
        close(c2p[0]);

        // --- CHANGED: Child SENDS first instead of receiving ---
        write(c2p[1], &byte, 1);
        printf("%d: sent %d to parent\n", getpid(), byte);

        read(p2c[0], &byte, 1);
        printf("%d: received %d back from parent\n", getpid(), byte);
        // --- END CHANGED ---

        close(p2c[0]);
        close(c2p[1]);
        exit(0);
    } else {
        // Parent process
        close(p2c[0]);
        close(c2p[1]);

        // --- CHANGED: Parent READS first instead of sending ---
        read(c2p[0], &byte, 1);
        printf("%d: received %d from child\n", getpid(), byte);

        write(p2c[1], &byte, 1);
        printf("%d: sent %d back to child\n", getpid(), byte);
        // --- END CHANGED ---

        close(p2c[1]);
        close(c2p[0]);
        wait(0);
        exit(0);
    }
}
```

---

## Alternate 3: Increment Handshake

**What changed:** Instead of echoing the same byte, the child adds 10 to the byte before sending it back to the parent. The parent prints both the original and modified byte.

```c
// user/inchandshake.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int p2c[2];
    int c2p[2];
    char byte;

    if (argc != 2) {
        fprintf(2, "Usage: inchandshake <byte>\n");
        exit(1);
    }

    byte = atoi(argv[1]);

    pipe(p2c);
    pipe(c2p);

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        close(p2c[1]);
        close(c2p[0]);

        read(p2c[0], &byte, 1);
        printf("%d: received %d from parent\n", getpid(), byte);

        // --- CHANGED: Add 10 to byte before sending back ---
        byte = byte + 10;
        printf("%d: sending %d (original + 10) to parent\n", getpid(), byte);
        // --- END CHANGED ---

        write(c2p[1], &byte, 1);

        close(p2c[0]);
        close(c2p[1]);
        exit(0);
    } else {
        close(p2c[0]);
        close(c2p[1]);

        char original = byte;
        write(p2c[1], &byte, 1);

        read(c2p[0], &byte, 1);
        // --- CHANGED: Print both original and modified value ---
        printf("%d: sent %d, received %d from child\n", getpid(), original, byte);
        // --- END CHANGED ---

        close(p2c[1]);
        close(c2p[0]);
        wait(0);
        exit(0);
    }
}
```

---

## Alternate 4: Multi-Byte Handshake

**What changed:** Instead of sending a single byte, the parent sends a 4-character string (the first 4 chars of argv[1]) to the child. The child prints each character and sends them all back.

```c
// user/multibytehs.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// --- CHANGED: Send 4 bytes instead of 1 ---
#define MSG_LEN 4
// --- END CHANGED ---

int main(int argc, char *argv[])
{
    int p2c[2];
    int c2p[2];
    char buf[MSG_LEN];

    if (argc != 2) {
        fprintf(2, "Usage: multibytehs <4-char-string>\n");
        exit(1);
    }

    // --- CHANGED: Copy up to MSG_LEN characters from argv ---
    int i;
    for (i = 0; i < MSG_LEN && argv[1][i] != 0; i++)
        buf[i] = argv[1][i];
    for (; i < MSG_LEN; i++)
        buf[i] = '0'; // pad with '0' if string is short
    // --- END CHANGED ---

    pipe(p2c);
    pipe(c2p);

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        close(p2c[1]);
        close(c2p[0]);

        // --- CHANGED: Read MSG_LEN bytes ---
        read(p2c[0], buf, MSG_LEN);
        printf("%d: received bytes", getpid());
        for (int j = 0; j < MSG_LEN; j++)
            printf(" %d", buf[j]);
        printf(" from parent\n");
        // --- END CHANGED ---

        write(c2p[1], buf, MSG_LEN);

        close(p2c[0]);
        close(c2p[1]);
        exit(0);
    } else {
        close(p2c[0]);
        close(c2p[1]);

        // --- CHANGED: Write and read MSG_LEN bytes ---
        write(p2c[1], buf, MSG_LEN);
        read(c2p[0], buf, MSG_LEN);
        printf("%d: received bytes", getpid());
        for (int j = 0; j < MSG_LEN; j++)
            printf(" %d", buf[j]);
        printf(" from child\n");
        // --- END CHANGED ---

        close(p2c[1]);
        close(c2p[0]);
        wait(0);
        exit(0);
    }
}
```

---

## Alternate 5: Negate Handshake

**What changed:** The child negates the byte (flips sign using two's complement: `byte = ~byte + 1`) before sending it back.

```c
// user/neghandshake.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int p2c[2];
    int c2p[2];
    char byte;

    if (argc != 2) {
        fprintf(2, "Usage: neghandshake <byte>\n");
        exit(1);
    }

    byte = atoi(argv[1]);

    pipe(p2c);
    pipe(c2p);

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        close(p2c[1]);
        close(c2p[0]);

        read(p2c[0], &byte, 1);
        printf("%d: received %d from parent\n", getpid(), byte);

        // --- CHANGED: Negate the byte (two's complement) ---
        byte = ~byte + 1;
        printf("%d: sending negated value %d to parent\n", getpid(), byte);
        // --- END CHANGED ---

        write(c2p[1], &byte, 1);

        close(p2c[0]);
        close(c2p[1]);
        exit(0);
    } else {
        close(p2c[0]);
        close(c2p[1]);

        write(p2c[1], &byte, 1);
        read(c2p[0], &byte, 1);
        printf("%d: received %d from child\n", getpid(), byte);

        close(p2c[1]);
        close(c2p[0]);
        wait(0);
        exit(0);
    }
}
```

---

## Alternate 6: Ping-Pong Count

**What changed:** Instead of one handshake, the parent and child ping-pong a byte back and forth N times (N is a second command-line argument). Each side prints every time it receives.

```c
// user/pingpong.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int p2c[2];
    int c2p[2];
    char byte;

    if (argc != 3) {
        // --- CHANGED: Takes a second arg for number of rounds ---
        fprintf(2, "Usage: pingpong <byte> <rounds>\n");
        exit(1);
    }

    byte = atoi(argv[1]);
    int rounds = atoi(argv[2]); // CHANGED: number of ping-pong rounds
    // --- END CHANGED ---

    pipe(p2c);
    pipe(c2p);

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        close(p2c[1]);
        close(c2p[0]);

        // --- CHANGED: Loop for N rounds ---
        for (int i = 0; i < rounds; i++) {
            read(p2c[0], &byte, 1);
            printf("%d: round %d, received %d from parent\n", getpid(), i + 1, byte);
            write(c2p[1], &byte, 1);
        }
        // --- END CHANGED ---

        close(p2c[0]);
        close(c2p[1]);
        exit(0);
    } else {
        close(p2c[0]);
        close(c2p[1]);

        // --- CHANGED: Loop for N rounds ---
        for (int i = 0; i < rounds; i++) {
            write(p2c[1], &byte, 1);
            read(c2p[0], &byte, 1);
            printf("%d: round %d, received %d from child\n", getpid(), i + 1, byte);
        }
        // --- END CHANGED ---

        close(p2c[1]);
        close(c2p[0]);
        wait(0);
        exit(0);
    }
}
```

---

## Alternate 7: Bitwise XOR Handshake

**What changed:** The child XORs the byte with 0xFF (flips all bits) before sending it back. The parent verifies the XOR was applied correctly.

```c
// user/xorhandshake.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// --- CHANGED: XOR mask constant ---
#define XOR_MASK 0xFF
// --- END CHANGED ---

int main(int argc, char *argv[])
{
    int p2c[2];
    int c2p[2];
    char byte;

    if (argc != 2) {
        fprintf(2, "Usage: xorhandshake <byte>\n");
        exit(1);
    }

    byte = atoi(argv[1]);

    pipe(p2c);
    pipe(c2p);

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        close(p2c[1]);
        close(c2p[0]);

        read(p2c[0], &byte, 1);
        printf("%d: received %d from parent\n", getpid(), byte);

        // --- CHANGED: XOR the byte before sending back ---
        byte = byte ^ XOR_MASK;
        printf("%d: sending XORed value %d to parent\n", getpid(), byte);
        // --- END CHANGED ---

        write(c2p[1], &byte, 1);

        close(p2c[0]);
        close(c2p[1]);
        exit(0);
    } else {
        close(p2c[0]);
        close(c2p[1]);

        char original = byte;
        write(p2c[1], &byte, 1);
        read(c2p[0], &byte, 1);

        // --- CHANGED: Verify XOR was applied ---
        printf("%d: received %d from child\n", getpid(), byte);
        if ((byte ^ XOR_MASK) == original)
            printf("%d: XOR verification passed\n", getpid());
        else
            printf("%d: XOR verification FAILED\n", getpid());
        // --- END CHANGED ---

        close(p2c[1]);
        close(c2p[0]);
        wait(0);
        exit(0);
    }
}
```

---

## Alternate 8: Two Children Handshake

**What changed:** Instead of one child, the parent forks two children. The parent sends the byte to both children (via separate pipes). Each child prints what it received and sends it back. The parent reads from both.

```c
// user/twochildhs.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    // --- CHANGED: Two sets of pipe pairs, one per child ---
    int p2c1[2], c2p1[2]; // pipes for child 1
    int p2c2[2], c2p2[2]; // pipes for child 2
    // --- END CHANGED ---
    char byte;

    if (argc != 2) {
        fprintf(2, "Usage: twochildhs <byte>\n");
        exit(1);
    }

    byte = atoi(argv[1]);

    pipe(p2c1);
    pipe(c2p1);
    pipe(p2c2);
    pipe(c2p2);

    // --- CHANGED: Fork two children ---
    int pid1 = fork();
    if (pid1 < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    if (pid1 == 0) {
        // Child 1
        close(p2c1[1]); close(c2p1[0]);
        close(p2c2[0]); close(p2c2[1]); close(c2p2[0]); close(c2p2[1]);

        read(p2c1[0], &byte, 1);
        printf("%d: child1 received %d from parent\n", getpid(), byte);
        write(c2p1[1], &byte, 1);

        close(p2c1[0]); close(c2p1[1]);
        exit(0);
    }

    int pid2 = fork();
    if (pid2 < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    if (pid2 == 0) {
        // Child 2
        close(p2c2[1]); close(c2p2[0]);
        close(p2c1[0]); close(p2c1[1]); close(c2p1[0]); close(c2p1[1]);

        read(p2c2[0], &byte, 1);
        printf("%d: child2 received %d from parent\n", getpid(), byte);
        write(c2p2[1], &byte, 1);

        close(p2c2[0]); close(c2p2[1]);
        exit(0);
    }
    // --- END CHANGED ---

    // Parent: close unused ends
    close(p2c1[0]); close(c2p1[1]);
    close(p2c2[0]); close(c2p2[1]);

    // Send to both children
    write(p2c1[1], &byte, 1);
    write(p2c2[1], &byte, 1);

    // Read from both children
    char b1, b2;
    read(c2p1[0], &b1, 1);
    printf("%d: received %d from child1\n", getpid(), b1);
    read(c2p2[0], &b2, 1);
    printf("%d: received %d from child2\n", getpid(), b2);

    close(p2c1[1]); close(c2p1[0]);
    close(p2c2[1]); close(c2p2[0]);
    wait(0);
    wait(0);
    exit(0);
}
```

---

## Alternate 9: Squared Handshake

**What changed:** The child squares the byte value (byte * byte, modulo 256 to fit in a char) before sending it back.

```c
// user/sqhandshake.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int p2c[2];
    int c2p[2];
    char byte;

    if (argc != 2) {
        fprintf(2, "Usage: sqhandshake <byte>\n");
        exit(1);
    }

    byte = atoi(argv[1]);

    pipe(p2c);
    pipe(c2p);

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        close(p2c[1]);
        close(c2p[0]);

        read(p2c[0], &byte, 1);
        printf("%d: received %d from parent\n", getpid(), byte);

        // --- CHANGED: Square the byte (mod 256 to fit in char) ---
        int squared = ((int)byte * (int)byte) % 256;
        byte = (char)squared;
        printf("%d: sending squared value %d to parent\n", getpid(), byte);
        // --- END CHANGED ---

        write(c2p[1], &byte, 1);

        close(p2c[0]);
        close(c2p[1]);
        exit(0);
    } else {
        close(p2c[0]);
        close(c2p[1]);

        write(p2c[1], &byte, 1);
        read(c2p[0], &byte, 1);
        printf("%d: received %d from child\n", getpid(), byte);

        close(p2c[1]);
        close(c2p[0]);
        wait(0);
        exit(0);
    }
}
```

---

## Alternate 10: Chain Handshake (Grandchild)

**What changed:** The child forks a grandchild. Parent sends byte to child, child forwards it to grandchild, grandchild sends it back to child, child sends it back to parent. Three-process chain.

```c
// user/chainhs.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int p2c[2]; // parent to child
    int c2p[2]; // child to parent
    char byte;

    if (argc != 2) {
        fprintf(2, "Usage: chainhs <byte>\n");
        exit(1);
    }

    byte = atoi(argv[1]);

    pipe(p2c);
    pipe(c2p);

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        // Child process
        close(p2c[1]);
        close(c2p[0]);

        // --- CHANGED: Child forks a grandchild with its own pipe pair ---
        int c2g[2]; // child to grandchild
        int g2c[2]; // grandchild to child
        pipe(c2g);
        pipe(g2c);

        int gpid = fork();
        if (gpid < 0) {
            fprintf(2, "fork failed\n");
            exit(1);
        }

        if (gpid == 0) {
            // Grandchild
            close(c2g[1]); close(g2c[0]);
            close(p2c[0]); close(c2p[1]);

            read(c2g[0], &byte, 1);
            printf("%d: grandchild received %d from child\n", getpid(), byte);
            write(g2c[1], &byte, 1);

            close(c2g[0]); close(g2c[1]);
            exit(0);
        }

        close(c2g[0]); close(g2c[1]);

        // Child reads from parent, forwards to grandchild
        read(p2c[0], &byte, 1);
        printf("%d: child received %d from parent\n", getpid(), byte);

        write(c2g[1], &byte, 1);
        read(g2c[0], &byte, 1);
        printf("%d: child received %d from grandchild\n", getpid(), byte);

        // Send back to parent
        write(c2p[1], &byte, 1);
        // --- END CHANGED ---

        close(c2g[1]); close(g2c[0]);
        close(p2c[0]); close(c2p[1]);
        wait(0);
        exit(0);
    } else {
        close(p2c[0]);
        close(c2p[1]);

        write(p2c[1], &byte, 1);
        read(c2p[0], &byte, 1);
        printf("%d: parent received %d from child\n", getpid(), byte);

        close(p2c[1]);
        close(c2p[0]);
        wait(0);
        exit(0);
    }
}
```

---

## Alternate 11: Countdown Handshake

**What changed:** Instead of echoing the byte, the child decrements it by 1 and sends it back. The parent keeps sending and receiving until the byte reaches 0, printing at each step.

```c
// user/countdownhs.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int p2c[2];
    int c2p[2];
    char byte;

    if (argc != 2) {
        fprintf(2, "Usage: countdownhs <byte>\n");
        exit(1);
    }

    byte = atoi(argv[1]);

    pipe(p2c);
    pipe(c2p);

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        close(p2c[1]);
        close(c2p[0]);

        // --- CHANGED: Keep receiving, decrementing, sending back until 0 ---
        while (1) {
            if (read(p2c[0], &byte, 1) <= 0)
                break;
            printf("%d: child received %d\n", getpid(), byte);
            if (byte <= 0)
                break;
            byte = byte - 1; // Decrement
            write(c2p[1], &byte, 1);
        }
        // --- END CHANGED ---

        close(p2c[0]);
        close(c2p[1]);
        exit(0);
    } else {
        close(p2c[0]);
        close(c2p[1]);

        // --- CHANGED: Loop sending/receiving until byte reaches 0 ---
        write(p2c[1], &byte, 1);
        while (1) {
            read(c2p[0], &byte, 1);
            printf("%d: parent received %d\n", getpid(), byte);
            if (byte <= 0)
                break;
            write(p2c[1], &byte, 1);
        }
        // --- END CHANGED ---

        close(p2c[1]);
        close(c2p[0]);
        wait(0);
        exit(0);
    }
}
```

---

## Alternate 12: Shift Left Handshake

**What changed:** The child shifts the byte left by 1 bit (multiplies by 2, mod 256) before sending it back.

```c
// user/shifthandshake.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int p2c[2];
    int c2p[2];
    char byte;

    if (argc != 2) {
        fprintf(2, "Usage: shifthandshake <byte>\n");
        exit(1);
    }

    byte = atoi(argv[1]);

    pipe(p2c);
    pipe(c2p);

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        close(p2c[1]);
        close(c2p[0]);

        read(p2c[0], &byte, 1);
        printf("%d: received %d from parent\n", getpid(), byte);

        // --- CHANGED: Shift left by 1 bit ---
        byte = (byte << 1) & 0xFF;
        printf("%d: sending left-shifted value %d to parent\n", getpid(), byte);
        // --- END CHANGED ---

        write(c2p[1], &byte, 1);

        close(p2c[0]);
        close(c2p[1]);
        exit(0);
    } else {
        close(p2c[0]);
        close(c2p[1]);

        write(p2c[1], &byte, 1);
        read(c2p[0], &byte, 1);
        printf("%d: received %d from child\n", getpid(), byte);

        close(p2c[1]);
        close(c2p[0]);
        wait(0);
        exit(0);
    }
}
```

---

## Alternate 13: Two-Byte Sum Handshake

**What changed:** The parent sends two bytes (from two command-line arguments). The child reads both, prints them, computes their sum (mod 256), and sends the single sum byte back.

```c
// user/sumhandshake.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int p2c[2];
    int c2p[2];

    // --- CHANGED: Require two byte arguments ---
    if (argc != 3) {
        fprintf(2, "Usage: sumhandshake <byte1> <byte2>\n");
        exit(1);
    }

    char b1 = atoi(argv[1]);
    char b2 = atoi(argv[2]);
    // --- END CHANGED ---

    pipe(p2c);
    pipe(c2p);

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        close(p2c[1]);
        close(c2p[0]);

        // --- CHANGED: Read two bytes, compute sum, send back one byte ---
        char buf[2];
        read(p2c[0], buf, 2);
        printf("%d: received %d and %d from parent\n", getpid(), buf[0], buf[1]);
        char sum = (buf[0] + buf[1]) & 0xFF;
        printf("%d: sending sum %d to parent\n", getpid(), sum);
        write(c2p[1], &sum, 1);
        // --- END CHANGED ---

        close(p2c[0]);
        close(c2p[1]);
        exit(0);
    } else {
        close(p2c[0]);
        close(c2p[1]);

        // --- CHANGED: Send two bytes ---
        char buf[2];
        buf[0] = b1;
        buf[1] = b2;
        write(p2c[1], buf, 2);

        char result;
        read(c2p[0], &result, 1);
        printf("%d: received sum %d from child\n", getpid(), result);
        // --- END CHANGED ---

        close(p2c[1]);
        close(c2p[0]);
        wait(0);
        exit(0);
    }
}
```

---

## Alternate 14: Echo With PID Handshake

**What changed:** The child sends back two bytes: the original byte AND its own PID (mod 256) as a second byte. The parent prints both.

```c
// user/pidhandshake.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int p2c[2];
    int c2p[2];
    char byte;

    if (argc != 2) {
        fprintf(2, "Usage: pidhandshake <byte>\n");
        exit(1);
    }

    byte = atoi(argv[1]);

    pipe(p2c);
    pipe(c2p);

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        close(p2c[1]);
        close(c2p[0]);

        read(p2c[0], &byte, 1);
        printf("%d: received %d from parent\n", getpid(), byte);

        // --- CHANGED: Send back original byte + PID byte ---
        char reply[2];
        reply[0] = byte;
        reply[1] = (char)(getpid() & 0xFF); // PID mod 256
        write(c2p[1], reply, 2);
        printf("%d: sent byte %d and pid-byte %d to parent\n", getpid(), reply[0], reply[1]);
        // --- END CHANGED ---

        close(p2c[0]);
        close(c2p[1]);
        exit(0);
    } else {
        close(p2c[0]);
        close(c2p[1]);

        write(p2c[1], &byte, 1);

        // --- CHANGED: Read two bytes from child ---
        char reply[2];
        read(c2p[0], reply, 2);
        printf("%d: received byte %d and child-pid-byte %d from child\n",
               getpid(), reply[0], reply[1]);
        // --- END CHANGED ---

        close(p2c[1]);
        close(c2p[0]);
        wait(0);
        exit(0);
    }
}
```

---

## Alternate 15: Conditional Handshake

**What changed:** The child only sends the byte back if it is even. If the byte is odd, the child prints an error and sends 0 instead.

```c
// user/condhandshake.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int p2c[2];
    int c2p[2];
    char byte;

    if (argc != 2) {
        fprintf(2, "Usage: condhandshake <byte>\n");
        exit(1);
    }

    byte = atoi(argv[1]);

    pipe(p2c);
    pipe(c2p);

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        close(p2c[1]);
        close(c2p[0]);

        read(p2c[0], &byte, 1);
        printf("%d: received %d from parent\n", getpid(), byte);

        // --- CHANGED: Only echo back if byte is even; send 0 if odd ---
        if (byte % 2 == 0) {
            printf("%d: byte is even, echoing back\n", getpid());
            write(c2p[1], &byte, 1);
        } else {
            printf("%d: byte is odd, sending 0 instead\n", getpid());
            byte = 0;
            write(c2p[1], &byte, 1);
        }
        // --- END CHANGED ---

        close(p2c[0]);
        close(c2p[1]);
        exit(0);
    } else {
        close(p2c[0]);
        close(c2p[1]);

        write(p2c[1], &byte, 1);
        read(c2p[0], &byte, 1);
        printf("%d: received %d from child\n", getpid(), byte);

        close(p2c[1]);
        close(c2p[0]);
        wait(0);
        exit(0);
    }
}
```

---

## Alternate 16: Swap Handshake

**What changed:** Parent sends two bytes. The child swaps them (byte1 becomes byte2, byte2 becomes byte1) and sends them back swapped.

```c
// user/swaphandshake.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int p2c[2];
    int c2p[2];

    // --- CHANGED: Two byte arguments ---
    if (argc != 3) {
        fprintf(2, "Usage: swaphandshake <byte1> <byte2>\n");
        exit(1);
    }

    char buf[2];
    buf[0] = atoi(argv[1]);
    buf[1] = atoi(argv[2]);
    // --- END CHANGED ---

    pipe(p2c);
    pipe(c2p);

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        close(p2c[1]);
        close(c2p[0]);

        read(p2c[0], buf, 2);
        printf("%d: received %d and %d from parent\n", getpid(), buf[0], buf[1]);

        // --- CHANGED: Swap the two bytes ---
        char temp = buf[0];
        buf[0] = buf[1];
        buf[1] = temp;
        printf("%d: swapped to %d and %d, sending to parent\n", getpid(), buf[0], buf[1]);
        // --- END CHANGED ---

        write(c2p[1], buf, 2);

        close(p2c[0]);
        close(c2p[1]);
        exit(0);
    } else {
        close(p2c[0]);
        close(c2p[1]);

        write(p2c[1], buf, 2);
        read(c2p[0], buf, 2);
        printf("%d: received %d and %d from child\n", getpid(), buf[0], buf[1]);

        close(p2c[1]);
        close(c2p[0]);
        wait(0);
        exit(0);
    }
}
```

---

## Alternate 17: Max Handshake

**What changed:** Parent sends two bytes. The child reads both, determines the maximum, and sends only the max byte back to the parent.

```c
// user/maxhandshake.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int p2c[2];
    int c2p[2];

    // --- CHANGED: Two byte arguments ---
    if (argc != 3) {
        fprintf(2, "Usage: maxhandshake <byte1> <byte2>\n");
        exit(1);
    }

    char buf[2];
    buf[0] = atoi(argv[1]);
    buf[1] = atoi(argv[2]);
    // --- END CHANGED ---

    pipe(p2c);
    pipe(c2p);

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        close(p2c[1]);
        close(c2p[0]);

        read(p2c[0], buf, 2);
        printf("%d: received %d and %d from parent\n", getpid(), buf[0], buf[1]);

        // --- CHANGED: Find max and send it back ---
        char maxval = buf[0] > buf[1] ? buf[0] : buf[1];
        printf("%d: sending max %d to parent\n", getpid(), maxval);
        write(c2p[1], &maxval, 1);
        // --- END CHANGED ---

        close(p2c[0]);
        close(c2p[1]);
        exit(0);
    } else {
        close(p2c[0]);
        close(c2p[1]);

        write(p2c[1], buf, 2);

        char result;
        read(c2p[0], &result, 1);
        printf("%d: received max %d from child\n", getpid(), result);

        close(p2c[1]);
        close(c2p[0]);
        wait(0);
        exit(0);
    }
}
```

---

## Alternate 18: ASCII Handshake

**What changed:** Instead of sending a numeric byte from atoi, the parent sends the raw ASCII character from argv[1][0]. The child prints the character and its ASCII value, then sends back the next character in the alphabet (byte + 1).

```c
// user/asciihandshake.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int p2c[2];
    int c2p[2];
    char byte;

    if (argc != 2) {
        fprintf(2, "Usage: asciihandshake <char>\n");
        exit(1);
    }

    // --- CHANGED: Use raw ASCII char instead of atoi ---
    byte = argv[1][0];
    // --- END CHANGED ---

    pipe(p2c);
    pipe(c2p);

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        close(p2c[1]);
        close(c2p[0]);

        read(p2c[0], &byte, 1);
        // --- CHANGED: Print char and its ASCII code ---
        printf("%d: received char '%c' (ASCII %d) from parent\n", getpid(), byte, byte);
        byte = byte + 1; // Next character in alphabet
        printf("%d: sending next char '%c' (ASCII %d) to parent\n", getpid(), byte, byte);
        // --- END CHANGED ---

        write(c2p[1], &byte, 1);

        close(p2c[0]);
        close(c2p[1]);
        exit(0);
    } else {
        close(p2c[0]);
        close(c2p[1]);

        write(p2c[1], &byte, 1);
        read(c2p[0], &byte, 1);
        // --- CHANGED: Print received char ---
        printf("%d: received char '%c' (ASCII %d) from child\n", getpid(), byte, byte);
        // --- END CHANGED ---

        close(p2c[1]);
        close(c2p[0]);
        wait(0);
        exit(0);
    }
}
```

---

## Alternate 19: Modulo Handshake

**What changed:** The child computes byte modulo a hardcoded divisor (7) and sends the remainder back instead of the original byte.

```c
// user/modhandshake.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// --- CHANGED: Hardcoded modulo divisor ---
#define DIVISOR 7
// --- END CHANGED ---

int main(int argc, char *argv[])
{
    int p2c[2];
    int c2p[2];
    char byte;

    if (argc != 2) {
        fprintf(2, "Usage: modhandshake <byte>\n");
        exit(1);
    }

    byte = atoi(argv[1]);

    pipe(p2c);
    pipe(c2p);

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        close(p2c[1]);
        close(c2p[0]);

        read(p2c[0], &byte, 1);
        printf("%d: received %d from parent\n", getpid(), byte);

        // --- CHANGED: Compute modulo and send remainder ---
        byte = byte % DIVISOR;
        printf("%d: sending %d mod %d = %d to parent\n", getpid(), byte, DIVISOR, byte);
        // --- END CHANGED ---

        write(c2p[1], &byte, 1);

        close(p2c[0]);
        close(c2p[1]);
        exit(0);
    } else {
        close(p2c[0]);
        close(c2p[1]);

        write(p2c[1], &byte, 1);
        read(c2p[0], &byte, 1);
        printf("%d: received %d from child\n", getpid(), byte);

        close(p2c[1]);
        close(c2p[0]);
        wait(0);
        exit(0);
    }
}
```

---

## Alternate 20: Accumulator Handshake

**What changed:** The parent sends the byte. The child adds its PID to the byte and sends it back. The parent then adds its own PID to what it received, and prints the final accumulated value.

```c
// user/accumhs.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int p2c[2];
    int c2p[2];
    char byte;

    if (argc != 2) {
        fprintf(2, "Usage: accumhs <byte>\n");
        exit(1);
    }

    byte = atoi(argv[1]);

    pipe(p2c);
    pipe(c2p);

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        close(p2c[1]);
        close(c2p[0]);

        read(p2c[0], &byte, 1);
        printf("%d: received %d from parent\n", getpid(), byte);

        // --- CHANGED: Add child PID (mod 256) to byte ---
        byte = (byte + (getpid() & 0xFF)) & 0xFF;
        printf("%d: accumulated value (byte + child PID) = %d\n", getpid(), byte);
        // --- END CHANGED ---

        write(c2p[1], &byte, 1);

        close(p2c[0]);
        close(c2p[1]);
        exit(0);
    } else {
        close(p2c[0]);
        close(c2p[1]);

        write(p2c[1], &byte, 1);
        read(c2p[0], &byte, 1);
        printf("%d: received %d from child\n", getpid(), byte);

        // --- CHANGED: Add parent PID (mod 256) to accumulated value ---
        byte = (byte + (getpid() & 0xFF)) & 0xFF;
        printf("%d: final accumulated value (+ parent PID) = %d\n", getpid(), byte);
        // --- END CHANGED ---

        close(p2c[1]);
        close(c2p[0]);
        wait(0);
        exit(0);
    }
}
```

---

## Quick Reference: All 20 Alternates

| # | Name | Key Change |
|---|------|------------|
| 1 | Double Handshake | Two full round trips, child increments byte |
| 2 | Reverse Handshake | Child sends first, parent echoes back |
| 3 | Increment Handshake | Child adds 10 to byte before returning |
| 4 | Multi-Byte Handshake | Send 4 characters instead of 1 byte |
| 5 | Negate Handshake | Child negates byte (two's complement) |
| 6 | Ping-Pong Count | N round trips via second CLI argument |
| 7 | XOR Handshake | Child XORs byte with 0xFF, parent verifies |
| 8 | Two Children Handshake | Parent forks two children, sends to both |
| 9 | Squared Handshake | Child squares byte (mod 256) |
| 10 | Chain Handshake | Three-process chain: parent > child > grandchild |
| 11 | Countdown Handshake | Decrements byte each round until 0 |
| 12 | Shift Left Handshake | Child left-shifts byte by 1 bit |
| 13 | Two-Byte Sum | Parent sends 2 bytes, child returns their sum |
| 14 | Echo With PID | Child sends back byte + its PID as 2nd byte |
| 15 | Conditional Handshake | Child echoes only if byte is even, else sends 0 |
| 16 | Swap Handshake | Parent sends 2 bytes, child swaps and returns |
| 17 | Max Handshake | Parent sends 2 bytes, child returns the max |
| 18 | ASCII Handshake | Sends raw char, child returns next letter |
| 19 | Modulo Handshake | Child returns byte mod 7 |
| 20 | Accumulator Handshake | Both child and parent add their PIDs to byte |

---

> **Note:** For every alternate, add the program name to `UPROGS` in `Makefile` (e.g., `$U/_doublehandshake\`). Run `make clean && make qemu` for a clean file system.
