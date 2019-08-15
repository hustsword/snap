
`timescale 1ns/1ps

//Don't touch the interface to connect to action-_wrapper!
module action_hdl_helloworld # (

           // Parameters of Axi Master Bus Interface AXI_CARD_MEM0 ; to DDR memory
           parameter C_M_AXI_CARD_MEM0_ID_WIDTH     = 4,
           parameter C_M_AXI_CARD_MEM0_ADDR_WIDTH   = 33,
           parameter C_M_AXI_CARD_MEM0_DATA_WIDTH   = 512,
           parameter C_M_AXI_CARD_MEM0_AWUSER_WIDTH = 1,
           parameter C_M_AXI_CARD_MEM0_ARUSER_WIDTH = 1,
           parameter C_M_AXI_CARD_MEM0_WUSER_WIDTH  = 1,
           parameter C_M_AXI_CARD_MEM0_RUSER_WIDTH  = 1,
           parameter C_M_AXI_CARD_MEM0_BUSER_WIDTH  = 1,
       
           // Parameters of Axi Slave Bus Interface AXI_CTRL_REG
           parameter C_S_AXI_CTRL_REG_DATA_WIDTH    = 32,
           parameter C_S_AXI_CTRL_REG_ADDR_WIDTH    = 32,
       
           // Parameters of Axi Master Bus Interface AXI_HOST_MEM ; to Host memory
           parameter C_M_AXI_HOST_MEM_ID_WIDTH      = 2, //to snap_core. Not used. Tie 0
           parameter ACTION_ID_WIDTH                = 4, //from snap_action_shim
           parameter C_M_AXI_HOST_MEM_ADDR_WIDTH    = 64,
           parameter C_M_AXI_HOST_MEM_DATA_WIDTH    = 512,
           parameter C_M_AXI_HOST_MEM_AWUSER_WIDTH  = 8,
           parameter C_M_AXI_HOST_MEM_ARUSER_WIDTH  = 8,
           parameter C_M_AXI_HOST_MEM_WUSER_WIDTH   = 1,
           parameter C_M_AXI_HOST_MEM_RUSER_WIDTH   = 1,
           parameter C_M_AXI_HOST_MEM_BUSER_WIDTH   = 1
)
(
                           input              clk                      ,
                           input              rst_n                    , 
                                                            

                           //---- AXI bus interfaced with SNAP core ----               
                             // AXI write address channel      
                           output    [C_M_AXI_HOST_MEM_ID_WIDTH - 1:0] m_axi_snap_awid          ,  
                           output    [C_M_AXI_HOST_MEM_ADDR_WIDTH - 1:0] m_axi_snap_awaddr        ,  
                           output    [0007:0] m_axi_snap_awlen         ,  
                           output    [0002:0] m_axi_snap_awsize        ,  
                           output    [0001:0] m_axi_snap_awburst       ,  
                           output    [0003:0] m_axi_snap_awcache       ,  
                           output    [0001:0] m_axi_snap_awlock        ,  
                           output    [0002:0] m_axi_snap_awprot        ,  
                           output    [0003:0] m_axi_snap_awqos         ,  
                           output    [0003:0] m_axi_snap_awregion      ,  
                           output    [C_M_AXI_HOST_MEM_AWUSER_WIDTH - 1:0] m_axi_snap_awuser        ,  
                           output             m_axi_snap_awvalid       ,  
                           input              m_axi_snap_awready       ,
                             // AXI write data channel         
                           output    [C_M_AXI_HOST_MEM_ID_WIDTH - 1:0] m_axi_snap_wid           , 
                           output    [C_M_AXI_HOST_MEM_DATA_WIDTH - 1:0] m_axi_snap_wdata         ,  
                           output    [(C_M_AXI_HOST_MEM_DATA_WIDTH/8) -1:0] m_axi_snap_wstrb         ,  
                           output             m_axi_snap_wlast         ,  
                           output             m_axi_snap_wvalid        ,  
                           input              m_axi_snap_wready        ,
                             // AXI write response channel     
                           output             m_axi_snap_bready        ,  
                           input     [C_M_AXI_HOST_MEM_ID_WIDTH - 1:0] m_axi_snap_bid           ,
                           input     [0001:0] m_axi_snap_bresp         ,
                           input              m_axi_snap_bvalid        ,
                             // AXI read address channel       
                           output    [C_M_AXI_HOST_MEM_ID_WIDTH - 1:0] m_axi_snap_arid          ,  
                           output    [C_M_AXI_HOST_MEM_ADDR_WIDTH - 1:0] m_axi_snap_araddr        ,  
                           output    [0007:0] m_axi_snap_arlen         ,  
                           output    [0002:0] m_axi_snap_arsize        ,  
                           output    [0001:0] m_axi_snap_arburst       ,  
                           output    [C_M_AXI_HOST_MEM_ARUSER_WIDTH - 1:0] m_axi_snap_aruser        , 
                           output    [0002:0] m_axi_snap_arcache       , 
                           output    [0001:0] m_axi_snap_arlock        ,  
                           output    [0002:0] m_axi_snap_arprot        , 
                           output    [0003:0] m_axi_snap_arqos         , 
                           output    [0008:0] m_axi_snap_arregion      , 
                           output             m_axi_snap_arvalid       , 
                           input              m_axi_snap_arready       ,
                             // AXI  ead data channel          
                           output             m_axi_snap_rready        , 
                           input     [C_M_AXI_HOST_MEM_ID_WIDTH - 1:0] m_axi_snap_rid           ,
                           input     [C_M_AXI_HOST_MEM_DATA_WIDTH - 1:0] m_axi_snap_rdata         ,
                           input     [0001:0] m_axi_snap_rresp         ,
                           input              m_axi_snap_rlast         ,
                           input              m_axi_snap_rvalid        ,

/*
                           //---- AXI bus interfaced with DDR ----               
                             // AXI write address channel      
                           output    [C_M_AXI_CARD_MEM0_ID_WIDTH - 1:0] m_axi_ddr_awid           ,  
                           output    [C_M_AXI_CARD_MEM0_ADDR_WIDTH - 1:0] m_axi_ddr_awaddr         ,  
                           output    [0007:0] m_axi_ddr_awlen          ,  
                           output    [0002:0] m_axi_ddr_awsize         ,  
                           output    [0001:0] m_axi_ddr_awburst        ,  
                           output    [0003:0] m_axi_ddr_awcache        ,  
                           output    [0001:0] m_axi_ddr_awlock         ,  
                           output    [0002:0] m_axi_ddr_awprot         ,  
                           output    [0003:0] m_axi_ddr_awqos          ,  
                           output    [0003:0] m_axi_ddr_awregion       ,  
                           output    [C_M_AXI_CARD_MEM0_AWUSER_WIDTH - 1:0] m_axi_ddr_awuser         ,  
                           output             m_axi_ddr_awvalid        ,  
                           input              m_axi_ddr_awready        ,
                             // AXI write data channel         
                           output    [C_M_AXI_CARD_MEM0_ID_WIDTH - 1:0] m_axi_ddr_wid            , 
                           output    [C_M_AXI_CARD_MEM0_DATA_WIDTH - 1:0] m_axi_ddr_wdata          ,  
                           output    [(C_M_AXI_CARD_MEM0_DATA_WIDTH/8) - 1:0] m_axi_ddr_wstrb          ,  
                           output             m_axi_ddr_wlast          ,  
                           output             m_axi_ddr_wvalid         ,  
                           input              m_axi_ddr_wready         ,
                             // AXI write response channel     
                           output             m_axi_ddr_bready         ,  
                           input     [C_M_AXI_CARD_MEM0_ID_WIDTH - 1:0] m_axi_ddr_bid            ,
                           input     [0001:0] m_axi_ddr_bresp          ,
                           input              m_axi_ddr_bvalid         ,
                             // AXI read address channel       
                           output    [C_M_AXI_CARD_MEM0_ID_WIDTH - 1:0] m_axi_ddr_arid           ,  
                           output    [C_M_AXI_CARD_MEM0_ADDR_WIDTH:0] m_axi_ddr_araddr         ,  
                           output    [0007:0] m_axi_ddr_arlen          ,  
                           output    [0002:0] m_axi_ddr_arsize         ,  
                           output    [0001:0] m_axi_ddr_arburst        ,  
                           output    [C_M_AXI_CARD_MEM0_ARUSER_WIDTH - 1:0] m_axi_ddr_aruser         , 
                           output    [0002:0] m_axi_ddr_arcache        , 
                           output    [0001:0] m_axi_ddr_arlock         ,  
                           output    [0002:0] m_axi_ddr_arprot         , 
                           output    [0003:0] m_axi_ddr_arqos          , 
                           output    [0008:0] m_axi_ddr_arregion       , 
                           output             m_axi_ddr_arvalid        , 
                           input              m_axi_ddr_arready        ,
                             // AXI  ead data channel          
                           output             m_axi_ddr_rready         , 
                           input     [C_M_AXI_CARD_MEM0_ID_WIDTH - 1:0] m_axi_ddr_rid            ,
                           input     [C_M_AXI_CARD_MEM0_DATA_WIDTH - 1:0] m_axi_ddr_rdata          ,
                           input     [0001:0] m_axi_ddr_rresp          ,
                           input              m_axi_ddr_rlast          ,
                           input              m_axi_ddr_rvalid         ,
                           */

                           //---- AXI Lite bus interfaced with SNAP core ----               
                             // AXI write address channel
                           output             s_axi_snap_awready       ,   
                           input     [C_S_AXI_CTRL_REG_ADDR_WIDTH - 1:0] s_axi_snap_awaddr        ,
                           input     [0002:0] s_axi_snap_awprot        ,
                           input              s_axi_snap_awvalid       ,
                             // axi write data channel             
                           output             s_axi_snap_wready        ,
                           input     [C_S_AXI_CTRL_REG_DATA_WIDTH - 1:0] s_axi_snap_wdata         ,
                           input     [(C_S_AXI_CTRL_REG_DATA_WIDTH/8) -1:0] s_axi_snap_wstrb         ,
                           input              s_axi_snap_wvalid        ,
                             // AXI response channel
                           output    [0001:0] s_axi_snap_bresp         ,
                           output             s_axi_snap_bvalid        ,
                           input              s_axi_snap_bready        ,
                             // AXI read address channel
                           output             s_axi_snap_arready       ,
                           input              s_axi_snap_arvalid       ,
                           input     [C_S_AXI_CTRL_REG_ADDR_WIDTH - 1:0] s_axi_snap_araddr        ,
                           input     [0002:0] s_axi_snap_arprot        ,
                             // AXI read data channel
                           output    [C_S_AXI_CTRL_REG_DATA_WIDTH - 1:0] s_axi_snap_rdata         ,
                           output    [0001:0] s_axi_snap_rresp         ,
                           input              s_axi_snap_rready        ,
                           output             s_axi_snap_rvalid        ,
                           
                           // Other signals
                           input      [31:0]  i_action_type            ,
                           input      [31:0]  i_action_version         ,
                           output             o_interrupt              ,
                           input              i_interrupt_ack
                       );

 reg [0000:0] app_ready;

always @(posedge clk) begin
  if (rst_n == 0) begin
      app_ready <= 0;
  end else begin
      app_ready <= 1;
  end
end
//All output IDs are 0.
//All input IDs are ignored.
assign m_axi_snap_awid = 0;
assign m_axi_snap_arid = 0;
assign m_axi_snap_wid = 0;

//lock signals are tied to 0
assign m_axi_snap_awlock = 0;
assign m_axi_snap_arlock = 0;

//internal engine has user signals. But interconnect kills them.
assign m_axi_snap_awuser = 0;
assign m_axi_snap_aruser = 0;


// ID fifos to deal with ID
wire rid_fifo_wen;
wire rid_fifo_ren;
wire wid_fifo_wen;
wire wid_fifo_ren;
wire rid_fifo_full;
wire rid_fifo_empty;
wire rid_wr_rst_busy;
wire rid_rd_rst_busy;
wire wid_fifo_full;
wire wid_fifo_empty;
wire wid_wr_rst_busy;
wire wid_rd_rst_busy;

wire [7:0] action_awid;
wire [7:0] action_arid;
wire [7:0] action_wid;
wire [7:0] action_rid;
wire [7:0] action_bid;

//Loop back fifo to return IDs
//Note: fifo data width is 8. ACTION_ID_WIDTH is 4. 
assign action_awid[7:4] = 4'd0;
assign action_arid[7:4] = 4'd0;
assign action_wid[7:4] = 4'd0;

assign rid_fifo_wen = m_axi_snap_arvalid && m_axi_snap_arready;
assign rid_fifo_ren = m_axi_snap_rvalid && m_axi_snap_rready;
assign wid_fifo_wen = m_axi_snap_awvalid && m_axi_snap_awready;
assign wid_fifo_ren = m_axi_snap_bvalid && m_axi_snap_bready;

    axi_rid_fifo rid_fifo (
        .clk            (clk)
        ,.srst          (~rst_n) 
        ,.wr_en         (rid_fifo_wen)
        ,.din           (action_arid)
        ,.rd_en         (rid_fifo_ren)
        ,.dout          (action_rid)
        ,.full          (rid_fifo_full)
        ,.empty         (rid_fifo_empty)
        ,.wr_rst_busy   (rid_wr_rst_busy)
        ,.rd_rst_busy   (rid_rd_rst_busy)
    );

    axi_wid_fifo wid_fifo (
        .clk            (clk)
        ,.srst          (~rst_n) 
        ,.wr_en         (wid_fifo_wen)
        ,.din           (action_awid)
        ,.rd_en         (wid_fifo_ren)
        ,.dout          (action_bid)
        ,.full          (wid_fifo_full)
        ,.empty         (wid_fifo_empty)
        ,.wr_rst_busy   (wid_wr_rst_busy)
        ,.rd_rst_busy   (wid_rd_rst_busy)
);


 snap_action_shim #(
           // Parameters of Axi Slave Bus Interface AXI_CTRL_REG
           .C_S_AXI_CTRL_REG_DATA_WIDTH   (C_S_AXI_CTRL_REG_DATA_WIDTH   ),
           .C_S_AXI_CTRL_REG_ADDR_WIDTH   (C_S_AXI_CTRL_REG_ADDR_WIDTH   ),
       
           // Parameters of Axi Master Bus Interface AXI_HOST_MEM ; to Host memory
           .C_M_AXI_HOST_MEM_ID_WIDTH     (ACTION_ID_WIDTH               ),
           .C_M_AXI_HOST_MEM_ADDR_WIDTH   (C_M_AXI_HOST_MEM_ADDR_WIDTH   ),
           .C_M_AXI_HOST_MEM_DATA_WIDTH   (C_M_AXI_HOST_MEM_DATA_WIDTH   ),
           .C_M_AXI_HOST_MEM_AWUSER_WIDTH (C_M_AXI_HOST_MEM_AWUSER_WIDTH ),
           .C_M_AXI_HOST_MEM_ARUSER_WIDTH (C_M_AXI_HOST_MEM_ARUSER_WIDTH ),
           .C_M_AXI_HOST_MEM_WUSER_WIDTH  (C_M_AXI_HOST_MEM_WUSER_WIDTH  ),
           .C_M_AXI_HOST_MEM_RUSER_WIDTH  (C_M_AXI_HOST_MEM_RUSER_WIDTH  ),
           .C_M_AXI_HOST_MEM_BUSER_WIDTH  (C_M_AXI_HOST_MEM_BUSER_WIDTH  )
 ) msnap_action_shim (
                        .clk                      (clk                      ),
                        .rst_n                    (rst_n                    ), 
                        .m_axi_snap_awid          (action_awid[ACTION_ID_WIDTH-1:0]),  
                        .m_axi_snap_awaddr        (m_axi_snap_awaddr        ),  
                        .m_axi_snap_awlen         (m_axi_snap_awlen         ),  
                        .m_axi_snap_awsize        (m_axi_snap_awsize        ),  
                        .m_axi_snap_awburst       (m_axi_snap_awburst       ),  
                        .m_axi_snap_awcache       (m_axi_snap_awcache       ),  
                        .m_axi_snap_awlock        (                         ),  
                        .m_axi_snap_awprot        (m_axi_snap_awprot        ),  
                        .m_axi_snap_awqos         (m_axi_snap_awqos         ),  
                        .m_axi_snap_awregion      (m_axi_snap_awregion      ),  
                        .m_axi_snap_awuser        (                         ),
                        .m_axi_snap_awvalid       (m_axi_snap_awvalid       ),  
                        .m_axi_snap_awready       (m_axi_snap_awready       ),
                        .m_axi_snap_wid           (action_wid[ACTION_ID_WIDTH-1:0]), 
                        .m_axi_snap_wdata         (m_axi_snap_wdata         ),  
                        .m_axi_snap_wstrb         (m_axi_snap_wstrb         ),  
                        .m_axi_snap_wlast         (m_axi_snap_wlast         ),  
                        .m_axi_snap_wvalid        (m_axi_snap_wvalid        ),  
                        .m_axi_snap_wready        (m_axi_snap_wready        ),
                        .m_axi_snap_bready        (m_axi_snap_bready        ),  
                        .m_axi_snap_bid           (action_bid[ACTION_ID_WIDTH-1:0]),
                        .m_axi_snap_bresp         (m_axi_snap_bresp         ),
                        .m_axi_snap_bvalid        (m_axi_snap_bvalid        ),
                        .m_axi_snap_arid          (action_arid[ACTION_ID_WIDTH-1:0]),  
                        .m_axi_snap_araddr        (m_axi_snap_araddr        ),  
                        .m_axi_snap_arlen         (m_axi_snap_arlen         ),  
                        .m_axi_snap_arsize        (m_axi_snap_arsize        ),  
                        .m_axi_snap_arburst       (m_axi_snap_arburst       ),  
                        .m_axi_snap_aruser        (                         ), 
                        .m_axi_snap_arcache       (m_axi_snap_arcache       ), 
                        .m_axi_snap_arlock        (                         ), 
                        .m_axi_snap_arprot        (m_axi_snap_arprot        ), 
                        .m_axi_snap_arqos         (m_axi_snap_arqos         ), 
                        .m_axi_snap_arregion      (m_axi_snap_arregion      ), 
                        .m_axi_snap_arvalid       (m_axi_snap_arvalid       ), 
                        .m_axi_snap_arready       (m_axi_snap_arready       ),
                        .m_axi_snap_rready        (m_axi_snap_rready        ), 
                        .m_axi_snap_rid           (action_rid[ACTION_ID_WIDTH-1:0]),
                        .m_axi_snap_rdata         (m_axi_snap_rdata         ),
                        .m_axi_snap_rresp         (m_axi_snap_rresp         ),
                        .m_axi_snap_rlast         (m_axi_snap_rlast         ),
                        .m_axi_snap_rvalid        (m_axi_snap_rvalid        ),
                        /*
                        .m_axi_ddr_awid           (m_axi_ddr_awid           ),  
                        .m_axi_ddr_awaddr         (m_axi_ddr_awaddr         ),  
                        .m_axi_ddr_awlen          (m_axi_ddr_awlen          ),  
                        .m_axi_ddr_awsize         (m_axi_ddr_awsize         ),  
                        .m_axi_ddr_awburst        (m_axi_ddr_awburst        ),  
                        .m_axi_ddr_awcache        (m_axi_ddr_awcache        ),  
                        .m_axi_ddr_awlock         (m_axi_ddr_awlock         ),  
                        .m_axi_ddr_awprot         (m_axi_ddr_awprot         ),  
                        .m_axi_ddr_awqos          (m_axi_ddr_awqos          ),  
                        .m_axi_ddr_awregion       (m_axi_ddr_awregion       ),  
                        .m_axi_ddr_awuser         (m_axi_ddr_awuser         ),  
                        .m_axi_ddr_awvalid        (m_axi_ddr_awvalid        ),  
                        .m_axi_ddr_awready        (m_axi_ddr_awready        ),
                        .m_axi_ddr_wid            (m_axi_ddr_wid            ), 
                        .m_axi_ddr_wdata          (m_axi_ddr_wdata          ),  
                        .m_axi_ddr_wstrb          (m_axi_ddr_wstrb          ),  
                        .m_axi_ddr_wlast          (m_axi_ddr_wlast          ),  
                        .m_axi_ddr_wvalid         (m_axi_ddr_wvalid         ),  
                        .m_axi_ddr_wready         (m_axi_ddr_wready         ),
                        .m_axi_ddr_bready         (m_axi_ddr_bready         ),  
                        .m_axi_ddr_bid            (m_axi_ddr_bid            ),
                        .m_axi_ddr_bresp          (m_axi_ddr_bresp          ),
                        .m_axi_ddr_bvalid         (m_axi_ddr_bvalid         ),
                        .m_axi_ddr_arid           (m_axi_ddr_arid           ),  
                        .m_axi_ddr_araddr         (m_axi_ddr_araddr         ),  
                        .m_axi_ddr_arlen          (m_axi_ddr_arlen          ),  
                        .m_axi_ddr_arsize         (m_axi_ddr_arsize         ),  
                        .m_axi_ddr_arburst        (m_axi_ddr_arburst        ),  
                        .m_axi_ddr_aruser         (m_axi_ddr_aruser         ), 
                        .m_axi_ddr_arcache        (m_axi_ddr_arcache        ), 
                        .m_axi_ddr_arlock         (m_axi_ddr_arlock         ), 
                        .m_axi_ddr_arprot         (m_axi_ddr_arprot         ), 
                        .m_axi_ddr_arqos          (m_axi_ddr_arqos          ), 
                        .m_axi_ddr_arregion       (m_axi_ddr_arregion       ), 
                        .m_axi_ddr_arvalid        (m_axi_ddr_arvalid        ), 
                        .m_axi_ddr_arready        (m_axi_ddr_arready        ),
                        .m_axi_ddr_rready         (m_axi_ddr_rready         ), 
                        .m_axi_ddr_rid            (m_axi_ddr_rid            ),
                        .m_axi_ddr_rdata          (m_axi_ddr_rdata          ),
                        .m_axi_ddr_rresp          (m_axi_ddr_rresp          ),
                        .m_axi_ddr_rlast          (m_axi_ddr_rlast          ),
                        .m_axi_ddr_rvalid         (m_axi_ddr_rvalid         ),
                        */
                        .s_axi_snap_awready       (s_axi_snap_awready       ),   
                        .s_axi_snap_awaddr        (s_axi_snap_awaddr        ),
                        .s_axi_snap_awprot        (s_axi_snap_awprot        ),
                        .s_axi_snap_awvalid       (s_axi_snap_awvalid       ),
                        .s_axi_snap_wready        (s_axi_snap_wready        ),
                        .s_axi_snap_wdata         (s_axi_snap_wdata         ),
                        .s_axi_snap_wstrb         (s_axi_snap_wstrb         ),
                        .s_axi_snap_wvalid        (s_axi_snap_wvalid        ),
                        .s_axi_snap_bresp         (s_axi_snap_bresp         ),
                        .s_axi_snap_bvalid        (s_axi_snap_bvalid        ),
                        .s_axi_snap_bready        (s_axi_snap_bready        ),
                        .s_axi_snap_arready       (s_axi_snap_arready       ),
                        .s_axi_snap_arvalid       (s_axi_snap_arvalid       ),
                        .s_axi_snap_araddr        (s_axi_snap_araddr        ),
                        .s_axi_snap_arprot        (s_axi_snap_arprot        ),
                        .s_axi_snap_rdata         (s_axi_snap_rdata         ),
                        .s_axi_snap_rresp         (s_axi_snap_rresp         ),
                        .s_axi_snap_rready        (s_axi_snap_rready        ),
                        .s_axi_snap_rvalid        (s_axi_snap_rvalid        ),
                        .i_app_ready              (app_ready                ),
                        .i_action_type            (i_action_type            ),
                        .i_action_version         (i_action_version         ),
                        .o_interrupt              (o_interrupt              ),
                        .i_interrupt_ack          (i_interrupt_ack          )
                       );

endmodule
