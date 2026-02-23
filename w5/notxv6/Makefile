CC = gcc
CFLAGS = -g -02 -DSOL_THREAD -DLAB_THREAD

all: ph-without-locks ph-with-mutex-locks

ph-without-locks: ph-without-locks.c
	$(CC) $(CFLAGS) ph-without-locks.c -o ph-without-locks -pthread

ph-with-mutex-locks: ph-with-mutex-locks.c
	$(CC) $(CFLAGS) ph-with-mutex-locks.c -o ph-with-mutex-locks -pthread

clean:
	rm -f ph-without-locks ph-with-mutex-locks

