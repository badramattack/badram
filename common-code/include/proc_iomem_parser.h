#ifndef PROC_IOMEM_PARSER
#define PROC_IOMEM_PARSER

#include <stdint.h>
#include <regex.h>
#include <stdbool.h>

typedef enum {
	//Regular RAM
	MT_SYSTEM_RAM,
	//This should be the areas we excluded with memmap kern param
	MT_RESERVED,
	//Catchall for all other types. You probably should not access these
	MT_OTHER,
} memory_type_t;

typedef struct{
    uint64_t start;
    uint64_t end;
    char name[256];
	//describe the type of this memory region
	memory_type_t mt;

} mem_range_t;

/**
 * @brief parse the regexp match into a mem_range_t
 * 
 * @param matches as returned by regexec
 * @param matches_len  length of matches
 * @param input_string string on which we matched the regexp
 * @param result caller allocated result param
 * @return int 0 on success
 */
int _regexp_matches_to_mem_range(regmatch_t* matches, size_t matches_len, char* input_string, mem_range_t* result);


/**
 * @brief Parses /proc/iomem and returns data in callee allocated array
 * 
 * @param ranges out param, filled with calle allocated result array
 * @param range_len out param, filled with len of ranges
 * @return int 0 on success
 */
int parse_mem_layout(mem_range_t** ranges, size_t* range_len);

#endif
