#!/bin/bash
snap_maint -vv
snap_nvdla --loadable $ACTION_ROOT/sw/nvdla-sw/regression/flatbufs/kmd/CDP/CDP_L0_0_small_fbuf --rawdump -vv
