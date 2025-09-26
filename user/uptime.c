#include "kernel/types.h"
#include "user/user.h"

int
main(int ac, char *av[])
{
    if (ac != 1) {
        fprintf(2, "Format: run uptime with no args\n");
        exit(1);
    }

    uint64 raw = uptime();
    int t = (int) raw;
    printf("system has been running for %d ticks\n", t);

    exit(0);
}

