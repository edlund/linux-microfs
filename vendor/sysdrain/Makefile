
# sysdrain - Drain CPU and RAM for fun and profit
# Copyright (C) 2014 Erik Edlund <erik.edlund@32767.se>
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

PWD := $(shell pwd)

ifndef INSTALL_PATH
INSTALL_PATH := /usr/local/bin
endif

CXXFLAGS += \
	-Wall \
	-Wextra \
	-pedantic \
	-std=c++11 \
	-D_FILE_OFFSET_BITS=64

ifdef DEBUG
CXXFLAGS += -ggdb
$(info DEBUG build)
endif

LDFLAGS += \
	-pthread

all:
	g++ $(CXXFLAGS) sysdrain.cpp $(LDFLAGS) -o sysdrain

clean:
	rm -f sysdrain

install:
	cp $(PWD)/sysdrain $(INSTALL_PATH)

uninstall:
	rm $(INSTALL_PATH)/sysdrain

