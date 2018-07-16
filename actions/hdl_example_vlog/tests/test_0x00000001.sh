#!/bin/bash
echo $SNAP_ROOT
echo $ACTION_ROOT
$SNAP_ROOT/software/tools/snap_maint -vv
$ACTION_ROOT/sw/snap_example_vlog -vv -t 10

