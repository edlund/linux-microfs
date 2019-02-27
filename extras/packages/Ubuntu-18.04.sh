#!/bin/bash

script_path=`readlink -f "$0"`
script_dir=`dirname "${script_path}"`

packages=(
	# required
	"build-essential"
	"check"
	"gcc"
	"g++"
	"git"
	"libtool"
	"make"
	"python"
	"zlib1g-dev"
	# recommended
	"autoconf"
	"automake"
	"gdb"
	"liblz4-dev"
	"liblzo2-dev"
	"liblzma-dev"
	"libzstd-dev"
	"perl"
	# extras
	"inotify-tools"
	"squashfs-tools"
)

source "${script_dir}/packages_apt.sh"

