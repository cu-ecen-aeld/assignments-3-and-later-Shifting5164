#!/bin/bash

readonly filedir=$1
readonly searchstr=$2

if [ -z "${filedir}" ] || [ -z "${searchstr}" ]; then 
	echo "no args"
	exit 1
fi

if [[ ! -d "${filedir}" ]]; then
	echo "no dir"
	exit 1
fi

readonly X="$(find "${filedir}" -type f 2>/dev/null | wc -l)"
readonly Y="$(find "${filedir}" -type f 2>/dev/null -exec grep -ir "${searchstr}" \; | wc -l)"
echo "The number of files are ${X} and the number of matching lines are ${Y}"
