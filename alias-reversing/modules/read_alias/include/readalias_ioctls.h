#ifndef READALIAS_IOCTLS_H
#define READALIAS_IOCTLS_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

enum flush_method {
  //Don't flush
  FM_NONE,
  //Flush using clflush
  FM_CLFLUSH,
  //Flush using wbinvd on all cores
  FM_WBINVD,
};

struct args {
  void* buffer;
  uint64_t   count;
  uint64_t   pa;
  enum flush_method   flush;
  //if 1, we access pages even if they are marked as reserved
  int access_reserved;
};

#define MEMCPY_TOPA    _IOW('f', 0x20, struct args*)
#define MEMCPY_FROMPA  _IOW('f', 0x21, struct args*)
#define FLUSH_PAGE     _IOW('f', 0x22, struct args*)
#define WBINVD_AC      _IOW('f', 0x23, struct args*)

//ioctl return code to indicate that page is reserved
#define RET_RESERVED 1
//ioctl return code to indicate that we failed to map the page
#define RET_MAPFAIL 2

#endif
