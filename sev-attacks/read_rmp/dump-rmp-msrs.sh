#!/bin/bash

set -e
echo "Reading RMP start and end MSRs using rdmsr. This might ask for sudo rights"

RMP_START=$(sudo rdmsr 0xc0010132)
RMP_END=$(sudo rdmsr 0xc0010133)
printf "Start\t0x${RMP_START}\n"
printf "End\t0x${RMP_END}\n"
