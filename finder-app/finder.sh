#!/bin/sh

readonly filedir=$1
readonly searchstr=$2

if [ -z "${filedir}" ] || [ -z "${searchstr}" ]; then 
	echo "no args"
	exit 1
fi

if [ ! -d "${filedir}" ]; then
	echo "no dir"
	exit 1
fi

readonly X="$(grep -rl "$searchstr" $filedir/* | wc -l)"
readonly Y="$(grep -r "$searchstr" $filedir/* | wc -l)"
echo "The number of files are ${X} and the number of matching lines are ${Y}"
