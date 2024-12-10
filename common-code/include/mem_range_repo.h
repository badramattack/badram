#ifndef MEM_RANGE_REPO_H
#define MEM_RANGE_REPO_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include<errno.h>

#include "helpers.h"
#include "proc_iomem_parser.h"


/**
 * @brief Serialize the entries in `mr` to csv. 
 * @return 0 on success
*/
int write_csv(char* path, mem_range_t* mr, uint64_t* alias_masks,  size_t len); 

/**
 * @brief Deserialize a csv file created with `write_csv` into mem_range_t and alias masks.
 * @param path : path to csv file
 * @param out_mr : Output parameter for the parsed memomory ranges. Caller must free
 * @param out_alias_masks : Output parameter for the parsed alias masks. Caller must free
 * @param out_len : Output parameter with length for `our_mr` and `our_alias_masks`
 * @return 0 on success
*/
int parse_csv(char* path, mem_range_t** out_mr, uint64_t** out_alias_masks, size_t* out_len);


#endif
