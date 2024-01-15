#!/bin/bash 

echo -e "bla1\n" > /dev/aesdchar
echo -e "bla2\n" > /dev/aesdchar
echo -e "bla31" > /dev/aesdchar
echo -e "bla32\n" > /dev/aesdchar

cat /dev/aesdchar
