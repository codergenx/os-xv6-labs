#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/param.h"
#include "user/user.h"

int
suffixmatch(char *a, char *b)
{
  int la = strlen(a);
  int lb = strlen(b);
  if(lb > la)
    return 0;
  return strcmp(a + la - lb, b) == 0;
}

void
run(char *cmd, char *file)
{
  if(fork() == 0){
    char *argv[MAXARG];
    int i = 0;
    argv[i++] = cmd;
    argv[i++] = file;
    argv[i] = 0;
    exec(cmd, argv);
    fprintf(2, "could not run %s\n", cmd);
    exit(1);
  }
  wait(0);
}

void
search(char *path, char *pat, char *cmd)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_FILE:
    if(suffixmatch(path, pat)){
      if(cmd)
        run(cmd, path);
      else
        printf("%s\n", path);
    }
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("path too long: %s\n", path);
      break;
    }
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0) continue;
      if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      search(buf, pat, cmd);
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  if(argc < 3){
    fprintf(2, "usage: search dir suffix [-run cmd]\n");
    exit(1);
  }

  char *path = argv[1];
  char *pat = argv[2];
  char *cmd = 0;

  if(argc == 5 && strcmp(argv[3], "-run") == 0)
    cmd = argv[4];

  search(path, pat, cmd);
  exit(0);
}

