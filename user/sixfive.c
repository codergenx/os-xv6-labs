#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

char *delims = " \t\r\n-.,/";

void handle_file(char *fname) {
    int file = open(fname, O_RDONLY);
    if (file < 0) {
        printf("Error: cannot open %s\n", fname);
        return;
    }

    char ch[1];
    char numbuf[32];
    int idx = 0;

    while (read(file, ch, 1) == 1) {
        if (ch[0] >= '0' && ch[0] <= '9') {
            if (idx < sizeof(numbuf) - 1) {
                numbuf[idx++] = ch[0];
            }
        } else {
            if (idx > 0) {
                numbuf[idx] = '\0';
                int n = atoi(numbuf);
                if (n % 5 == 0 || n % 6 == 0) {
                    printf("%d\n", n);
                }
                idx = 0;
            }
        }
    }

    if (idx > 0) {
        numbuf[idx] = '\0';
        int n = atoi(numbuf);
        if (n % 5 == 0 || n % 6 == 0) {
            printf("%d\n", n);
        }
    }

    close(file);
}

int
main(int parc, char *parv[])
{
    if (parc < 2) {
        printf("Usage: %s <file1> [file2 ...]\n", parv[0]);
        exit(1);
    }

    for (int j = 1; j < parc; j++) {
        handle_file(parv[j]);
    }

    exit(0);
}



