# BadRAM: Practical Memory Aliasing Attacks on Trusted Execution Environments

This repository contains the scripts and tools to perform BadRAM attacks, as well as the proof-of-concept and end-to-end attacks presented in [our paper](https://badram.eu/badram.pdf).

## Directory structure

* `common-code` contains a static library with helper functions that are used throughout this project.
* `alias-reversing` contains kernel modules and userspace tools for reversing the alias memory mapping.
  * `alias-reversing/modules/read_alias` offers a generic read/write to physical memory API and builds a static library used by other code parts.
  * `alias-reversing/apps/find-alias-individual` is a smart tool to reverse the aliasing. Checks each memory region individually, as they do not always have the same aliasing function. Results are exported as csv and and be read/written with the tools in `common-code`.
  * `alias-reversing/apps/test-alias` takes the aliases exported by `find-alias-individual` and checks that they apply for each address of the corresponding memory range. May lead to crashes if the aliased memory is used by the system.
* `scripts` provides the Raspberry Pi Pico scripts to read, unlock, and overwrite the SPD data for DDR4 (ee1004) and DDR5 (spd5118).
* `sev-attacks` contains the PoC and end-to-end attacks on AMD SEV-SNP.
  * `sev-attacks/simple-replay` shows a basic POC in a cooperative scenario.
  * `sev-attacks/rw_pa`: tool to perform basic capture and replay attacks
  * `sev-attacks/replay_vmsa` : basic PoC that replays the VM's VMSA with a modified register state
  * `sev-attacks/read_rmp` : PoC for swapping the PFNs of two GFNs
  * `sev-atacks/guest_context_replay` : PoC for replaying the attestation report
  * `sev-attacks/gpa2hpa-kernel-patches` : Patches for host kernel that add an ioctl to translate GPAs to HPAs
* `sgx-attacks` provides the PoC attack on "classic" Intel SGX.

## Getting started

### Hardware requirements

To interface with the SPD chip and overwrite its data, a microcontroller, like the Raspberry Pi Pico, and DDR4 or DDR5 sockets are required.

| Component         | Cost  | Link |
|-------------------|-------|------|
| Raspberry Pi Pico | $5    | [Link](https://www.raspberrypi.com/products/raspberry-pi-pico/) |
| DDR Socket        | $1-5  | DDR4 [[1]](https://www.amphenol-cs.com/product-series/ddr4.html) [[2]](https://www.te.com/en/products/connectors/sockets/memory-sockets/dimm-sockets/ddr4-sockets.html)/DDR5 [[1]](https://www.amphenol-cs.com/product-series/ddr5-memory-module-sockets-smt.html) [[2]](https://www.te.com/en/products/connectors/sockets/memory-sockets/dimm-sockets/ddr5-dimm-sockets.html) |
| 9V source         | $2    | 9V battery / Boost convertor|

### Creating aliases

To create aliases, modify the SPD contents to report one more row bit than the DIMM originally has. The relevant SPD bytes that need to be modified are shown below.

<details>
<summary>DDR4</summary>

* Byte `0x4`, bits 3-0: Total SDRAM capacity per die, in megabits  
  This capacity has to be doubled to reflect the additional row address bit.

  | Bits | Mapping |
  |------|---------|
  | 0000 | 256 Mb  |
  | 0001 | 512 Mb  |
  | 0010 | 1 Gb    |
  | 0011 | 2 Gb    |
  | 0100 | 4 Gb    |
  | 0101 | 8 Gb    |
  | 0110 | 16 Gb   |
  | 0111 | 32 Gb   |
  | 1000 | 12 Gb   |
  | 1000 | 24 Gb   |
  | other | Reserved    | 

* Byte `0x5`, bits 5-3: Row address bits  
  The number of row address bits has to be incremented.

  | Bits | Row bits |
  |------|---------|
  | 000  | 12  |
  | 001  | 13  |
  | 010  | 14  |
  | 011  | 15  |
  | 100  | 16  |
  | 101  | 17  |
  | 110  | 18  |
  | other | Reserved    | 

* Bytes `0x7e`-`0x7f`: CRC checksum  
  This must be updated since the data changed. This can also be calculated by the scripts in `./scripts/ee1004`.

* Bytes `0x145`-`0x148`: Module serial number (optional)  
  Might need to be changed since the motherboard may cache the SPD data based on the serial number
</details>

<details>
<summary>DDR5</summary>

* Byte `0x4`, bits 4-0: Total SDRAM density per die   
  This capacity has to be doubled to reflect the additional row address bit.

  | Bits | Mapping |
  |------|---------|
  | 00000 | No memory; not defined  |
  | 00001 | 4 Gb    |
  | 00010 | 8 Gb    |
  | 00011 | 12 Gb   |
  | 00100 | 16 Gb   |
  | 00101 | 24 Gb   |
  | 00110 | 32 Gb   |
  | 00111 | 48 Gb   |
  | 01000 | 64 Gb   |
  | other | Reserved    | 

* Byte 5, bits 4-0: Row address bits  
  The number of row address bits has to be incremented.

  | Bits | Row bits |
  |------|---------|
  | 00000  | 16  |
  | 00001  | 17  |
  | 00010  | 18  |
  | other | Reserved    | 

* Bytes `0x1fe`-`0x1ff`: CRC checksum  
  This must be updated since the data changed. This can also be calculated by the scripts in `./scripts/spd5118`.

* Bytes `0x205`-`0x208`: Module serial number (optional)  
  Might need to be changed since the motherboard may cache the SPD data based on the serial number
</details>

### Mapping out aliased region in Linux

To prevent accidental overwrites, the use of the aliased region can be prevented using the `memmap` kernel parameter:
```
memmap=nn$ss
```
This will mark region `ss` to `ss+nn` as reserved. Note that using this parameter in GRUB requires escaping the `$` as `\$`. If you enter it through `/etc/default/grub`, you also have to escape the `\`:
```
GRUB_CMDLINE_LINUX_DEFAULT="memmap=nn\\\$ss"
```

### Finding aliases

You can use `find-alias-individual` to find the aliases for each memory region on your system. This tool will produce an `aliases.csv` file, which can be later used to find the alias of any address on your machine.

## Cite

```
@inproceedings{badramsp25,
  title     = {{BadRAM}: Practical Memory Aliasing Attacks on Trusted Execution Environments},
  author    = {De Meulemeester, Jesse and Wilke, Luca and Oswald, David and Eisenbarth, Thomas and Verbauwhede, Ingrid and Van Bulck, Jo},
  booktitle = {46th {IEEE} Symposium on Security and Privacy ({S\&P})},
  month     = May,
  year      = 2025,
}
```
