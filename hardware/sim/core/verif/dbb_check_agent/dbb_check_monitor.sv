`ifndef _DBB_CHECK_MONITOR_SV_
`define _DBB_CHECK_MONITOR_SV_

//-------------------------------------------------------------------------------------
//
// CLASS: dbb_check_monitor
//
// XXX
//-------------------------------------------------------------------------------------

class dbb_check_monitor extends uvm_monitor;

    string                tID;
    virtual interface axi_vip_if `AXI_VIP_DBB_CHECK_PARAMS dbb_check_agent_vif;
    axi_vip_dbb_check_passthrough_t  axi_vip_dbb_check_passthrough;    
    axi_monitor_transaction dbb_monitor_transaction;    

    dbb_check_transaction      txn;
    uvm_analysis_port #(dbb_check_transaction) dbb_check_tran_port;

    int rd_num;
    int wr_num;
    int void_num;
    bit [31:0] data_size;        
    //------------------------CONFIGURATION PARAMETERS--------------------------------
    // DBB_CHECK_MONITOR Configuration Parameters. These parameters can be controlled through 
    // the UVM configuration database
    // @{

    // Trace player has three work modes for different usages:
    // CMOD_ONLY:   only cmod is working
    // RTL_ONLY:    only DUT rtl is working
    // CROSS_CHECK: default verfication mode, cross check between RTL and CMOD
    string                      work_mode = "CROSS_CHECK";
    //event                       action_tb_finish;

    // }@

    `uvm_component_utils_begin(dbb_check_monitor)
        `uvm_field_string(work_mode, UVM_ALL_ON)
    `uvm_component_utils_end

    extern function new(string name = "dbb_check_monitor", uvm_component parent = null);

    // UVM Phases
    // Can just enable needed phase
    // @{

    extern function void build_phase(uvm_phase phase);
    extern function void connect_phase(uvm_phase phase);
    //extern function void end_of_elaboration_phase(uvm_phase phase);
    extern function void start_of_simulation_phase(uvm_phase phase);
    extern task          run_phase(uvm_phase phase);
    //extern task          reset_phase(uvm_phase phase);
    //extern task          configure_phase(uvm_phase phase);
    extern task          main_phase(uvm_phase phase);
    extern task          shutdown_phase(uvm_phase phase);
    //extern function void extract_phase(uvm_phase phase);
    //extern function void check_phase(uvm_phase phase);
    //extern function void report_phase(uvm_phase phase);
    //extern function void final_phase(uvm_phase phase);

    // }@
    extern task collect_transactions();

endclass : dbb_check_monitor

// Function: new
// Creates a new dbb check monitor
function dbb_check_monitor::new(string name = "dbb_check_monitor", uvm_component parent = null);
    super.new(name, parent);
    tID = get_type_name();    
    dbb_check_tran_port = new("dbb_check_tran_port", this);
    wr_num = 0;
    rd_num = 0;
    void_num = 0;   
endfunction : new

// Function: build_phase
// XXX
function void dbb_check_monitor::build_phase(uvm_phase phase);
    super.build_phase(phase);
    `uvm_info(tID, $sformatf("build_phase begin ..."), UVM_LOW)

    //if($value$plusargs("WORK_MODE=%0s", work_mode)) begin
    //    `uvm_info(tID, $sformatf("Setting WORK_MODE:%0s", work_mode), UVM_MEDIUM)
    //end
endfunction : build_phase

// Function: connect_phase
// XXX
function void dbb_check_monitor::connect_phase(uvm_phase phase);
    super.connect_phase(phase);
    `uvm_info(tID, $sformatf("connect_phase begin ..."), UVM_LOW)

    if(!uvm_config_db#(virtual axi_vip_if `AXI_VIP_DBB_CHECK_PARAMS)::get(this, "", "dbb_check_agent_vif", dbb_check_agent_vif)) begin
        `uvm_fatal(tID, "No virtual interface specified fo dbb_check_monitor")
    end 
    axi_vip_dbb_check_passthrough = new("axi_vip_dbb_check_passthrough", dbb_check_agent_vif);    
    endfunction : connect_phase

function void dbb_check_monitor::start_of_simulation_phase(uvm_phase phase);
    super.start_of_simulation_phase(phase);
    `uvm_info(tID, $sformatf("start_of_simulation_phase begin ..."), UVM_LOW)

endfunction : start_of_simulation_phase
// Task: main_phase
// XXX
task dbb_check_monitor::run_phase(uvm_phase phase);

    super.run_phase(phase);
    `uvm_info(tID, $sformatf("run_phase begin ..."), UVM_LOW)
        
endtask : run_phase

// Task: main_phase
// XXX
task dbb_check_monitor::main_phase(uvm_phase phase);
    super.main_phase(phase);
    `uvm_info(tID, $sformatf("main_phase begin ..."), UVM_LOW)
    //axi_vip_dbb_check_passthrough.start_master();        
    //axi_vip_dbb_check_passthrough.start_slave();
    axi_vip_dbb_check_passthrough.start_monitor();
    collect_transactions();
endtask : main_phase

task dbb_check_monitor::shutdown_phase(uvm_phase phase);
    super.shutdown_phase(phase);
    `uvm_info(tID, $sformatf("shutdown_phase begin ..."), UVM_LOW)
endtask : shutdown_phase;

task dbb_check_monitor::collect_transactions();
    `uvm_info(tID, $sformatf("dbb check collect_transactions begin ..."), UVM_MEDIUM)  
    forever begin
        dbb_monitor_transaction=new("dbb_monitor_transaction");
        txn=new();
        data_size = 32'b1;
        //Turn off current monitor transaction depth check 
        axi_vip_dbb_check_passthrough.monitor.disable_transaction_depth_checks();
        axi_vip_dbb_check_passthrough.monitor.item_collected_port.get(dbb_monitor_transaction);
        if(dbb_monitor_transaction.get_cmd_type()== XIL_AXI_READ) begin
            rd_num ++;
            txn.trans=dbb_check_transaction::READ;
            txn.addr=dbb_monitor_transaction.get_addr();
            txn.data=dbb_monitor_transaction.get_data_beat(0);
            txn.byte_size=(data_size << dbb_monitor_transaction.get_size());            
            dbb_check_tran_port.write(txn);
            `uvm_info(tID, $sformatf("DBB Monitor Detects a Read:"), UVM_LOW);
            txn.print();
        end
        else if(dbb_monitor_transaction.get_cmd_type()==XIL_AXI_WRITE) begin
            wr_num ++;
            txn.trans=dbb_check_transaction::WRITE;            
            txn.addr=dbb_monitor_transaction.get_addr();
            txn.data=dbb_monitor_transaction.get_data_beat(0);
            txn.byte_size=(data_size << dbb_monitor_transaction.get_size());            
            dbb_check_tran_port.write(txn);
            `uvm_info(tID, $sformatf("DBB Monitor Detects a Write:"), UVM_LOW);            
            txn.print();            
        end
        else begin
            `uvm_info(tID, $sformatf("DBB monitor detects nothing"), UVM_LOW)
            void_num ++;
        end
    end
//join
endtask : collect_transactions


`endif // _DBB_CHECK_MONITOR_SV_
