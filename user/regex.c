#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

int regex_here(char*, char*);
int regex_star(int, char*, char*);

int regex_match(char *rule, char *txt) {
    if (rule[0] == '^')
        return regex_here(rule + 1, txt);
    do {
        if (regex_here(rule, txt))
            return 1;
    } while (*txt++ != '\0');
    return 0;
}

int regex_here(char *rule, char *txt) {
    if (rule[0] == '\0')
        return 1;
    if (rule[1] == '*')
        return regex_star(rule[0], rule + 2, txt);
    if (rule[0] == '$' && rule[1] == '\0')
        return *txt == '\0';
    if (*txt != '\0' && (rule[0] == '.' || rule[0] == *txt))
        return regex_here(rule + 1, txt + 1);
    return 0;
}

int regex_star(int c, char *rule, char *txt) {
    do {
        if (regex_here(rule, txt))
            return 1;
    } while (*txt != '\0' && (*txt++ == c || c == '.'));
    return 0;
}

void explore(char *dirpath, char *pat, int runflag, char *cmd_args[], int cmd_count, int use_regex);

void run_cmd(char *fpath, char *cmd_args[], int cmd_count) {
    int id = fork();
    if (id < 0) {
        fprintf(2, "error: fork failed\n");
        exit(1);
    }
    if (id == 0) {
        char *temp[MAXARG];
        int j;
        for (j = 0; j < cmd_count; j++)
            temp[j] = cmd_args[j];
        temp[j] = fpath;
        temp[j + 1] = 0;
        exec(temp[0], temp);
        fprintf(2, "error: exec %s failed\n", temp[0]);
        exit(1);
    } else {
        wait(0);
    }
}

void explore(char *dirpath, char *pat, int runflag, char *cmd_args[], int cmd_count, int use_regex) {
    char pathbuf[512], *pos;
    int fd;
    struct dirent dent;
    struct stat info;

    if ((fd = open(dirpath, 0)) < 0) {
        fprintf(2, "error: cannot open %s\n", dirpath);
        return;
    }
    if (fstat(fd, &info) < 0) {
        fprintf(2, "error: cannot stat %s\n", dirpath);
        close(fd);
        return;
    }

    switch (info.type) {
    case T_FILE:
        for (pos = dirpath + strlen(dirpath); pos >= dirpath && *pos != '/'; pos--);
        pos++;
        int ok = 0;
        if (use_regex)
            ok = regex_match(pat, pos);
        else
            ok = (strcmp(pat, "") == 0 || strcmp(pos, pat) == 0);

        if (ok) {
            if (runflag)
                run_cmd(dirpath, cmd_args, cmd_count);
            else
                printf("%s\n", dirpath);
        }
        break;

    case T_DIR:
        if (strlen(dirpath) + 1 + DIRSIZ + 1 > sizeof pathbuf) {
            printf("error: path too long\n");
            break;
        }
        strcpy(pathbuf, dirpath);
        pos = pathbuf + strlen(pathbuf);
        *pos++ = '/';
        while (read(fd, &dent, sizeof(dent)) == sizeof(dent)) {
            if (dent.inum == 0)
                continue;
            memmove(pos, dent.name, DIRSIZ);
            pos[DIRSIZ] = 0;
            if (stat(pathbuf, &info) < 0) {
                printf("error: cannot stat %s\n", pathbuf);
                continue;
            }
            if (strcmp(dent.name, ".") == 0 || strcmp(dent.name, "..") == 0)
                continue;
            explore(pathbuf, pat, runflag, cmd_args, cmd_count, use_regex);
        }
        break;
    }
    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(2, "usage: finder <dir> [pattern] [-exec prog...] [-regex]\n");
        exit(1);
    }

    char *start = argv[1];
    char *pat = "";
    int runflag = 0;
    int regexflag = 0;
    char *cmd_args[MAXARG];
    int cmd_count = 0;

    int i = 2;
    while (i < argc) {
        if (strcmp(argv[i], "-exec") == 0) {
            runflag = 1;
            i++;
            if (i >= argc) {
                fprintf(2, "finder: -exec missing command\n");
                exit(1);
            }
            while (i < argc && cmd_count < MAXARG - 2) {
                if (strcmp(argv[i], "-regex") == 0)
                    break;
                cmd_args[cmd_count++] = argv[i++];
            }
        } else if (strcmp(argv[i], "-regex") == 0) {
            regexflag = 1;
            i++;
        } else {
            if (strcmp(pat, "") == 0)
                pat = argv[i];
            i++;
        }
    }

    if (regexflag && strcmp(pat, "") == 0) {
        fprintf(2, "finder: need a pattern with -regex\n");
        exit(1);
    }

    cmd_args[cmd_count] = 0;
    explore(start, pat, runflag, cmd_args, cmd_count, regexflag);
    exit(0);
}

