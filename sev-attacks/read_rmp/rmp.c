#include "rmp.h"

void dump_rmp_entry(char* prefix, rmp_entry_t e, FILE* stream) {
  uint64_t gpa = e.info.gpa;
  fprintf(stream, "%s assigned=%d pagesize=%d immutable=%d, gpa=0x%jx asid=%d vmsa=%d validated=%d\n",
    prefix,
    e.info.assigned,
    e.info.pagesize,
    e.info.immutable,
    gpa,
    e.info.asid,
    e.info.vmsa,
    e.info.validated
  );
}

void dump_rmp(rmp_entry_t* rmp, size_t len, FILE* stream) {
	
  printf("Printing rmp entries\n");
  for(size_t idx = 0; idx < len; idx++) {
    uint64_t gpa = rmp[idx].info.gpa;
    fprintf(stream, "Entry at offset idx 0x%05ju for hpa 0x%jx : assigned=%d pagesize=%d immutable=%d, gpa=0x%09jx asid=%d vmsa=%d validated=%d\n",
      idx,
      idx * 4096,      
      rmp[idx].info.assigned,
      rmp[idx].info.pagesize,
      rmp[idx].info.immutable,
      gpa,
      rmp[idx].info.asid,
      rmp[idx].info.vmsa,
      rmp[idx].info.validated
    );
	}
}

bool rmp_entries_eq(rmp_entry_t a, rmp_entry_t b) {
  bool eq = a.info.assigned  == b.info.assigned  &&
    a.info.pagesize  == b.info.pagesize  &&
    a.info.immutable == b.info.immutable &&
    a.info.rsvd1     == b.info.rsvd1     &&
    a.info.asid      == b.info.asid      &&
    a.info.vmsa      == b.info.vmsa      &&
    a.info.validated == b.info.validated &&
    a.info.gpa       == b.info.gpa       &&
    a.info.rsvd2     == b.info.rsvd2;
  return eq;
}

bool rmp_entries_eq_but_gpa(rmp_entry_t old, rmp_entry_t new, uint64_t new_gpa) {
  bool eq = old.info.assigned  == new.info.assigned  &&
    old.info.pagesize  == new.info.pagesize  &&
    old.info.immutable == new.info.immutable &&
    old.info.rsvd1     == new.info.rsvd1     &&
    old.info.asid      == new.info.asid      &&
    old.info.vmsa      == new.info.vmsa      &&
    old.info.validated == new.info.validated &&
    old.info.rsvd2     == new.info.rsvd2;
  bool gpa_neq = old.info.gpa != new.info.gpa;
  bool new_exp_gpa = new.info.gpa == new_gpa;
  return eq && gpa_neq && new_exp_gpa;
}
