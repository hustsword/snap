set action_hw $::env(ACTION_ROOT)/hw
set verilog_dir  $::env(ACTION_ROOT)/hw/verilog
set action_ipdir $::env(ACTION_ROOT)/fpga_ip

add_files -scan_for_includes -norecurse $action_hw
add_files -scan_for_includes -norecurse $verilog_dir/snap_adapter
add_files -scan_for_includes -norecurse $verilog_dir/core

#User IPs
foreach usr_ip [list \
                $action_ipdir/bram_1744x16                   \
                $action_ipdir/bram_dual_port_64x512          \
                $action_ipdir/fifo_48x16_async               \
                $action_ipdir/fifo_512x64_sync_bram          \
                $action_ipdir/fifo_80x16_async               \
                $action_ipdir/unit_fifo_48x16_async          \
                $action_ipdir/fifo_sync_32_512i512o          \
                $action_ipdir/fifo_sync_32_5i5o              \
                $action_ipdir/ram_512i_512o_dual_64          \
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

