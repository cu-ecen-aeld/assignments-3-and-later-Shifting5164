#!/bin/bash

set -e

make
sshpass -p ktest scp -r * ktest@192.168.56.101:/tmp
