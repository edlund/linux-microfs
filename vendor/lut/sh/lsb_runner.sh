#!/bin/bash

script_path=`readlink -f "$0"`
script_dir=`dirname "${script_path}"`

sep="-"
path="${script_dir}"
before=""
after=".sh"

options="s:p:b:a:"
while getopts $options option
do
	case $option in
		s ) sep=$OPTARG ;;
		p ) path=$OPTARG ;;
		b ) prefix=$OPTARG ;;
		a ) after=$OPTARG ;;
	esac
done

desc="`lsb_release -d`"
regex="^Description:\s+(\w+)\s+([-\w\.]+)"
name="`python -c "import re, sys; [sys.stdout.write(g + '${sep}') for g in re.match('${regex}', '${desc}').groups()]"`"

path="${path}/${before}${name::-1}${after}"

if [[ ! -f "${path}" ]] ; then
	echo "$0: could not find the release specific script to run"
	echo "$0: ${path} is not a regular file"
	exit 1
fi

eval "${path}"
exit 0

