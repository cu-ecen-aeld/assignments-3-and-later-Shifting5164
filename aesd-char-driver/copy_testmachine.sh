#!/bin/bash

set -e

make clean
make
sshpass -p ktest scp -r * ktest@192.168.56.101:/tmp
sshpass -p ktest scp .././assignment-autotest/test/assignment9/drivertest.sh ktest@192.168.56.101:/tmp


echo -e "\n*** DONE ***\n"
