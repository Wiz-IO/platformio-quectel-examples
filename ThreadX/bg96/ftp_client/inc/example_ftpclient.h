/******************************************************************************
*@file    example_tcpclient.h
*@brief   Detection of network state and notify the main task to create ftp client.
*            Client will connect to server and perform Login, ChangeDir, ListDir, DownloadFile, 
*            UploadFile, Logout operations.
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#ifndef __EXAMPLE_FTPCLINET_H__
#define __EXAMPLE_FTPCLINET_H__

#if defined(__EXAMPLE_FTPCLIENT__)
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
#define DEF_LOC_ADDR	 "127.0.0.1"
#define DEF_SRV_ADDR    "220.180.239.212"
#define DEF_SRV_PORT    (21)
#define DEF_BACKLOG_NUM    (5)
#define CLIENT_WAIT_TIME   30000

#define FTP_UPLOAD_FILE      "upload.txt"
#define FTP_DOWNLOAD_FILE    "download.txt"

#define RECV_BUF_SIZE   (128)
#define SENT_BUF_SIZE   (128)

#define SLEEP_30S	    (30000)
#define TIMER_1MIN		(60000)
#define MAX_CYCLE_TIMES (10)

#define LEFT_SHIFT_OP(N)		(1 << (N))
#define _htons(x) \
        ((unsigned short)((((unsigned short)(x) & 0x00ff) << 8) | \
                  (((unsigned short)(x) & 0xff00) >> 8)))

#define QUEC_APN_LEN	  		(QAPI_DSS_CALL_INFO_APN_MAX_LEN + 1)
#define QUEC_APN_USER_LEN 		(QAPI_DSS_CALL_INFO_USERNAME_MAX_LEN + 1)
#define QUEC_APN_PASS_LEN 		(QAPI_DSS_CALL_INFO_PASSWORD_MAX_LEN + 1)
#define QUEC_DEV_NAME_LEN 		(QAPI_DSS_CALL_INFO_DEVICE_NAME_MAX_LEN + 1)

#define FTP_RESPONSE_RESTART          110
#define FTP_RESPONSE_READY_LATER      120
#define FTP_RESPONSE_OPENED           125
#define FTP_RESPONSE_OPEN_DATA        150
#define FTP_RESPONSE_CMD_OK           200
#define FTP_RESPONSE_CMD_FAILED       202
#define FTP_RESPONSE_SYS_STATE        211
#define FTP_RESPONSE_DIR_STATE        212
#define FTP_RESPONSE_FILE_STATE       213
#define FTP_RESPONSE_HELP_INFO        214
#define FTP_RESPONSE_NAME_TYPE        215
#define FTP_RESPONSE_READY_NEW        220
#define FTP_RESPONSE_BYE              221
#define FTP_RESPONSE_DAT_OPENED       225
#define FTP_RESPONSE_DAT_SUCCESS      226
#define FTP_RESPONSE_ENTER_PASV       227
#define FTP_RESPONSE_USER_LOGIN       230
#define FTP_RESPONSE_TLS_OK           234
#define FTP_RESPONSE_DAT_FINISH       250
#define FTP_RESPONSE_PATHNAME         257
#define FTP_RESPONSE_NEED_PWD         331
#define FTP_RESPONSE_NEED_ACC         332
#define FTP_RESPONSE_NEXT_CMD         350
#define FTP_RESPONSE_SERVICE_END      421
#define FTP_RESPONSE_DAT_FAILED       425
#define FTP_RESPONSE_CLOSE_CONN       426
#define FTP_RESPONSE_FILE_UNOPER      450
#define FTP_RESPONSE_REQUEST_ABORT    451
#define FTP_RESPONSE_ERROR_MEMORY     452
#define FTP_RESPONSE_WRONG_FORMAT     500
#define FTP_RESPONSE_INVALID_PARA     501
#define FTP_RESPONSE_CMD_UNREALIZED   502
#define FTP_RESPONSE_WRONG_SEQUENCE   503
#define FTP_RESPONSE_FUNCTION_FAILED  504
#define FTP_RESPONSE_NOT_LOGIN        530
#define FTP_RESPONSE_NO_ACCOUNT       532
#define FTP_RESPONSE_REQUEST_FAILED   550
#define FTP_RESPONSE_REQUEST_STOP     551
#define FTP_RESPONSE_FILE_REQ_STOP    552
#define FTP_RESPONSE_INVALID_FILE_N   553

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
typedef enum
{   
  FTP_COMMAND_NONE,
  FTP_COMMAND_AUTH,
  FTP_COMMAND_USER,
  FTP_COMMAND_PASS,
  FTP_COMMAND_PBSZ,
  FTP_COMMAND_PROT,
  FTP_COMMAND_SYST,
  FTP_COMMAND_TYPE,//4    
  FTP_COMMAND_REST,
  FTP_COMMAND_PASV,
  FTP_COMMAND_PORT,
  FTP_COMMAND_RETR,
  FTP_COMMAND_STOR,
  FTP_COMMAND_APPE,
  FTP_COMMAND_STOU,
  FTP_COMMAND_QUIT,
  FTP_COMMAND_ABOR,
  FTP_COMMAND_NOOP,
  FTP_COMMAND_DELE, 
  FTP_COMMAND_MKD,
  FTP_COMMAND_RMD,
  FTP_COMMAND_SIZE,
  FTP_COMMAND_RNFR,
  FTP_COMMAND_RNTO,
  FTP_COMMAND_LIST,
  FTP_COMMAND_NLST,
  FTP_COMMAND_CWD,
  FTP_COMMAND_PWD,
  FTP_COMMAND_MLSD,
  FTP_COMMAND_MDTM,
  FTP_COMMAND_AUTH_TLS,
  FTP_COMMAND_PROT_P,
  FTP_COMMAND_MAX
}ftp_command_enum;
typedef enum
{   
  FTP_OPER_NONE,
  FTP_OPER_LOGIN,
  FTP_OPER_CD,
  FTP_OPER_PWD,
  FTP_OPER_LIST,
  FTP_OPER_GET,
  FTP_OPER_PUT,
  FTP_OPER_MAX
}ftp_operation_enum;
/*===========================================================================
                             DECLARATION
===========================================================================*/
int32 tcp_inet_ntoa(const qapi_DSS_Addr_t inaddr, uint8 *buf, int32 buflen);
void tcp_show_sysinfo(void);

void tcp_uart_dbg_init(void);
void tcp_uart_debug_print(const char* fmt, ...);

int tcp_netctrl_start(void);
int tcp_netctrl_stop(void);

int quec_dataservice_entry(void);
#endif /*__EXAMPLE_FTPCLINET__*/

#endif /*__EXAMPLE_FTPCLINET_H__*/

