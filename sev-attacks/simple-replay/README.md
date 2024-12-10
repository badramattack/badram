# simple-replay
This folder contains proof-of-concept code for a simple ciphertext replay on SEV-SNP. It assumes a cooperative debug scenario where a dummy application inside the guest prints the required addresses and contains dummy `getchar` wait statements to manually synchronize the reads/writes via a CLI interface.

## Build

To work with recent SEV-SNP software stacks, this requires you to follow the README in `../gpa2hpa-kernel-patches` to apply the GPA2HPA KVM patch to your kernel. You will only need to rebuild the KVM module, not the whole kernel. The folder contains a README with more instructions.

Afterwards, you can build the attack code as follows:
1) Set and export `KERNEL_PATCH_UAPI` to the uapi header files exported while building the GPA2HPA patched KVM module
2) If you don't execute this on the targeted machine, set and export `KERNEL_PATH` to point to the kernel headers of the targeted kernel. Otherwise, the headers for the currently running kernel will be used.  
3) Build everything with `make`.

This will build 3 artifacts
- ``
- `./build/binaries/badram-vm-victim`
- `./build/binaries/badram-sev-reply`
- `../../alias-reversing/modules/read_alias/kmod_readalias.ko`


## Use

We assume that you know the aliasing function for the manipulated DIMM. You can use the `fai` tool from `alias-reversing/apps/find-alias-individual` to get this function. Besides printing the results to the command line, it will also create the `aliases.csv` file which can ben used with this attack tool.

If you don't use the `fai` tool, you can also manually create the `aliases.csv` file. The format is as follows
```csv
#start pa, end pa, alias xor mask
<your content goes here>
```
Do not remove the header line. `start pa` and `end pa` specify a contiguous physical address range and `alias xor mask` specifies the value that we need to xor to an address to obtain the alias. All values require the `0x` hex prefix for the parser to work.

1) Issue `sudo insmod <path to kmod_readalias.ko>` to load the read alias kernel module
1) Copy `badram_vm_victim` to the VM and run it with sudo. Let <VICTIM_GPA> be the gpa shown in the `membuffer ...` line
2) On the host, run `sudo ./badram-sev-replay $(pidof qemu-system-x86_64) ./aliases.csv gpa2hpa_kern <VICTIM_GPA>`
3) Follow the instructions from both apps to capture and replay the ciphertext.

If successful, the final value for the memory buffer displayed by `badram_vm_victim` should be all zeroes instead of `0xff`. See `./badram-simple-replay-demo.webm` for a recording of a successful attack.