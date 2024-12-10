#!/bin/bash

RMP_START=$(sudo rdmsr 0xc0010132)
RMP_END=$(sudo rdmsr 0xc0010133)
echo "RMP_START=${RMP_START} RMP_END=${RMP_END}"
sudo ./read_rmp 0x${RMP_START} 0x${RMP_END}
