#!/bin/bash

set -e

make clean
make
sshpass -p ktest scp -r * ktest@192.168.56.101:/tmp

echo -e "\n*** DONE ***\n"
