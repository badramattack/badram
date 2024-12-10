#ifndef QEMU_GPA2HPA
#define QEMU_GPA2HPA


/**
 * @brief Use the QEMU QMP interface to translate a GPA to a HPA
 * @brief gpa: for which we wan the hpa
 * @qmp_ip: ip on which the QEMU QMP interface is listening
 * @port: on which the QEMU QMP interface is listening
 * @out_hpa: output param. Filled with the hpa corresponding to `gpa`
 * @return 0 on success
*/
int qemu_gpa_to_hpa(uint64_t gpa, char* qmp_ip, uint16_t port, uint64_t* out_hpa);

#endif
