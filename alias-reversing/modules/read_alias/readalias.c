#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>


#include "include/readalias_ioctls.h"
#include "include/readalias.h"

static int kmod_fd = -1;
static int random_fd = -1;


//err_log, _get_rand_bytes and _hexdump are copy paste from common-code but this allows
//use to include this lib here. Since we also want to compile a lib from this code this would be confusing

#define err_log(fmt, ...) fprintf(stderr, "%s:%d : " fmt, __FILE__, __LINE__, ##__VA_ARGS__);

static int _get_rand_bytes(void *p, size_t len) {
  if (random_fd == -1) {
    random_fd = open("/dev/urandom", O_RDWR);
    if (random_fd < 0) return random_fd;
  }

  size_t nb_read = read(random_fd, p, len);

  return nb_read != len;
}

static void _hexdump(uint8_t* a, const size_t n)
{
	for(size_t i = 0; i < n; i++) {
    if (a[i]) printf("\x1b[31m%02x \x1b[0m", a[i]);
    else printf("%02x ", a[i]);
    if (i % 64 == 63) printf("\n");
  }
	printf("\n");
}

uint64_t __next_page(uint64_t pa) {
  return ((pa >> PAGE_SHIFT) + 1) << PAGE_SHIFT;
}

int wbinvd_ac(void) {
  if( kmod_fd < 0) {
    err_log("%s:%d: driver not openened\n", __FILE__, __LINE__);
    return -1;
  }
  struct args args = {0};
  return ioctl(kmod_fd, WBINVD_AC, &args);
}

int __memcpy_topa(uint64_t dst, void* src, size_t count, enum flush_method fm, page_stats_t* out_stats, bool err_on_access_fail, bool access_reserved) {
  if (kmod_fd < 0) {
    printf("%s:%d: driver not openened\n", __FILE__, __LINE__);
    return -1;
  }

  // The kernel module requires that only one page can be copied to at a time.
  uint64_t offset = dst % PAGE_SIZE;

  struct args args = {
    .pa = dst,
    .buffer = src,
    .count = offset ? MIN(PAGE_SIZE - offset, count) : MIN(PAGE_SIZE, count),
    .flush = fm,
    .access_reserved = access_reserved,
  };

  size_t remaining = count;

  while  (remaining != 0) {
    switch (ioctl(kmod_fd, MEMCPY_TOPA, &args)) {
      case 0:
        break;
      case 1:
        out_stats->reserved_pages += 1;
        if( err_on_access_fail ) {
          return -1;
        }
        break;
      case 2:
        out_stats->map_failed += 1;
        if( err_on_access_fail ) {
          return -1;
        }
        break;
      default:
        return -1;
    }

    remaining -= args.count;
    args.pa = __next_page(args.pa);
    args.buffer = (unsigned char*)args.buffer + args.count;
    args.count = MIN(remaining, PAGE_SIZE);
  }

  return 0;
}

int __memcpy_frompa(void* dst, uint64_t src, size_t count, enum flush_method fm, page_stats_t* out_stats, bool err_on_access_fail, bool access_reserved) {
  if (kmod_fd < 0) {
    printf("%s:%d: driver not openened\n", __FILE__, __LINE__);
    return -1;
  }

  // The kernel module requires that only one page can be copied from at a time.
  uint64_t offset = src % PAGE_SIZE;

  struct args args = {
    .pa = src,
    .buffer = dst,
    .count = offset ? MIN(PAGE_SIZE - offset, count) : MIN(PAGE_SIZE, count),
    .flush = fm,
    .access_reserved = access_reserved,
  };

  size_t remaining = count;

  while  (remaining != 0) {
    switch (ioctl(kmod_fd, MEMCPY_FROMPA, &args)) {
      case 0:
        break;
      case 1:
        out_stats->reserved_pages += 1;
        if( err_on_access_fail ) {
          return -1;
        }
        break;
      case 2:
        out_stats->map_failed += 1;
        if(err_on_access_fail) {
          return -1;
        }
        break;
      default:
        return -1;
    }

    remaining -= args.count;
    args.pa = __next_page(args.pa);
    args.buffer = (unsigned char*)args.buffer + args.count;
    args.count = MIN(remaining, PAGE_SIZE);
  }

  return 0;
}

int open_kmod() {
  if( kmod_fd == - 1) {
    kmod_fd = open("/dev/readalias_dev", O_RDWR);
  }
  return kmod_fd < 0 ? kmod_fd : 0;
}

void close_kmod() {
  if( kmod_fd != - 1 ) {
    close(kmod_fd);
    kmod_fd = - 1;
  }
}

int memcpy_topa(uint64_t dst, void* src, size_t count, page_stats_t* out_stats, bool err_on_access_fail) {
  return __memcpy_topa(dst, src, count, FM_NONE, out_stats, err_on_access_fail, false);
}

int memcpy_topa_ext(uint64_t dst, void* src, size_t count, struct pamemcpy_cfg* cfg) {
  return __memcpy_topa(dst, src, count, cfg->flush_method, &(cfg->out_stats), cfg->err_on_access_fail, cfg->access_reserved);
}

int memcpy_frompa(void* dst, uint64_t src, size_t count, page_stats_t* out_stats, bool err_on_access_fail) {
  return __memcpy_frompa(dst, src, count, FM_NONE, out_stats, err_on_access_fail, false);
}

int memcpy_frompa_ext(void* dst, uint64_t src, size_t count, struct pamemcpy_cfg* cfg) {
  return __memcpy_frompa(dst, src , count , cfg->flush_method , &(cfg->out_stats) ,
    cfg->err_on_access_fail , cfg->access_reserved );
}

int clflush_range(uint64_t pa, size_t count, page_stats_t* out_stats, bool err_on_access_fail) {
  if (kmod_fd < 0) {
    printf("%s:%d: driver not openened\n", __FILE__, __LINE__);
    return -1;
  }

  struct args args = {
    .pa = pa,
  };

  size_t remaining = count;
  if (remaining % PAGE_SIZE == 0) remaining++;

  while  (remaining != 0) {
    // TODO: This might not be directly possible when the memory range is protected.
    switch (ioctl(kmod_fd, FLUSH_PAGE, &args)) {
      case 0:
        break;
      case 1:
        out_stats->reserved_pages += 1;
        if( err_on_access_fail ) {
          return -1;
        }
        break;
      case 2:
        out_stats->map_failed += 1;
        if( err_on_access_fail ) {
          return -1;
        }
        break;
      default:
        return -1;
    }

    remaining -= MIN(remaining, PAGE_SIZE);
    args.pa = __next_page(args.pa);
  }

  return 0;
}


int flush_ext(uint64_t pa, size_t count, struct pamemcpy_cfg* cfg) {
  switch(cfg->flush_method) {
    case FM_NONE:
      return 0;
    case FM_CLFLUSH:
      return clflush_range(pa, count, &(cfg->out_stats), cfg->err_on_access_fail);
    case FM_WBINVD:
      return wbinvd_ac();
    default:
      err_log("unknown flush method %d\n", cfg->flush_method);
      return -1;
  }
}

int check_alias(uint64_t source_pa, uint64_t alias_candidate, struct pamemcpy_cfg* memcpy_cfg, bool verbose) {
    const size_t msg_len = 64;
    uint8_t m1[msg_len], m2[msg_len], mxor[msg_len], buf1[msg_len], buf2[msg_len], bufxor[msg_len];
    _get_rand_bytes(m1, msg_len);
    _get_rand_bytes(m2, msg_len);
    for(size_t i = 0; i < msg_len; i++) {
        mxor[i] = m1[i] ^ m2[i];
    }

    if( flush_ext(source_pa, msg_len, memcpy_cfg) ) {
        if(verbose) err_log("flush_range for 0x%jx failed\n", source_pa);
        return CHECK_ALIAS_ERR_ACCESS;
    }

    if( memcpy_topa_ext(source_pa, m1, msg_len, memcpy_cfg) ) {
        if(verbose) err_log("memcpy_topa for 0x%jx failed\n", source_pa);
        return CHECK_ALIAS_ERR_ACCESS;
    }

    //read alias_candidate_pa
    if( memcpy_frompa_ext(buf1, alias_candidate, msg_len, memcpy_cfg) ) {
        if(verbose) err_log("memcpy_frompa for 0x%jx failed\n", alias_candidate);
        return CHECK_ALIAS_ERR_ACCESS;
    }


    //write m2 to source_pa
    if( memcpy_topa_ext(source_pa, m2, msg_len, memcpy_cfg) ) {
        if(verbose) err_log("memcpy_topa for source_pa 0x%jx failed\n", source_pa);
        return CHECK_ALIAS_ERR_ACCESS;
    }

    //read from target_pa
    if( memcpy_frompa_ext(buf2, alias_candidate, msg_len, memcpy_cfg) ) {
        if(verbose) err_log("memcpy_frompa for target_pa 0x%jx failed\n", alias_candidate);
        return CHECK_ALIAS_ERR_ACCESS;
    }

    /*
    * We wrote  m1 and m2 to source_pa and read them through alias_candidate_pa
    * buf1 and buf2 contain the values read through surce_pa.
    * To account for memory scrambling, we dont compare them directly but check if
    * buf1^buf2 matches m1^m2
    */

//#define FIND_ALIAS_DEBUG
#ifdef FIND_ALIAS_DEBUG
    printf("alias_candidate: 0x%jx\n", alias_candidate);
    printf("m1: ");
    _hexdump(m1, msg_len);
    printf("buf1: ");
    _hexdump(buf1, msg_len);

    printf("m2: ");
    _hexdump(m2, msg_len);
    printf("buf2: ");
    _hexdump(buf2, msg_len);

    printf("m1 ^ m2: ");
    _hexdump(mxor, msg_len);
#endif
           
    int found_matching_xor = 1;
    for(size_t i = 0; i < msg_len; i++) {
        bufxor[i] = buf1[i] ^ buf2[i];
        if( mxor[i] != bufxor[i]) {
            found_matching_xor = 0;
        }
    }
#ifdef FIND_ALIAS_DEBUG
    printf("buf1 ^ buf2: ");
    _hexdump(bufxor, msg_len);
#endif

    if( found_matching_xor ) {
        if(verbose) {
            uint64_t pa_xor = source_pa ^ alias_candidate;
            printf("Found alias for 0x%jx at 0x%jx! xor diff = 0x%jx\n", source_pa, alias_candidate, pa_xor);
            printf("buf1: ");
            _hexdump(buf1, msg_len);
            printf("buf2: ");
            _hexdump(buf2, msg_len);
             printf("got xor: ");
            _hexdump(mxor, msg_len);
            printf("want xor: ");
            _hexdump(mxor, msg_len);

            printf("Reserved Page Errors: %ju, Map Failed Errors: %ju\n",
                memcpy_cfg->out_stats.reserved_pages, memcpy_cfg->out_stats.map_failed);
        }
        return 0;
    }
    return CHECK_ALIAS_ERR_NO_ALIAS;
}
