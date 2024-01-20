#!/bin/bash

./reload
./test_write_read.sh

rm ./test 2>/dev/null
dd if=/dev/aesdchar skip=0 count=1 bs=1 of=./test
echo "data:"
cat test
echo -e "\n"

#----------------------------


rm ./test 2>/dev/null
dd if=/dev/aesdchar skip=0 count=8 bs=1 of=./test
echo "data:"
cat test
echo -e "\n"


#----------------------------


rm ./test 2>/dev/null
dd if=/dev/aesdchar skip=2 count=1 bs=1 of=./test 
echo "data:"
cat test
echo -e "\n"

#----------------------------
dd if=/dev/aesdchar skip=2 count=3 bs=1 of=./test 
echo "data:"
cat test
echo -e "\n"

#----------------------------
dmesg | tail 
