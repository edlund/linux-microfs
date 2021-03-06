
# microfs - Minimally Improved Compressed Read Only File System
# Copyright (C) 2012, 2013, 2014, 2015, 2016, 2017, ..., +%Y
# Erik Edlund <erik.edlund@32767.se>
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

ifndef CC
CC := gcc
endif
ifndef HOSTCC
HOSTCC := gcc
endif

ifndef LIB_ZLIB
LIB_ZLIB := 1
endif
ifndef LIB_LZ4
LIB_LZ4 := 0
endif
ifndef LIB_LZO
LIB_LZO := 0
endif
ifndef LIB_XZ
LIB_XZ := 0
endif
ifndef LIB_ZSTD
LIB_ZSTD := 0
endif

ifndef INSTALL_HOSTPROG_PATH
INSTALL_HOSTPROG_PATH := /usr/local/bin
endif

VENDOR_BIN := $(PWD)/vendor/bin

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
ifeq ($(DEBUG_READS),1)
ccflags-y += -DDEBUG_READS
$(info DEBUG_READS build)
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
	microfs_decompressor_data.o \
	microfs_decompressor_data_singleton.o \
	microfs_decompressor_data_percpu.o \
	microfs_decompressor_data_queue.o \
	microfs_decompressor_impl_buffer.o \
	microfs_decompressor_zlib.o \
	microfs_decompressor_lz4.o \
	microfs_decompressor_lzo.o \
	microfs_decompressor_xz.o \
	microfs_decompressor_zstd.o \
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
	hostprogs_lib_zlib.o \
	hostprogs_lib_lz4.o \
	hostprogs_lib_lzo.o \
	hostprogs_lib_xz.o \
	hostprogs_lib_zstd.o
HOSTLOADLIBES_microfscki := -lrt

microfsmki-objs := \
	hostprog_microfsmki.o \
	hostprogs.o \
	hostprogs_lib.o \
	hostprogs_lib_zlib.o \
	hostprogs_lib_lz4.o \
	hostprogs_lib_lzo.o \
	hostprogs_lib_xz.o \
	hostprogs_lib_zstd.o
HOSTLOADLIBES_microfsmki := -lrt

microfslib-objs := \
	hostprog_microfslib.o \
	hostprogs.o \
	hostprogs_lib.o \
	hostprogs_lib_zlib.o \
	hostprogs_lib_lz4.o \
	hostprogs_lib_lzo.o \
	hostprogs_lib_xz.o \
	hostprogs_lib_zstd.o

test-objs := \
	test.o \
	test_master.o \
	test_hostprogs.o \
	hostprogs.o
HOSTLOADLIBES_test := -lcheck -lsubunit -lm -lrt -lpthread

HOSTPROG_LIBS := -lz -lpthread
HOSTLOADLIBES_microfscki += $(HOSTPROG_LIBS)
HOSTLOADLIBES_microfsmki += $(HOSTPROG_LIBS)
HOSTLOADLIBES_microfslib += $(HOSTPROG_LIBS)
ifeq ($(LIB_ZLIB),1)
$(info -lz build)
ccflags-y += -DMICROFS_DECOMPRESSOR_ZLIB
HOST_EXTRACFLAGS += -DHOSTPROGS_LIB_ZLIB
endif

ifeq ($(LIB_LZ4),1)
$(info -llz4 build)
HOSTLOADLIBES_microfscki += -llz4
HOSTLOADLIBES_microfsmki += -llz4
HOSTLOADLIBES_microfslib += -llz4
ccflags-y += -DMICROFS_DECOMPRESSOR_LZ4
HOST_EXTRACFLAGS += -DHOSTPROGS_LIB_LZ4
endif

ifeq ($(LIB_LZO),1)
$(info -llzo2 build)
HOSTLOADLIBES_microfscki += -llzo2
HOSTLOADLIBES_microfsmki += -llzo2
HOSTLOADLIBES_microfslib += -llzo2
ccflags-y += -DMICROFS_DECOMPRESSOR_LZO
HOST_EXTRACFLAGS += -DHOSTPROGS_LIB_LZO
endif

ifeq ($(LIB_XZ),1)
$(info -llzma build)
HOSTLOADLIBES_microfscki += -llzma
HOSTLOADLIBES_microfsmki += -llzma
HOSTLOADLIBES_microfslib += -llzma
ccflags-y += -DMICROFS_DECOMPRESSOR_XZ
HOST_EXTRACFLAGS += -DHOSTPROGS_LIB_XZ
endif

ifeq ($(LIB_ZSTD),1)
$(info -lzstd build)
HOSTLOADLIBES_microfscki += -lzstd
HOSTLOADLIBES_microfsmki += -lzstd
HOSTLOADLIBES_microfslib += -lzstd
ccflags-y += -DMICROFS_DECOMPRESSOR_ZSTD
HOST_EXTRACFLAGS += -DHOSTPROGS_LIB_ZSTD
endif

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
	make -C $(PWD)/vendor/sysdrain all install \
		"DEBUG=$(DEBUG)" \
		"INSTALL_PATH=$(VENDOR_BIN)"
	$(PWD)/vendor/lut/manage.sh -i -p "$(VENDOR_BIN)"
	export PATH="$(PATH):$(VENDOR_BIN)" ; \
		$(PWD)/test ; \
		$(PWD)/test.sh $(CHECKARGS)
	$(PWD)/vendor/lut/manage.sh -u -p "$(VENDOR_BIN)"
	make -C $(PWD)/vendor/sysdrain uninstall clean \
		"INSTALL_PATH=$(VENDOR_BIN)"

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

devenv:
	$(PWD)/vendor/lut/sh/lsb_runner.sh -p "extras/packages"
