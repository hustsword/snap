set action_hw $::env(ACTION_ROOT)/hw
set regex_verilog_dir  $::env(ACTION_ROOT)/hw/engines/regex/
set regex_ipdir $::env(ACTION_ROOT)/ip/engines/regex/

add_files -scan_for_includes -norecurse $action_hw
add_files -scan_for_includes -norecurse $regex_verilog_dir/snap_adapter
add_files -scan_for_includes -norecurse $regex_verilog_dir/core

#User IPs
foreach usr_ip [list \
                $regex_ipdir/bram_1744x16                   \
                $regex_ipdir/bram_dual_port_64x512          \
                $regex_ipdir/fifo_48x16_async               \
                $regex_ipdir/fifo_512x64_sync_bram          \
                $regex_ipdir/fifo_80x16_async               \
                $regex_ipdir/unit_fifo_48x16_async          \
                $regex_ipdir/fifo_sync_32_512i512o          \
                $regex_ipdir/fifo_sync_32_5i5o              \
                $regex_ipdir/ram_512i_512o_dual_64          \
               ] {
  foreach usr_ip_xci [exec find $usr_ip -name *.xci] {
    puts "                        importing user IP $usr_ip_xci (in string_match core)"
    add_files -norecurse $usr_ip_xci >> $log_file
    set_property generate_synth_checkpoint false  [ get_files $usr_ip_xci] >> $log_file
    generate_target {instantiation_template}      [ get_files $usr_ip_xci] >> $log_file
    generate_target all                           [ get_files $usr_ip_xci] >> $log_file
    export_ip_user_files -of_objects              [ get_files $usr_ip_xci] -no_script -sync -force -quiet >> $log_file
  }
}

# Set the action_string_match.v file to systemverilog mode for $clog2()
# support
set_property file_type SystemVerilog [get_files string_match_core_top.v]
