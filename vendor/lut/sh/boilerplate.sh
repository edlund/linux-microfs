#!/bin/bash

# boilerplate.sh: Boilerplate code necessary in order to make
# bash scripting in general and error handling in particular
# a little easier.
# 
# Copyright (C) 2015 Erik Edlund <erik.edlund@32767.se>
# 
# Redistribution and use in source and binary forms, with or
# without modification, are permitted provided that the
# following conditions are met:
# 
#  * Redistributions of source code must retain the above
#  copyright notice, this list of conditions and the following
#  disclaimer.
# 
#  * Redistributions in binary form must reproduce the above
#  copyright notice, this list of conditions and the following
#  disclaimer in the documentation and/or other materials
#  provided with the distribution.
# 
#  * Neither the name of Erik Edlund, nor the names of its
#  contributors may be used to endorse or promote products
#  derived from this software without specific prior written
#  permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
# CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# To use boilerplate.sh:
# 
#     source "boilerplate.sh"

if [ "`basename $0`" == "boilerplate.sh" ] ; then
	echo "`basename $0` does not do much on its own"
	exit 0
fi

_prev_command=""
_current_command=""

declare -a _exit_cmds
declare -a _exit_cmd_types

declare -r _EXIT_CMD_0=0
declare -r _EXIT_CMD_1=1
declare -r _EXIT_CMD_ANY=2

_verbose_atexit=0

_err_trapped=0
_err_ignored=0

trap '_prev_command=$_current_command; _current_command=$BASH_COMMAND' DEBUG

# Backend for atexit*.
_atexit() {
	local type="$1"
	local cmd="$2"
	local n=${#_exit_cmds[@]}
	_exit_cmds[$n]="${cmd}"
	_exit_cmd_types[$n]="${type}"
}

# Remove the last atexit command.
_atexit_pop() {
	local n=${#_exit_cmds[@]}-1
	unset _exit_cmds[$n]
	unset _exit_cmd_types[$n]
}

# Always run the given command $* at exit.
atexit() {
	_atexit $_EXIT_CMD_ANY "$*"
}

# Only run the given command $* on "exit 0".
atexit_0() {
	_atexit $_EXIT_CMD_0 "$*"
}

# Only run the given command $* on "exit 1".
atexit_1() {
	_atexit $_EXIT_CMD_1 "$*"
}

_trap_EXIT() {
	for (( i=${#_exit_cmds[@]}-1 ; i>=0 ; i-- )) ; do
		local type="${_exit_cmd_types[$i]}"
		local cmd="${_exit_cmds[$i]}"
		if (( \
			( $type == $_EXIT_CMD_ANY ) \
			|| ( $_err_trapped == 0 && $type == $_EXIT_CMD_0 ) \
			|| ( $_err_trapped == 1 && $type == $_EXIT_CMD_1 ) \
		)) ; then
			if (( $_verbose_atexit == 1 || $_err_trapped == 1 )) ; then
				echo "${cmd}"
			fi
			eval "${cmd}"
		fi
	done
}
trap '_trap_EXIT' EXIT

source_ERR() {
	_prev_command="source $1"
}

trap_ERR() {
	_err_ignored=0
}
untrap_ERR() {
	_err_ignored=1
}

_trap_ERR() {
	if (( $_err_ignored == 0 )) ; then
		local line_num=$1
		local exit_code=$2
		local failed_command=$_prev_command
		echo -n "$0: Error on or near line ${line_num}"
		echo -n ", \"${failed_command}\" failed with status code ${exit_code}"
		echo ""
		_err_trapped=1
		exit 1
	fi
}
trap '_trap_ERR $LINENO $?' ERR

snore() {
	echo -n "$3"
	for i in $(seq 1 $2) ; do
		sleep "$1"
		echo -n "."
	done
	echo ""
}

implode() {
	local IFS="$1"
	shift
	echo "$*"
}

