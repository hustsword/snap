#!/bin/bash
############################################################################
############################################################################
##
## Copyright 2017 International Business Machines
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     http://www.apache.org/licenses/LICENSE#2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions AND
## limitations under the License.
##
############################################################################
############################################################################
echo "                        action config says ACTION_ROOT is $ACTION_ROOT"
echo "                        action config says FPGACHIP is $FPGACHIP"
echo "                        action config says there are $NUM_OF_DATABASE_KERNELS kernels used in this action"

REGEX_DESIGN=engines/regex
REGEX_IP=../ip/engines/regex
HDL_PP=$ACTION_ROOT/../../scripts/utils/hdl_pp/

if [ -L $REGEX_DESIGN ]; then
    unlink $REGEX_DESIGN 
fi

if [ -L $REGEX_IP ]; then
    unlink $REGEX_IP
fi

if [ -z $NUM_OF_DATABASE_KERNELS ]; then
    echo "No number of database kernels specified, check your snap_config!"
    exit 1
fi

STRING_MATCH_VERILOG=../string-match-fpga/verilog

# Create the link to regex engine
if [ -z $STRING_MATCH_VERILOG ]; then
    echo "WARNING!!! Please set STRING_MATCH_VERILOG to the path of string match verilog"
else
    if [ ! -d ./engines ]; then
        mkdir engines
    fi

    cd engines
    ln -s ../$STRING_MATCH_VERILOG regex 
    cd ../../

    if [ ! -d ./ip/engines ]; then
        mkdir -p ./ip/engines
    fi

    cd ip/engines
    ln -s ../../hw/$STRING_MATCH_VERILOG/../fpga_ip regex
    cd ../../hw/
fi

echo "                        Generating defs.h"
if [ ! -f ./defs.h ]; then
    touch defs.h
else
    rm defs.h
fi
echo "#define NUM_KERNELS ${NUM_OF_DATABASE_KERNELS}" > defs.h

for i in $(find -name \*.v_source); do
    vcp=${i%.v_source}.vcp
    v=${i%.v_source}.v
    echo "                         Processing $i"
    $HDL_PP/vcp -i $i -o $vcp -imacros ./defs.h || exit 1
    perl -I $HDL_PP/plugins -Meperl $HDL_PP/eperl -o $v $vcp || exit 1
done                         

# Create the IP for regex engine
if [ ! -d $STRING_MATCH_VERILOG/../fpga_ip/managed_ip_project ]; then
    echo "                        Call all_ip_gen.pl to generate regex IPs"
    $STRING_MATCH_VERILOG/../fpga_ip/all_ip_gen.pl -fpga_chip $FPGACHIP -outdir $STRING_MATCH_VERILOG/../fpga_ip
fi

# Create IP for the framework
if [ ! -d $ACTION_ROOT/ip/framework/framework_ip_prj ]; then
    echo "                        Call create_framework_ip.tcl to generate framework IPs"
    vivado -mode batch -source $ACTION_ROOT/ip/tcl/create_framework_ip.tcl -notrace -nojournal -tclargs $ACTION_ROOT $FPGACHIP $NUM_OF_DATABASE_KERNELS
fi

