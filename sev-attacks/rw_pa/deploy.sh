#!/bin/sh

if [ -z "${DEPLOY_URI}" ]; then
    echo "Please define DEPLOY_URI"
	exit -1
fi
printf "\n\n###\nDeploying to ${DEPLOY_URI}\n###\n\n"
scp ./build/binaries/rw_pa ../../alias-reversing/modules/read_alias/kmod_readalias.ko ${DEPLOY_URI}
