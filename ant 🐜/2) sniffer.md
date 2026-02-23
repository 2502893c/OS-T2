# Task 2: Sniffer - 20 Alternate Versions

## Original Task Summary

The original **sniffer** task exploits a bug in xv6 where:
- `memset(mem, 0, sz)` is omitted in `uvmalloc()` (kernel/vm.c), so newly allocated pages are NOT zeroed
- `memset` garbage-fill lines in `kalloc.c` are also omitted
- `secret.c` writes a secret string to memory, then exits (freeing its pages)
- `sniffer.c` calls `sbrk()` to allocate memory, scans through it for the secret string left behind, and prints it
- The exploit: freed pages retain their old contents, so sniffer can read data from the previous process

**Core technique:** Call `sbrk()` to get unzeroed pages, scan them byte by byte for printable strings that look like the secret.

---

## Alternate 1: Find the Longest Printable String

**What changed:** Instead of finding a specific secret, sniffer finds and prints the longest contiguous printable ASCII string (length >= 4) found in the unzeroed pages.

```c
// user/sniffer.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    // --- CHANGED: We find the LONGEST printable string instead of a known secret ---
    int total = 64 * 4096; // allocate 64 pages
    char *mem = sbrk(total);
    if (mem == (char *)-1) {
        fprintf(2, "sbrk failed\n");
        exit(1);
    }

    char *best_start = 0;
    int best_len = 0;
    char *cur_start = 0;
    int cur_len = 0;

    for (int i = 0; i < total; i++) {
        // CHANGED: Check if character is printable ASCII (32-126)
        if (mem[i] >= 32 && mem[i] <= 126) {
            if (cur_len == 0)
                cur_start = &mem[i];
            cur_len++;
        } else {
            // CHANGED: Track the longest string found
            if (cur_len > best_len && cur_len >= 4) {
                best_start = cur_start;
                best_len = cur_len;
            }
            cur_len = 0;
        }
    }
    // Check last run
    if (cur_len > best_len && cur_len >= 4) {
        best_start = cur_start;
        best_len = cur_len;
    }

    // CHANGED: Print the longest string found
    if (best_len > 0) {
        for (int i = 0; i < best_len; i++)
            printf("%c", best_start[i]);
        printf("\n");
    } else {
        printf("no printable string found\n");
    }
    // --- END CHANGED ---

    exit(0);
}
```

---

## Alternate 2: Print ALL Printable Strings

**What changed:** Instead of finding one secret, sniffer prints every contiguous printable ASCII string of length >= 5 found in the allocated pages, one per line.

```c
// user/sniffer.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int total = 64 * 4096;
    char *mem = sbrk(total);
    if (mem == (char *)-1) {
        fprintf(2, "sbrk failed\n");
        exit(1);
    }

    // --- CHANGED: Print ALL printable strings of length >= 5 ---
    int min_len = 5; // minimum string length to print
    char *start = 0;
    int len = 0;

    for (int i = 0; i < total; i++) {
        if (mem[i] >= 32 && mem[i] <= 126) {
            if (len == 0)
                start = &mem[i];
            len++;
        } else {
            // CHANGED: Print every string that meets the minimum length
            if (len >= min_len) {
                for (int j = 0; j < len; j++)
                    printf("%c", start[j]);
                printf("\n");
            }
            len = 0;
        }
    }
    if (len >= min_len) {
        for (int j = 0; j < len; j++)
            printf("%c", start[j]);
        printf("\n");
    }
    // --- END CHANGED ---

    exit(0);
}
```

---

## Alternate 3: Count Occurrences of a Target String

**What changed:** Instead of just printing the secret, sniffer takes a target string as a command-line argument and counts how many times that string appears in the unzeroed memory pages.

```c
// user/sniffer.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    // --- CHANGED: Takes a target string argument and counts occurrences ---
    if (argc != 2) {
        fprintf(2, "Usage: sniffer <target_string>\n");
        exit(1);
    }

    char *target = argv[1];
    int tlen = strlen(target);
    // --- END CHANGED ---

    int total = 64 * 4096;
    char *mem = sbrk(total);
    if (mem == (char *)-1) {
        fprintf(2, "sbrk failed\n");
        exit(1);
    }

    // --- CHANGED: Scan memory for occurrences of target ---
    int count = 0;
    for (int i = 0; i <= total - tlen; i++) {
        int match = 1;
        for (int j = 0; j < tlen; j++) {
            if (mem[i + j] != target[j]) {
                match = 0;
                break;
            }
        }
        if (match)
            count++;
    }

    printf("found \"%s\" %d time(s) in memory\n", target, count);
    // --- END CHANGED ---

    exit(0);
}
```

---

## Alternate 4: Find Strings Starting With a Prefix

**What changed:** Sniffer takes a prefix as argument and prints every printable string in unzeroed memory that starts with that prefix.

```c
// user/sniffer.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    // --- CHANGED: Takes a prefix argument ---
    if (argc != 2) {
        fprintf(2, "Usage: sniffer <prefix>\n");
        exit(1);
    }
    char *prefix = argv[1];
    int plen = strlen(prefix);
    // --- END CHANGED ---

    int total = 64 * 4096;
    char *mem = sbrk(total);
    if (mem == (char *)-1) {
        fprintf(2, "sbrk failed\n");
        exit(1);
    }

    // --- CHANGED: Find printable strings and check if they start with prefix ---
    char *start = 0;
    int len = 0;

    for (int i = 0; i < total; i++) {
        if (mem[i] >= 32 && mem[i] <= 126) {
            if (len == 0)
                start = &mem[i];
            len++;
        } else {
            // CHANGED: Only print strings that begin with the given prefix
            if (len >= plen) {
                int match = 1;
                for (int j = 0; j < plen; j++) {
                    if (start[j] != prefix[j]) {
                        match = 0;
                        break;
                    }
                }
                if (match) {
                    for (int j = 0; j < len; j++)
                        printf("%c", start[j]);
                    printf("\n");
                }
            }
            len = 0;
        }
    }
    // --- END CHANGED ---

    exit(0);
}
```

---

## Alternate 5: Hex Dump of Non-Zero Pages

**What changed:** Instead of searching for printable strings, sniffer prints a hex dump of the first N non-zero bytes found in the allocated pages. N defaults to 64.

```c
// user/sniffer.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// --- CHANGED: Hex dump mode instead of string search ---
#define DUMP_COUNT 64

// Helper to print a single hex digit
void
printhex(unsigned char c)
{
    char hex[] = "0123456789abcdef";
    printf("%c%c", hex[(c >> 4) & 0xf], hex[c & 0xf]);
}
// --- END CHANGED ---

int
main(int argc, char *argv[])
{
    int total = 64 * 4096;
    char *mem = sbrk(total);
    if (mem == (char *)-1) {
        fprintf(2, "sbrk failed\n");
        exit(1);
    }

    // --- CHANGED: Print hex dump of first DUMP_COUNT non-zero bytes ---
    int printed = 0;
    for (int i = 0; i < total && printed < DUMP_COUNT; i++) {
        if (mem[i] != 0) {
            printhex((unsigned char)mem[i]);
            printf(" ");
            printed++;
            if (printed % 16 == 0)
                printf("\n");
        }
    }
    if (printed % 16 != 0)
        printf("\n");

    printf("(%d non-zero bytes dumped)\n", printed);
    // --- END CHANGED ---

    exit(0);
}
```

---

## Alternate 6: Find Secret by Known Length

**What changed:** Sniffer takes an expected secret length as argument. It searches unzeroed pages for the first printable string matching exactly that length.

```c
// user/sniffer.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    // --- CHANGED: Takes expected secret length as argument ---
    if (argc != 2) {
        fprintf(2, "Usage: sniffer <expected_length>\n");
        exit(1);
    }
    int expected_len = atoi(argv[1]);
    // --- END CHANGED ---

    int total = 64 * 4096;
    char *mem = sbrk(total);
    if (mem == (char *)-1) {
        fprintf(2, "sbrk failed\n");
        exit(1);
    }

    // --- CHANGED: Find first printable string of exactly expected_len ---
    char *start = 0;
    int len = 0;
    int found = 0;

    for (int i = 0; i < total && !found; i++) {
        if (mem[i] >= 33 && mem[i] <= 126) { // printable non-space
            if (len == 0)
                start = &mem[i];
            len++;
        } else {
            // CHANGED: Match string by exact length
            if (len == expected_len) {
                for (int j = 0; j < len; j++)
                    printf("%c", start[j]);
                printf("\n");
                found = 1;
            }
            len = 0;
        }
    }

    if (!found)
        printf("no string of length %d found\n", expected_len);
    // --- END CHANGED ---

    exit(0);
}
```

---

## Alternate 7: Sniff Only Alphabetic Secrets

**What changed:** Sniffer only looks for strings that consist entirely of lowercase alphabetic characters (a-z), ignoring any strings with digits or special characters.

```c
// user/sniffer.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int total = 64 * 4096;
    char *mem = sbrk(total);
    if (mem == (char *)-1) {
        fprintf(2, "sbrk failed\n");
        exit(1);
    }

    // --- CHANGED: Only find strings of pure lowercase letters ---
    char *start = 0;
    int len = 0;

    for (int i = 0; i < total; i++) {
        // CHANGED: Only accept lowercase a-z
        if (mem[i] >= 'a' && mem[i] <= 'z') {
            if (len == 0)
                start = &mem[i];
            len++;
        } else {
            if (len >= 4) { // minimum 4 chars
                for (int j = 0; j < len; j++)
                    printf("%c", start[j]);
                printf("\n");
            }
            len = 0;
        }
    }
    if (len >= 4) {
        for (int j = 0; j < len; j++)
            printf("%c", start[j]);
        printf("\n");
    }
    // --- END CHANGED ---

    exit(0);
}
```

---

## Alternate 8: Sniff Only Numeric Secrets

**What changed:** Sniffer only looks for strings consisting entirely of digits (0-9), printing any numeric sequence of length >= 3.

```c
// user/sniffer.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int total = 64 * 4096;
    char *mem = sbrk(total);
    if (mem == (char *)-1) {
        fprintf(2, "sbrk failed\n");
        exit(1);
    }

    // --- CHANGED: Only find strings of pure digits ---
    char *start = 0;
    int len = 0;

    for (int i = 0; i < total; i++) {
        // CHANGED: Only accept digits 0-9
        if (mem[i] >= '0' && mem[i] <= '9') {
            if (len == 0)
                start = &mem[i];
            len++;
        } else {
            if (len >= 3) { // minimum 3 digits
                for (int j = 0; j < len; j++)
                    printf("%c", start[j]);
                printf("\n");
            }
            len = 0;
        }
    }
    if (len >= 3) {
        for (int j = 0; j < len; j++)
            printf("%c", start[j]);
        printf("\n");
    }
    // --- END CHANGED ---

    exit(0);
}
```

---

## Alternate 9: Sniff and Reverse the Secret

**What changed:** Sniffer finds the first printable string of length >= 4 and prints it in reverse order.

```c
// user/sniffer.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int total = 64 * 4096;
    char *mem = sbrk(total);
    if (mem == (char *)-1) {
        fprintf(2, "sbrk failed\n");
        exit(1);
    }

    // --- CHANGED: Find first printable string >= 4 and print it REVERSED ---
    char *start = 0;
    int len = 0;
    int found = 0;

    for (int i = 0; i < total && !found; i++) {
        if (mem[i] >= 33 && mem[i] <= 126) {
            if (len == 0)
                start = &mem[i];
            len++;
        } else {
            if (len >= 4) {
                // CHANGED: Print characters in reverse order
                for (int j = len - 1; j >= 0; j--)
                    printf("%c", start[j]);
                printf("\n");
                found = 1;
            }
            len = 0;
        }
    }

    if (!found)
        printf("no string found\n");
    // --- END CHANGED ---

    exit(0);
}
```

---

## Alternate 10: Sniff With Page Offset Reporting

**What changed:** For every printable string found (length >= 4), sniffer prints the page number and offset within that page where the string was found, along with the string itself.

```c
// user/sniffer.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE 4096

int
main(int argc, char *argv[])
{
    int total = 64 * PGSIZE;
    char *mem = sbrk(total);
    if (mem == (char *)-1) {
        fprintf(2, "sbrk failed\n");
        exit(1);
    }

    // --- CHANGED: Report page number and offset for each found string ---
    char *start = 0;
    int len = 0;
    int start_idx = 0;

    for (int i = 0; i < total; i++) {
        if (mem[i] >= 33 && mem[i] <= 126) {
            if (len == 0) {
                start = &mem[i];
                start_idx = i;
            }
            len++;
        } else {
            if (len >= 4) {
                // CHANGED: Print page number and offset within page
                int page = start_idx / PGSIZE;
                int offset = start_idx % PGSIZE;
                printf("page %d offset %d: ", page, offset);
                for (int j = 0; j < len; j++)
                    printf("%c", start[j]);
                printf("\n");
            }
            len = 0;
        }
    }
    // --- END CHANGED ---

    exit(0);
}
```

---

## Alternate 11: Sniff and Compare Against a Wordlist

**What changed:** Sniffer takes multiple arguments (a wordlist). It scans memory and prints only those words from the wordlist that are found in the unzeroed pages.

```c
// user/sniffer.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// Simple substring search
int
memfind(char *haystack, int hlen, char *needle, int nlen)
{
    for (int i = 0; i <= hlen - nlen; i++) {
        int match = 1;
        for (int j = 0; j < nlen; j++) {
            if (haystack[i + j] != needle[j]) {
                match = 0;
                break;
            }
        }
        if (match)
            return 1;
    }
    return 0;
}

int
main(int argc, char *argv[])
{
    // --- CHANGED: Takes a wordlist as multiple arguments ---
    if (argc < 2) {
        fprintf(2, "Usage: sniffer <word1> <word2> ...\n");
        exit(1);
    }
    // --- END CHANGED ---

    int total = 64 * 4096;
    char *mem = sbrk(total);
    if (mem == (char *)-1) {
        fprintf(2, "sbrk failed\n");
        exit(1);
    }

    // --- CHANGED: Check each word from the wordlist against memory ---
    for (int w = 1; w < argc; w++) {
        if (memfind(mem, total, argv[w], strlen(argv[w]))) {
            printf("FOUND: %s\n", argv[w]);
        } else {
            printf("NOT FOUND: %s\n", argv[w]);
        }
    }
    // --- END CHANGED ---

    exit(0);
}
```

---

## Alternate 12: Sniff Unique Strings Only

**What changed:** Sniffer finds all printable strings of length >= 4 but only prints each unique string once, skipping duplicates.

```c
// user/sniffer.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// --- CHANGED: Simple dedup using a small buffer of found strings ---
#define MAX_STRINGS 64
#define MAX_STR_LEN 128

char found_strings[MAX_STRINGS][MAX_STR_LEN];
int found_count = 0;

int
is_duplicate(char *s, int len)
{
    for (int i = 0; i < found_count; i++) {
        int match = 1;
        for (int j = 0; j < len; j++) {
            if (found_strings[i][j] != s[j]) {
                match = 0;
                break;
            }
        }
        if (match && found_strings[i][len] == 0)
            return 1;
    }
    return 0;
}
// --- END CHANGED ---

int
main(int argc, char *argv[])
{
    int total = 64 * 4096;
    char *mem = sbrk(total);
    if (mem == (char *)-1) {
        fprintf(2, "sbrk failed\n");
        exit(1);
    }

    char *start = 0;
    int len = 0;

    for (int i = 0; i < total; i++) {
        if (mem[i] >= 33 && mem[i] <= 126) {
            if (len == 0)
                start = &mem[i];
            len++;
        } else {
            // --- CHANGED: Only print if not a duplicate ---
            if (len >= 4 && len < MAX_STR_LEN && !is_duplicate(start, len)) {
                for (int j = 0; j < len; j++) {
                    printf("%c", start[j]);
                    if (found_count < MAX_STRINGS)
                        found_strings[found_count][j] = start[j];
                }
                if (found_count < MAX_STRINGS) {
                    found_strings[found_count][len] = 0;
                    found_count++;
                }
                printf("\n");
            }
            // --- END CHANGED ---
            len = 0;
        }
    }

    exit(0);
}
```

---

## Alternate 13: Sniff and XOR Decrypt

**What changed:** Sniffer assumes the secret was XOR-encrypted with a single-byte key (given as argument). It scans memory, XOR-decrypts each byte with the key, and prints resulting printable strings.

```c
// user/sniffer.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    // --- CHANGED: Takes an XOR key as argument ---
    if (argc != 2) {
        fprintf(2, "Usage: sniffer <xor_key_byte>\n");
        exit(1);
    }
    char key = (char)atoi(argv[1]);
    // --- END CHANGED ---

    int total = 64 * 4096;
    char *mem = sbrk(total);
    if (mem == (char *)-1) {
        fprintf(2, "sbrk failed\n");
        exit(1);
    }

    // --- CHANGED: XOR decrypt each byte before checking printability ---
    char *start = 0;
    int len = 0;
    // We need a decrypted buffer since we can't modify sbrk memory safely for output
    char decrypted[256];

    for (int i = 0; i < total; i++) {
        char c = mem[i] ^ key; // CHANGED: XOR decrypt
        if (c >= 33 && c <= 126) {
            if (len == 0)
                start = &mem[i];
            if (len < 255)
                decrypted[len] = c;
            len++;
        } else {
            if (len >= 4 && len < 256) {
                decrypted[len] = 0;
                printf("%s\n", decrypted);
            }
            len = 0;
        }
    }
    if (len >= 4 && len < 256) {
        decrypted[len] = 0;
        printf("%s\n", decrypted);
    }
    // --- END CHANGED ---

    exit(0);
}
```

---

## Alternate 14: Sniff and Write to File

**What changed:** Instead of printing to stdout, sniffer writes all found printable strings to a file called "sniffed.txt".

```c
// user/sniffer.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int
main(int argc, char *argv[])
{
    int total = 64 * 4096;
    char *mem = sbrk(total);
    if (mem == (char *)-1) {
        fprintf(2, "sbrk failed\n");
        exit(1);
    }

    // --- CHANGED: Open a file to write results ---
    int fd = open("sniffed.txt", O_WRONLY | O_CREATE);
    if (fd < 0) {
        fprintf(2, "cannot open sniffed.txt\n");
        exit(1);
    }
    // --- END CHANGED ---

    char *start = 0;
    int len = 0;
    char newline = '\n';

    for (int i = 0; i < total; i++) {
        if (mem[i] >= 33 && mem[i] <= 126) {
            if (len == 0)
                start = &mem[i];
            len++;
        } else {
            if (len >= 4) {
                // --- CHANGED: Write to file instead of stdout ---
                write(fd, start, len);
                write(fd, &newline, 1);
                // --- END CHANGED ---
            }
            len = 0;
        }
    }

    close(fd);
    printf("results written to sniffed.txt\n");

    exit(0);
}
```

---

## Alternate 15: Count Non-Zero Pages

**What changed:** Instead of finding strings, sniffer counts how many of the allocated pages contain at least one non-zero byte (i.e., were previously used and not zeroed).

```c
// user/sniffer.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE 4096

int
main(int argc, char *argv[])
{
    // --- CHANGED: Count dirty (non-zero) pages ---
    int num_pages = 64;
    int total = num_pages * PGSIZE;
    char *mem = sbrk(total);
    if (mem == (char *)-1) {
        fprintf(2, "sbrk failed\n");
        exit(1);
    }

    int dirty_pages = 0;

    for (int p = 0; p < num_pages; p++) {
        int is_dirty = 0;
        // CHANGED: Scan each page for any non-zero byte
        for (int off = 0; off < PGSIZE; off++) {
            if (mem[p * PGSIZE + off] != 0) {
                is_dirty = 1;
                break;
            }
        }
        if (is_dirty) {
            dirty_pages++;
            printf("page %d: dirty\n", p);
        }
    }

    printf("%d out of %d pages contain leftover data\n", dirty_pages, num_pages);
    // --- END CHANGED ---

    exit(0);
}
```

---

## Alternate 16: Sniff Strings Containing a Substring

**What changed:** Sniffer takes a substring as argument. It scans unzeroed pages and prints only the printable strings that contain that substring.

```c
// user/sniffer.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// --- CHANGED: Helper to check if string contains substring ---
int
contains(char *str, int slen, char *sub, int sublen)
{
    if (sublen > slen)
        return 0;
    for (int i = 0; i <= slen - sublen; i++) {
        int match = 1;
        for (int j = 0; j < sublen; j++) {
            if (str[i + j] != sub[j]) {
                match = 0;
                break;
            }
        }
        if (match)
            return 1;
    }
    return 0;
}
// --- END CHANGED ---

int
main(int argc, char *argv[])
{
    // --- CHANGED: Takes a substring filter argument ---
    if (argc != 2) {
        fprintf(2, "Usage: sniffer <substring>\n");
        exit(1);
    }
    char *sub = argv[1];
    int sublen = strlen(sub);
    // --- END CHANGED ---

    int total = 64 * 4096;
    char *mem = sbrk(total);
    if (mem == (char *)-1) {
        fprintf(2, "sbrk failed\n");
        exit(1);
    }

    char *start = 0;
    int len = 0;

    for (int i = 0; i < total; i++) {
        if (mem[i] >= 33 && mem[i] <= 126) {
            if (len == 0)
                start = &mem[i];
            len++;
        } else {
            // --- CHANGED: Only print if it contains the target substring ---
            if (len >= 4 && contains(start, len, sub, sublen)) {
                for (int j = 0; j < len; j++)
                    printf("%c", start[j]);
                printf("\n");
            }
            // --- END CHANGED ---
            len = 0;
        }
    }

    exit(0);
}
```

---

## Alternate 17: Sniff and Print String Lengths

**What changed:** Sniffer finds all printable strings of length >= 3 and prints only their lengths (not their contents), giving a "fingerprint" of what data is leftover in memory.

```c
// user/sniffer.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int total = 64 * 4096;
    char *mem = sbrk(total);
    if (mem == (char *)-1) {
        fprintf(2, "sbrk failed\n");
        exit(1);
    }

    // --- CHANGED: Print only the LENGTH of each found string ---
    int len = 0;
    int string_count = 0;

    for (int i = 0; i < total; i++) {
        if (mem[i] >= 33 && mem[i] <= 126) {
            len++;
        } else {
            if (len >= 3) {
                // CHANGED: Print length instead of content
                printf("string %d: length %d\n", string_count, len);
                string_count++;
            }
            len = 0;
        }
    }

    printf("total strings found: %d\n", string_count);
    // --- END CHANGED ---

    exit(0);
}
```

---

## Alternate 18: Sniff With Incremental sbrk

**What changed:** Instead of one big sbrk call, sniffer calls sbrk one page at a time (4096 bytes per call) and scans each page individually, printing which pages had data.

```c
// user/sniffer.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE 4096
#define NUM_PAGES 64

int
main(int argc, char *argv[])
{
    // --- CHANGED: Allocate one page at a time with sbrk ---
    for (int p = 0; p < NUM_PAGES; p++) {
        char *page = sbrk(PGSIZE); // CHANGED: incremental sbrk
        if (page == (char *)-1) {
            fprintf(2, "sbrk failed at page %d\n", p);
            exit(1);
        }

        // Scan this page for printable strings
        char *start = 0;
        int len = 0;

        for (int i = 0; i < PGSIZE; i++) {
            if (page[i] >= 33 && page[i] <= 126) {
                if (len == 0)
                    start = &page[i];
                len++;
            } else {
                if (len >= 4) {
                    // CHANGED: Include page number in output
                    printf("[page %d] ", p);
                    for (int j = 0; j < len; j++)
                        printf("%c", start[j]);
                    printf("\n");
                }
                len = 0;
            }
        }
        if (len >= 4) {
            printf("[page %d] ", p);
            for (int j = 0; j < len; j++)
                printf("%c", start[j]);
            printf("\n");
        }
    }
    // --- END CHANGED ---

    exit(0);
}
```

---

## Alternate 19: Sniff and Uppercase the Secret

**What changed:** Sniffer finds the first printable string of length >= 4 and prints it converted to all uppercase letters.

```c
// user/sniffer.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// --- CHANGED: Helper to convert char to uppercase ---
char
to_upper(char c)
{
    if (c >= 'a' && c <= 'z')
        return c - ('a' - 'A');
    return c;
}
// --- END CHANGED ---

int
main(int argc, char *argv[])
{
    int total = 64 * 4096;
    char *mem = sbrk(total);
    if (mem == (char *)-1) {
        fprintf(2, "sbrk failed\n");
        exit(1);
    }

    char *start = 0;
    int len = 0;
    int found = 0;

    for (int i = 0; i < total && !found; i++) {
        if (mem[i] >= 33 && mem[i] <= 126) {
            if (len == 0)
                start = &mem[i];
            len++;
        } else {
            if (len >= 4) {
                // --- CHANGED: Print string in all uppercase ---
                for (int j = 0; j < len; j++)
                    printf("%c", to_upper(start[j]));
                printf("\n");
                found = 1;
                // --- END CHANGED ---
            }
            len = 0;
        }
    }

    if (!found)
        printf("no string found\n");

    exit(0);
}
```

---

## Alternate 20: Sniff and Count Character Frequencies

**What changed:** Sniffer scans all non-zero bytes in the allocated pages and prints a frequency table of how many times each printable ASCII character appears.

```c
// user/sniffer.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int total = 64 * 4096;
    char *mem = sbrk(total);
    if (mem == (char *)-1) {
        fprintf(2, "sbrk failed\n");
        exit(1);
    }

    // --- CHANGED: Count frequency of each printable ASCII character ---
    int freq[128];
    for (int i = 0; i < 128; i++)
        freq[i] = 0;

    for (int i = 0; i < total; i++) {
        unsigned char c = (unsigned char)mem[i];
        if (c >= 33 && c <= 126) // printable non-space
            freq[c]++;
    }

    // CHANGED: Print frequency table for characters that appeared
    printf("Character frequencies in leaked memory:\n");
    for (int c = 33; c <= 126; c++) {
        if (freq[c] > 0)
            printf("  '%c' : %d\n", c, freq[c]);
    }
    // --- END CHANGED ---

    exit(0);
}
```

---

## Quick Reference: All 20 Alternates

| # | Name | Key Change |
|---|------|------------|
| 1 | Longest Printable String | Finds and prints the single longest printable string |
| 2 | Print All Strings | Prints every printable string of length >= 5 |
| 3 | Count Target Occurrences | Takes target as arg, counts how many times it appears |
| 4 | Prefix Filter | Prints strings starting with a given prefix |
| 5 | Hex Dump | Prints hex dump of first 64 non-zero bytes |
| 6 | Find By Known Length | Finds first printable string matching an exact length |
| 7 | Alphabetic Only | Only finds pure lowercase a-z strings |
| 8 | Numeric Only | Only finds pure digit 0-9 strings |
| 9 | Reverse Print | Finds first string and prints it reversed |
| 10 | Page Offset Report | Prints page number and offset for each string |
| 11 | Wordlist Compare | Takes multiple args, reports which are found in memory |
| 12 | Unique Strings Only | Deduplicates, prints each string only once |
| 13 | XOR Decrypt | XOR-decrypts each byte with a key before scanning |
| 14 | Write to File | Writes found strings to sniffed.txt instead of stdout |
| 15 | Count Dirty Pages | Counts how many pages have non-zero data |
| 16 | Substring Filter | Prints strings containing a given substring |
| 17 | String Lengths Only | Prints only the lengths of found strings |
| 18 | Incremental sbrk | Allocates one page at a time, scans per-page |
| 19 | Uppercase Output | Finds first string and prints it uppercased |
| 20 | Character Frequencies | Prints frequency table of printable chars in memory |

---

> **Note:** For every alternate, add `sniffer` and `secret` to `UPROGS` in `Makefile`. The exploit relies on the lab's removed `memset` calls. Run `make clean && make qemu` for a clean file system. Test with: `$ secret mysecret` then `$ sniffer`.
