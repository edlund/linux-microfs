#!/bin/bash

script_path=`readlink -f "$0"`
script_dir=`dirname "${script_path}"`

packages=(
	# required
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
	"perl"
	# extras
	"cramfsprogs"
	"inotify-tools"
	"squashfs-tools"
)

source "${script_dir}/packages_apt.sh"

