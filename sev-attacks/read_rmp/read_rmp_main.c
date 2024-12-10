#include <limits.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>


#include "proc_iomem_parser.h"
#include "readalias.h"
#include "helpers.h"
#include "mem_range_repo.h"

#include "rmp.h"

typedef struct {
  mem_range_t* mrs;
  uint64_t* alias_masks;
  //lenght for both mrs and alias_masks
  size_t len;
} alias_data_t;

/**
 * @param start : of rmp range as reported by msr
*  @param end : of rmp_range as reported my msr
*  @param dump_path : If NULL, dump rmp to stdout, else write to this path
* 
*/
int run(uint64_t rmp_start, uint64_t rmp_end, char* dump_path) {

  
  //Read and print RMP via direct access

  //msr reports inclusive end, we want exclusive
  rmp_end += 1;
  size_t rmp_buf_bytes = rmp_end - rmp_start;
  printf("rmp spans 0x%jx bytes\n", rmp_buf_bytes);
  uint8_t* rmp_buf = malloc(rmp_buf_bytes);
  uint8_t* buf1 = NULL;
  uint8_t* buf2 = NULL;
  page_stats_t stats;
  FILE* dump_file = stdout;
  if( dump_path ) {
    dump_file = fopen(dump_path, "w");
    if(!dump_file) {
      err_log("failed to create file %s for writing : %s\n", dump_path, strerror(errno))
      goto error;
    }
  } 

  if( open_kmod() ) {
    err_log("failed to open driver\n");
    goto error;
  }

  if( memcpy_frompa(rmp_buf,rmp_start, rmp_buf_bytes , &stats, true)) {
    err_log("direct read of rmp at 0x%jx failed\n", rmp_start) ;
    goto error;
  }
  //first 16 KiB are "processor reserved", rmp entries start afterwards
  rmp_entry_t* rmp = (rmp_entry_t*) (rmp_buf + OFFSET_RMP_ENTRIES);
  size_t rmp_len = (rmp_buf_bytes - OFFSET_RMP_ENTRIES) / sizeof(rmp_entry_t);

  dump_rmp(rmp , rmp_len, dump_file);


  //TODO: quick and dirty test for writing to rmp entry. refactor into own program/function
  /*//access entries on last rmp page via alias
  uint64_t pa_last_rmp_page = rmp_end - 4096;
  uint64_t alias_last_rmp_page;
  printf("Lat RMP page is at 0x%jx\n", pa_last_rmp_page);
  if( get_alias(pa_last_rmp_page, alias_data.mrs , alias_data.alias_masks , alias_data.len , &alias_last_rmp_page )) {
    err_log("failed to get alias for last rmp page at 0x%jx\n", pa_last_rmp_page);
    goto error;
  }


  printf("Have %ju rmp entries\n", rmp_len);
  //16 byte per entry, so 256 entries per page, want first entry on last page
  buf1 = malloc(4096);
  memcpy(buf1, rmp+(rmp_len - 256), 4096);
  ((rmp_entry_t*)buf1)[0].info.gpa = 0xdedbeef;
  printf("overwriting gpa of first entry on last page via alias 0x%jx\n", alias_last_rmp_page);
  if( wbinvd_ac() ) {
    err_log("flush failed\n");
    goto error;
  }
  if( memcpy_topa(alias_last_rmp_page, buf1 , 4096 ,&stats,true )) {
    err_log("failed to write to last rmp page at 0x%jx via alias 0x%jx\n", pa_last_rmp_page, alias_last_rmp_page);  
    goto error;
  }
  if( wbinvd_ac() ) {
    err_log("flush failed\n");
    goto error;
  }

  //read last rmp page through orig addr
  printf("Reading again through original address 0x%jx\n", pa_last_rmp_page);
  buf2 = malloc(4096);
  if( memcpy_frompa(buf2, pa_last_rmp_page , 4096 ,&stats , true )) {
    err_log("failed to read last rmp page through orig addr 0x%jx\n", pa_last_rmp_page);
    goto error;
  }
  rmp_entry_t* orig_entry = rmp+(rmp_len-256);
  rmp_entry_t* manip_entry = (rmp_entry_t*)buf2;

  printf("Orig Entry Raw: ");
  hexdump((uint8_t*)orig_entry, sizeof(rmp_entry_t) );
  printf("Manip Entry Raw: ");
  hexdump((uint8_t*) manip_entry, sizeof(rmp_entry_t));

  {
    uint64_t g1 = orig_entry->info.gpa;
    uint64_t g2 = manip_entry->info.gpa;
    printf("Orig gpa:\t0x%09jx\n", g1);
    printf("Manip gpa:\t0x%09jx\n", g2);
  }*/


  int ret = 0;
  goto cleanup;
error:
  ret = -1;
cleanup:
  if( rmp_buf ) {
    free(rmp_buf);
  }
  if( buf1 ) {
    free(buf1);
  }
  if( buf2 ) {
    free(buf2);
  }
  if( dump_file != NULL && dump_file != stdout) {
    fclose(dump_file);
  }
  close_kmod();
  return ret;
}

int main(int argc, char** argv) {
  if( argc != 4) {
    printf("Usage: read_rmp <pa of rmp start> <pa of rmp end> {\"stdout\",<path to file>}\n");
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

  char* dump_path = NULL;
  char* stdout_flag = "stdout";
  if( 0 != memcmp(argv[3], stdout_flag, strlen(stdout_flag))) {
    dump_path = argv[3];
  } 
  return run(rmp_start, rmp_end, dump_path);  
}
