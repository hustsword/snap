#!/bin/bash
. ./env.sh
#~/projects/nsa121b/capi-util-ecap-update/capi-reset.sh
echo $SNAP_ROOT
echo $ACTION_ROOT
$SNAP_ROOT/software/tools/snap_maint -vv
if [[ ! -z $1 ]]; then
    cp $ACTION_ROOT/tests/$1 packet.txt
fi
cp $ACTION_ROOT/tests/pattern.txt pattern.txt
$ACTION_ROOT/sw/direct/db_direct -f -t 10 $*
