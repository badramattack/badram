#ifndef KVM_IOCTLS_H
#define KVM_IOCTLS_H

#include<stdint.h>


/**
 * @brief Use the gpa2hpa kvm patches to translate gpa to hpa. Works with
 * SEV-SNP MEMFD backed memory.
 * @paramter gpa: gpa for which we want the hpa
 * @paramter qemu_pid: PID of the qemu process running the targeted VM
 * @parameter out_hpa: Output param. Filled with the hpa for `gpa`
 * @returns: 0 on success
*/
int ioctl_gpa_to_hpa(uint64_t gpa, uint64_t qemu_pid, uint64_t* out_hpa);


/**
 * @brief Flush TLB for all VCPUs
 * @paramter qemu_pid: PID of the qemu process running the targeted VM
*/
int ioctl_tlb_flush(uint64_t qemu_pid);


/**
 * @brief Request that the two gfns point to the respective pfns (i.e. the pfn are already the new values
 * if you ware in the "swapping scenario")
 * We need to do this in one ioctl to avoid TLB flushes
 * VMEXIT. Have not implemented feedback channel yet. See dmesg for status
 * messages
 * @paramter qemu_pid: PID of the qemu process running the targeted VM
 * @paramter gfn1,pfn1,gfn2,pfn2 : guest frame number and the new host page frame number that they should
 * be mapped to
 * @returns: 0 if ictols succeeds. NO INFO about success/status of remapping
 * See dmesg for now to get this information
*/
int ioctl_remap_gfns(uint64_t qemu_pid, uint64_t gfn1, uint64_t pfn1, uint64_t gfn2, uint64_t pfn2);


typedef enum {
	PG_LEVEL_NONE,
	PG_LEVEL_4K,
	PG_LEVEL_2M,
	PG_LEVEL_1G,
	PG_LEVEL_512G,
	PG_LEVEL_NUM
} pg_level_t

;

/**
 * @brief Get the raw page table entry for the given gpa at the given page table level
 * @parameter qemu_pid : PID of the qemu process running the targeted VM
 * @paramter gpa : lookup page table entry for this address
 * @paramter goal_pt_level : page table level for which the entry is returned
 * @paremter out_spte : Output parameter. Filled with the raw page table entry
 * @paramter out_hap : Output paramter. Convenience paramter, that is filled with the hpa that
 * can be extracted from out_spte
*/
int ioctl_get_spte(uint64_t qemu_pid, uint64_t gpa, pg_level_t goal_pt_level,
 uint64_t* out_spte, uint64_t* out_hpa);


/**
 * @brief blocks untill VM is in paused state. VM is kept in paused state until
 * ioctl_resume_vm_blocking is called
 * @parameter qemu_pid : PID of the qemu process running the targeted VM
 * @returns 0 on success. At this point the VM is paused
*/
int ioctl_pause_vm_blocking(uint64_t qemu_pid);


/**
 @brief resume a previously blocked VM.
 * @parameter qemu_pid : PID of the qemu process running the targeted VM
 * @returns 0 on success. At this point the VM is about to be resume. There
 * is a small racy timing window
*/
int ioctl_resume_vm_blocking(uint64_t qemu_pid);
#endif
