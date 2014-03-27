
# microfs - Minimally Improved Compressed Read Only File System
# Copyright (C) 2012 Erik Edlund <erik.edlund@32767.se>
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

ifndef CC
CC := gcc
endif
ifndef HOSTCC
HOSTCC := gcc
endif

ifndef INSTALL_HOSTPROG_PATH
INSTALL_HOSTPROG_PATH := /usr/local/bin
endif

HOST_EXTRACFLAGS += \
	-Wall \
	-Wextra \
	-pedantic \
	-std=c11 \
	-D_GNU_SOURCE \
	-D_FILE_OFFSET_BITS=64

ifeq ($(DEBUG),1)
ccflags-y += -DDEBUG
HOST_EXTRACFLAGS += -ggdb
$(info DEBUG build)
ifeq ($(DEBUG_SPAM),1)
ccflags-y += -DDEBUG_SPAM
$(info DEBUG_SPAM build)
endif
ifeq ($(DEBUG_INODES),1)
ccflags-y += -DDEBUG_INODES
$(info DEBUG_INODES build)
endif
endif

# clean-files := ...

obj-m := microfs.o
microfs-y := \
	microfs_super.o \
	microfs_read.o \
	microfs_decompressor.o \
	microfs_decompressor_zlib.o \
	microfs_inode.o \
	microfs_compat.o

hostprogs-y := \
	microfscki \
	microfsmki \
	microfslib \
	test

microfscki-objs := \
	hostprog_microfscki.o \
	hostprogs.o \
	hostprogs_lib.o \
	hostprogs_lib_zlib.o
HOSTLOADLIBES_microfscki := -lrt

microfsmki-objs := \
	hostprog_microfsmki.o \
	hostprogs.o \
	hostprogs_lib.o \
	hostprogs_lib_zlib.o
HOSTLOADLIBES_microfsmki := -lrt

microfslib-objs := \
	hostprog_microfslib.o \
	hostprogs_lib.o \
	hostprogs_lib_zlib.o

test-objs := \
	test.o \
	test_master.o \
	test_hostprogs.o \
	hostprogs.o
HOSTLOADLIBES_test := -lcheck -lm -lrt -lpthread

HOSTLOADLIBES_microfscki += -lz
HOSTLOADLIBES_microfsmki += -lz
HOSTLOADLIBES_microfslib += -lz
ccflags-y += -DMICROFS_DECOMPRESSOR_ZLIB
HOST_EXTRACFLAGS += -DHOSTPROGS_LIB_ZLIB

always := $(hostprogs-y)

requirevar-%:
	@if test "$($(*))" = "" ; then \
		echo "Required variable $(*) not set"; \
		exit 1; \
	fi

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	make -C $(PWD)/tools -f Makefile.extra all CC=$(HOSTCC) DEBUG=$(DEBUG)

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	make -C $(PWD)/tools -f Makefile.extra clean

# Usage: make check [CHECKARGS="..."]
# 
check: all
	$(PWD)/test
	$(PWD)/test.sh $(CHECKARGS)

# Usage: make remotecheck \
#     REMOTEHOST="localhost" REMOTEPORT="2222" REMOTEUSER="erik" \
#     REMOTEDEST="~" [REMOTEMKARGS="..."]
# 
remotecheck: requirevar-REMOTEHOST requirevar-REMOTEPORT \
		requirevar-REMOTEUSER requirevar-REMOTEDEST
	rsync --rsh="ssh -p $(REMOTEPORT)" -av --delete --exclude=.git \
		$(PWD) $(REMOTEUSER)@$(REMOTEHOST):$(REMOTEDEST)
	ssh -t -p $(REMOTEPORT) $(REMOTEUSER)@$(REMOTEHOST) \
		"cd $(REMOTEDEST)/`basename $(PWD)` && make check $(REMOTEMKARGS)"

install: all
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules_install
	make -C $(PWD)/tools -f Makefile.extra install INSTALL_PATH=$(INSTALL_HOSTPROG_PATH)
	cp $(PWD)/microfscki $(INSTALL_HOSTPROG_PATH)
	cp $(PWD)/microfsmki $(INSTALL_HOSTPROG_PATH)

uninstall:
	make -C $(PWD)/tools -f Makefile.extra uninstall INSTALL_PATH=$(INSTALL_HOSTPROG_PATH)
	rm $(INSTALL_HOSTPROG_PATH)/microfscki
	rm $(INSTALL_HOSTPROG_PATH)/microfsmki
