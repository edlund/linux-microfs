
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

ifdef DEBUG
ccflags-y := -DDEBUG
$(info DEBUG build)
ifdef DEBUG_SPAM
ccflags-y += -DDEBUG_SPAM
$(info DEBUG_SPAM build)
endif
ifdef DEBUG_INODES
ccflags-y += -DDEBUG_INODES
$(info DEBUG_INODES build)
endif
endif

ifndef CC
CC := gcc
endif
ifndef HOSTCC
HOSTCC := gcc
endif

ifndef INSTALL_HOSTPROG_PATH
INSTALL_HOSTPROG_PATH := /bin/
endif

HOST_EXTRACFLAGS += \
	-Wall \
	-Wextra \
	-pedantic \
	-std=c11 \
	-D_GNU_SOURCE \
	-ggdb

# clean-files := ...

obj-m := microfs.o
microfs-y := \
	microfs_super.o \
	microfs_read.o \
	microfs_inflate.o \
	microfs_inode.o \
	microfs_compat.o

hostprogs-y := \
	microfscki \
	microfsmki \
	test

microfscki-objs := hostprog_microfscki.o hostprogs.o
HOSTLOADLIBES_microfscki := -lrt -lz

microfsmki-objs := hostprog_microfsmki.o hostprogs.o
HOSTLOADLIBES_microfsmki := -lrt -lz

test-objs := \
	test.o \
	test_master.o \
	test_hostprogs.o \
	hostprogs.o
HOSTLOADLIBES_test := -lcheck -lm -lrt -lpthread

always := $(hostprogs-y)

requirevar-%:
	@if test "$($(*))" = "" ; then \
		echo "Required variable $(*) not set"; \
		exit 1; \
	fi

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	make -C $(PWD)/tools -f Makefile.extra all CC=$(HOSTCC)

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
	cp $(PWD)/microfscki $(INSTALL_HOSTPROG_PATH)
	cp $(PWD)/microfsmki $(INSTALL_HOSTPROG_PATH)
