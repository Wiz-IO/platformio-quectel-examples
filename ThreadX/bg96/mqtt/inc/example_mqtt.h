/******************************************************************************
*@file    example_mqtt.h
*@brief   example of mqtt
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/

#ifndef __EXAMPLE_MQTT_H__
#define __EXAMPLE_MQTT_H__

#include "stdio.h"
#include "qapi_mqtt.h"
#include "qapi_dss.h"

#define CLI_MQTT_SVR_NAME       "api-dev.devicewise.com"
#define CLI_MQTT_SVR            "52.200.8.131"
#define MQTT_CALIST_BIN         "mqtt_calist.bin"
#define MQTT_CERT_BIN           "mqtt_cert.bin"

#define MQTTS_CA_CRT            "/datatx/ca.crt"
#define MQTTS_CLI_CRT           "/datatx/client.crt"
#define MQTTS_CLI_KEY           "/datatx/client.key"
#define _htons(x)           ((unsigned short)((((unsigned short)(x) & 0x00ff) << 8) | (((unsigned short)(x) & 0xff00) >> 8)))
#define _ntohs(x)           ((unsigned short)((((unsigned short)(x) & 0x00ff) << 8) | (((unsigned short)(x) & 0xff00) >> 8)))
#define THREAD_STACK_SIZE       (1024 * 16)
#define THREAD_PRIORITY         (180)
#define QUEC_APN_LEN	  		(QAPI_DSS_CALL_INFO_APN_MAX_LEN + 1)
#define QUEC_APN_USER_LEN 		(QAPI_DSS_CALL_INFO_USERNAME_MAX_LEN + 1)
#define QUEC_APN_PASS_LEN 		(QAPI_DSS_CALL_INFO_PASSWORD_MAX_LEN + 1)
#define QUEC_DEV_NAME_LEN 		(QAPI_DSS_CALL_INFO_DEVICE_NAME_MAX_LEN + 1)

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

#define LEFT_SHIFT_OP(N)		(1 << (N))
typedef enum DSS_SIG_EVENTS
{
	DSS_SIG_EVT_INV_E		= LEFT_SHIFT_OP(0),
	DSS_SIG_EVT_NO_CONN_E	= LEFT_SHIFT_OP(1),
	DSS_SIG_EVT_CONN_E		= LEFT_SHIFT_OP(2),
	DSS_SIG_EVT_DIS_E		= LEFT_SHIFT_OP(3),
	DSS_SIG_EVT_EXIT_E		= LEFT_SHIFT_OP(4),
	DSS_SIG_EVT_MAX_E		= LEFT_SHIFT_OP(5)
} DSS_Signal_Evt_e;

#ifdef IP_V6
#define ADDRBUF_LEN IP6_ADDRBUF_LEN
#else
#define ADDRBUF_LEN 40
#endif

/*===========================================================================
                             DECLARATION
===========================================================================*/
int32 tcp_inet_ntoa(const qapi_DSS_Addr_t inaddr, uint8 *buf, int32 buflen);
void tcp_show_sysinfo(void);
void get_ip_from_url(const char *hostname, struct ip46addr *ipaddr);

int tcp_netctrl_start(void);
int tcp_netctrl_stop(void);

int quec_dataservice_entry(void);
void i2c_uart_dbg_init(void);
void i2c_uart_debug_print(const char* fmt, ...);

#endif /* __EXAMPLE_MQTT_H__ */

