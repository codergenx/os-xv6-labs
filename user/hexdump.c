#include "kernel/types.h"
#include "user/user.h"

void hexdump(char *pattern, char *ptr) {
    while (*pattern) {
        switch (*pattern) {
        case 'i': {
            int *ival = (int *) ptr;
            printf("%d\n", *ival);
            ptr += 4;
            break;
        }
        case 'p': {
            long long *pval = (long long *) ptr;
            printf("0x%llx\n", *pval);
            ptr += 8;
            break;
        }
        case 'h': {
            short *sval = (short *) ptr;
            printf("%d\n", *sval);
            ptr += 2;
            break;
        }
        case 'c': {
            char *ch = (char *) ptr;
            printf("%c\n", *ch);
            ptr += 1;
            break;
        }
        case 's': {
            char **strptr = (char **) ptr;
            printf("%s\n", *strptr);
            ptr += 8;
            break;
        }
        case 'S': {
            char *cstring = (char *) ptr;
            printf("%s\n", cstring);
            return;
        }
        default:
            printf("hexdump: unknown format [%c]\n", *pattern);
            return;
        }
        pattern++;
    }
}

int
main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(2, "Usage: hexdump <pattern> <string>\n");
        exit(1);
    }

    char *pattern = argv[1];
    char *data    = argv[2];

    hexdump(pattern, data);
    exit(0);
}
