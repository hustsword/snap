// ****************************************************************
// (C) Copyright International Business Machines Corporation 2017
// Author: Gou Peng Fei (shgoupf@cn.ibm.com)
// ****************************************************************

#ifndef F_CONSTANTS
#define F_CONSTANTS

/* Header file for SNAP Framework DATABASE DIRECT code */
#define ACTION_TYPE_DATABASE     0x00000003	/* Action Type */

#define ACTION_STATUS_L                0x230
#define ACTION_STATUS_H                0x234
#define ACTION_STATUS_MEMCPY_DONE      0       
#define ACTION_STATUS_PKT_DATA_USED_UP 1       
#define ACTION_STATUS_STAT_DATA_COMP   2       
#define ACTION_STATUS_STAT_FLUSH_DONE  3       
#define ACTION_STATUS_STAT_USED_UP     4       
#define ACTION_STATUS_ERROR_AXI_START  8       
#define ACTION_STATUS_ERROR_AXI_END    23       
#define ACTION_STATUS_TOTAL_NUM_START  32
#define ACTION_STATUS_TOTAL_NUM_END    63

#define ACTION_CONTROL_L               0x238
#define ACTION_CONTROL_H               0x23C
#define ACTION_CONTROL_PATT_START      0       
#define ACTION_CONTROL_PKT_PATT_EN     1
#define ACTION_CONTROL_STAT_EN         2       
#define ACTION_CONTROL_FLUSH           3       

#define ACTION_PKT_INIT_ADDR_L         0x240
#define ACTION_PKT_INIT_ADDR_H         0x244
#define ACTION_PATT_INIT_ADDR_L        0x248
#define ACTION_PATT_INIT_ADDR_H        0x24C
#define ACTION_PATT_CARD_DDR_ADDR_L    0x250
#define ACTION_PATT_CARD_DDR_ADDR_H    0x254
#define ACTION_STAT_INIT_ADDR_L        0x258
#define ACTION_STAT_INIT_ADDR_H        0x25C
#define ACTION_PKT_TOTAL_NUM_L         0x260
#define ACTION_PKT_TOTAL_NUM_H         0x264
#define ACTION_PATT_TOTAL_NUM_L        0x268
#define ACTION_PATT_TOTAL_NUM_H        0x26C
#define ACTION_STAT_TOTAL_SIZE_L       0x270
#define ACTION_STAT_TOTAL_SIZE_H       0x274
#define ACTION_DEBUG0_L                0x280
#define ACTION_DEBUG0_H                0x284

#define INPUT_PACKET_STAT_WIDTH         48
#define INPUT_BATCH_WIDTH               1024
#define INPUT_BATCH_PER_PACKET          16
#define PATTERN_STAT_WIDTH              104
#define INPUT_PACKET_WIDTH              512
#define OUTPUT_STAT_WIDTH               80
#define PATTERN_NUM_NFA_STATES          8
#define PATTERN_NUM_NFA_TOKEN           8
#define NUM_STRING_MATCH_PIPELINE       512

#define MAX_STATE_NUM                   8//16
#define MAX_TOKEN_NUM                   8//16
#define MAX_CHAR_NUM                    8//32
#define MAX_CHAR_PER_TOKEN              8//16
#define PATTERN_ID_WIDTH                32
#define NUM_OF_PU                       8

// The width of pattern is calculated per the following equation
#define PATTERN_WIDTH_BITS (PATTERN_ID_WIDTH+MAX_CHAR_NUM*16+MAX_STATE_NUM*8+8+8+MAX_STATE_NUM+MAX_STATE_NUM*MAX_CHAR_NUM+MAX_STATE_NUM*MAX_STATE_NUM+MAX_STATE_NUM*MAX_STATE_NUM+MAX_STATE_NUM)

#define PATTERN_WIDTH_BYTES (PATTERN_WIDTH_BITS/8)

#endif
