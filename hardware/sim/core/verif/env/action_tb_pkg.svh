`ifndef _ACTION_TB_PKG_SVH_
`define _ACTION_TB_PKG_SVH_

//UVM ENV
import uvm_pkg::*;
`include "uvm_macros.svh"

//AXI VIP PKG
import axi_vip_pkg::*;
import axi_vip_dbb_check_pkg::*;
`define AXI_VIP_DBB_CHECK_PARAMS  #(axi_vip_dbb_check_VIP_PROTOCOL, axi_vip_dbb_check_VIP_ADDR_WIDTH, axi_vip_dbb_check_VIP_DATA_WIDTH, axi_vip_dbb_check_VIP_DATA_WIDTH, axi_vip_dbb_check_VIP_ID_WIDTH, axi_vip_dbb_check_VIP_ID_WIDTH, axi_vip_dbb_check_VIP_AWUSER_WIDTH, axi_vip_dbb_check_VIP_WUSER_WIDTH, axi_vip_dbb_check_VIP_BUSER_WIDTH, axi_vip_dbb_check_VIP_ARUSER_WIDTH, axi_vip_dbb_check_VIP_RUSER_WIDTH, axi_vip_dbb_check_VIP_SUPPORTS_NARROW, axi_vip_dbb_check_VIP_HAS_BURST, axi_vip_dbb_check_VIP_HAS_LOCK, axi_vip_dbb_check_VIP_HAS_CACHE, axi_vip_dbb_check_VIP_HAS_REGION, axi_vip_dbb_check_VIP_HAS_PROT, axi_vip_dbb_check_VIP_HAS_QOS, axi_vip_dbb_check_VIP_HAS_WSTRB, axi_vip_dbb_check_VIP_HAS_BRESP, axi_vip_dbb_check_VIP_HAS_RRESP, axi_vip_dbb_check_VIP_HAS_ARESETN)

//DBB CHECK AGENT
`include "../dbb_check_agent/dbb_check_pkg.svh"

//VERIF TOP
`include "action_tb_env.sv"
`include "action_tb_base_test.sv"

`endif // _ACTION_TB_PKG_SVH_
