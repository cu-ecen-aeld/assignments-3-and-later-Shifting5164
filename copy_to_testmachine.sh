#!/bin/bash

set -e 

(
cd aesd-char-driver
./copy_testmachine.sh
)

(
cd server
./copy_testmachine.sh
)
