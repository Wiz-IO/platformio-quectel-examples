#ifndef __EXAMPLE_SIM_INFO_H__
#define __EXAMPLE_SIM_INFO_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__EXAMPLE_SIM_INFO__)
extern qapi_Status_t qapi_QT_SIM_RDY_Check(char* status);
extern qapi_Status_t qapi_QT_SIM_IMSI_Get(char* imsi);
extern qapi_Status_t qapi_QT_SIM_MSISDN_Get(char* msisdn);
extern qapi_Status_t qapi_QT_SIM_CCID_Get(char* ccid);
void qt_uart_dbg(qapi_UART_Handle_t uart_hdlr, const char* fmt, ...);

#endif

#endif/*__EXAMPLE_SIM_INFO_H__*/
