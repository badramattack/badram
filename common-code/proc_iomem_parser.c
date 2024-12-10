#include "include/proc_iomem_parser.h"
#include "include/helpers.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

int _regexp_matches_to_mem_range(regmatch_t* matches, size_t matches_len, char* input_string, mem_range_t* result) {
    if( matches_len != 4 ) {
        err_log( "expected 4 matches but got %lu\n", matches_len);
        return -1;
    }

    for( size_t i = 0; i < matches_len; i++) {
        if( matches[i].rm_so == -1 ) {
            err_log( "entry for match %lu is invalid\n", i);
            return -1;
        }
        regmatch_t* m = &matches[i];

        switch (i) {
            case 0:
            break; //nothing to do here
            case 1: //fallthtrough
            case 2: {
                //do hex string to uint64_t
                uint64_t v;
                if( do_stroul(input_string + m->rm_so, 16, &v)) {
                    err_log( "do_stroul failed on match group %lu\n", i);
                }
                if( i == 1) {
                    result->start = v;
                } else {
                    result->end = v;
                }
            }
            break;
            case 3: {
                size_t match_len = m->rm_eo - m->rm_so - 1;
                if( input_string[m->rm_eo] == '\n') {
                    match_len -= 1;
                }
                if( match_len > sizeof(result->name) ) {
                    err_log( "match to large: %ju\n", match_len);
                    return -1;
                }
                strncpy(result->name, input_string + m->rm_so, match_len);
                result->name[match_len] = '\0';

                if( 0 == strcmp("System RAM", result->name)) {
										result->mt = MT_SYSTEM_RAM;
                } else if( 0 == strcmp("Reserved", result->name)) {
										result->mt = MT_RESERVED;
								} else {
										result->mt = MT_OTHER;
								}
            }
            break;
            default:
                err_log( "unexpected entry index\n");
                return -1;
        }
    }

    return 0;
}


int parse_mem_layout(mem_range_t** ranges, size_t* range_len) {
    int ret = 0;
    const char* iomem_path = "/proc/iomem";
    printf("Opening iomem file at %s\n", iomem_path);
    FILE* iomem = fopen(iomem_path, "r");
    if( iomem == NULL ) {
        err_log( "failed to open %s : %s", iomem_path, strerror(errno));
        goto error;

    }

    char line_buf[256];
    regex_t re;
    regmatch_t groups[4];
    size_t groups_len = sizeof(groups)/sizeof(*groups);

    //iterate over file once to get max number of entries
    size_t results_buf_len = 0;
    while( fgets(line_buf, sizeof(line_buf), iomem) != NULL ) {
        results_buf_len += 1;
    }
    if( ferror(iomem)) {
        err_log( "error reading from %s : %s", iomem_path, strerror(errno));
        goto error;
    }
    if( fseek(iomem, 0, SEEK_SET)) {
        err_log( "failed to reset stream to start\n");
        goto error;
    }

    //now we know the max number of entries -> alloc result array
    mem_range_t* results_buf = malloc(sizeof(mem_range_t) * results_buf_len);
    size_t results_buf_next_idx = 0;


    //main loop over each line
    if( regcomp(&re, "^([0-9a-f]+)-([0-9a-f]+) : (.*)$", REG_EXTENDED)) {
        err_log( "failed to parse static regexp, this should never happend\n");
        goto error;
    }
    while( fgets(line_buf, sizeof(line_buf), iomem) != NULL ) {

        //in either of these cases our regexp did not match and we ignore the line
        if( regexec(&re, line_buf, groups_len, groups, 0) ) {
            continue;
        }
        if( groups[0].rm_so == -1 ) {
            continue;
        }
        
        //regeexp matched, extract information
        if( _regexp_matches_to_mem_range(groups, groups_len, line_buf, results_buf + results_buf_next_idx)) {
            err_log( "regexp_matches_to_mem_range failed\n");
            goto error;
        }
        results_buf_next_idx += 1;
    }
    //check if we left the loop due to I/O error
    if( ferror(iomem)) {
        err_log( "error reading from %s : %s", iomem_path, strerror(errno));
        goto error;
    }

    //we overapproximated the size, resize to only hold the actually used elements
    if( results_buf_next_idx != results_buf_len ) {
        *ranges = malloc(sizeof(mem_range_t) * results_buf_next_idx);
        *range_len = results_buf_next_idx;
        memcpy(*ranges, results_buf, results_buf_next_idx * sizeof(mem_range_t));
        free(results_buf);
    }


    goto cleanup;
    error:
        ret = -1;
    cleanup:
    if(iomem) {
        fclose(iomem);
    }
    return ret;
}
