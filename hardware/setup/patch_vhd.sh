#!/bin/bash
if [ "$DDRI_USED" == "TRUE" ]; then
  sed -i 's/\(.*\)\(-- only for DDRI_USED=TRUE\)\(.*\)/\1\3\2/' $1/$2
  sed -i 's/\(.*\)\(-- only for DDRI_USED!=TRUE\)\(.*\)/\2\1\3/' $1/$2
else
  sed -i 's/\(.*\)\(-- only for DDRI_USED!=TRUE\)\(.*\)/\1\3\2/' $1/$2
  sed -i 's/\(.*\)\(-- only for DDRI_USED=TRUE\)\(.*\)/\2\1\3/' $1/$2
fi

if [ "$DDR3_USED" == "TRUE" ]; then
  sed -i 's/\(.*\)\(-- only for DDR3_USED=TRUE\)\(.*\)/\1\3\2/' $1/$2
  sed -i 's/\(.*\)\(-- only for DDR3_USED!=TRUE\)\(.*\)/\2\1\3/' $1/$2
else
  sed -i 's/\(.*\)\(-- only for DDR3_USED!=TRUE\)\(.*\)/\1\3\2/' $1/$2
  sed -i 's/\(.*\)\(-- only for DDR3_USED=TRUE\)\(.*\)/\2\1\3/' $1/$2
fi

if [ "$DDR4_USED" == "TRUE" ]; then
  sed -i 's/\(.*\)\(-- only for DDR4_USED=TRUE\)\(.*\)/\1\3\2/' $1/$2
  sed -i 's/\(.*\)\(-- only for DDR4_USED!=TRUE\)\(.*\)/\2\1\3/' $1/$2
else
  sed -i 's/\(.*\)\(-- only for DDR4_USED!=TRUE\)\(.*\)/\1\3\2/' $1/$2
  sed -i 's/\(.*\)\(-- only for DDR4_USED=TRUE\)\(.*\)/\2\1\3/' $1/$2
fi

if [ "$BRAM_USED" == "TRUE" ]; then
  sed -i 's/\(.*\)\(-- only for BRAM_USED=TRUE\)\(.*\)/\1\3\2/' $1/$2
  sed -i 's/\(.*\)\(-- only for BRAM_USED!=TRUE\)\(.*\)/\2\1\3/' $1/$2
else
  sed -i 's/\(.*\)\(-- only for BRAM_USED!=TRUE\)\(.*\)/\1\3\2/' $1/$2
  sed -i 's/\(.*\)\(-- only for BRAM_USED=TRUE\)\(.*\)/\2\1\3/' $1/$2
fi

NAME=`basename $2`

if ([ "$NAME" == "psl_accel_sim.vhd" ] || [ "$NAME" == "psl_accel_syn.vhd" ]); then
  sed -i 's/C_AXI_CARD_MEM0_ID_WIDTH[ ^I]*:[ ^I]*integer[ ^I]*:=[ ^I]*[0-9]*/C_AXI_CARD_MEM0_ID_WIDTH       : integer   := '$NUM_OF_ACTIONS'/' $1/$2
  sed -i 's/C_AXI_HOST_MEM_ID_WIDTH[ ^I]*:[ ^I]*integer[ ^I]*:=[ ^I]*[0-9]*/C_AXI_HOST_MEM_ID_WIDTH        : integer   := '$NUM_OF_ACTIONS'/' $1/$2
fi