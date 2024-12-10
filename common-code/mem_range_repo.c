#include "include/mem_range_repo.h"

int write_csv(char* path, mem_range_t* mr, uint64_t* alias_masks,  size_t len) {
  FILE* f = fopen(path, "w");
  if(!f) {
    err_log("Failed to create file %s : %s", path, strerror(errno))
    return -1;
  }

  //write csv header
  if(-1 == fprintf(f,"#start pa, end pa, alias xor mask\n")){
    err_log("failed to write : %s\n", strerror(errno));
    fclose(f);
    return -1;
  }
  //write csv data
  for(size_t i = 0; i < len; i++) {
    if( -1 == fprintf(f, "0x%jx,0x%jx,0x%jx\n", mr[i].start, mr[i].end, alias_masks[i])) {
      err_log("failed to write : %s\n", strerror(errno));
      fclose(f);
      return -1;
    }
  }

  fclose(f);
  return 0;
}


int parse_csv(char* path, mem_range_t** out_mr, uint64_t** out_alias_masks, size_t* out_len) {
  FILE* f = fopen(path, "r");
  char* buf = NULL;
  mem_range_t* mr = NULL;
  uint64_t* alias_masks = NULL;
  size_t idx;
  int retval = 0;
  if(!f) {
    err_log("Failed to open %s for reading : %s\n", path, strerror(errno));
    return -1;
  }

  //iterate over file to get number of entries. Afterwards alloc result
  //array and iterate over file again
  size_t len = 0;
  uint64_t dummy;
  size_t buf_bytes = 512;
  buf = (char*)malloc(buf_bytes);
  if( fgets(buf, buf_bytes, f) == NULL ) {
    err_log("Failed to read header row : %s\n", strerror(errno));
    goto error;
  }
  if( ferror(f) ) {
    err_log("Error reading from %s : %s\n", path, strerror(errno));
    goto error;
  }
  while(  3 == fscanf(f, "0x%jx,0x%jx,0x%jx\n", &dummy, &dummy, &dummy )) {
    len += 1;
  }
  if( ferror(f) ) {
    err_log("Error reading from %s : %s\n", path, strerror(errno));
    goto error;
  }
  if( fseek(f, 0, SEEK_SET)) {
    err_log("Error resetting file position : %s\n", strerror(errno));
    goto error;
  }
  if( len < 1 ) {
    err_log("Input file %s does not contain any entries\n", path);
    goto error;
  }



  mr = (mem_range_t*)malloc(sizeof(mem_range_t) * len);
  alias_masks = (uint64_t*)malloc(sizeof(uint64_t) * len);
  idx = 0;
  if( fgets(buf, buf_bytes, f) == NULL ) {
    err_log("Failed to read header row : %s\n", strerror(errno));
    goto error;
  }
  if( ferror(f) ) {
    err_log("Error reading from %s : %s\n", path, strerror(errno));
    goto error;
  }
  while( 3 == fscanf(f, "0x%jx,0x%jx,0x%jx\n", &(mr[idx].start), &(mr[idx].end), &(alias_masks[idx]) ) ){
    char* dummy_name = "Not restored by parser\n";
    memcpy(mr[idx].name, dummy_name, strlen(dummy_name));
    idx += 1;
  }
  if( ferror(f) ) {
    err_log("Error reading from %s : %s\n", path, strerror(errno));
    goto error;
  }
  if( idx != len ) {
    err_log("Expected to read %ju entries but got %ju\n", len, idx);
    goto error;
  }

  *out_len = len;
  *out_mr =mr;
  *out_alias_masks = alias_masks;
  
  
goto cleanup;
error:
  retval = -1;
  //only free on error, otherwise we return them in output arg
  if( mr ) {
    free(mr);
  }
  if( alias_masks ) {
    free(alias_masks);
  }
cleanup:
  if(f) {
    fclose(f);
  }
  if( buf ) {
    free(buf);
  }
  return retval;
  
}
