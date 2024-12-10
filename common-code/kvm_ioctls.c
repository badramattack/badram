#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>


#include "include/kvm_ioctls.h"
#include "include/helpers.h"

//uapi headers of the host kernel WITH gpa2hpa patches
#include "linux/kvm.h"

static int _kvm_fd = -1;

/**
 * @brief open file descriptor if not already open
 * @returns 0 on success
*/
static int ensure_init_kvm_fd() {
  if( _kvm_fd == -1 ) {
    _kvm_fd = open("/dev/kvm", O_RDWR|O_CLOEXEC);
  }
  return _kvm_fd == -1;
}

/**
 * @brief close file descriptor if it is open
*/
static void close_kvm_fd() {
    if( _kvm_fd != -1  ) {
        close(_kvm_fd);
        _kvm_fd = -1;
    }
}

static char* ioctl_err_prefix = "ioctl failed (do you have KVM badram patches?)";

int ioctl_gpa_to_hpa(uint64_t gpa, uint64_t qemu_pid, uint64_t* out_hpa) {
    if( ensure_init_kvm_fd() ) {
      err_log("failed to open kvm api : %s", strerror(errno));
      return -1;
    }
    struct kvm_badram_gpa_to_hpa_args args = {
        .qemu_pid = qemu_pid,
        .gpa = gpa,
        .out_hpa = 0,
    };

    if(ioctl(_kvm_fd, KVM_BADRAM_GPA_TO_HPA, &args) < 0) {
        err_log("%s : %s\n", ioctl_err_prefix, strerror(errno));
        goto error;
    }
    printf("hpa is 0x%llx\n", args.out_hpa);
    *out_hpa = args.out_hpa;

    
    int ret = 0;
    goto cleanup;
error:
    ret = -1;
cleanup:
    close_kvm_fd();
    return ret;
}

int ioctl_tlb_flush(uint64_t qemu_pid) {
    if( ensure_init_kvm_fd() ) {
      err_log("failed to open kvm api : %s", strerror(errno));
      return -1;
    }
    struct  kvm_badram_flush_tlb_args args = {
        .qemupid = qemu_pid,
    };

    if(ioctl(_kvm_fd, KVM_BADRAM_FLUSH_TLB, &args) < 0) {
        err_log("%s : %s\n", ioctl_err_prefix, strerror(errno));
        goto error;
    }
    int ret = 0;
    goto cleanup;
error:
    ret = -1;
cleanup:
    close_kvm_fd();
    return ret;
}


int ioctl_remap_gfns(uint64_t qemu_pid, uint64_t gfn1, uint64_t pfn1, uint64_t gfn2, uint64_t pfn2) {
    if( ensure_init_kvm_fd() ) {
      err_log("failed to open kvm api : %s", strerror(errno));
      return -1;
    }
    struct  kvm_badram_remap_gfn_args args = {
        .qemupid = qemu_pid,
        .gfns = {gfn1, gfn2},
        .new_pfns = {pfn1, pfn2},
    };

    if(ioctl(_kvm_fd, KVM_BADRAM_REMAP_GFN, &args) < 0) {
        err_log("%s : %s\n", ioctl_err_prefix, strerror(errno));
        goto error;
    }
    int ret = 0;
    goto cleanup;
error:
    ret = -1;
cleanup:
    close_kvm_fd();
    return ret;
}

int ioctl_get_spte(uint64_t qemu_pid, uint64_t gpa, pg_level_t goal_pt_level,
 uint64_t* out_spte, uint64_t* out_hpa) {
    if( ensure_init_kvm_fd() ) {
      err_log("failed to open kvm api : %s", strerror(errno));
      return -1;
    }

    
    struct kvm_badram_get_pt_entry_args args = {
    	.gpa = gpa,
    	.qemupid = qemu_pid,
    	.goal_level = goal_pt_level,
    	.out_spte = 0,
    	.out_hpa =0,
    };

    if(ioctl(_kvm_fd, KVM_BADRAM_GET_PT_ENTRY, &args) < 0) {
        err_log("%s : %s\n", ioctl_err_prefix, strerror(errno));
        goto error;
    }
    *out_spte = args.out_spte;
    *out_hpa = args.out_hpa;
    int ret = 0;
    goto cleanup;
error:
    ret = -1;
cleanup:
    close_kvm_fd();
    return ret;
}
int ioctl_pause_vm_blocking(uint64_t qemu_pid) {
    if( ensure_init_kvm_fd() ) {
      err_log("failed to open kvm api : %s", strerror(errno));
      return -1;
    }
    struct kvm_badram_pause_vm_args args = {
    	.qemupid = qemu_pid,
    };

    if(ioctl(_kvm_fd, KVM_BADRAM_PAUSE_VM , &args) < 0) {
        err_log("%s : %s\n", ioctl_err_prefix, strerror(errno));
        goto error;
    }
    int ret = 0;
    goto cleanup;
error:
    ret = -1;
cleanup:
    close_kvm_fd();
    return ret;
}
int ioctl_resume_vm_blocking(uint64_t qemu_pid) {
    if( ensure_init_kvm_fd() ) {
      err_log("failed to open kvm api : %s", strerror(errno));
      return -1;
    }
    struct kvm_badram_resume_vm_args args = {
    	.qemupid = qemu_pid,
    };

    if(ioctl(_kvm_fd, KVM_BADRAM_RESUME_VM , &args) < 0) {
        err_log("%s : %s\n", ioctl_err_prefix, strerror(errno));
        goto error;
    }
    int ret = 0;
    goto cleanup;
error:
    ret = -1;
cleanup:
    close_kvm_fd();
    return ret;
}
