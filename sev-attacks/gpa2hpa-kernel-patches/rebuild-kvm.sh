#!/bin/bash
# This script rebuilds the kvm kernel module (includes kvm_amd)
# And installs it to the module directory
# You still need to reload with `sudo modprobe -r kvm_amd kvm && sudo modprobe kvm_amd`
# for the changes to take effect

#abort on first error
set -e

#We want the version to match the one of the snp kernel, such that
# modules_install overwrites the existing modules directory with the
# new versions. This way modprobe will load the new versions.
# The AMD build scripts sets LOCALVERSION in the config file.
# If we dont set LOCALVERSION explictly, the build system will a a prefix
# to the new
EV=""
# make clean M=arch/x86/kvm
make -j $(nproc) LOCALVERSION=${EV} scripts
make -j $(nproc) LOCALVERSION=${EV} prepare
make -j $(nproc) LOCALVERSION=${EV} modules_prepare
make -j $(nproc) LOCALVERSION=${EV} M=arch/x86/kvm
sudo make LOCALVERSION=${EV} M=arch/x86/kvm modules_install

#This will only export/install the headers meant for usage by userspace
#Do not write these headers to /lib/modules or /usr/src/linux-headers-*
#If you build a kernel module, you need to use the stock headers from the kernel installation
#If you want to build a usperspace program, you will need to include the userspace headers
#exported in this step
#make LOCALVERSION=${EV} INSTALL_HDR_PATH=/home/luca/badram/user-headers headers_install
if [ -z ${INSTALL_HDR_PATH} ]; then
  printf "\n\n###\nINSTALL_HDR_PATH not set. Skipping exporting uapi headers\n###\n\n"
else
  printf "\n\n###\nInstalling uapi headers to ${INSTALL_HDR_PATH}\n###\n\n"
  make LOCALVERSION=${EV}  headers_install
fi
