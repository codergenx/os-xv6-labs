#include "kernel/types.h"
#include "kernel/riscv.h"
#include "kernel/sysinfo.h"
#include "user/user.h"

// Simple helper to get system info safely
void
get_sysinfo(struct sysinfo *s)
{
  if (sysinfo(s) < 0) {
    printf("sysinfotest: system call failed!\n");
    exit(1);
  }
}

int
main(void)
{
  struct sysinfo before, after;

  // Get initial system info (before memory allocation)
  get_sysinfo(&before);

  // Try allocating one page (4KB)
  void *mem = malloc(4096);
  if (mem == 0) {
    printf("sysinfotest: couldn't allocate memory\n");
    exit(1);
  }

  // Get system info again after allocation
  get_sysinfo(&after);

  // Check if free memory actually reduced
  if (after.freemem >= before.freemem) {
    printf("sysinfotest: free memory didn't drop (something's off)\n");
    exit(1);
  }

  // Process count should not change during this test
  if (after.nproc != before.nproc) {
    printf("sysinfotest: process count changed unexpectedly\n");
    exit(1);
  }

  // Free the memory back
  free(mem);

  printf("sysinfo check: everything looks fine\n");
  exit(0);
}

