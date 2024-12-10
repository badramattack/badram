# GPA2HPA KVM Patch

This is a small patch that adds an additional ioctl to KVM to translate GPAs to HPAs.
While QEMU has a nice `gpa2hpa` HMP command, it does not work for if the MEMFD backend is
used. This is the case for more recent SEV-SNP software stacks.

Tested with the official AMD SEV-SNP kernel branch `snp-host-latest` at commit `6b293770dac2fc37e7a880a321045d54bc88b0ce`.

## Build
This assumes that your kernel sources are already configured etc..
This is e.g. the case if you follow the manual in the AMD repo to build the kernel.

1) Apply the patch
2) Copy the buildscript to the kernel source directory. Run it with `INSTALL_HDR_PATH=<path where uapi headers should be saved> ./rebuild-kvm.sh `. You will need the uapi header files to build the `sev-attacks/simple-replay` tool.
3) Reload the KVM modules with `sudo modprobe -r kvm_amd kvm && sudo modprobe kvm_amd`
