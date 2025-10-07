// kernel/sysinfo.h
// This struct is used to pass system info from kernel to user.

struct sysinfo {
  uint64 freemem;  // total free memory in bytes
  uint64 nproc;    // number of active (non-unused) processes
};

