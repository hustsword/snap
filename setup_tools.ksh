#!/bin/bash
if [ -z $CTEPATH ]; then
    echo "Please set your $CTEPATH"
    echo "export CTEPATH=<your ctepath>"
    exit 1
fi

export XILINX_VIVADO=$CTEPATH/tools/xilinx/2018.2/Vivado/2018.2
export XILINXD_LICENSE_FILE=2100@pokwinlic1.pok.ibm.com

export CDS_INST_DIR=$CTEPATH/tools/cds/Xcelium/18.03
export CDS_LIC_FILE=5280@auslnxlic01.austin.ibm.com

export PATH=${CDS_INST_DIR}/tools/bin:${XILINX_VIVADO}/bin:${XILINX_HLS}/bin:$PATH
export UVM_HOME=$CTEPATH/tools/cds/Incisiv/latest/tools/methodology/UVM/CDNS-1.2/

#use C++ 4.8
export PATH=/opt/rh/devtoolset-2/root/usr/bin/:$PATH

# Please don't commit the IES_LIBS settings if you set it to your own directory ... 
# Set IES_LIBS manually every time you source setup.ksh ...
unset  IES_LIBS
echo "Set up completed."

if [ -z $IES_LIBS ]; then
    echo "Please set IES_LIBS to the ies lib path by the following command:"
    echo "export IES_LIBS=<your ies path>"
fi
