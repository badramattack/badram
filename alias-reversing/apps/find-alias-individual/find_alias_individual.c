#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>


#include "readalias.h"
#include "proc_iomem_parser.h"
#include "helpers.h"
#include "mem_range_repo.h"
#include "readalias_ioctls.h"


//Controls debug prints in find_alias functions
//#define FIND_ALIAS_DEBUG

//bundles all mem ranges together with the ones filtered for
//searching aliases
struct mem_layout {
    mem_range_t* mem_ranges;
    size_t mem_ranges_len;
    mem_range_t* filtered_ranges;
    size_t filtered_ranges_len;
};

void free_mem_layout(struct mem_layout m) {
    if(m.mem_ranges) free(m.mem_ranges);
    if(m.filtered_ranges) free(m.filtered_ranges);
}

//physical address and the corresponding memory range
struct mem_range_pa {
    uint64_t pa;
    mem_range_t mr;
};

/**
 * @brief Serialize memory regions together with their aliases to file. To indicate that an entry does not have a valid
   alias, store 0 in the corresponding `alias_pa` entry. Entries without valid alias will not be serialized
 * @returns 0 on success
*/
int store_valid_aliases(char* path, struct mem_range_pa* source , uint64_t*  alias_pa, size_t len) {
    //count valid entries
    size_t valid_entries = 0;
    for(size_t i = 0; i < len; i++) {
        if( alias_pa[i] != 0 ) {
            valid_entries += 1;
        }
    }
    //compute alias from source_pa and alias_pa and filter valid entries into new array
    mem_range_t* mr_with_alias = malloc(sizeof(mem_range_t) * valid_entries);
    uint64_t* alias_masks = malloc(sizeof(uint64_t) * valid_entries);
    for( size_t next_idx=0, i = 0; i < len; i++ ) {
        if( alias_pa[i] != 0 ) {
            mr_with_alias[next_idx] = source[i].mr;
            alias_masks[next_idx] = source[i].pa ^ alias_pa[i];
            next_idx += 1;
        }
    }
    //serialize
    int res = write_csv(path, mr_with_alias , alias_masks , valid_entries );
    free(mr_with_alias);
    free(alias_masks);
    return res;
    
}


/**
 * @brief Tries to find an alias for `source_pa`. Since we assume
 * deactivated scrambling, simply write the marker value once an then scan
 * for it
 * 
 * @param source_pa search alias for this physical address
 * @param out_alias out param. On success filled with pa of alias
 * @return int 0 on success
 */
int find_alias_no_scrambling(uint64_t source_pa, uint64_t* out_alias, mem_range_t* sys_ram, size_t sys_ram_len, bool access_reserved) {
    const size_t msg_len = 64;
    uint8_t m1[msg_len], buf1[msg_len];
    get_rand_bytes(m1, msg_len);

    page_stats_t tmp;
    //write m1 to source_pa
    if( clflush_range(source_pa, msg_len, &tmp, true) ) {
        err_log("flush_range for 0x%jx failed\n", source_pa)
        return -1;
    }

    //write marker value only once. Without scramblign we can simple search for another occurence
    //of this value
    if( memcpy_topa(source_pa, m1, msg_len, &tmp, true) ) {
        err_log("memcpy_topa for 0x%jx failed\n", source_pa);
        return -1;
    }
    if( wbinvd_ac() ) {
        err_log("wbinvd_ac() failed");
        return -1;
    }

    size_t total_memory_bytes = 0;
    for( size_t i = 0; i < sys_ram_len; i++) {
        total_memory_bytes += sys_ram[i].end - sys_ram[i].start;
    }
    size_t total_processed_bytes = 0;
    const size_t print_progress_interval = 1 << 30;
  
  
    page_stats_t total_page_stats = {0};
    struct pamemcpy_cfg memcpy_cfg = {
        .out_stats = {0},
        .access_reserved = access_reserved,
        .err_on_access_fail = true,
        .flush_method = FM_CLFLUSH,
    };
    for( size_t sys_ram_idx = 0; sys_ram_idx < sys_ram_len; sys_ram_idx++) {
        mem_range_t* mr = sys_ram+sys_ram_idx;
        printf("[%ju/%ju[: Searching mem range %s [0x%jx,0x%jx[ (%.2f GiB)\n",
        sys_ram_idx, sys_ram_len, mr->name, mr->start, mr->end, (double)(mr->end - mr->start)/(1<<30));
        uint64_t aligned_start = mr->start;
        if((aligned_start % 4096) != 0) {
            aligned_start += (4096 - (mr->start % 4096));
        }
        printf("aligned_start = 0x%jx\n", aligned_start);
        for( uint64_t alias_candidate_pa = aligned_start; alias_candidate_pa < mr->end; alias_candidate_pa += 4096) {
            //printf("doing pa = 0x%jx\n", target_pa);
            if( (total_processed_bytes % print_progress_interval) == print_progress_interval) {
                printf("Total progress %0.2f\n", (double)total_processed_bytes/total_memory_bytes);
            }
            total_processed_bytes += 4096;
            if( alias_candidate_pa == source_pa) {
            continue;
            }


            //read alias_candidate_pa
            if( memcpy_frompa_ext(buf1, alias_candidate_pa, msg_len, &memcpy_cfg) ) {
                err_log("memcpy_frompa for 0x%jx failed\n", alias_candidate_pa);
                //return -1;
                continue;
            }

#ifdef FIND_ALIAS_DEBUG
            printf("alias_candidate: 0x%jx\n", alias_candidate_pa);
            printf("m1: ");
            hexdump(m1, msg_len);
            printf("buf1: ");
            hexdump(buf1, msg_len);

#endif
           

            if( 0 == memcmp(m1, buf1, msg_len) ) {
                uint64_t pa_xor = source_pa ^ alias_candidate_pa;
                printf("Found alias for 0x%jx at 0x%jx! xor diff = 0x%jx\n", source_pa, alias_candidate_pa, pa_xor);
                printf("buf1: ");
                hexdump(buf1, msg_len);
                printf("buf2: ");

                *out_alias = alias_candidate_pa;
                printf("Reserved Page Errors: %ju, Map Failed Errors: %ju\n",
                    total_page_stats.reserved_pages, total_page_stats.map_failed);
                return 0;
            }
        }

        printf("Reserved Page Errors: %ju, Map Failed Errors: %ju\n",
            memcpy_cfg.out_stats.reserved_pages, memcpy_cfg.out_stats.map_failed);

        total_page_stats.reserved_pages += memcpy_cfg.out_stats.reserved_pages;
        total_page_stats.map_failed += memcpy_cfg.out_stats.map_failed;
        memcpy_cfg.out_stats.map_failed = 0;
        memcpy_cfg.out_stats.reserved_pages = 0;
    }
  
    printf("Reserved Page Errors: %ju, Map Failed Errors: %ju\n",
        total_page_stats.reserved_pages, total_page_stats.map_failed);
    //if we reach the end of this function, we have not found an alias. Thus, return error_code
    return -1;

}


/**
 * @brief Tries to find an alias for `source_pa`. Works even if memory
 * scrambling is activated. If you know that memory scrambling is deactivated,
 * you might want to use find_alias_no_scrambling for better performance
 * 
 * @param source_pa search alias for this physical address
 * @param out_alias out param. On success filled with pa of alias
 * @param access_reserved : If true, try go access pages marked as reserved. Might lead to crashes, especially when writing to them
 * @return int 0 on success
 */
int find_alias_scrambling(uint64_t source_pa, uint64_t* out_alias, mem_range_t* sys_ram, size_t sys_ram_len,bool access_reserved) {
    const size_t msg_len = 64;
    uint8_t m1[msg_len], m2[msg_len], mxor[msg_len], buf1[msg_len], buf2[msg_len], bufxor[msg_len];
    get_rand_bytes(m1, msg_len);
    get_rand_bytes(m2, msg_len);
    for(size_t i = 0; i < msg_len; i++) {
        mxor[i] = m1[i] ^ m2[i];
    }

    size_t total_memory_bytes = 0;
    for( size_t i = 0; i < sys_ram_len; i++) {
        total_memory_bytes += sys_ram[i].end - sys_ram[i].start;
    }
    size_t total_processed_bytes = 0;
    const size_t print_progress_interval = 1 << 30;
  
  
    page_stats_t total_page_stats = {0};
    for( size_t sys_ram_idx = 0; sys_ram_idx < sys_ram_len; sys_ram_idx++) {
        mem_range_t* mr = sys_ram+sys_ram_idx;
        printf("[%ju/%ju[: Searching mem range %s [0x%jx,0x%jx[ (%.2f GiB)\n",
        sys_ram_idx, sys_ram_len, mr->name, mr->start, mr->end, (double)(mr->end - mr->start)/(1<<30));
        page_stats_t local_page_stats = {0};
        struct pamemcpy_cfg memcpy_cfg = {
            .out_stats = {0},
            .err_on_access_fail = true,
            .access_reserved = access_reserved,
            .flush_method = FM_CLFLUSH,
        };
        uint64_t aligned_start = mr->start;
        if((aligned_start % 4096) != 0) {
            aligned_start += (4096 - (mr->start % 4096));
        }
        printf("aligned_start = 0x%jx\n", aligned_start);
        for( uint64_t alias_candidate_pa = aligned_start; alias_candidate_pa < mr->end; alias_candidate_pa += 4096) {
            //printf("doing pa = 0x%jx\n", target_pa);
            if( (total_processed_bytes % print_progress_interval) == print_progress_interval) {
                printf("Total progress %0.2f\n", (double)total_processed_bytes/total_memory_bytes);
            }
            total_processed_bytes += 4096;
            if( alias_candidate_pa == source_pa) {
            continue;
            }

            //write m1 to source_pa
            if( clflush_range(alias_candidate_pa, msg_len, &local_page_stats, true) ) {
                err_log("flush_range for 0x%jx failed\n", alias_candidate_pa)
                //return -1;
                continue;
            }
            if( memcpy_topa_ext(source_pa, m1, msg_len, &memcpy_cfg) ) {
                err_log("memcpy_topa for 0x%jx failed\n", source_pa);
                //return -1;
                continue;
            }

            if( memcpy_frompa_ext(buf1, alias_candidate_pa, msg_len, &memcpy_cfg) ) {
                err_log("memcpy_frompa for 0x%jx failed\n", alias_candidate_pa);
                //return -1;
                continue;
            }
            if( clflush_range(alias_candidate_pa, msg_len, &local_page_stats, true) ) {
                err_log("flush_range for target_pa 0x%jx failed\n", alias_candidate_pa);
                //return -1;
                continue;
            }


            //write m2 to source_pa
            if( memcpy_topa_ext(source_pa, m2, msg_len, &memcpy_cfg) ) {
                err_log("memcpy_topa for source_pa 0x%jx failed\n", source_pa);
                //return -1;
                continue;
            }
            if( clflush_range(source_pa, msg_len, &local_page_stats, true) ) {
                err_log("flush_range for source_pa 0x%jx failed\n", source_pa);
                //return -1;
                continue;
            }

            //read from target_pa
            if( memcpy_frompa_ext(buf2, alias_candidate_pa, msg_len, &memcpy_cfg) ) {
                err_log("memcpy_frompa for target_pa 0x%jx failed\n", alias_candidate_pa);
                continue;
            }

            /*
            * We wrote  m1 and m2 to source_pa and read them through alias_candidate_pa
            * buf1 and buf2 contain the values read through surce_pa.
            * To account for memory scrambling, we dont compare them directly but check if
            * buf1^buf2 matches m1^m2
            */


#ifdef FIND_ALIAS_DEBUG
        printf("alias_candidate: 0x%jx\n", alias_candidate_pa);
        printf("m1: ");
        hexdump(m1, msg_len);
        printf("buf1: ");
        hexdump(buf1, msg_len);

        printf("m2: ");
        hexdump(m2, msg_len);
        printf("buf2: ");
        hexdump(buf2, msg_len);

        printf("m1 ^ m2: ");
        hexdump(mxor, msg_len);
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
            hexdump(bufxor, msg_len);
#endif

            if( found_matching_xor ) {
                uint64_t pa_xor = source_pa ^ alias_candidate_pa;
                printf("Found alias for 0x%jx at 0x%jx! xor diff = 0x%jx\n", source_pa, alias_candidate_pa, pa_xor);
                printf("buf1: ");
                hexdump(buf1, msg_len);
                printf("buf2: ");
                hexdump(buf2, msg_len);
                 printf("got xor: ");
                hexdump(mxor, msg_len);
                printf("want xor: ");
                hexdump(mxor, msg_len);

                *out_alias = alias_candidate_pa;
                printf("Reserved Page Errors: %ju, Map Failed Errors: %ju\n",
                    total_page_stats.reserved_pages, total_page_stats.map_failed);
                return 0;
            }
        }
        printf("Reserved Page Errors: %ju, Map Failed Errors: %ju\n",
            local_page_stats.reserved_pages, local_page_stats.map_failed);
        total_page_stats.reserved_pages += local_page_stats.reserved_pages;
        total_page_stats.map_failed += local_page_stats.map_failed;
    }
  
    printf("Reserved Page Errors: %ju, Map Failed Errors: %ju\n",
        total_page_stats.reserved_pages, total_page_stats.map_failed);
    //if we reach the end of this function, we have not found an alias. Thus, return error_code
    return -1;

}

/**
 * @brief Helper functions that returns true if `addr` is part of
 * one of the given memory ranges
 * 
 * @param addr 
 * @param mem_ranges 
 * @param mem_ranges_len 
 * @return true if `addr` is part of one of the given memory ranges
 * @return false  else
 */
bool is_in_mem_ranges(uint64_t addr, mem_range_t* mem_ranges, size_t mem_ranges_len) {
    for (size_t i = 0; i < mem_ranges_len; i++) {
        if( addr >= mem_ranges[i].start && addr < mem_ranges[i].end) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Find address in `mr` that is 4096 byte aligned and accessible
 * @param mr : memory range to search
 * @param access_reserved : If true, we try to access reserved memory ranges. Might lead to crashes
 * @param out_pa : ouput param, filled with found pa
 * @return 0 on success
 */
static int find_accessible_pa_in_mem_range(mem_range_t* mr, uint64_t* out_pa, bool access_reserved) {
    uint64_t aligned_start = mr->start + (4096 - (mr->start % 4096));
    struct pamemcpy_cfg cfg = {
        .access_reserved = access_reserved,
        .err_on_access_fail = true,
        .flush_method = FM_NONE,
        .out_stats = {0},
    };
    for( uint64_t candidate = aligned_start; candidate < mr->end; candidate += 4096) {
        char buf[64];
        if( memcpy_frompa_ext(buf, candidate, sizeof(buf), &cfg)) {
            continue;
        }
        *out_pa = candidate;
        return 0;
    }
    return -1;
}


/**
 * @brief Parse each line from `file_path` as uint64_t. Number base is choosen by prefix. 
 * @param file_path : path to input file. One number per line
 * @param out_source_pa : Output param. Filled with callee allocated array holding the parsed numbers
 * @param out_source_pa_len: Output param. Filled with length of `out_source_pa`
 * @return 0 on success
 * 
*/
int parse_source_pa_from_csv(char* file_path, uint64_t** out_source_pa, size_t* out_source_pa_len) {

    FILE* csv_file = fopen(file_path, "r");
    if( !csv_file ) {
        err_log("failed to open %s : %s", file_path, strerror(errno));
        goto error;
    }

    char line_buf[256];
    *out_source_pa_len = 0;
    while( fgets(line_buf, sizeof(line_buf), csv_file) != NULL ) {
        uint64_t tmp;
        if( do_stroul(line_buf, 0 , &tmp )) {
            continue;
        }
        *out_source_pa_len += 1;
    }
    if( ferror(csv_file)) {
        err_log( "error reading from %s : %s", file_path, strerror(errno));
        goto error;
    }
    if( fseek(csv_file, 0, SEEK_SET)) {
        err_log("failed to reset file position to 0\n");
        goto error;
    }

    *out_source_pa = malloc(sizeof(uint64_t) * (*out_source_pa_len) );
    size_t next_idx = 0;
    while( fgets(line_buf, sizeof(line_buf), csv_file) != NULL ) {
        if( do_stroul(line_buf, 0 , *out_source_pa+next_idx)) {
            err_log("failed to parse '%s' to uint64_t : %s\n", line_buf, strerror(errno));
            goto error;
        }
        next_idx += 1;
    }
    if( ferror(csv_file)) {
        err_log( "error reading from %s : %s", file_path, strerror(errno));
        goto error;
    }    
    
    int ret = 0;
    goto cleanup;
error:
    ret = -1;
cleanup:
    if( csv_file ) {
        fclose(csv_file);
    }
    return ret;
}

/**
 * @brief Find memory range for the given physical address `pa`
 * @returns -1 on error, idx of memory range for pa on success
*/
static int pa_to_mr(uint64_t pa, mem_range_t* mr, size_t mr_len) {
    for(size_t i =0; i < mr_len; i++) {
        if( (pa >= mr[i].start) && (pa < mr[i].end)) {
            return i;
        }
    }
    return -1;
}

struct cli_flags {
    //Optional arg to read source_pa from file
    char* source_pa_path;
    //Optional arg to read memory ranges considered for alias search from file
    char* memrange_path;
    //If true, access reserved ranges anyways and don't filter them from
    //the mem ranges
    bool access_reserved;
    //If true, use the more performant alias check that only works with
    //scrambling disabled
    bool no_scrambling;
    //write output to this path
    char* output_path;
};

int parse_cli_flags(int argc, char** argv, struct cli_flags* out_cli_flags) {
    const char* verb_find_source_pa_arg = "--source-pa-file";
    const char* verb_find_no_scrambling_flag = "--no-scrambling";
    const char* verb_find_memrange_arg = "--mem-range-file";
    const char* common_output_path = "--out";
    const char* common_access_reserved_flag = "--access-reserved";
    int idx = 0;
    out_cli_flags->output_path = "aliases.csv";
    while( idx < argc ) {
        if( 0 == memcmp(common_access_reserved_flag, argv[idx], strlen(common_access_reserved_flag))) {
            out_cli_flags->access_reserved = true;
            idx += 1;
        } else if( 0 == memcmp(verb_find_source_pa_arg, argv[idx], strlen(verb_find_source_pa_arg))) {
            if( (idx+1) >= argc ) {
                printf("Missing value for \"%s\"\n", verb_find_source_pa_arg);
                return -1;
            }
            out_cli_flags->source_pa_path = argv[idx+1];
            idx += 2;
        } else if(0 == memcmp(common_output_path, argv[idx], strlen(common_output_path))) {
            if( (idx+1) >= argc ) {
                printf("Missing value for \"%s\"\n", verb_find_source_pa_arg);
                return -1;
            }
            out_cli_flags->output_path = argv[idx+1];
            idx += 2;
        } else if( 0 == memcmp(verb_find_no_scrambling_flag, argv[idx], strlen(verb_find_no_scrambling_flag))) {
            out_cli_flags->no_scrambling = true;
            idx += 1;
        } else if(0 == memcmp( verb_find_memrange_arg, argv[idx], strlen(common_output_path))) {
            if( (idx+1) >= argc ) {
                printf("Missing value for \"%s\"\n", verb_find_source_pa_arg);
                return -1;
            }
            out_cli_flags->memrange_path = argv[idx+1];
            idx += 2;
        } else {
          idx += 1;  
        }
    }
    return 0;
}

enum main_mode {
    MODE_REPORT,
    MODE_FIND,
};

void print_usage(void) {
    printf("Usage: either supply <report> or <find> as first argument to select the main operation mode\n");
    printf("Common Options:\n");
    printf("\t--access-reserved : include truly reserved memory ranges (may lead to crashes, but may be required for RMP memory range)\n");
    printf("\n\"find\" specific options\n");
    printf("\t--out <FILE>: Default=aliases.csv : CSV file where the found aliases are stored\n");
    printf("\t--no-scrambling : Use more efficient alias test that only works if memory scrambling is disabled\n");
    printf("\t--source-pa-file <FILE> : Optional. Only search aliases for these PAs\n");
    printf("\t--mem-range-file <FILE> : Optional. Only consider these memory ranges when searching aliases.");


    
    printf("\n\"report\" specific options\n");
    printf("\t--out <FILE> : Optional, store memranges for usage with \"find\" command\n");
    
    printf("\t`find` takes an optional `--input` arg to pass in a file with one source_pa per line. We search an alias for each source_pa\n");
}


/**
 * @brief parse proc iomem and print some basic stats
 * @returns 0 on success
*/
int parse_and_print_memory_layout(struct mem_layout* out_mem_layout, bool access_reserved) {
    mem_range_t* mem_ranges = NULL;
    size_t mem_ranges_len;
    mem_range_t* alias_candidate_ranges = NULL;

    //parse list of all memory ranges from /proc/iomem
    if( parse_mem_layout(&mem_ranges, &mem_ranges_len)) {
        err_log( "parse_mem_layout failed\n");
        return -1;
    }


    if(wbinvd_ac()) {
        printf("wbinvd failed\n");
        goto error;
    }
    //size of the regular RAM memory in bytes
    size_t system_ram_bytes = 0;

    //Filter mem ranges with wrong type or only unaccesible addresses
    //mem_ranges_len is upper bound for size, we cut down to actually number of used entries afterwards
    alias_candidate_ranges = malloc(sizeof(mem_range_t) * mem_ranges_len);
    size_t alias_candidate_ranges_next_idx = 0;
    for(size_t i = 0; i < mem_ranges_len; i++) {
        mem_range_t* m = mem_ranges+i;
        uint64_t accessible_pa_in_mr;
        if( (m->mt != MT_SYSTEM_RAM) && (m->mt != MT_RESERVED)) {
            continue;
        }
        //ignore memory ranges without any accessible physical addresses.
        //These are reserved memory range where the reserved is not due to 
        //our memmap kern param. However, on some systems the RMP was in such memory ranges. Thus access_reserved allows
        //to overwrite this
        if( find_accessible_pa_in_mem_range(m, &accessible_pa_in_mr, access_reserved )) {
            continue;
        }
        system_ram_bytes += m->end - m->start;
        printf("Start=0x%09jx, End=0x%09jx Length=0x%09jx, Name=%s\n", m->start, m->end, m->end - m->start,  m->name);
        printf("Start=0x%09jx, End=0x%09jx Length=0x%09jx (%0.2f GiB), Name=%s\n",
            m->start, m->end, m->end - m->start, ((double)m->end - m->start)/(1<<30),  m->name);
        alias_candidate_ranges[alias_candidate_ranges_next_idx] = *m;
        alias_candidate_ranges_next_idx += 1;
    }
    printf("Have %.2f GiB of system ram sections in iomem\n", (double)system_ram_bytes/(1<<30));
    //cut down to actually used size
    size_t alias_candidate_ranges_len = alias_candidate_ranges_next_idx;
    {
        mem_range_t* tmp = malloc(sizeof(mem_range_t) * alias_candidate_ranges_len);
        memcpy(tmp, alias_candidate_ranges, sizeof(mem_range_t) * alias_candidate_ranges_len);
        free(alias_candidate_ranges);
        alias_candidate_ranges = tmp;
    }
    int ret = 0;
    out_mem_layout->mem_ranges = mem_ranges;
    out_mem_layout->mem_ranges_len = mem_ranges_len;
    out_mem_layout->filtered_ranges = alias_candidate_ranges;
    out_mem_layout->filtered_ranges_len = alias_candidate_ranges_len;
    goto cleanup;
error:
    ret = -1;
    if( mem_ranges ) {
        free(mem_ranges);
    }
    if( alias_candidate_ranges ) {
        free(alias_candidate_ranges);
    }
cleanup:
    return ret;
}


int select_candidates_for_mem_range(struct mem_layout mem_layout, struct cli_flags flags, struct mem_range_pa** out_mem_range_pas, size_t* out_len) {
    size_t source_pa_len;
    uint64_t* source_pa = NULL;
    mem_range_t* source_pa_mem_ranges = NULL;
    mem_range_t* alias_candidate_ranges = mem_layout.filtered_ranges;
    size_t alias_candidate_ranges_len = mem_layout.filtered_ranges_len;

    /*Either parse pre-configured mem range + candidate list from file, or search one candidate for each
     * memory range
    */
    if( flags.source_pa_path != NULL ) {
        printf("Parsing source_pa from input file\n");
        if( parse_source_pa_from_csv(flags.source_pa_path, &source_pa, &source_pa_len )) {
            err_log("failed to parse input file\n");
            goto error;
        }
        source_pa_mem_ranges = malloc(sizeof(mem_range_t) * source_pa_len); 
        printf("Parsed source_pa values:\n");
        for(size_t i = 0; i < source_pa_len; i++) {
            int mr_idx = pa_to_mr(source_pa[i], alias_candidate_ranges, alias_candidate_ranges_len );
            if( -1 == mr_idx ) {
                printf("0x%09jx does not belong to any known memory range\n", source_pa[i]);
                goto error;
            }
            source_pa_mem_ranges[i] = alias_candidate_ranges[i];
            printf("\t0x%09jx\n", source_pa[i]);
        }
    } else {
        printf("Selecting one source_pa per memory region\n");
        //addresses for which we want to find aliases. By default we search alias for one address
        //from each alias candiate memory range. This should give us sufficient confidence to find all 
        //alias masks
        printf("Searching alias for the following addrs:\n");
        source_pa_len = alias_candidate_ranges_len;
        printf("source_pa_len=%ju\n",source_pa_len);
        source_pa = malloc(sizeof(uint64_t) * source_pa_len);
        //stores the mem_range_t for the corresponding source_pa entry
        source_pa_mem_ranges = malloc(sizeof(mem_range_t) * source_pa_len);
        size_t next_idx = 0;
        for (size_t i = 0; i < alias_candidate_ranges_len ; i++) {
            mem_range_t* m = alias_candidate_ranges+i;
            if( m->mt != MT_SYSTEM_RAM) {
                continue;
            }
            uint64_t pa;
            if( find_accessible_pa_in_mem_range(m, &pa, flags.access_reserved )) {
                printf("Did not find accessible pa in mem range. We have already checked for this, so it should never happen here!\n");
                goto error;
            }
            source_pa[next_idx] = pa;
            source_pa_mem_ranges[next_idx] = *m;
            if( (source_pa[next_idx] % 64) != 0) {
                //align to next cacheline
                source_pa[next_idx] = (source_pa[next_idx] + 64 ) & (~0x3fULL);
            }
            printf("\tFor mem range [0x%jx,0x%jx[ search alias for 0x%09jx\n", m->start, m->end, source_pa[next_idx]);
            next_idx += 1;
        }
        //shrink source_pa to actually used size
        source_pa_len = next_idx;
        source_pa = realloc(source_pa, sizeof(uint64_t) * source_pa_len);
        source_pa_mem_ranges = realloc(source_pa_mem_ranges, sizeof(mem_range_t) * source_pa_len);
    }


    //Check that each entry in source_pa is accessible
    {
        const size_t buf_len = 64;
        char buf[64];
        for(size_t i = 0; i < source_pa_len; i++) {
            page_stats_t stats;
            if( memcpy_frompa(buf, source_pa[i], buf_len, &stats , true)) {
                printf("Failed to access source_pa 0x%09jx", source_pa[i]);
                int mr_idx = pa_to_mr(source_pa[i], mem_layout.mem_ranges, mem_layout.mem_ranges_len);
                if( mr_idx == -1) {
                    printf("\n\tFailed to get mem range for this pa. This should not happen\n");
                } else {
                    mem_range_t* mr = mem_layout.mem_ranges+mr_idx;
                    printf("\n\tMemRange: start=0x%09jx end=0x%09jx name=%s\n", mr->start, mr->end, mr->name);
                }
                goto error;
            }
        }
    }

    int ret = 0;
    *out_mem_range_pas = malloc(sizeof(struct mem_range_pa) * source_pa_len);
    for(size_t i = 0; i < source_pa_len; i++ ) {
        (*out_mem_range_pas)[i].pa = source_pa[i];
        (*out_mem_range_pas)[i].mr = source_pa_mem_ranges[i];
    }
    *out_len = source_pa_len;
    goto cleanup;
error:
    ret = - 1;
    if(source_pa) free(source_pa);
    if(source_pa_mem_ranges) free(source_pa_mem_ranges);
cleanup:
    return ret;
}


int mode_report(struct cli_flags flags) {
    struct mem_layout mem_layout = {0};
    if(parse_and_print_memory_layout(&mem_layout, flags.access_reserved)) {
        err_log("determine_candidate_me_ranges failed\n");
        return -1;
    }

    if(!flags.output_path) {
        printf("Done!\n. If you want to save the output for usage with the find command, supply the --out <FILE> flag.");
        return 0;
    }

    //write to output file
    printf("Writing memranges to \"%s\"\n", flags.output_path);
    FILE* out_file = fopen(flags.output_path, "w");
    if(!out_file) {
        err_log("failed to create output file at \"%s\" : %s",flags.output_path, strerror(errno));
        return -1;
    }
    if( fprintf(out_file,"#(inclusive) start PA,(exclusive) end PA\n" ) < 0 ) {
        err_log("failed to write csv header : %s\n", strerror(errno));
        fclose(out_file);
        return -1;
    }
    for( size_t i = 0; i < mem_layout.filtered_ranges_len; i++ ) {
        if( fprintf(out_file, "0x%09jx,0x%09jx\n",mem_layout.filtered_ranges[i].start, mem_layout.filtered_ranges[i].end) < 0 ) {
            err_log("failed to write memrange entry at idx %ju : %s\n", i, strerror(errno));
            fclose(out_file);
        }
    }
    fclose(out_file);
    return 0;
}

/**
 @brief Parse user specified memory ranges from the given file
 Format: Optional csv header row (starting with #). Afterwards each row is parsed as
 "0x%jx,0x%jx" where the first entry is the start address and the second entry is the end
 address of the memory range
 @param path : file path
 @param ml : Output parameter. Filled with the parsed data. Caller needs to free
 @returns : 0 On success
*/
static int parse_memranges_from_file(char* path, struct mem_layout* ml) {
    FILE* in_file = NULL;
    mem_range_t* mr = NULL;
    char* buf = NULL;
    size_t idx;
    int ret;


    in_file = fopen(path, "r");
    if(!in_file) {
        err_log("failed to open memrange file \"%s\" : %s ", path, strerror(errno));
        return -1;
    }

    //iterate over file to get number of entries. Afterwards alloc result
    //array and iterate over file again
    size_t len = 0;
    uint64_t dummy;
    size_t buf_bytes = 512;
    fpos_t payload_file_offset;
    buf = (char*)malloc(buf_bytes);
    if( fgets(buf, buf_bytes, in_file) == NULL ) {
        err_log("Failed to read header row : %s\n", strerror(errno));
        goto error;
    }
    if( ferror(in_file) ) {
        err_log("Error reading from %s : %s\n", path, strerror(errno));
        goto error;
    }
    //if first entry was not header row but already a real entry, reset file position
    if(buf[0] == '0' && buf[1] == 'x') {
        if(fseek(in_file, 0 , SEEK_SET )) {
            err_log("fseek failed : %s\n", strerror(errno));
            goto error;
        }
    }
    if( fgetpos(in_file, &payload_file_offset)) {
        err_log("fgetpos failed : %s\n", strerror(errno));
        goto error;
    }
    while(  2 == fscanf(in_file, "0x%jx,0x%jx\n", &dummy, &dummy )) {
        len += 1;
    }
    if( ferror(in_file) ) {
        err_log("Error reading from %s : %s\n", path, strerror(errno));
        goto error;
    }
    if( fseek(in_file, 0, SEEK_SET)) {
        err_log("Error resetting file position : %s\n", strerror(errno));
        goto error;
    }
    if( len < 1 ) {
        err_log("Input file %s does not contain any entries\n", path);
        goto error;
    }



    mr = (mem_range_t*)malloc(sizeof(mem_range_t) * len);
    idx = 0;
    if(fsetpos(in_file, &payload_file_offset)) {
        err_log("fsetpos failed\n");
        goto error;
    }
    while( 2 == fscanf(in_file, "0x%jx,0x%jx\n", &(mr[idx].start), &(mr[idx].end))){
        char* dummy_name = "Not restored by parser\n";
        memcpy(mr[idx].name, dummy_name, strlen(dummy_name));
        idx += 1;
    }
    if( ferror(in_file) ) {
        err_log("Error reading from %s : %s\n", path, strerror(errno));
        goto error;
    }
    if( idx != len ) {
        err_log("Expected to read %ju entries but got %ju\n", len, idx);
        goto error;
    }

    ml->filtered_ranges_len = len;
    ml->filtered_ranges = malloc(sizeof(mem_range_t) * len);
    memcpy(ml->filtered_ranges, mr, sizeof(mem_range_t) * len);

    ml->mem_ranges_len = len;
    ml->mem_ranges = malloc(sizeof(mem_range_t) * len);
    memcpy(ml->mem_ranges, mr, sizeof(mem_range_t) * len);

    ret = 0;
    goto cleanup;
error:
    ret = -1;
cleanup:
    if(mr) free(mr);
    if(in_file) fclose(in_file);
    if(buf) free(buf);
    return ret;
}

int mode_find(struct cli_flags flags) {
    struct mem_layout mem_layout = {0};
    struct mem_range_pa* source_candidates = NULL;
    size_t source_candidates_len;
    
    //Either parse memranges from user supplied file or parse them from /proc/iomem
    if(flags.memrange_path) {
        if(parse_memranges_from_file(flags.memrange_path, &mem_layout )) {
            err_log("failed to parse memranges from file \"%s\"\n", flags.memrange_path);
            return -1;
        }
    } else {
        if(parse_and_print_memory_layout(&mem_layout, flags.access_reserved)) {
            err_log("determine_candidate_me_ranges failed\n");
            return -1;
        }
    }

    if(select_candidates_for_mem_range(mem_layout, flags, &source_candidates, &source_candidates_len)) {
        err_log("select_candidates_for_mem_range failed\n")
        goto error;
    }

    //Search alias for each source_pa
    uint64_t* alias_pa = calloc(sizeof(uint64_t), source_candidates_len);
    for( size_t i = 0; i < source_candidates_len; i++) {
        printf("[%ju/%ju[: Searching alias for 0x%jx\n", i, source_candidates_len, source_candidates[i].pa);
        //First check if any of the already found alias shifts work
        bool existing_alias_worked = false;
        for(size_t j = 0; j < i; j++) {
            //invalid value
            if( alias_pa[j] == 0) {
                continue;
            }

            uint64_t alias_mask = source_candidates[j].pa ^ alias_pa[j];
            uint64_t alias_candidate = source_candidates[i].pa ^ alias_mask;
            struct pamemcpy_cfg cfg = {
                .access_reserved = flags.access_reserved,
                .err_on_access_fail = true,                
                .flush_method = FM_CLFLUSH,
                .out_stats = {0},
            };
            if( check_alias(source_candidates[i].pa, alias_candidate, &cfg, true)) {
                continue;
            }
            existing_alias_worked = true;
            alias_pa[i] = alias_candidate;
            break;
        }

        if( existing_alias_worked ) {
            printf("Found alias using existing alias mask 0x%09jx\n", source_candidates[i].pa ^ alias_pa[i]);
            continue;
        }

        //Otherwise sweep memory range
        if( flags.no_scrambling ) {
            if(find_alias_no_scrambling(source_candidates[i].pa, alias_pa+i, mem_layout.filtered_ranges, mem_layout.filtered_ranges_len,flags.access_reserved)) {
                err_log( "find_alias_no_scrambling for 0x%jx failed\n", source_candidates[i].pa);
            }
        } else {   
            if(find_alias_scrambling(source_candidates[i].pa, alias_pa+i, mem_layout.filtered_ranges, mem_layout.filtered_ranges_len,flags.access_reserved)) {
                err_log( "find_alias_scrambling for 0x%jx failed\n", source_candidates[i].pa);
            }
        }
    }

    printf("Result\n");
    for( size_t i = 0;i < source_candidates_len; i++) {
        struct mem_range_pa* sc = source_candidates+i;
        if( alias_pa[i] != 0 ) {
            printf("Mem Range{.start=0x%09jx, .end=0x%09jx} : orig=0x%09jx alias=0x%09jx xor=0x%09jx\n",
            sc->mr.start, sc->mr.end, sc->pa, alias_pa[i], sc->pa ^ alias_pa[i]);
        } else {
            printf("orig=0x%09jx alias=Not found. MemRange{ start=0x%jx end=0x%jx name=%s }\n",
                sc->pa, sc->mr.start, sc->mr.end, sc->mr.name);
        }
    }

    printf("Writing aliases to: %s", flags.output_path);
    if( store_valid_aliases(flags.output_path,source_candidates, alias_pa, source_candidates_len) ) {
        printf("Failed to store alias to file\n");
        goto error;        
    }
    free(alias_pa);


    int ret = 0;
    goto cleanup;
error:
    perror("Error in main. Last errno is: ");
    ret = -1;
cleanup:
    free_mem_layout(mem_layout);
    if(source_candidates) free(source_candidates);
    close_kmod();
    return ret; 
}

int run(enum main_mode mode, struct cli_flags flags ) {
    int ret = 0;
    if (open_kmod()) {
        printf( "failed to communicate with kernel driver : %s\n", strerror(errno));
        goto error;
    }

    switch (mode) {
        case MODE_REPORT:
            ret = mode_report(flags);
            break;
        case MODE_FIND:
            ret = mode_find(flags);
            break;
        default:
            err_log("Unknown mode %d\n", mode);
            goto error;
    }

    goto cleanup;
error:
    ret = -1;
cleanup:
    close_kmod();
    return ret;
}

int main(int argc, char** argv) {
    const char* verb_report_only = "report";
    const char* verb_find = "find";
    if( argc < 2 ) {
        print_usage();
        return -1;
    }
    enum main_mode main_mode;

    if(0 == memcmp((char*)verb_find, argv[1], strlen(verb_find))) {
        main_mode = MODE_FIND;
    }else if(0 == memcmp(verb_report_only, argv[1], strlen(verb_report_only))) {
        main_mode = MODE_REPORT;
    }else {
        print_usage();
        return -1;
    }
    struct cli_flags cli_flags = {0};
    if( parse_cli_flags(argc, argv, &cli_flags )) {
        err_log("Failed to parse cli args\n");
        print_usage();
        return -1;
    }

    if( run(main_mode, cli_flags) ) {
        err_log("main run function failed\n");
        return -1;
    }
    return 0;
}
