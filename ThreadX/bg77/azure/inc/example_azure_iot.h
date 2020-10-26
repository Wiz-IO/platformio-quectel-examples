/******************************************************************************
*@file    example_tcpclient.h
*@brief   Detection of network state and notify the main task to create tcp client.
*         If server send "Exit" to client, client will exit immediately.*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#ifndef __EXAMPLE_TCPCLINET_H__
#define __EXAMPLE_TCPCLINET_H__

#if defined(__EXAMPLE_AZURE_IOT__)


/*===========================================================================
						   Header file
===========================================================================*/
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "iothub_client_ll.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/xlogging.h"

#include "serializer.h"
#include "iothub_client_ll.h"
#include "iothubtransportmqtt.h"
#include "iothub_client_core_common.h"

#include "qapi_fs_types.h"
#include "qapi_dss.h"
#include "qapi_uart.h"
#include "qapi_timer.h"
#include "qapi_diag.h"
#include "qapi_location.h"

#include "iothub.h"
#include "iothub_device_client.h"
#include "iothub_client_options.h"
#include "iothub_message.h"
#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/shared_util_options.h"
#include "parson.h"
#include "pnp_device.h"
/*===========================================================================
                             DEFINITION
===========================================================================*/
#define DEF_SRC_TYPE    SOCK_STREAM	//SOCK_STREAM(TCP) SOCK_DGRAM(UDP)
#define DEF_LOC_ADDR	 "127.0.0.1"
#define DEF_SRV_ADDR    "220.180.239.212"
#define DEF_SRV_PORT    (8007)
#define DEF_BACKLOG_NUM (5)

#define RECV_BUF_SIZE   (128)
#define SENT_BUF_SIZE   (128)

#define SLEEP_30S	    (30000)
#define TIMER_1MIN		(60000)
#define MAX_CYCLE_TIMES (10)

#define AZURE_CALIST_BIN         "ca_list.bin"
#define AZURE_CERT_BIN           "cert.bin"

#define AZURE_CA_CRT            "/datatx/azure.crt"
#define AZURE_CLI_CRT           "/datatx/host.crt"
#define AZURE_CLI_KEY           "/datatx/host.key"

#define QUEC_APN_LEN	  		(QAPI_DSS_CALL_INFO_APN_MAX_LEN + 1)
#define QUEC_APN_USER_LEN 		(QAPI_DSS_CALL_INFO_USERNAME_MAX_LEN + 1)
#define QUEC_APN_PASS_LEN 		(QAPI_DSS_CALL_INFO_PASSWORD_MAX_LEN + 1)
#define QUEC_DEV_NAME_LEN 		(QAPI_DSS_CALL_INFO_DEVICE_NAME_MAX_LEN + 1)

#define QT_DS_PROFILE_MAX_APN_STRING_LEN (101)
#define QT_DS_PROFILE_MAX_USERNAME_LEN (128)
#define QT_DS_PROFILE_MAX_PASSWORD_LEN (128)


#define LEFT_SHIFT_OP(N)		(1 << (N))
#define _htons(x) \
        ((unsigned short)((((unsigned short)(x) & 0x00ff) << 8) | \
                  (((unsigned short)(x) & 0xff00) >> 8)))

typedef enum DSS_Net_Evt_TYPE
{
	DSS_EVT_INVALID_E = 0x00,   /**< Invalid event. */
	DSS_EVT_NET_IS_CONN_E,      /**< Call connected. */
	DSS_EVT_NET_NO_NET_E,       /**< Call disconnected. */
	DSS_EVT_NET_RECONFIGURED_E, /**< Call reconfigured. */
	DSS_EVT_NET_NEWADDR_E,      /**< New address generated. */
	DSS_EVT_NET_DELADDR_E,      /**< Delete generated. */
	DSS_EVT_NIPD_DL_DATA_E,
	DSS_EVT_MAX_E
} DSS_Net_Evt_Type_e;

typedef enum DSS_Lib_Status
{
	DSS_LIB_STAT_INVALID_E = -1,
	DSS_LIB_STAT_FAIL_E,
	DSS_LIB_STAT_SUCCESS_E,
	DSS_LIB_STAT_MAX_E
} DSS_Lib_Status_e;

typedef enum DSS_SIG_EVENTS
{
	DSS_SIG_EVT_INV_E		= LEFT_SHIFT_OP(0),
	DSS_SIG_EVT_NO_CONN_E	= LEFT_SHIFT_OP(1),
	DSS_SIG_EVT_CONN_E		= LEFT_SHIFT_OP(2),
	DSS_SIG_EVT_DIS_E		= LEFT_SHIFT_OP(3),
	DSS_SIG_EVT_EXIT_E		= LEFT_SHIFT_OP(4),
	DSS_SIG_EVT_MAX_E		= LEFT_SHIFT_OP(5)
} DSS_Signal_Evt_e;

typedef enum NW_SIG_EVENTS
{
	NW_SIG_EVT_UNENABLE = LEFT_SHIFT_OP(0),
	NW_SIG_EVT_ENABLE =  LEFT_SHIFT_OP(1)
}NW_Signal_Evt_e;

#if 0
typedef enum
{
	QT_NW_DS_PROFILE_PDP_IPV4 = 1,
	QT_NW_DS_PROFILE_PDP_IPV6 = 2,
	QT_NW_DS_PROFILE_PDP_IPV4V6 = 3,
	QT_NW_DS_PROFILE_PDP_MAX
}qapi_QT_NW_DS_PROFILE_PDP_TYPE_e;

typedef enum {
	QT_NW_DS_PROFILE_AUTH_PAP = 0,
	QT_NW_DS_PROFILE_AUTH_CHAP = 1,
	QT_NW_DS_PROFILE_AUTH_PAP_CHAP = 2,
	QT_NW_DS_PROFILE_AUTH_TYPE_MAX
}qapi_QT_NW_DS_PROFILE_AUTH_TYPE_e;


typedef struct {
	qapi_QT_NW_DS_PROFILE_PDP_TYPE_e pdp_type;
	qapi_QT_NW_DS_PROFILE_AUTH_TYPE_e auth_type;
	unsigned char apn[QT_DS_PROFILE_MAX_APN_STRING_LEN + 1];
	unsigned char user_name[QT_DS_PROFILE_MAX_USERNAME_LEN + 1];
	unsigned char pass_word[QT_DS_PROFILE_MAX_PASSWORD_LEN + 1];
}qapi_QT_NW_DS_Profile_PDP_Context_t;
#endif

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

/*===========================================================================
                             DECLARATION
===========================================================================*/
int32 tcp_inet_ntoa(const qapi_DSS_Addr_t inaddr, uint8 *buf, int32 buflen);
void tcp_show_sysinfo(void);

void tcp_uart_dbg_init(void);
void tcp_uart_debug_print(const char* fmt, ...);

int tcp_netctrl_start(void);
int tcp_netctrl_stop(void);
//int azure_Task(char* lat, char* lon);
bool IsSendingEnds();
int azure_setup();
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

int quec_dataservice_entry(void);
#endif /*__EXAMPLE_TCPCLINET__*/

#endif /*__EXAMPLE_TCPCLINET_H__*/
