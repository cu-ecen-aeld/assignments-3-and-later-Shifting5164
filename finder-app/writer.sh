#!/bin/bash

readonly writefile=$1
readonly writestr=$2

if [ -z "${writefile}" ] || [ -z "${writestr}" ]; then
        echo "no args"
        exit 1
fi

mkdir -p "$(dirname "${writefile}")"
touch "${writefile}"
echo "${writestr}" > "${writefile}"
