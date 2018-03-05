#!/bin/bash
. ./env.sh
#~/projects/nsa121b/capi-util-ecap-update/capi-reset.sh
echo $SNAP_ROOT
echo $ACTION_ROOT
$SNAP_ROOT/software/tools/snap_maint -vv
cp $ACTION_ROOT/tests/packet.txt packet.txt
cp $ACTION_ROOT/tests/pattern.txt pattern.txt
$ACTION_ROOT/sw/string_match -v -t 10

