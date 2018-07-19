############################################################################
############################################################################
##
## Copyright 2016-2018 International Business Machines
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions AND
## limitations under the License.
##
############################################################################
############################################################################

set root_dir      $::env(SNAP_HARDWARE_ROOT)
set logs_dir      $::env(LOGS_DIR)
set logfile       $logs_dir/snap_build.log
set fpgacard      $::env(FPGACARD)
set ila_debug     [string toupper $::env(ILA_DEBUG)]
set vivadoVer     [version -short]

#Checkpoint directory
set dcp_dir $root_dir/build/Checkpoints
set ::env(DCP_DIR) $dcp_dir

#Report directory
set rpt_dir        $root_dir/build/Reports
set ::env(RPT_DIR) $rpt_dir

#Image directory
set img_dir $root_dir/build/Images
set ::env(IMG_DIR) $img_dir

#Remove temp files
set ::env(REMOVE_TMP_FILES) TRUE

#Define widths of each column
set widthCol1 24
set widthCol2 24
set widthCol3 36
set widthCol4 22
set ::env(WIDTHCOL1) $widthCol1
set ::env(WIDTHCOL2) $widthCol2
set ::env(WIDTHCOL3) $widthCol3
set ::env(WIDTHCOL4) $widthCol4


##
## open snap project
puts [format "%-*s%-*s%-*s%-*s"  $widthCol1 "" $widthCol2 "open framework project" $widthCol3 "" $widthCol4 "[clock format [clock seconds] -format {%T %a %b %d %Y}]"]
open_project $root_dir/viv_project/framework.xpr >> $logfile

# for test!
#set_param synth.elaboration.rodinMoreOptions {set rt::doParallel false}

##
## synthesis project
set step      synth_design
set logfile   $log_dir/${step}.log
set directive [get_property STEPS.SYNTH_DESIGN.ARGS.DIRECTIVE [get_runs synth_1]]
#set command   "synth_design -mode default -directive $directive"
#set command   "synth_design -mode default -keep_equivalent_registers -directive $directive"
set command   "synth_design -mode default -keep_equivalent_registers -directive $directive -fanout_limit 12"
puts [format "%-*s %-*s %-*s %-*s"  $widthCol1 "" $widthCol2 "start synthesis" $widthCol3 "with directive: $directive" $widthCol4 "[clock format [clock seconds] -format {%T %a %b %d %Y}]"]

if { [catch "$command > $logfile" errMsg] } {
  puts [format "%-*s %-*s %-*s %-*s"  $widthCol1 "" $widthCol2 "" $widthCol3 "ERROR: synthesis failed" $widthCol4 "" ]
  puts [format "%-*s %-*s %-*s %-*s"  $widthCol1 "" $widthCol2 "" $widthCol3 "       please check $logfile" $widthCol4 "" ]

  if { ![catch {current_instance}] } {
      write_checkpoint -force ./Checkpoints/${step}_error.dcp    >> $logfile
  }
  exit 42
} else {
  write_checkpoint   -force ./Checkpoints/${step}.dcp          >> $logfile
  report_utilization -file  ./Reports/${step}_utilization.rpt -quiet
}


##
## run synthese
source $root_dir/setup/snap_synth_step.tcl


##
## Vivado 2017.4 has problems to place the SNAP core logic, if they can place inside the PSL
if { $vivadoVer == "2017.4" } {
  puts [format "%-*s %-*s %-*s %-*s"  $widthCol1 "" $widthCol2 "reload opt_desing DCP" $widthCol3 "" $widthCol4 "[clock format [clock seconds] -format {%T %a %b %d %Y}]"]
  close_project                              >> $log_file
  open_checkpoint ./Checkpoints/${step}.dcp  >> $log_file


  puts [format "%-*s %-*s %-*s %-*s"  $widthCol1 "" $widthCol2 "Prevent placing inside" $widthCol3 "PSL" $widthCol4 "[clock format [clock seconds] -format {%T %a %b %d %Y}]"]
  set_property EXCLUDE_PLACEMENT 1 [get_pblocks b_nestedpsl]
}

##
## placing design
set step      place_design
set logfile   $log_dir/${step}.log
if { $vivadoVer == "2017.4" } {
  set directive Explore
} else {
  set directive [get_property STEPS.PLACE_DESIGN.ARGS.DIRECTIVE [get_runs impl_1]]
}
set command   "place_design -directive $directive"
puts [format "%-*s %-*s %-*s %-*s"  $widthCol1 "" $widthCol2 "start place_design" $widthCol3 "with directive: $directive" $widthCol4 "[clock format [clock seconds] -format {%T %a %b %d %Y}]"]

if { [catch "$command > $logfile" errMsg] } {
  puts [format "%-*s %-*s %-*s %-*s"  $widthCol1 "" $widthCol2 "" $widthCol3 "ERROR: place_design failed" $widthCol4 "" ]
  puts [format "%-*s %-*s %-*s %-*s"  $widthCol1 "" $widthCol2 "" $widthCol3 "       please check $logfile" $widthCol4 "" ]

  if { ![catch {current_instance}] } {
    write_checkpoint -force ./Checkpoints/${step}_error.dcp    >> $logfile
  }
  exit 42
} else {
  write_checkpoint   -force ./Checkpoints/${step}.dcp          >> $logfile
}

##
## physical optimizing design
set step      phys_opt_design
set logfile   $log_dir/${step}.log
if { $vivadoVer == "2017.4" } {
  set directive AggressiveExplore
  #set directive Explore
  #set directive AggressiveFanoutOpt
} else {
  set directive [get_property STEPS.PHYS_OPT_DESIGN.ARGS.DIRECTIVE [get_runs impl_1]]
}
#set command   "phys_opt_design  -fanout_opt -bram_register_opt -directive $directive"
set command   "phys_opt_design  -directive $directive"
puts [format "%-*s %-*s %-*s %-*s"  $widthCol1 "" $widthCol2 "start phys_opt_design" $widthCol3 "with directive: $directive" $widthCol4 "[clock format [clock seconds] -format {%T %a %b %d %Y}]"]

if { [catch "$command > $logfile" errMsg] } {
  puts [format "%-*s %-*s %-*s %-*s"  $widthCol1 "" $widthCol2 "" $widthCol3 "ERROR: phys_opt_design failed" $widthCol4 "" ]
  puts [format "%-*s %-*s %-*s %-*s"  $widthCol1 "" $widthCol2 "" $widthCol3 "       please check $logfile" $widthCol4 "" ]

  if { ![catch {current_instance}] } {
    write_checkpoint -force ./Checkpoints/${step}_error.dcp    >> $logfile
  }
  exit 42
} else {
  write_checkpoint   -force ./Checkpoints/${step}.dcp          >> $logfile
}


##
## routing design
set step      route_design
set logfile   $log_dir/${step}.log
if { $vivadoVer == "2017.4" } {
  set directive Explore
} else {
  set directive [get_property STEPS.ROUTE_DESIGN.ARGS.DIRECTIVE [get_runs impl_1]]
}
set command   "route_design -directive $directive"
puts [format "%-*s %-*s %-*s %-*s"  $widthCol1 "" $widthCol2 "start route_design" $widthCol3 "with directive: $directive" $widthCol4 "[clock format [clock seconds] -format {%T %a %b %d %Y}]"]

if { [catch "$command > $logfile" errMsg] } {
  puts [format "%-*s %-*s %-*s %-*s"  $widthCol1 "" $widthCol2 "" $widthCol3 "ERROR: route_design failed" $widthCol4 "" ]
  puts [format "%-*s %-*s %-*s %-*s"  $widthCol1 "" $widthCol2 "" $widthCol3 "       please check $logfile" $widthCol4 "" ]

  if { ![catch {current_instance}] } {
    write_checkpoint -force ./Checkpoints/${step}_error.dcp    >> $logfile
  }
  exit 42
} else {
  write_checkpoint   -force ./Checkpoints/${step}.dcp          >> $logfile
}


##
## physical optimizing routed design
set step      opt_routed_design
set logfile   $log_dir/${step}.log
if { $vivadoVer == "2017.4" } {
  set directive Explore
} else {
  set directive [get_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.ARGS.DIRECTIVE [get_runs impl_1]]
}
set command   "phys_opt_design  -directive $directive"
puts [format "%-*s %-*s %-*s %-*s"  $widthCol1 "" $widthCol2 "start opt_routed_design" $widthCol3 "with directive: $directive" $widthCol4 "[clock format [clock seconds] -format {%T %a %b %d %Y}]"]

if { [catch "$command > $logfile" errMsg] } {
  puts [format "%-*s %-*s %-*s %-*s"  $widthCol1 "" $widthCol2 "" $widthCol3 "ERROR: opt_routed_design failed" $widthCol4 "" ]
  puts [format "%-*s %-*s %-*s %-*s"  $widthCol1 "" $widthCol2 "" $widthCol3 "       please check $logfile" $widthCol4 "" ]

}

##
## ToDo: necessary for other cards?
if { $fpgacard != "AD8K5" } {
  puts [format "%-*s%-*s%-*s%-*s"  $widthCol1 "" $widthCol2 "" $widthCol3 "reread snap_impl.xdc" $widthCol4 "[clock format [clock seconds] -format {%T %a %b %d %Y}]"]
  read_xdc $root_dir/setup/snap_impl.xdc >> $logfile
}


##
## run implementation in the base flow
set ::env(IMPL_FLOW) BASE
source $root_dir/setup/snap_impl_step.tcl


##
## writing bitstream
source $root_dir/setup/snap_bitstream_step.tcl


##
## writing debug probes
if { $ila_debug == "TRUE" } {
  set step     write_debug_probes
  set logfile  $logs_dir/${step}.log
  puts [format "%-*s%-*s%-*s%-*s"  $widthCol1 "" $widthCol2 "writing debug probes" $widthCol3 "" $widthCol4 "[clock format [clock seconds] -format {%T %a %b %d %Y}]"]
  write_debug_probes $img_dir/$IMAGE_NAME.ltx >> $logfile
}


##
## removing temporary checkpoint files
if { $::env(REMOVE_TMP_FILES) == "TRUE" } {
  puts [format "%-*s%-*s%-*s%-*s"  $widthCol1 "" $widthCol2 "removing temp files" $widthCol3 "" $widthCol4 "[clock format [clock seconds] -format {%T %a %b %d %Y}]"]
  exec rm -rf $dcp_dir/synth.dcp
  exec rm -rf $dcp_dir/opt_design.dcp
  exec rm -rf $dcp_dir/place_design.dcp
  exec rm -rf $dcp_dir/phys_opt_design.dcp
  exec rm -rf $dcp_dir/route_design.dcp
}

close_project >> $logfile
