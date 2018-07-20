`timescale 1ns/1ps

module memcpy_engine #(
                       parameter ADDR_WIDTH    = 64,
                       parameter DATA_WIDTH    = 512
                       )
                      (
                       input                           clk            ,
                       input                           rst_n          , 

                       //---- memory copy parameters----          
                       input      [ADDR_WIDTH - 1:0]   memcpy_src_addr,
                       input      [ADDR_WIDTH - 1:0]   memcpy_tgt_addr,
                       input      [007:0]              memcpy_len     , // in terms of bytes
                       input                           memcpy_start   ,
                       output                          memcpy_done    ,
                                                               
                       //---- write channel ----
                       input                           lcl_ibusy      ,
                       output                          lcl_istart     ,
                       output     [ADDR_WIDTH - 1:0]   lcl_iaddr      ,
                       output     [007:0]              lcl_inum       ,
                       input                           lcl_irdy       ,
                       output reg                      lcl_den        ,
                       output reg [DATA_WIDTH - 1:0]   lcl_din        ,
                       output                          lcl_idone      ,

                       //---- read channel ----
                       input                           lcl_obusy      ,
                       output                          lcl_ostart     ,
                       output     [ADDR_WIDTH - 1:0]   lcl_oaddr      ,
                       output     [007:0]              lcl_onum       ,
                       input                           lcl_ordy       ,
                       output reg                      lcl_rden       ,
                       input                           lcl_dv         ,
                       input      [DATA_WIDTH - 1:0]   lcl_dout       ,
                       input                           lcl_odone      
                       };

 
//--------------------------------------------
 wire wr_on, rd_on;
 reg [7:0] wr_cnt;
 parameter RD_IDLE   = 7'h01, 
           RD_INIT   = 7'h02,
           RD_N4KB   = 7'h04,
           RD_CLEN   = 7'h08,
           RD_START  = 7'h10,
           RD_INPROC = 7'h20,
           RD_DONE   = 7'h40;
//--------------------------------------------


//---- data loopback ----
 always@(posedge clk or negedge rst_n) 
   if (~rst_n)
     lcl_rden <= 1'b0;
   else if (wr_on | rd_on)
     begin
       if (lcl_odone)
         lcl_rden <= 1'b0;
       else if (wr_cnt != lcl_iaddr - 8'd1)
         lcl_rden <= lcl_ordy & lcl_irdy;
     end
   else
     lcl_rden <= 1'b0;

 always@(posedge clk or negedge rst_n) 
   if (~rst_n)
     begin
       lcl_den <= 1'b0;
       lcl_din <= 'd0;
     end
   else
     begin
       lcl_den <= lcl_dv;
       lcl_din <= lcl_dout;
     end

 always@(posedge clk or negedge rst_n)   // control the read data number during write state
   if (~rst_n)
     wr_cnt <= 8'd0;
   else if (wr_on)
     begin
       if (lcl_rden)
         wr_cnt <= wr_cnt + 8'd1;
     end
   else
     wr_cnt <= 8'd0;

//---- memory read burst control ----
 memcpy_statemachine mrd_st(
                            .clk          (clk           ),
                            .rst_n        (rst_n         ), 
                            .memcpy_start (memcpy_start  ),
                            .memcpy_len   (memcpy_len    ),
                            .memcpy_addr  (memcpy_addr   ),
                            .burst_start  (lcl_ostart    ),
                            .burst_len    (lcl_onum      ),
                            .burst_addr   (lcl_oaddr     ),
                            .burst_on     (rd_on         ),
                            .burst_done   (),
                            .memcpy_done  (memcpy_rd_done)
                           );

//---- memory writing burst control ----
 memcpy_statemachine mwr_st(
                            .clk          (clk           ),
                            .rst_n        (rst_n         ), 
                            .memcpy_start (memcpy_start  ),
                            .memcpy_len   (memcpy_len    ),
                            .memcpy_addr  (memcpy_addr   ),
                            .burst_start  (lcl_istart    ),
                            .burst_len    (lcl_inum      ),
                            .burst_addr   (lcl_iaddr     ),
                            .burst_on     (wr_on         ),
                            .burst_done   (lcl_idone     ),
                            .memcpy_done  (memcpy_wr_done)
                           );

//---- entire memory copy is done ----
 assign memcpy_done = memcpy_wr_done && memcpy_rd_done;


endmodule                                                        
