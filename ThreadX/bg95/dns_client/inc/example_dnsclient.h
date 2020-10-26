/******************************************************************************
*@file    example_dnsclient.h
*@brief   Detection of network state and parser domain name via DNS server
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#ifndef __EXAMPLE_DNSCLINET_H__
#define __EXAMPLE_DNSCLINET_H__

#if defined(__EXAMPLE_DNSCLIENT__)
/*===========================================================================
						   Header file
===========================================================================*/
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "qapi_fs_types.h"
#include "qapi_dss.h"
#include "qapi_uart.h"
#include "qapi_timer.h"
#include "qapi_diag.h"

/*===========================================================================
                             DEFINITION
===========================================================================*/
#define DEF_SRC_TYPE    SOCK_STREAM	//SOCK_STREAM(TCP) SOCK_DGRAM(UDP)
#define DEF_LOC_ADDR	"127.0.0.1"
#define DEF_SRV_ADDR    "220.180.239.212"
#define DEF_SRV_PORT    (8498)
#define DEF_BACKLOG_NUM (5)

#define RECV_BUF_SIZE   (128)
#define SENT_BUF_SIZE   (128)

#define SLEEP_30S	    (30000)
#define TIMER_1MIN		(60000)
#define MAX_CYCLE_TIMES (10)

#define QUEC_APN_LEN	  		(QAPI_DSS_CALL_INFO_APN_MAX_LEN + 1)
#define QUEC_APN_USER_LEN 		(QAPI_DSS_CALL_INFO_USERNAME_MAX_LEN + 1)
#define QUEC_APN_PASS_LEN 		(QAPI_DSS_CALL_INFO_PASSWORD_MAX_LEN + 1)
#define QUEC_DEV_NAME_LEN 		(QAPI_DSS_CALL_INFO_DEVICE_NAME_MAX_LEN + 1)

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

/*===========================================================================
                             DECLARATION
===========================================================================*/
int32 dss_inet_ntoa(const qapi_DSS_Addr_t inaddr, uint8 *buf, int32 buflen);
void dss_show_sysinfo(void);

void dss_uart_dbg_init(void);
void dns_uart_debug_print(const char* fmt, ...);

int dss_netctrl_start(void);
int dss_netctrl_stop(void);

int quec_dataservice_entry(void);
#endif /*__EXAMPLE_TCPCLINET__*/

#endif /*__EXAMPLE_TCPCLINET_H__*/
