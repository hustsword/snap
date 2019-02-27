#
# Copyright 2017 International Business Machines
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

#
# Generate HDL version of the HLS sources
#
# The generated HDL depends on the chip which is used and
# therefore must match what is being used to build the
# toplevel SNAP bitstream.
#
# FIXME Pass part_number and other parameters from toplevel
#      build-system as required.
#

# This is solution specific. Check if we can replace this by generics too.

# Finding $SNAP_ROOT
ifndef SNAP_ROOT
# check if we are in sw folder of an action (three directories below snap root)
ifneq ("$(wildcard ../../../../ActionTypes.md)","")
SNAP_ROOT=$(abspath ../../../../)
export SNAP_ROOT=$(abspath ../../../../)
else
$(info You are not building your software from the default directory (/path/to/snap/actions/<action_name>/sw) or specified a wrong $$SNAP_ROOT.)
$(error Please make sure that $$SNAP_ROOT is set up correctly.)
endif
endif

CXX=g++
EXTRA_CFLGAS+= -Wall -Wendif-labels -Wmissing-format-attribute -Wformat-security -fno-strict-aliasing -fwrapv -Wno-format-truncation -Wformat -Werror=format-security -Wdate-time  -fdebug-prefix-map=/build/postgresql-10-ByQ2hv/postgresql-10-10.6=. -fstack-protector-strong  

CPPFLAGS+= -O3 $(EXTRA_CFLGAS) -g -fPIC -I. -I./ -I/usr/include/postgresql/10/server -I/usr/include/postgresql/internal  -D_FORTIFY_SOURCE=2 -D_GNU_SOURCE -I/usr/include/libxml2  -I/usr/include/mit-krb5 

LDFLAGS+= $(EXTRA_CFLGAS) -g -fPIC -L/usr/lib/powerpc64le-linux-gnu -Wl,-Bsymbolic-functions -Wl,-z,relro -Wl,-z,now  -L/usr/lib/powerpc64le-linux-gnu/mit-krb5 -Wl,--as-needed

LDLIBS+= -lcxl -lpthread -lsnap

all: config all_build

config:
	./action_config.sh

fregex.o: utils/fregex.cpp
	$(CXX) -c $(CPPFLAGS) $($(@:.o=)_CPPFLAGS) -DNODEBUG $< -o $@

regex_config.o: utils/regex_config.cpp
	$(CXX) -c $(CPPFLAGS) $($(@:.o=)_CPPFLAGS) -DNODEBUG $< -o $@
	
projs += psql_regex_capi

psql_regex_capi: fregex.o regex_config.o psql_regex_capi.o
	$(CXX) $(LDFLAGS) $($(@)_LDFLAGS) $($(@)_objs) $($(@)_libs) -shared -o $@.so $^ $(LDLIBS) 
	sudo ./install.sh
	psql -f ./uninstall_psql_regex_capi.sql
	#psql -f ./psql_regex_capi.source

# If you have the host code outside of the default snap directory structure, 
# change to /path/to/snap/actions/software.mk
include $(SNAP_ROOT)/actions/software.mk
