/**
* This program is intended to run inside the targeted VM to demonstrage the GPA remapping via RMP manipulation
* vunlnerability.
* Experiment:
* 1) Allocates two 2MB buffers A and B and fill them with different values. Print their GPAs and wait for the HV to do the remapping. The current backend seems to use 2MB pages by default. Thus A and B should be on different pages
* 2) After HV is done with remapping, print the values again. The content of A and B should now be swapped
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include <sys/mman.h>
#include <unistd.h>

#include "parse_pagemap.h"
#include "helpers.h"

#define MAP_HUGE_2MB    (21 << MAP_HUGE_SHIFT)

typedef struct
{
    char *name;
    uint64_t vaddr;
} code_gadget_t;



int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    //bytes in one megabyte
    const size_t MIB = 1 << 20;
    const size_t mem_buffer_bytes = 4 * MIB;
    uint8_t *mem_buffer = (uint8_t *)mmap(NULL, mem_buffer_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE | MAP_HUGETLB | MAP_HUGE_2MB, -1, 0);
    if (mem_buffer == MAP_FAILED)
    {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    if( ( (uint64_t)mem_buffer % 4096) !=0 ) {
        printf("mem_buffer not correctly aligned\n");
        exit(EXIT_FAILURE);
    }
    printf("Setting 2MB page A to 0 and 2 MB page B to 0xff\n");
    uint8_t* pA = mem_buffer;
    uint8_t* pB = mem_buffer + 2 * MIB;
    memset(pA, 0, 2 * MIB);
    memset(pB, 0xff, 2 * MIB);

    // print interesting code locations
    code_gadget_t gadgets[] = {
        {
            .name = "2MB page A (0)",
            .vaddr = (uint64_t)pA,
        },
        {
            .name = "2MB page B (0xff)",
            .vaddr = (uint64_t)pB,
        },

    };

    pid_t pid = getpid();
    for (size_t i = 0; i < sizeof(gadgets) / sizeof(gadgets[0]); i++)
    {
        code_gadget_t *g = gadgets + i;
        uint64_t paddr;
        if (virt_to_phys_user(&paddr, pid, g->vaddr))
        {
            printf("Failed to translate vaddr 0x%jx of gadget %s to paddr\n", g->vaddr, g->name);
            return -1;
        }
        printf("%s 0x%jx\n", g->name, paddr);
        printf("%s_vaddr 0x%jx\n", g->name, g->vaddr);
    }


    printf("Do the remapping, then press enter\n");
    getchar();

    printf("First 64 bytes of 2MB page A (should now be 0xff instead of 0x00) : \n");
    hexdump((uint8_t*)pA, 64);
    printf("First 64 bytes of 2MB page B (should now be 0x00 instead of 0xff) : \n");
    hexdump((uint8_t*)pB, 64);

    munmap(mem_buffer, mem_buffer_bytes);
    return 0;
}
