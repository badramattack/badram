char __attribute__((aligned(0x1000))) buffer[4096];

static void flush(void *p)
{
	  asm volatile("clflush 0(%0)\n" : : "c"(p) : "rax");
	  asm volatile("mfence\n");
}

void *get_buffer_addr( void )
{
  return &buffer;
}

void initialize_buffer( void )
{
    for (int i = 0; i < 4096; ++i) {
        buffer[i] = i%256;
    }

    // Flush the written value to DRAM so it is visible from the alias
    for (int i = 0; i < 4096; i += 64) {
        flush(&buffer[i]);
    }
}

void write_to_buffer(int offset)
{
    buffer[offset]++;

    // Flush the written value to DRAM so it is visible from the alias
    flush(&buffer[offset]);
}
