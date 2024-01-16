#!/bin/bash 

echo "test0" > /dev/aesdchar
echo "test1" > /dev/aesdchar
echo "test2" > /dev/aesdchar
echo "test3" > /dev/aesdchar
echo "test4" > /dev/aesdchar
echo "test5" > /dev/aesdchar
echo "test6" > /dev/aesdchar
echo "test7" > /dev/aesdchar
echo "test8" > /dev/aesdchar
echo "test9" > /dev/aesdchar
echo "test10" > /dev/aesdchar


cat /dev/aesdchar

printf "%s" "blaa" > /dev/aesdchar ; printf "%s\n" "aat" > /dev/aesdchar

cat /dev/aesdchar