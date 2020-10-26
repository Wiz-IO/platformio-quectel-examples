/******************************************************************************
*@file    example_atfwd.h
*@brief   example of AT forward service
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#ifndef __EXAMPLE_ATFWD_H__
#define __EXAMPLE_ATFWD_H__

#if defined(__EXAMPLE_ATFWD__)
typedef  unsigned char      boolean; 

#define QUEC_AT_RESULT_ERROR_V01 0 /**<  Result ERROR. 
                                         This is to be set in case of ERROR or CME ERROR. 
                                         The response buffer contains the entire details. */
#define QUEC_AT_RESULT_OK_V01 1    /**<  Result OK. This is to be set if the final response 
                                         must send an OK result code to the terminal. 
                                         The response buffer must not contain an OK.  */
#define QUEC_AT_MASK_EMPTY_V01  0  /**<  Denotes a feed back mechanism for AT reg and de-reg */
#define QUEC_AT_MASK_NA_V01 1 /**<  Denotes presence of Name field  */
#define QUEC_AT_MASK_EQ_V01 2 /**<  Denotes presence of equal (=) operator  */
#define QUEC_AT_MASK_QU_V01 4 /**<  Denotes presence of question mark (?)  */
#define QUEC_AT_MASK_AR_V01 8 /**<  Denotes presence of trailing argument operator */

void qt_uart_dbg(qapi_UART_Handle_t uart_hdlr, const char* fmt, ...);

#endif /*__EXAMPLE_ATFWD__*/

#endif /*__EXAMPLE_ATFWD_H__*/

