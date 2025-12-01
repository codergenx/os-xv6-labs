#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/stat.h"

#define TYPE_EXEC  1
#define TYPE_REDIR 2
#define TYPE_PIPE  3
#define TYPE_LIST  4
#define TYPE_BG    5

#define ARG_LIMIT 10
#define HIST_LIMIT 10
#define CMD_LIMIT 100

char cmdlog[HIST_LIMIT][CMD_LIMIT];
int log_count = 0;
int log_index = 0;

struct basecmd { int kind; };

struct run {
  int kind;
  char *args[ARG_LIMIT];
  char *endargs[ARG_LIMIT];
};

struct redirect {
  int kind;
  struct basecmd *sub;
  char *path;
  char *epath;
  int mode;
  int fd;
};

struct pipechain {
  int kind;
  struct basecmd *left;
  struct basecmd *right;
};

struct listchain {
  int kind;
  struct basecmd *left;
  struct basecmd *right;
};

struct background {
  int kind;
  struct basecmd *sub;
};

int fork_wrap(void);
void die(char*);
struct basecmd *parse(char*);
void execute(struct basecmd*) __attribute__((noreturn));

int strncmp2(const char *a, const char *b, int n) {
  for(int i=0;i<n;i++){
    if(a[i]!=b[i]) return a[i]-b[i];
    if(a[i]==0) return 0;
  }
  return 0;
}

int is_tty(void) {
  struct stat st;
  if(fstat(0,&st)<0) return 0;
  return st.type==T_DEVICE;
}

void log_add(char *c) {
  if(strlen(c)==0) return;
  if(log_count>0 && strcmp(cmdlog[log_count-1],c)==0) return;
  if(log_count<HIST_LIMIT){
    strcpy(cmdlog[log_count],c);
    log_count++;
  } else {
    for(int i=0;i<HIST_LIMIT-1;i++){
      strcpy(cmdlog[i],cmdlog[i+1]);
    }
    strcpy(cmdlog[HIST_LIMIT-1],c);
  }
  log_index=log_count;
}

char* suggest(char *buf,int pos){
  char *cands[]={"ls","cat","echo","find","grep","sleep",
                 "uptime","cd","mkdir","rm","sh",0};
  for(int i=0;cands[i];i++){
    if(strncmp2(buf,cands[i],pos)==0) return cands[i];
  }
  return 0;
}

int readcmd(char *buf,int nbuf){
  if(!is_tty()){
    memset(buf,0,nbuf);
    gets(buf,nbuf);
    if(buf[0]==0) return -1;
    return 0;
  }
  write(2,"?> ",3);
  memset(buf,0,nbuf);
  int pos=0;
  while(pos<nbuf-1){
    int cc=read(0,buf+pos,1);
    if(cc<1) break;
    if(buf[pos]=='\t'){
      char *s=suggest(buf,pos);
      if(s){
        int l=strlen(s);
        if(l>pos){
          strcpy(buf+pos,s+pos);
          for(int i=pos;i<l;i++) write(2,buf+i,1);
          pos=l;
        }
      }
    } else if(buf[pos]=='\n'){
      buf[pos]=0;
      if(pos>0) log_add(buf);
      return 0;
    } else if(buf[pos]==0x7f || buf[pos]=='\b'){
      if(pos>0){ pos--; write(2,"\b \b",3); }
    } else if(buf[pos]==0x1b){
      char seq[2];
      if(read(0,seq,2)==2 && seq[0]=='['){
        if(seq[1]=='A'){
          if(log_index>0){
            log_index--;
            strcpy(buf,cmdlog[log_index]);
            pos=strlen(buf);
            write(2,"\r?> ",4);
            write(2,buf,pos);
          }
        } else if(seq[1]=='B'){
          if(log_index<log_count-1){
            log_index++;
            strcpy(buf,cmdlog[log_index]);
            pos=strlen(buf);
            write(2,"\r?> ",4);
            write(2,buf,pos);
          } else if(log_index==log_count-1){
            log_index=log_count;
            buf[0]=0; pos=0;
            write(2,"\r?> ",4);
          }
        }
      }
    } else {
      write(2,buf+pos,1);
      pos++;
    }
  }
  if(pos==0) return -1;
  buf[pos]=0;
  return 0;
}

int showlog(void){
  for(int i=0;i<log_count;i++){
    printf("%d: %s\n",i+1,cmdlog[i]);
  }
  return 0;
}

void execute(struct basecmd *c){
  int p[2];
  struct background *bc;
  struct run *rc;
  struct listchain *lc;
  struct pipechain *pc;
  struct redirect *rd;
  if(c==0) exit(1);
  switch(c->kind){
  default:
    die("execute");
  case TYPE_EXEC:
    rc=(struct run*)c;
    if(rc->args[0]==0) exit(1);
    if(strcmp(rc->args[0],"history")==0){
      showlog();
      exit(0);
    }
    exec(rc->args[0],rc->args);
    fprintf(2,"exec %s failed\n",rc->args[0]);
    break;
  case TYPE_REDIR:
    rd=(struct redirect*)c;
    close(rd->fd);
    if(open(rd->path,rd->mode)<0){
      fprintf(2,"open %s failed\n",rd->path);
      exit(1);
    }
    execute(rd->sub);
    break;
  case TYPE_LIST:
    lc=(struct listchain*)c;
    if(fork_wrap()==0) execute(lc->left);
    wait(0);
    execute(lc->right);
    break;
  case TYPE_PIPE:
    pc=(struct pipechain*)c;
    if(pipe(p)<0) die("pipe");
    if(fork_wrap()==0){
      close(1); dup(p[1]);
      close(p[0]); close(p[1]);
      execute(pc->left);
    }
    if(fork_wrap()==0){
      close(0); dup(p[0]);
      close(p[0]); close(p[1]);
      execute(pc->right);
    }
    close(p[0]); close(p[1]);
    wait(0); wait(0);
    break;
  case TYPE_BG:
    bc=(struct background*)c;
    int pid=fork_wrap();
    if(pid==0){
      execute(bc->sub);
    } else {
      printf("[%d] background\n",pid);
    }
    break;
  }
  exit(0);
}

int main(void){
  static char buf[100];
  int fd;
  while((fd=open("console",O_RDWR))>=0){
    if(fd>=3){ close(fd); break; }
  }
  while(readcmd(buf,sizeof(buf))>=0){
    char *c=buf;
    while(*c==' '||*c=='\t') c++;
    if(*c=='\n') continue;
    if(strcmp(c,"history")==0){ showlog(); continue; }
    if(c[0]=='c'&&c[1]=='d'&&c[2]==' '){
      c[strlen(c)-1]=0;
      if(chdir(c+3)<0) fprintf(2,"cannot cd %s\n",c+3);
    } else {
      if(fork_wrap()==0) execute(parse(c));
      wait(0);
    }
  }
  exit(0);
}

void die(char *s){
  fprintf(2,"%s\n",s);
  exit(1);
}

int fork_wrap(void){
  int pid=fork();
  if(pid==-1) die("fork");
  return pid;
}

struct basecmd* mkexec(void){
  struct run *c=malloc(sizeof(*c));
  memset(c,0,sizeof(*c));
  c->kind=TYPE_EXEC;
  return (struct basecmd*)c;
}

struct basecmd* mkredir(struct basecmd *sub,char *f,char *ef,int m,int fd){
  struct redirect *c=malloc(sizeof(*c));
  memset(c,0,sizeof(*c));
  c->kind=TYPE_REDIR;
  c->sub=sub; c->path=f; c->epath=ef; c->mode=m; c->fd=fd;
  return (struct basecmd*)c;
}

struct basecmd* mkpipe(struct basecmd *l,struct basecmd *r){
  struct pipechain *c=malloc(sizeof(*c));
  memset(c,0,sizeof(*c));
  c->kind=TYPE_PIPE;
  c->left=l; c->right=r;
  return (struct basecmd*)c;
}

struct basecmd* mklist(struct basecmd *l,struct basecmd *r){
  struct listchain *c=malloc(sizeof(*c));
  memset(c,0,sizeof(*c));
  c->kind=TYPE_LIST;
  c->left=l; c->right=r;
  return (struct basecmd*)c;
}

struct basecmd* mkbg(struct basecmd *s){
  struct background *c=malloc(sizeof(*c));
  memset(c,0,sizeof(*c));
  c->kind=TYPE_BG;
  c->sub=s;
  return (struct basecmd*)c;
}

char spaces[]=" \t\r\n\v";
char syms[]="<|>&;()";

int token(char **ps,char *es,char **q,char **eq){
  char *s=*ps; int t;
  while(s<es&&strchr(spaces,*s)) s++;
  if(q) *q=s;
  t=*s;
  switch(*s){
  case 0: break;
  case '|': case '(': case ')': case ';': case '&': case '<':
    s++; break;
  case '>':
    s++; if(*s=='>'){ t='+'; s++; }
    break;
  default:
    t='a';
    while(s<es&&!strchr(spaces,*s)&&!strchr(syms,*s)) s++;
    break;
  }
  if(eq) *eq=s;
  while(s<es&&strchr(spaces,*s)) s++;
  *ps=s;
  return t;
}

int peek(char **ps,char *es,char *toks){
  char *s=*ps;
  while(s<es&&strchr(spaces,*s)) s++;
  *ps=s;
  return *s&&strchr(toks,*s);
}

struct basecmd *line(char**,char*);
struct basecmd *ppipe(char**,char*);
struct basecmd *pexec(char**,char*);
struct basecmd *finalize(struct basecmd*);

struct basecmd* parse(char *s){
  char *es=s+strlen(s);
  struct basecmd *c=line(&s,es);
  peek(&s,es,"");
  if(s!=es){ fprintf(2,"leftovers: %s\n",s); die("syntax"); }
  finalize(c);
  return c;
}

struct basecmd* line(char **ps,char *es){
  struct basecmd *c=ppipe(ps,es);
  while(peek(ps,es,"&")){
    token(ps,es,0,0);
    c=mkbg(c);
  }
  if(peek(ps,es,";")){
    token(ps,es,0,0);
    c=mklist(c,line(ps,es));
  }
  return c;
}

struct basecmd* ppipe(char **ps,char *es){
  struct basecmd *c=pexec(ps,es);
  if(peek(ps,es,"|")){
    token(ps,es,0,0);
    c=mkpipe(c,ppipe(ps,es));
  }
  return c;
}

struct basecmd* redirs(struct basecmd *c,char **ps,char *es){
  int t; char *q,*eq;
  while(peek(ps,es,"<>")){
    t=token(ps,es,0,0);
    if(token(ps,es,&q,&eq)!='a') die("redirect file");
    switch(t){
    case '<': c=mkredir(c,q,eq,O_RDONLY,0); break;
    case '>': c=mkredir(c,q,eq,O_WRONLY|O_CREATE|O_TRUNC,1); break;
    case '+': c=mkredir(c,q,eq,O_WRONLY|O_CREATE,1); break;
    }
  }
  return c;
}

struct basecmd* pblock(char **ps,char *es){
  if(!peek(ps,es,"(")) die("pblock");
  token(ps,es,0,0);
  struct basecmd *c=line(ps,es);
  if(!peek(ps,es,")")) die("missing )");
  token(ps,es,0,0);
  c=redirs(c,ps,es);
  return c;
}

struct basecmd* pexec(char **ps,char *es){
  char *q,*eq; int t,argc;
  struct run *rc; struct basecmd *ret;
  if(peek(ps,es,"(")) return pblock(ps,es);
  ret=mkexec();
  rc=(struct run*)ret; argc=0;
  ret=redirs(ret,ps,es);
  while(!peek(ps,es,"|)&;")){
    if((t=token(ps,es,&q,&eq))==0) break;
    if(t!='a') die("syntax");
    rc->args[argc]=q; rc->endargs[argc]=eq;
    argc++;
    if(argc>=ARG_LIMIT) die("too many args");
    ret=redirs(ret,ps,es);
  }
  rc->args[argc]=0; rc->endargs[argc]=0;
  return ret;
}

struct basecmd* finalize(struct basecmd *c){
  int i;
  struct background *bc;
  struct run *rc;
  struct listchain *lc;
  struct pipechain *pc;
  struct redirect *rd;
  if(c==0) return 0;
  switch(c->kind){
  case TYPE_EXEC:
    rc=(struct run*)c;
    for(i=0;rc->args[i];i++) *rc->endargs[i]=0;
    break;
  case TYPE_REDIR:
    rd=(struct redirect*)c;
    finalize(rd->sub);
    *rd->epath=0;
    break;
  case TYPE_PIPE:
    pc=(struct pipechain*)c;
    finalize(pc->left);
    finalize(pc->right);
    break;
  case TYPE_LIST:
    lc=(struct listchain*)c;
    finalize(lc->left);
    finalize(lc->right);
    break;
  case TYPE_BG:
    bc=(struct background*)c;
    finalize(bc->sub);
    break;
  }
  return c;
}

    
