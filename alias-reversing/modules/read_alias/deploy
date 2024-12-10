#!/bin/sh

if [[ -z "${DEPLOY_ADDR}" ]]; then
  echo "Missing DEPLOY_ADDR env var. Did you source environment.sh"
  exit -1
fi
scp test_aliases fai kmod_readalias.ko ${DEPLOY_ADDR}
