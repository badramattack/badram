#ifndef HELPERS_H
#define HELPERS_H

#include <stdio.h>
#include <stdint.h>

#include "proc_iomem_parser.h"

#define err_log(fmt, ...) fprintf(stderr, "%s:%d : " fmt, __FILE__, __LINE__, ##__VA_ARGS__);

/**
 * @brief Read len random bytes from urandom into p
*/
int get_rand_bytes(void *p, size_t len);

/**
 * @brief Print next n bytes of a
*/
void hexdump(uint8_t* a, const size_t n);

/**
 * @brief Wrapper with error handling around strtoul
 *
 * @param str string to parse as number
 * @param base as described in strtoul doc
 * @param result result param
 * @return 0 on success
 */
int do_stroul(char *str, int base, uint64_t *result);


/**
 * @brief Compute the alias for the given pa
 * @brief mrs: memory range with known alias masks
 * @brief alias_masks: value to xor to an addr from the corresponding memory range to get the aliased pa
 * @rief len: length of mrs and alias_masks (i.e. both have this length)
 * @brief out_alias: Output param, filled with the alias pa
 * @returns: 0 on success
*/
int get_alias(uint64_t pa, mem_range_t* mrs, uint64_t* alias_masks, size_t len, uint64_t* out_alias);

#endif
