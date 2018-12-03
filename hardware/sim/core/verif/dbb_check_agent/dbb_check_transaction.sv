
// ****************************************************************
// (C) Copyright International Business Machines Corporation 2017
//              All Rights Reserved -- Property of IBM
//                     *** IBM Confidential ***
// ****************************************************************
//------------------------------------------------------------------------------
//
// CLASS: dbb_check_transaction
//
//------------------------------------------------------------------------------
`ifndef DBB_CHECK_TRANSACTION_SV
`define DBB_CHECK_TRANSACTION_SV


class dbb_check_transaction extends uvm_sequence_item;

    typedef enum bit { READ, WRITE } uvm_dbb_check_txn_e;

    rand bit [63:0]    addr;
    rand bit [511:0]   data;
    rand int           byte_size;    
    rand uvm_dbb_check_txn_e trans;

    `uvm_object_utils_begin(dbb_check_transaction)
        `uvm_field_int      (addr,              UVM_DEFAULT)
        `uvm_field_int      (data,              UVM_DEFAULT)
        `uvm_field_int      (byte_size,          UVM_ALL_ON)        
        `uvm_field_enum     (uvm_dbb_check_txn_e, trans, UVM_ALL_ON)        
    `uvm_object_utils_end

    // new - constructor
    function new (string name = "dbb_check_transaction_inst");
        super.new(name);
    endfunction : new

    function string convert2string(); 
      return $sformatf("addr='h%h, data='h%0h", addr, data);
   endfunction : convert2string

endclass : dbb_check_transaction

`endif

