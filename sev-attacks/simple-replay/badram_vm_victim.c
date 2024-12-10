/**
* This program is intended to run inside the targeted VM. It will allocate a buffer and share its gpa on the cmline.
Then it will set the buffer to zero, wait for user input, set the buffer to 0xff, wait for user input and finally print
the buffer. This allows to easily showcase a memory replay.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include <sys/mman.h>
#include <unistd.h>

#include "parse_pagemap.h"

typedef struct
{
    char *name;
    uint64_t vaddr;
} code_gadget_t;

static void hexdump(uint8_t* a, const size_t n)
{
	for(size_t i = 0; i < n; i++) {
    if (a[i]) printf("\x1b[31m%02x \x1b[0m", a[i]);
    else printf("%02x ", a[i]);
    if (i % 64 == 63) printf("\n");
  }
	printf("\n");
}


int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    uint64_t *mem_buffer = (uint64_t *)mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    if (mem_buffer == MAP_FAILED)
    {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    printf("Setting mem_buffer to zero\n");
    memset(mem_buffer, 0, 4096);

    // print interesting code locations
    code_gadget_t gadgets[] = {
        {
            .name = "mem_buffer",
            .vaddr = (uint64_t)mem_buffer,
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


    printf("Capture the ciphertext, then press enter\n");
    getchar();
    printf("Setting buffer to 0xff\n");
    memset(mem_buffer, 0xff, 4096);
    printf("Replay the old ciphertext, then press enter\n");
    getchar();

    printf("First 64 bytes of mem_buffer");
    hexdump((uint8_t*)mem_buffer, 64);
}
