set action_hw $::env(ACTION_ROOT)/hw
set verilog_dir  $::env(ACTION_ROOT)

add_files -scan_for_includes -norecurse $action_hw

#User IPs
foreach usr_ip [list \
                $verilog_dir/ip/fifo_sync_32_512i512o  \
               ] {
  foreach usr_ip_xci [exec find $usr_ip -name *.xci] {
    puts "                        importing user IP $usr_ip_xci (in string_match core)"
    add_files -norecurse $usr_ip_xci >> $log_file
    set_property generate_synth_checkpoint false  [get_files "$usr_ip_xci"]
    export_ip_user_files -of_objects  [get_files "$usr_ip_xci"] -force >> $log_file
  }
}
