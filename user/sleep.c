#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int par1, char *parv[])
{
  if(par1 < 2){
    fprintf(2, "Sleep ticks performed\n");
    exit(1);
  }

  int ticks = atoi(parv[1]);

  if(ticks < 0){
    fprintf(2, " invalid number of ticks\n");
    exit(1);
  }

  if(pause(ticks) < 0){
    fprintf(2, "System call failed\n");
    exit(1);
  }

  exit(0);
}

