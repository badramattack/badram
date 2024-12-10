#Find Aliases Individual Tool

## Background
For this section we assume that you a DIMM that reports twice the actual size due to SPD data manipulation. Depending on the exact aliasing function, the system might run very unstable or even crash during boot. To improve system stability we use the `memmap` Linux kernel paramter to prevent the system from using the whole upper half of the physical memory range. Assuming a 32 GiB DIMM that has been manipulated to report 64 GiB, we start the Linux kernel with `memmap=0x800000000$0x800000000`. (If you enter this on the GRUB command line during boot, you need to escape the `$` with `\$`. If you want to change this via the `GRUB_CMDLINE_LINUX` variable in the `/etc/default/grub` config file, you need to use `\\\$` to also escape the slash itself)

To recover the aliased address *ALIAS(A)* for a phyiscal address *A* we peform the following experiment
1) Loop over the whole physical address space. Let *B* be the currently tested candidate
2) Write a random marker value *m1* to *A*
3) Read from *B* and store the result in *bm1*
4) Write a random marker value *m2* to *A*
5) Read from *B* and store the result in *bm2*
6) Check if `m1 xor m2` equals `bm1 xor bm2`. If true, *B* is the alias for *A*.

The reasons for the two-step xor comparison instead of simply checking if *B* contains *m1*
is that many systems use memory scrambling, where the memory controller xors a physical address depdendent value to payload data before sending it to the DIMM. When accessing address *A* through its alias *ALIAS(A)* the memory controller will xor a different scrambling value due to the differences in the physical address. However, since the scrambling value is xored to the data, the xor
of *m1* and *m2* will still be equal to the xor of *bm1* and *bm2*.

## Tool Design
During our experiments, we found that on some systems the physical address space is not contiguous but fractured. Furthermore different ranges of the address space seem to require different shifts/alias functions. The `fai` tools first parses `/proc/iomem` to get a list of all physical memory ranges known to the sytem. Next, it filters all memory ranges that do not corespond to the main memory. Afterwards it performs the experiment from the previous section for one address of each remaining memory range. The outputs are stored in `aliases.csv`. Depending on the RAM size, the tool might require a bit of time. We recommend to run it with e.g. `tmux`.

## Build

1) If you don't build on the target system, you will need to to point the build system to the Linux kernel headers of the target system by setting `export KERNEL_PATH <path to headers>`
2) `make`

This will produces the following artifacts:
- `./build/binaries/fai`
- `../../modules/read_alias/kmod_readalias.ko`

## Use
For basic operations, runs the tool as follows:
1) `sudo insmod <path to kmod_readalias.ko`
2) `sudo ./fai find`

If your machine allows to disable memory scrambling, you can add the `--no-scrambling`
parameter to speed up the search.

The tool will output the final results on the commandline and
also save them in the `aliases.csv` text file. You will need this
text file in the next section.
The output should look like this.
```
Reserved Page Errors: 16387, Map Failed Errors: 0
Result
Mem Range{.start=0x000100000, .end=0x02fffffff} : orig=0x000101000 alias=0x850131000 xor=0x850030000
Mem Range{.start=0x03000b000, .end=0x075cfffff} : orig=0x03000c000 alias=0x88003c000 xor=0x8b0030000
Mem Range{.start=0x076000000, .end=0x08c1fefff} : orig=0x076001000 alias=0x8c6031000 xor=0x8b0030000
Mem Range{.start=0x08c35f000, .end=0x08c392017} : orig=0x08c360000 alias=0x8dc350000 xor=0x850030000
Mem Range{.start=0x08c3fbc58, .end=0x08c40d017} : orig=0x08c3fc000 alias=0x8dc3cc000 xor=0x850030000
Mem Range{.start=0x08c415058, .end=0x08c52cfff} : orig=0x08c416000 alias=0x8dc426000 xor=0x850030000
Mem Range{.start=0x08c52e000, .end=0x092203fff} : orig=0x08c52f000 alias=0x8dc51f000 xor=0x850030000
Mem Range{.start=0x0aa3ff000, .end=0x0abffffff} : orig=0x0aa400000 alias=0x8fa430000 xor=0x850030000
Mem Range{.start=0x100000000, .end=0x7ffffffff} : orig=0x100001000 alias=0x900031000 xor=0x800030000
Mem Range{.start=0x1000000000, .end=0x104f0fffff} : orig=0x1000001000 alias=0x800031000 xor=0x1800030000  
```
Each line corresponds to one memory range witht the given start and end address. The address `orig` is the address for which the searched the alias and `alias` is the found alias address. `xor` is the alias shift/mask obtained by xoring `orig` and `alias`. It is assumed that the alias is the same for all addresses from the same memory range.

### Reversing RMP alias

On the "horus" machine, I ran into some issues when reversing the alias function for the memory range that contains the RMP.
The memory range was in a reserved range that could not be accesses with standard ioremap. Furthermore, the alias function was different for different
parts of the memory range, breaking one of the basic assumptions of the fai tool.

To cope with this situation, use the `--input` param of the tools, to feed it a list of physical addresses for which it should search an alias.
Choose these addresses such that they are distributed over the RMP memory range.
In addition, you need to use the `--access-reserved` option

You need to disabled SEV-SNP in the BIOS before reverse engineering. Otherwise, we cannot write to the RMP memory range. On our system, the RMP was always
at the same physical addresses.
