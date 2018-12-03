`ifndef _DBB_CHECK_SCOREBOARD_
`define _DBB_CHECK_SCOREBOARD_

//------------------------------------------------------------------------------
//
// CLASS: dbb_check_sbd
//
//------------------------------------------------------------------------------
`uvm_analysis_imp_decl(_dbb_check)

class dbb_check_scoreboard extends uvm_component;

    string tID;

    uvm_analysis_imp_dbb_check #(dbb_check_transaction, dbb_check_scoreboard) aimp_dbb_check;

    int total_num;
    int compare_num;

    bit [7:0] memory_model[longint unsigned];    
    bit memory_tag[longint unsigned];    

    `uvm_component_utils_begin(dbb_check_scoreboard)
    `uvm_component_utils_end

    extern function new(string name = "dbb_check_scoreboard", uvm_component parent = null);

    extern function void build_phase(uvm_phase phase);
    extern function void connect_phase(uvm_phase phase);
    //extern function void end_of_elaboration_phase(uvm_phase phase);
    //extern function void start_of_simulation_phase(uvm_phase phase);
    //extern task          run_phase(uvm_phase phase);
    //extern task          reset_phase(uvm_phase phase);
    //extern task          configure_phase(uvm_phase phase);
    extern task          main_phase(uvm_phase phase);
    //extern task          shutdown_phase(uvm_phase phase);
    //extern function void extract_phase(uvm_phase phase);
    extern function void check_phase(uvm_phase phase);
    //extern function void report_phase(uvm_phase phase);
    //extern function void final_phase(uvm_phase phase);

    extern function bit exist_data(longint unsigned addr, int byte_size);
    extern function bit check_data_err(bit[63:0] addr, bit[511:0] data, int byte_size);
    extern function dbb_check_transaction get_mem_trans(bit[63:0] addr, int byte_size);

    extern function void print_mem();

    extern function void write_dbb_check(dbb_check_transaction dbb_check_tran);
    extern function void reset();
    extern function void result_check();
    extern task check_txn();

endclass : dbb_check_scoreboard

function dbb_check_scoreboard::new(string name = "dbb_check_scoreboard", uvm_component parent = null);
    super.new(name, parent);
    tID = get_type_name();
    aimp_dbb_check = new("aimp_dbb_check", this);
    total_num = 0;
    compare_num = 0;

endfunction : new

function void dbb_check_scoreboard::build_phase(uvm_phase phase);
    super.build_phase(phase);
    `uvm_info(tID, $sformatf("build_phase begin ..."), UVM_HIGH)
endfunction : build_phase

function void dbb_check_scoreboard::connect_phase(uvm_phase phase);
    super.connect_phase(phase);
    `uvm_info(tID, $sformatf("connect_phase begin ..."), UVM_HIGH)
endfunction : connect_phase

task dbb_check_scoreboard::main_phase(uvm_phase phase);
    super.run_phase(phase);
    `uvm_info(tID, $sformatf("run_phase begin ..."), UVM_LOW)
    fork
        reset();
        check_txn();
    join
endtask: main_phase

function void dbb_check_scoreboard::check_phase(uvm_phase phase);
    super.check_phase(phase);
    `uvm_info(tID, $sformatf("check_phase begin ..."), UVM_HIGH)
    result_check();
endfunction : check_phase

function bit dbb_check_scoreboard::check_data_err(bit[63:0] addr, bit[511:0] data, int byte_size);
    for(int i=0; i<byte_size; i++)begin
        if(data[8*i+7-:8] != memory_model[addr+i])begin
            return 1;
        end
    end
    return 0;
endfunction: check_data_err

function bit dbb_check_scoreboard::exist_data(longint unsigned addr, int byte_size);
    for(int i=0; i<byte_size; i++)begin
        if(!memory_tag.exists(addr+i))
            return 0;
    end
    return 1;
endfunction: exist_data

function void dbb_check_scoreboard::print_mem();
        foreach(memory_tag[i])begin
            $display("Memory addr:%h,data:%h.", i, memory_model[i]);
        end
endfunction: print_mem

function dbb_check_transaction dbb_check_scoreboard::get_mem_trans(bit[63:0] addr, int byte_size);
    get_mem_trans = new("get_mem_trans");
    get_mem_trans.addr=addr;
    get_mem_trans.byte_size=byte_size;
    for(int i=0; i<byte_size; i++)
        get_mem_trans.data[i*8+7-:8]=memory_model[addr+i];
endfunction: get_mem_trans

function void dbb_check_scoreboard::reset();
    foreach(memory_model[i])   
        memory_model[i]=0;
    foreach(memory_tag[i])   
        memory_tag[i]=0;
    total_num = 0;
    compare_num = 0;
endfunction : reset

function void dbb_check_scoreboard::write_dbb_check(dbb_check_transaction dbb_check_tran);
    if(dbb_check_tran.trans == dbb_check_transaction::WRITE)begin
        for(int i=0; i<dbb_check_tran.byte_size; i++)begin
            memory_tag[dbb_check_tran.addr+i] = 1'b1;
            memory_model[dbb_check_tran.addr+i] = dbb_check_tran.data[8*i+7-:8];
        end
    end
    else begin
        if(exist_data(dbb_check_tran.addr, dbb_check_tran.byte_size) == 1 && check_data_err(dbb_check_tran.addr, dbb_check_tran.data, dbb_check_tran.byte_size) == 1)begin
            `uvm_error(tID, $sformatf("DBB read data miscompare!\nThe expected trans is:\n%s and the response trans is:\n%s", get_mem_trans(dbb_check_tran.addr, dbb_check_tran.byte_size).sprint(), dbb_check_tran.sprint()))
        end
        if(exist_data(dbb_check_tran.addr, dbb_check_tran.byte_size) == 1 && check_data_err(dbb_check_tran.addr, dbb_check_tran.data, dbb_check_tran.byte_size)== 0)begin
            `uvm_info(tID, $sformatf("DBB read data compared successfully!\nThe trans is:\n%s", dbb_check_tran.sprint()), UVM_LOW)
            compare_num++;
        end
    end
    total_num++;
    //dbb_check_tran_q.push_back(dbb_check_tran);
endfunction : write_dbb_check

task dbb_check_scoreboard::check_txn();
endtask : check_txn

function void dbb_check_scoreboard::result_check();   
    `uvm_info(tID, $sformatf("The total number of transactions detected in DBB scoreboard is%d, and the compared number is %d.", total_num, compare_num), UVM_LOW);                    
endfunction : result_check
`endif

