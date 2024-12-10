# SGX Membuster PoC

This is a small PoC to show how the aliased memory can infer the addresses being modified. The userspace program asks the enclave to change a random byte in its buffer, and compares the changes to the EPC through the aliased memory.

## Build

This PoC can be build using `make all`, this will produce two binaries:
- `./build/binaries/app`
- `../../alias-reversing/modules/read_alias/kmod_readalias.ko`

## Use

Use `fai` to generate the csv file, or manually create `aliases.csv` if you know the aliasing function.

For instance, the `aliases.csv` for the Intel NUC with a single 16GB -> 32 GB SODIMM:
```
#start pa, end pa, alias xor mask
0x0,0x800000000,0x400000000
```

Start the program with `make run`. This will:
1. Build everything
2. Insert the kernel module `kmod_readalias.ko`
3. Run the app which instantiates the enclave

### Expected output

When successful, the userspace app should see the cacheline corresponding to the modified byte change in the EPC.

```
Buffer allocated at va=0x7f84e3e17000 -- pa=0x70975000 -- alias=470975000

Taking initial snapshot of buffer from alias=470975000
Modifying enclave buffer at offset 0x567
Comparing EPC contents for changes
 --> Cacheline changed at pa=0x70975540 (offset 0x540)
```
