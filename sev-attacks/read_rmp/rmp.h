#ifndef RMP_H
#define RMP_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define OFFSET_RMP_ENTRIES  (16 * 1024)

/*
 * The RMP entry format is not architectural. The format is defined in PPR
 * Family 19h Model 01h, Rev B1 processor.
 *Use GNU GCC Bit Fields: https://www.gnu.org/software/c-intro-and-ref/manual/html_node/Bit-Fields.html
 */
typedef struct __packed {
        union {
                struct {
                        uint64_t
                                assigned        : 1,
                                pagesize        : 1,
                                immutable       : 1,
                                rsvd1           : 9,
                                gpa             : 39,
                                asid            : 10,
                                vmsa            : 1,
                                validated       : 1,
                                rsvd2           : 1;
                } info;
                uint64_t low;
        };
        uint64_t high;
} rmp_entry_t;

void dump_rmp_entry(char* prefix, rmp_entry_t e, FILE* stream);

/**
 * @brief Print rmp entries to given stream
*/
void dump_rmp(rmp_entry_t* rmp, size_t len,FILE* stream);

/**
 * @brief Check that `old` and `new` are equal for all fields but the gpa. Check that
 * `new` has the gpa `new_pa`
 * @returns true if all checks succeeded
*/
bool rmp_entries_eq_but_gpa(rmp_entry_t old, rmp_entry_t new, uint64_t new_gpa);


/**
 * @brief Check that `old` and `new` are equal for all fields.
 * @returns true if all check successful
*/
bool rmp_entries_eq(rmp_entry_t a, rmp_entry_t b);
#endif
