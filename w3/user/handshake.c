#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(2, "wrong usage\n");
        exit(1);
    }

    int byteSend = atoi(argv[1]);

    int ptc[2];
    int ctp[2];

    pipe(ptc);
    pipe(ctp);

    int pid = fork();

    if (pid == 0)
    {
        close(ptc[1]);
        close(ctp[0]);

        char receivedByte;
        read(ptc[0], &receivedByte, 1);

        printf("%d: received %d from parent\n", getpid(), receivedByte);

        write(ctp[1], &receivedByte, 1);

        close(ptc[0]);
        close(ctp[1]);

        exit(0);
    }

    else
    {
        close(ptc[0]);
        close(ctp[1]);

        char byte = (char)byteSend;
        write(ptc[1], &byte, 1);

        char receivedByte;
        read(ctp[0], &receivedByte, 1);

        printf("%d: received %d from child\n", getpid(), receivedByte);

        close(ptc[1]);
        close(ctp[0]);

        exit(0);
    }
}