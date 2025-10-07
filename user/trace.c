#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// Simple trace program for xv6
// Usage: trace <mask> <command> [args...]
// Example: trace 32 grep hello README

int
main(int argc, char *argv[])
{
  // Check if user gave enough arguments
  if(argc < 3){
    fprintf(2, "Usage: trace <mask> <command> [args...]\n");
    exit(1);
  }

  // Convert the first argument (mask) from string to int
  int mask = atoi(argv[1]);

  // Call the trace system call
  if(trace(mask) < 0){
    fprintf(2, "trace: syscall didn't work properly\n");
    exit(1);
  }

  // Run the actual command after setting the trace
  exec(argv[2], &argv[2]);

  // If exec fails, print an error
  fprintf(2, "trace: couldn't run %s\n", argv[2]);
  exit(1);
}

