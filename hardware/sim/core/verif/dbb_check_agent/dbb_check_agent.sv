// ****************************************************************
// (C) Copyright International Business Machines Corporation 2018
//              All Rights Reserved -- Property of IBM
//                     *** IBM Confidential ***
// ****************************************************************
//------------------------------------------------------------------------------
//
// CLASS: dbb_check_agent
//
//------------------------------------------------------------------------------
`ifndef DBB_CHECK_AGENT_SV
`define DBB_CHECK_AGENT_SV


class dbb_check_agent extends uvm_agent;

    string                tID;

    dbb_check_monitor mon;
    dbb_check_scoreboard sbd;

    uvm_active_passive_enum is_active = UVM_PASSIVE;

    `uvm_component_utils_begin(dbb_check_agent)
        `uvm_field_enum(uvm_active_passive_enum, is_active, UVM_ALL_ON)
    `uvm_component_utils_end

    extern function new(string name = "dbb_check_agent", uvm_component parent = null);
    extern function void build_phase(uvm_phase phase);
    extern function void connect_phase(uvm_phase phase);

endclass : dbb_check_agent

// Function: new
// Creates a new dbb check agent
function dbb_check_agent::new(string name = "dbb_check_agent", uvm_component parent = null);
    super.new(name, parent);
    tID = get_type_name();
endfunction : new

// Function: build_phase
// XXX
function void dbb_check_agent::build_phase(uvm_phase phase);
    super.build_phase(phase);
    `uvm_info(tID, $sformatf("build_phase begin ..."), UVM_HIGH)
    mon = dbb_check_monitor::type_id::create("mon", this);
    sbd = dbb_check_scoreboard::type_id::create("sdb", this);    
endfunction : build_phase

// Function: connect_phase
// XXX
function void dbb_check_agent::connect_phase(uvm_phase phase);
    super.connect_phase(phase);
    `uvm_info(tID, $sformatf("connect_phase begin ..."), UVM_HIGH)
    //Connect monitor to scoreboard
    mon.dbb_check_tran_port.connect(sbd.aimp_dbb_check);    
endfunction : connect_phase

`endif
