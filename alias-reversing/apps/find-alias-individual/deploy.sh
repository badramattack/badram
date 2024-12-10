#!/bin/sh

if [ -z "${DEPLOY_URI}" ]; then
    echo "Please define  DEPLOY_URI"
	exit -1
fi
printf "\n\n###\nDeploying to ${DEPLOY_URI}\n###\n\n"
scp ../../modules/read_alias/kmod_readalias.ko ./build/binaries/fai ${DEPLOY_URI}
