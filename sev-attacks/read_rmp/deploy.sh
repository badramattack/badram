#!/bin/sh

if [ -z "${DEPLOY_URI}" ]; then
    echo "Please define  DEPLOY_URI"
	exit -1
fi
printf "\n\n###\nDeploying to ${DEPLOY_URI}\n###\n\n"
scp ./build/binaries/badram-gpa-swap-victim ./build/binaries/read_rmp ./build/binaries/swap_attack ./dump-rmp-msrs.sh ../../alias-reversing/modules/read_alias/kmod_readalias.ko ${DEPLOY_URI}
