#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>


#include "readalias_ioctls.h"

#define PAGE_SIZE  4096
#define PAGE_SHIFT 12

typedef struct  {
  size_t reserved_pages;
  size_t map_failed;
} page_stats_t;


/**
 * Open the kernel module
 * 
 * @returns Whether the kernel module was opened successfully.
 */
int open_kmod(void);

/**
 * Close the kernel module
 * 
 * @returns Whether the kernel module was closed successfully.
 */
void close_kmod(void);


struct pamemcpy_cfg {
 //Output parameter filled with information about mapping errors
 page_stats_t out_stats;
 //If true abort if there is an access errror
 bool err_on_access_fail;
 //If true, try to access reserved pages anyways
 bool access_reserved;
 //Control flushing method or disable flushing
 enum flush_method flush_method;
};

/**
 * Copy memory to physical address.
 * 
 * @param dst: The physical address of the destination.
 * @param src: The buffer to copy from.
 * @param count: The number of bytes to copy.
 * @param out_stats: Output param. Filled with information about mapping errors and reserved pages errors
 * @param err_on_access_fail: If true, we return -1 when there is an error accessing the page. Otherwise we
 * only docucment this in `out_stats`
 * 
 * @returns 0 on success
 */
int memcpy_topa(uint64_t dst, void* src, size_t count, page_stats_t* out_stats, bool err_on_access_fail);

/**
 *@brief Like memcpy_topa but with more options
 *@parameter cfg : control behaviour. Also contains some output paramters
 *@returns 0 on success
*/
int memcpy_topa_ext(uint64_t dst, void* src, size_t count, struct pamemcpy_cfg* cfg);

/**
 * Copy memory from physical address.
 * 
 * @param dst: The buffer to copy to.
 * @param src: The physical address to copy from.
 * @param out_stats: Informaiton about mapping and reserved pages errors
 * @param count: The number of bytes to copy.
 * @param out_stats: Output param. Filled with information about mapping errors and reserved pages errors
 * @param err_on_access_fail: If true, we return -1 when there is an error accessing the page. Otherwise we
 * only docucment this in `out_stats`

 * @returns 0 on success
 */
int memcpy_frompa(void* dst, uint64_t src, size_t count, page_stats_t* out_stats, bool err_on_access_fail);

/**
 *@brief Like memcpy_frompa but with more options
 *@parameter cfg : control behaviour. Also contains some output paramters
 *@returns 0 on success
*/
int memcpy_frompa_ext(void* dst, uint64_t src, size_t count, struct pamemcpy_cfg* cfg);

/**
 * Flush a given memory range from the cache. This function will flush at the
 * granularity of a page.
 * 
 * @param pa: The starting physical address to flush.
 * @param count: The number of bytes to flush.
 * @param out_stats: Output param. Filled with information about mapping errors and reserved pages errors
 * @param err_on_access_fail: If true, we return -1 when there is an error accessing the page. Otherwise we
 * only docucment this in `out_stats`
 * 
 * @returns 0 on success
 */
int clflush_range(uint64_t pa, size_t count, page_stats_t* out_stats, bool err_on_access_fail);

/**
 @brief Issue `wbinvd` instruction on all cpu cores
*/
int wbinvd_ac(void);


/**
 * @brief Flush the given memory range using the selected method
*/
int flush_ext(uint64_t pa, size_t count, struct pamemcpy_cfg* cfg);

//check alias failed to perform a memory access
//this might happend if the page is reserved
#define CHECK_ALIAS_ERR_ACCESS -1
//check alias could perform all memory accesses but alias_candidate was no alias
#define CHECK_ALIAS_ERR_NO_ALIAS -2

/**
 * @brief Check if `alis_candidate` is an alias for source_pa. Both addrs
 * must have at least 64 byte alignment
 * @param source_pa
 * @param alias_candidate
 * @param memcpy_cfg : config options for pa memcpy functions
 * @param verbose : if true, log more error information 
 * @return 0 on success, CHECK_ALIAS_ERR_ACCESS on access error, CHECK_ALIAS_ERR_NO_ALIAS if access succeeded but the candidate is no alias
*/
int check_alias(uint64_t source_pa, uint64_t alias_candidate, struct pamemcpy_cfg* memcpy_cfg,bool verbose);
