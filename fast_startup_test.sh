#!/bin/bash

killall aesdsocket
./server/aesdsocket -d
./assignment-autotest/test/assignment6/sockettest.sh
killall aesdsocket
