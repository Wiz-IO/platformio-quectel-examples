/******************************************************************************
*@file    example_ssl.c
*@brief   Detection of network state and notify the main task to create ssl client.
*         If server send "Exit" to client, client will exit immediately.
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/

#if defined(__EXAMPLE_SSL__)
/*===========================================================================
						   Header file
===========================================================================*/
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "qapi_fs_types.h"
#include "qapi_status.h"
#include "qapi_socket.h"
#include "qapi_dss.h"
#include "qapi_netservices.h"
#include "qapi_device_info.h"
#include "qapi_fs.h"

#include "qapi_uart.h"
#include "qapi_timer.h"
#include "qapi_diag.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "qapi_fs.h"
#include "example_ssl.h"

/*===========================================================================
                             DEFINITION
===========================================================================*/
#define QL_DEF_APN	        "CMNET"
#define DSS_ADDR_INFO_SIZE	5
#define DSS_ADDR_SIZE       16

#define GET_ADDR_INFO_MIN(a, b) ((a) > (b) ? (b) : (a))

#define QUEC_SSL_UART_DBG
#ifdef QUEC_SSL_UART_DBG
#define SSL_UART_DBG(...)	\
{\
	ssl_uart_debug_print(__VA_ARGS__);	\
}
#else
#define SSL_UART_DBG(...)
#endif

#define THREAD_STACK_SIZE    (1024 * 16)
#define THREAD_PRIORITY      (180)
#define BYTE_POOL_SIZE       (1024 * 16)

#define CMD_BUF_SIZE  100

#define DEFAULT_PUB_TIME 5

#define NW_SIGNAL_ENABLE //for checking network status by device_info
#define SSL_IO_SELECT//I/O multiplexing for receiving data
/*===========================================================================
                           Global variable
===========================================================================*/
/* SSLClient dss thread handle */
#ifdef QAPI_TXM_MODULE
	static TX_THREAD *dss_thread_handle; 
#else
	static TX_THREAD _dss_thread_handle;
	static TX_THREAD *ts_thread_handle = &_dss_thread_handle;
#endif

#ifdef QAPI_TXM_MODULE
	static TX_THREAD *ssl_thread_handle; 
#endif

void* ssl_nw_signal_handle = NULL;	/* Related to nework indication */

qapi_Device_Info_Hndl_t device_info_hndl;

static unsigned char ssl_dss_stack[THREAD_STACK_SIZE];
static unsigned char ssl_rx_stack[THREAD_STACK_SIZE];

TX_EVENT_FLAGS_GROUP *ssl_signal_handle;

qapi_DSS_Hndl_t ssl_dss_handle = NULL;	            /* Related to DSS netctrl */

static char apn[QUEC_APN_LEN];					/* APN */
static char apn_username[QUEC_APN_USER_LEN];	/* APN username */
static char apn_passwd[QUEC_APN_PASS_LEN];		/* APN password */

/* @Note: If netctrl library fail to initialize, set this value will be 1,
 * We should not release library when it is 1. 
 */
signed char ssl_netctl_lib_status = DSS_LIB_STAT_INVALID_E;
unsigned char ssl_datacall_status = DSS_EVT_INVALID_E;

TX_BYTE_POOL *byte_pool_ssl;
#define SSL_BYTE_POOL_SIZE		10*8*1024
UCHAR free_memory_ssl[SSL_BYTE_POOL_SIZE];

qapi_Net_SSL_Obj_Hdl_t ssl_obj_hdl = 0;

/* uart rx tx buffer */
static char *uart_rx_buff = NULL;	/*!!! should keep this buffer as 4K Bytes */
static char *uart_tx_buff = NULL;

/* uart config para*/
static QT_UART_CONF_PARA uart_conf =
{
	NULL,
	QT_QAPI_UART_PORT_01,
	NULL,
	0,
	NULL,
	0,
	115200
};

qapi_Net_SSL_Con_Hdl_t ssl_conn_handle = QAPI_NET_SSL_INVALID_HANDLE;
/*===========================================================================
                               FUNCTION
===========================================================================*/
static void ssl_uart_rx_cb(uint32_t num_bytes, void *cb_data)
{
	if(num_bytes == 0)
	{
		qapi_UART_Receive(uart_conf.hdlr, uart_conf.rx_buff, uart_conf.rx_len, NULL);
		return;
	}
	else if(num_bytes >= uart_conf.rx_len)
	{
		num_bytes = uart_conf.rx_len;
	}

	IOT_DEBUG("QT# uart_rx_cb data [%d][%s]", num_bytes, uart_conf.rx_buff);
	memset(uart_conf.rx_buff, 0, uart_conf.rx_len);

	qapi_UART_Receive(uart_conf.hdlr, uart_conf.rx_buff, uart_conf.rx_len, NULL);
}

static void ssl_uart_tx_cb(uint32_t num_bytes, void *cb_data)
{
	IOT_DEBUG("QT# uart_tx_cb, send [%d]", num_bytes);
}

void ssl_uart_dbg_init()
{
	qapi_Status_t status;
	qapi_UART_Open_Config_t uart_cfg;
	
  	if (TX_SUCCESS != tx_byte_allocate(byte_pool_ssl, (VOID *)&uart_rx_buff, 4*1024, TX_NO_WAIT))
  	{
  		IOT_DEBUG("tx_byte_allocate [uart_rx_buff] failed!");
    	return;
  	}

  	if (TX_SUCCESS != tx_byte_allocate(byte_pool_ssl, (VOID *)&uart_tx_buff, 4*1024, TX_NO_WAIT))
  	{
  		IOT_DEBUG("tx_byte_allocate [uart_tx_buff] failed!");
    	return;
  	}

    uart_conf.rx_buff = uart_rx_buff;
	uart_conf.tx_buff = uart_tx_buff;
	uart_conf.tx_len = 4096;
	uart_conf.rx_len = 4096;

	/* debug uart init 			*/
	uart_cfg.baud_Rate			= 115200;
	uart_cfg.enable_Flow_Ctrl	= QAPI_FCTL_OFF_E;
	uart_cfg.bits_Per_Char		= QAPI_UART_8_BITS_PER_CHAR_E;
	uart_cfg.enable_Loopback	= 0;
	uart_cfg.num_Stop_Bits		= QAPI_UART_1_0_STOP_BITS_E;
	uart_cfg.parity_Mode		= QAPI_UART_NO_PARITY_E;
	uart_cfg.rx_CB_ISR			= (qapi_UART_Callback_Fn_t)&ssl_uart_rx_cb;
	uart_cfg.tx_CB_ISR			= (qapi_UART_Callback_Fn_t)&ssl_uart_tx_cb;

	status = qapi_UART_Open(&uart_conf.hdlr, uart_conf.port_id, &uart_cfg);
	IOT_DEBUG("QT# qapi_UART_Open [%d] status %d", (qapi_UART_Port_Id_e)uart_conf.port_id, status);
	if (QAPI_OK != status)
	{
		return;
	}
	status = qapi_UART_Power_On(uart_conf.hdlr);
	if (QAPI_OK != status)
	{
		return;
	}
	
	/* start uart receive */
	status = qapi_UART_Receive(uart_conf.hdlr, uart_conf.rx_buff, uart_conf.rx_len, NULL);
	IOT_DEBUG("QT# qapi_UART_Receive [%d] status %d", (qapi_UART_Port_Id_e)uart_conf.port_id, status);
}
void ssl_uart_debug_print(const char* fmt, ...) 
{
	va_list arg_list;
    char dbg_buffer[128] = {0};
    
	va_start(arg_list, fmt);
    vsnprintf((char *)(dbg_buffer), sizeof(dbg_buffer), (char *)fmt, arg_list);
    va_end(arg_list);
		
    qapi_UART_Transmit(uart_conf.hdlr, dbg_buffer, strlen(dbg_buffer), NULL);
    //qapi_Timer_Sleep(50, QAPI_TIMER_UNIT_MSEC, true);   //Do not add delay unless necessary
}

/*
@func
	dss_net_event_cb
@brief
	Initializes the DSS netctrl library for the specified operating mode.
*/
static void ssl_net_event_cb
( 
	qapi_DSS_Hndl_t 		hndl,
	void 				   *user_data,
	qapi_DSS_Net_Evt_t 		evt,
	qapi_DSS_Evt_Payload_t *payload_ptr 
)
{
	qapi_Status_t status = QAPI_OK;
	
	SSL_UART_DBG("Data test event callback, event: %d\n", evt);

	switch (evt)
	{
		case QAPI_DSS_EVT_NET_IS_CONN_E:
		{
			SSL_UART_DBG("Data Call Connected.\n");
			ssl_show_sysinfo();
			/* Signal main task */
  			tx_event_flags_set(ssl_signal_handle, DSS_SIG_EVT_CONN_E, TX_OR);
			ssl_datacall_status = DSS_EVT_NET_IS_CONN_E;
			
			break;
		}
		case QAPI_DSS_EVT_NET_NO_NET_E:
		{
			SSL_UART_DBG("Data Call Disconnected.\n");
			
			if (DSS_EVT_NET_IS_CONN_E == ssl_datacall_status)
			{
				/* Release Data service handle and netctrl library */
				if (ssl_dss_handle)
				{
					status = qapi_DSS_Rel_Data_Srvc_Hndl(ssl_dss_handle);
					if (QAPI_OK == status)
					{
						SSL_UART_DBG("Release data service handle success\n");
						tx_event_flags_set(ssl_signal_handle, DSS_SIG_EVT_EXIT_E, TX_OR);
					}
				}
				
				if (DSS_LIB_STAT_SUCCESS_E == ssl_netctl_lib_status)
				{
					qapi_DSS_Release(QAPI_DSS_MODE_GENERAL);
				}
			}
			else
			{
				/* DSS status maybe QAPI_DSS_EVT_NET_NO_NET_E before data call establishment */
				tx_event_flags_set(ssl_signal_handle, DSS_SIG_EVT_NO_CONN_E, TX_OR);
			}

			break;
		}
		default:
		{
			SSL_UART_DBG("Data Call status is invalid.\n");
			
			/* Signal main task */
  			tx_event_flags_set(ssl_signal_handle, DSS_SIG_EVT_INV_E, TX_OR);
			ssl_datacall_status = DSS_EVT_INVALID_E;
			break;
		}
	}
}

void ssl_show_sysinfo(void)
{
	int i   = 0;
	int j 	= 0;
	unsigned int len = 0;
	uint8 buff[DSS_ADDR_SIZE] = {0}; 
	qapi_Status_t status;
	qapi_DSS_Addr_Info_t info_ptr[DSS_ADDR_INFO_SIZE];

	status = qapi_DSS_Get_IP_Addr_Count(ssl_dss_handle, &len);
	if (QAPI_ERROR == status)
	{
		SSL_UART_DBG("Get IP address count error\n");
		return;
	}
		
	status = qapi_DSS_Get_IP_Addr(ssl_dss_handle, info_ptr, len);
	if (QAPI_ERROR == status)
	{
		SSL_UART_DBG("Get IP address error\n");
		return;
	}
	
	j = GET_ADDR_INFO_MIN(len, DSS_ADDR_INFO_SIZE);
	
	for (i = 0; i < j; i++)
	{
		SSL_UART_DBG("<--- static IP address information --->\n");
		ssl_inet_ntoa(info_ptr[i].iface_addr_s, buff, DSS_ADDR_SIZE);
		SSL_UART_DBG("static IP: %s\n", buff);

		memset(buff, 0, sizeof(buff));
		ssl_inet_ntoa(info_ptr[i].gtwy_addr_s, buff, DSS_ADDR_SIZE);
		SSL_UART_DBG("Gateway IP: %s\n", buff);

		memset(buff, 0, sizeof(buff));
		ssl_inet_ntoa(info_ptr[i].dnsp_addr_s, buff, DSS_ADDR_SIZE);
		SSL_UART_DBG("Primary DNS IP: %s\n", buff);

		memset(buff, 0, sizeof(buff));
		ssl_inet_ntoa(info_ptr[i].dnss_addr_s, buff, DSS_ADDR_SIZE);
		SSL_UART_DBG("Second DNS IP: %s\n", buff);
	}

	SSL_UART_DBG("<--- End of system info --->\n");
}

/*
@func
	ssl_set_data_param
@brief
	Set the Parameter for Data Call, such as APN and network tech.
*/
static int ssl_set_data_param(void)
{
    qapi_DSS_Call_Param_Value_t param_info;
	
	/* Initial Data Call Parameter */
	memset(apn, 0, sizeof(apn));
	memset(apn_username, 0, sizeof(apn_username));
	memset(apn_passwd, 0, sizeof(apn_passwd));
	strlcpy(apn, QL_DEF_APN, QAPI_DSS_CALL_INFO_APN_MAX_LEN);

    if (NULL != ssl_dss_handle)
    {
        /* set data call param */
        param_info.buf_val = NULL;
        param_info.num_val = QAPI_DSS_RADIO_TECH_UNKNOWN;	//Automatic mode(or DSS_RADIO_TECH_LTE)
        SSL_UART_DBG("Setting tech to Automatic\n");
        qapi_DSS_Set_Data_Call_Param(ssl_dss_handle, QAPI_DSS_CALL_INFO_TECH_PREF_E, &param_info);

		/* set apn */
        param_info.buf_val = apn;
        param_info.num_val = strlen(apn);
        SSL_UART_DBG("Setting APN - %s\n", apn);
        qapi_DSS_Set_Data_Call_Param(ssl_dss_handle, QAPI_DSS_CALL_INFO_APN_NAME_E, &param_info);
#ifdef QUEC_CUSTOM_APN
		/* set apn username */
		param_info.buf_val = apn_username;
        param_info.num_val = strlen(apn_username);
        SSL_UART_DBG("Setting APN USER - %s\n", apn_username);
        qapi_DSS_Set_Data_Call_Param(ssl_dss_handle, QAPI_DSS_CALL_INFO_USERNAME_E, &param_info);

		/* set apn password */
		param_info.buf_val = apn_passwd;
        param_info.num_val = strlen(apn_passwd);
        SSL_UART_DBG("Setting APN PASSWORD - %s\n", apn_passwd);
        qapi_DSS_Set_Data_Call_Param(ssl_dss_handle, QAPI_DSS_CALL_INFO_PASSWORD_E, &param_info);
#endif
		/* set IP version(IPv4 or IPv6) */
        param_info.buf_val = NULL;
        param_info.num_val = QAPI_DSS_IP_VERSION_4;
        SSL_UART_DBG("Setting family to IPv%d\n", param_info.num_val);
        qapi_DSS_Set_Data_Call_Param(ssl_dss_handle, QAPI_DSS_CALL_INFO_IP_VERSION_E, &param_info);
    }
    else
    {
        SSL_UART_DBG("Dss handler is NULL!!!\n");
		return -1;
    }
	
    return 0;
}

/*
@func
	ssl_inet_ntoa
@brief
	utility interface to translate ip from "int" to x.x.x.x format.
*/
int32 ssl_inet_ntoa
(
	const qapi_DSS_Addr_t    inaddr, /* IPv4 address to be converted         */
	uint8                   *buf,    /* Buffer to hold the converted address */
	int32                    buflen  /* Length of buffer                     */
)
{
	uint8 *paddr  = (uint8 *)&inaddr.addr.v4;
	int32  rc = 0;

	if ((NULL == buf) || (0 >= buflen))
	{
		rc = -1;
	}
	else
	{
		if (-1 == snprintf((char*)buf, (unsigned int)buflen, "%d.%d.%d.%d",
							paddr[0],
							paddr[1],
							paddr[2],
							paddr[3]))
		{
			rc = -1;
		}
	}

	return rc;
} /* ssl_inet_ntoa() */

/*
@func
	ssl_netctrl_init
@brief
	Initializes the DSS netctrl library for the specified operating mode.
*/
static int ssl_netctrl_init(void)
{
	int ret_val = 0;
	qapi_Status_t status = QAPI_OK;

	SSL_UART_DBG("Initializes the DSS netctrl library\n");

	/* Initializes the DSS netctrl library */
	if (QAPI_OK == qapi_DSS_Init(QAPI_DSS_MODE_GENERAL))
	{
		ssl_netctl_lib_status = DSS_LIB_STAT_SUCCESS_E;
		SSL_UART_DBG("qapi_DSS_Init success\n");
	}
	else
	{
		/* @Note: netctrl library has been initialized */
		ssl_netctl_lib_status = DSS_LIB_STAT_FAIL_E;
		SSL_UART_DBG("DSS netctrl library has been initialized.\n");
	}
	
	/* Registering callback ssl_dss_handleR */
	do
	{
		SSL_UART_DBG("Registering Callback ssl_dss_handle\n");
		
		/* Obtain data service handle */
		status = qapi_DSS_Get_Data_Srvc_Hndl(ssl_net_event_cb, NULL, &ssl_dss_handle);
		SSL_UART_DBG("ssl_dss_handle %d, status %d\n", ssl_dss_handle, status);
		
		if (NULL != ssl_dss_handle)
		{
			SSL_UART_DBG("Registed ssl_dss_handler success\n");
			break;
		}

		/* Obtain data service handle failure, try again after 10ms */
		qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_MSEC, true);
	} while(1);

	return ret_val;
}

/*
@func
	ssl_netctrl_start
@brief
	Start the DSS netctrl library, and startup data call.
*/
int ssl_netctrl_start(void)
{
	int rc = 0;
	qapi_Status_t status = QAPI_OK;
		
	rc = ssl_netctrl_init();
	if (0 == rc)
	{
		/* Get valid DSS handler and set the data call parameter */
		ssl_set_data_param();
	}
	else
	{
		SSL_UART_DBG("quectel_dss_init fail.\n");
		return -1;
	}

	SSL_UART_DBG("qapi_DSS_Start_Data_Call start!!!.\n");
	status = qapi_DSS_Start_Data_Call(ssl_dss_handle);
	if (QAPI_OK == status)
	{
		SSL_UART_DBG("Start Data service success.\n");
		return 0;
	}
	else
	{
		return -1;
	}
}

/*
@func
	ssl_netctrl_release
@brief
	Cleans up the DSS netctrl library and close data service.
*/
int ssl_netctrl_stop(void)
{
	qapi_Status_t stat = QAPI_OK;
	
	if (ssl_dss_handle)
	{
		stat = qapi_DSS_Stop_Data_Call(ssl_dss_handle);
		if (QAPI_OK == stat)
		{
			SSL_UART_DBG("Stop data call success\n");
		}
	}
	
	return 0;
}	

/*
@func
	quec_dataservice_entry
@brief
	The entry of data service task.
*/
void quec_dataservice_thread(ULONG param)
{
	ULONG dss_event = 0;

	/* Start data call */
	ssl_netctrl_start();

	while (1)
	{
		/* Wait disconnect signal */
		tx_event_flags_get(ssl_signal_handle, DSS_SIG_EVT_DIS_E, TX_OR, &dss_event, TX_WAIT_FOREVER);
		if (dss_event & DSS_SIG_EVT_DIS_E)
		{
			/* Stop data call and release resource */
			ssl_netctrl_stop();
			SSL_UART_DBG("Data service task exit.\n");
			break;
		}
	}

	SSL_UART_DBG("Data Service Thread Exit!\n");
	return;
}

static qapi_Status_t ssl_conn_obj_config(SSL_INST *ssl)
{
	qapi_Status_t ret_val = QAPI_OK;
	
	/* default eight encryption suites */
	ssl->config.cipher[0] = QAPI_NET_TLS_RSA_WITH_AES_128_CBC_SHA;
	ssl->config.cipher[1] = QAPI_NET_TLS_RSA_WITH_AES_256_CBC_SHA;
	ssl->config.cipher[2] = QAPI_NET_TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA;
	ssl->config.cipher[3] = QAPI_NET_TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA;
	ssl->config.cipher[4] = QAPI_NET_TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA;
	ssl->config.cipher[5] = QAPI_NET_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA;
	ssl->config.cipher[6] = QAPI_NET_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA;
	ssl->config.cipher[7] = QAPI_NET_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384;
	
	ssl->config.max_Frag_Len = 4096;
	ssl->config.max_Frag_Len_Neg_Disable = 0;
	ssl->config.protocol = QAPI_NET_SSL_PROTOCOL_TLS_1_2;
	ssl->config.verify.domain = 0;
	ssl->config.verify.match_Name[0] = '\0';
	ssl->config.verify.send_Alert = 0;
	ssl->config.verify.time_Validity = 0;	/* Don't check certification expiration */

	return ret_val;
}

void ssl_receive_thread(ULONG param)
{
	SSL_RX_INFO *ssl_rx = (SSL_RX_INFO *)param;
	int recv_len = 0;
	int ret_val = -1;
	fd_set  fdset;
	char recv_buff[RECV_BUF_SIZE];

	SSL_UART_DBG("start receive data thread, sock_fd:%d\n", ssl_rx->sock_fd);
	
	while(1)
	{
		qapi_fd_zero(&fdset);
		qapi_fd_set(ssl_rx->sock_fd, &fdset);
		ret_val = qapi_select(&fdset, NULL, NULL, TX_WAIT_FOREVER);

		if(ret_val > 0)
		{
			if(qapi_fd_isset(ssl_rx->sock_fd, &fdset) != 0)
			{
				memset(recv_buff, 0, sizeof(recv_buff));
		
				recv_len = qapi_Net_SSL_Read(ssl_rx->sslCon, recv_buff, RECV_BUF_SIZE);
				if (recv_len > 0)
				{
					if (0 == strncmp(recv_buff, "Exit", 4))
					{
						qapi_Net_SSL_Shutdown(ssl_rx->sslCon);
						ssl_rx->sslCon = QAPI_NET_SSL_INVALID_HANDLE;
						qapi_socketclose(ssl_rx->sock_fd);
						ssl_rx->sock_fd = -1;
						tx_event_flags_set(ssl_signal_handle, DSS_SIG_EVT_DIS_E, TX_OR);
						SSL_UART_DBG("SSL Client Exit!!!\n");
						break;
					}

					/* Reveive data */
					SSL_UART_DBG("[SSL Client]@len[%d], @Recv: %s\n", recv_len, recv_buff);
				}
				else if(recv_len == 0)
				{
					SSL_UART_DBG("SSL socket connection is closed.\n");
					qapi_Net_SSL_Shutdown(ssl_rx->sslCon);
					ssl_rx->sslCon = QAPI_NET_SSL_INVALID_HANDLE;
					qapi_socketclose(ssl_rx->sock_fd);
					ssl_rx->sock_fd = -1;
					tx_event_flags_set(ssl_signal_handle, DSS_SIG_EVT_DIS_E, TX_OR);
					break;
				}
			}
		}
	}
}

static int start_tcp_session_with_tls(SSL_INST *ssl)
{
	int result = 0;
	int ret = -1;
	int32 sock_fd = -1;
	int sent_len = 0;
	char sent_buff[SENT_BUF_SIZE];
	SSL_RX_INFO ssl_rx;
	struct sockaddr_in server_addr;

	memset(&ssl_rx, 0, sizeof(SSL_RX_INFO));
	do
	{
		sock_fd = qapi_socket(AF_INET, DEF_SRC_TYPE, 0);
		if (sock_fd < 0)
		{
			SSL_UART_DBG("Create socket error\n");	
			result = -1;
			break;
		}

		SSL_UART_DBG("<-- Create socket[%d] success -->\n", sock_fd);
		memset(sent_buff, 0, sizeof(sent_buff));
		memset(&server_addr, 0, sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = _htons(DEF_SRV_PORT);
		server_addr.sin_addr.s_addr = inet_addr(DEF_SRV_ADDR);

		/* Connect to server */
		if (-1 == qapi_connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)))
		{
			SSL_UART_DBG("Connect to servert error\n");
			result = -1;
			break;
		}
		else
		{
			ssl_rx.sock_fd = sock_fd;
		}

		SSL_UART_DBG("<-- SSL[%lu] Connecting -->\n", ssl->sslCtx);
		SSL_UART_DBG("<-- Add socket handle to SSL connection -->\n");
		result = qapi_Net_SSL_Fd_Set(ssl->sslCon, sock_fd);
		if (result < 0)
		{
			SSL_UART_DBG("ERROR: Unable to add socket handle to SSL (%d)", result);
			result = -2;
			break;
		}

		SSL_UART_DBG("<-- Start TLS handshake with server -->\n");
		result = qapi_Net_SSL_Connect(ssl->sslCon);
		qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_MSEC, true);
		if(result < QAPI_OK)
		{
			if (result == QAPI_SSL_OK_HS)
			 {
				 /** The peer's SSL certificate is trusted, CN matches the host name, time is valid */
				 SSL_UART_DBG("The certificate is trusted\n");
				 ssl_rx.sslCon = ssl->sslCon;
				 ssl_conn_handle = ssl->sslCon;
			 }
			 else if (result == QAPI_ERR_SSL_CERT_CN)
			 {
				 /** The peer's SSL certificate is trusted, CN does not matches the host name, time is valid */
				 SSL_UART_DBG("ERROR: The certificate is trusted, but the host name is not valid\n"); 
				 result = -2;
				 break;
			 }
			 else if (result == QAPI_ERR_SSL_CERT_TIME)
			 {
				 /** The SSL certificate of the peer is trusted, CN matches the host name, time has expired. */
				 SSL_UART_DBG("ERROR: The certificate is expired.\n");
				 result = -2;
				 break;
			 }
			 else if (result == QAPI_ERR_SSL_CERT_NONE)
			 {
				 /** The SSL certificate of the peer is not trusted.*/
				 SSL_UART_DBG("ERROR: The certificate is not trusted.\n");
				 result = -2;
				 break;
			 }
			 else
			 {
				 SSL_UART_DBG("ERROR: SSL connect failed, %d\n", result);
				 result = -2;
				 break;
			 }
		}

		strcpy(sent_buff, "Hello Quectel, Test SSL Demo!");
		sent_len = qapi_Net_SSL_Write(ssl->sslCon, sent_buff, strlen(sent_buff));
		if(sent_len != strlen(sent_buff))
		{
			SSL_UART_DBG("ERROR: SSL send failed\n");
			result = -2;
			break;
		}
		else
		{
			SSL_UART_DBG("Client send data success, len: %d, data: %s\n", sent_len, sent_buff);
		}

		/* To receive data on the same SSL Session, user need to create a recv thread and use the same SSL connection Descriptor. */
#ifdef QAPI_TXM_MODULE
		if (TX_SUCCESS != txm_module_object_allocate((VOID *)&ssl_thread_handle, sizeof(TX_THREAD))) 
		{
			result = -2;;
		}
#endif
		ret = tx_thread_create(ssl_thread_handle, "SSL RX Thread", ssl_receive_thread, (ULONG)(&ssl_rx),
								ssl_rx_stack, THREAD_STACK_SIZE, THREAD_PRIORITY, THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);
		if (ret != TX_SUCCESS)
		{
			IOT_INFO("Thread creation failed\n");
			result = -2;
		}
	
	}while(0);

	return result;
}
static int read_from_EFS_file(const char *name, void **buffer_ptr, size_t *buffer_size)
{
	int bytes_read;
	int efs_fd = -1;
	struct qapi_FS_Stat_Type_s stat;
	uint8 *file_buf = NULL;
	stat.st_size = 0;

	if ((!name) || (!buffer_ptr) ||(!buffer_size))
	{
		SSL_UART_DBG("Reading SSL from EFS file failed!\n");
		return QAPI_ERROR;
	}

	if (qapi_FS_Open(name, QAPI_FS_O_RDONLY_E, &efs_fd) < 0)
	{
		SSL_UART_DBG("Opening EFS EFS file %s failed\n", name);
		return QAPI_ERROR;
	}

	if (qapi_FS_Stat_With_Handle(efs_fd, &stat) < 0) 
	{
		SSL_UART_DBG("Reading EFS file %s failed\n", name);
		return QAPI_ERROR;
	}

	SSL_UART_DBG("Reading EFS file size %d \n", stat.st_size);
	
	tx_byte_allocate(byte_pool_ssl, (VOID **) &file_buf, stat.st_size, TX_NO_WAIT);
	if (file_buf  == NULL) 
	{
		SSL_UART_DBG("SSL_cert_store: QAPI_ERR_NO_MEMORY \n");
		return QAPI_ERR_NO_MEMORY;
	}

	qapi_FS_Read(efs_fd, file_buf, stat.st_size, &bytes_read);
	if ((bytes_read < 0) || (bytes_read != stat.st_size)) 
	{
		tx_byte_release(file_buf);
		SSL_UART_DBG("SSL_cert_store: Reading EFS file error\n");
		return QAPI_ERROR;
	}

	*buffer_ptr = file_buf;
	*buffer_size = stat.st_size;

	qapi_FS_Close(efs_fd);

	return QAPI_OK;
}

static int ssl_verify_method(ssl_verify_method_e *verify_method)
{
	int ca_fd = -1;
	int cert_fd = -1;
	int key_fd = -1;
	qapi_FS_Status_t fs_stat = QAPI_OK;

	*verify_method = SSL_NO_METHOD;

	/* check CAList is exist or not */
	fs_stat = qapi_FS_Open(SSL_CA_PEM, QAPI_FS_O_RDONLY_E, &ca_fd);
	if ((fs_stat != QAPI_OK) || (ca_fd < 0))
	{
		SSL_UART_DBG("Cannot find or open CAList file in EFS\n");
		*verify_method = SSL_NO_METHOD;
	}
	else
	{
		*verify_method = SSL_ONEWAY_METHOD;
		
		/* check client certificate is exist or not */
		fs_stat = qapi_FS_Open(SSL_CERT_PEM, QAPI_FS_O_RDONLY_E, &cert_fd);
		if ((fs_stat != QAPI_OK) || (cert_fd < 0))
		{
			SSL_UART_DBG("Cannot find or open Cert file in EFS\n");
			*verify_method = SSL_ONEWAY_METHOD;
		}
		else
		{
			*verify_method = SSL_TWOWAY_METHOD;
			
			/* if client certificate is exist, client private key must be existed */
			fs_stat = qapi_FS_Open(SSL_PREKEY_PEM, QAPI_FS_O_RDONLY_E, &key_fd);
			if ((fs_stat != QAPI_OK) || (key_fd < 0))
			{
				SSL_UART_DBG("Cannot find or open key file in EFS\n");
				return -1;	//miss client key 
			}
			else
			{
				qapi_FS_Close(key_fd);
			}
			
			qapi_FS_Close(cert_fd);
		}

		qapi_FS_Close(ca_fd);
	}

	return 0;
}

static qapi_Status_t ssl_cert_load(SSL_INST *ssl)
{
	qapi_Status_t result = QAPI_OK;
	int32_t ret_val = 0;

#if 0	
	int cert_data_buf_len;
	char *cert_data_buf;
#endif

	uint8_t *cert_Buf = NULL; 
	uint32_t cert_Size = 0;
	uint8_t *key_Buf = NULL;
	uint32_t key_Size = 0;
	qapi_Net_SSL_Cert_Info_t cert_info;
	qapi_NET_SSL_CA_Info_t calist_info[QAPI_NET_SSL_MAX_CA_LIST];
	ssl_verify_method_e verify_method;

	SSL_UART_DBG("Start ssl cert load process\n");

	memset(&cert_info, 0, sizeof(qapi_Net_SSL_Cert_Info_t));

	if (0 != ssl_verify_method(&verify_method))
	{
		SSL_UART_DBG("Miss certificates in EFS, error return\n");
		return QAPI_ERROR;
	}
	
	switch (verify_method)
	{
		case SSL_TWOWAY_METHOD:
		{
			/* Read the client certificate information */
		    ret_val = read_from_EFS_file((char *)SSL_CERT_PEM, (void **)&cert_Buf, (size_t *)&cert_Size);
			SSL_UART_DBG("Read %s, result %d\n", SSL_CERT_PEM, ret_val);
		    if (QAPI_OK != ret_val) 
		    {
		         SSL_UART_DBG("ERROR: Reading client certificate from EFS failed!! \r\n");
		         result = QAPI_ERROR;
		    }

		    /* Read the client key information */
		    ret_val = read_from_EFS_file((char *)SSL_PREKEY_PEM, (void **)&key_Buf, (size_t *)&key_Size);
			SSL_UART_DBG("Read %s, result %d\n", SSL_PREKEY_PEM, ret_val);
		    if (QAPI_OK != ret_val) 
		    {
		         SSL_UART_DBG("ERROR: Reading key information from EFS failed!! \r\n");
		         result = QAPI_ERROR;
		    }

			/* Update the client certificate information */
			cert_info.cert_Type = QAPI_NET_SSL_CERTIFICATE_E; 
		    cert_info.info.cert.cert_Buf = cert_Buf;
		    cert_info.info.cert.cert_Size = cert_Size;
		    cert_info.info.cert.key_Buf = key_Buf;
		    cert_info.info.cert.key_Size = key_Size;

			/* Convert and store the certificate */ 
			result = qapi_Net_SSL_Cert_Convert_And_Store(&cert_info, SSL_CERT_BIN);
			SSL_UART_DBG("%s qapi_Net_SSL_Cert_Convert_And_Store: %d\n", SSL_CERT_BIN, result);
			
			if (result == QAPI_OK)
			{
				if (qapi_Net_SSL_Cert_Load(ssl->sslCtx, QAPI_NET_SSL_CERTIFICATE_E, SSL_CERT_BIN) < 0)
				{
					SSL_UART_DBG("ERROR: Unable to load client cert from FLASH\n");
					//return QCLI_STATUS_ERROR_E;
				}
				else
				{
					tx_byte_release(cert_Buf);
					tx_byte_release(key_Buf);
				}
			}

			/* continue to load CAList */
		}
		case SSL_ONEWAY_METHOD:
		{
			/* Store CA List */
			ret_val = read_from_EFS_file(SSL_CA_PEM, (void **)&calist_info[0].ca_Buf, (size_t *)&calist_info[0].ca_Size);
			SSL_UART_DBG("Read %s, result %d\n", SSL_CA_PEM, ret_val);

			if (QAPI_OK != ret_val) 
			{
				SSL_UART_DBG("ERROR: Reading ca information from EFS failed!! \r\n");
				result = QAPI_ERROR;
			}

			cert_info.info.ca_List.ca_Info[0] = &calist_info[0];
			cert_info.info.ca_List.ca_Cnt = 1;
			cert_info.cert_Type = QAPI_NET_SSL_CA_LIST_E;
			
			result = qapi_Net_SSL_Cert_Convert_And_Store(&cert_info, SSL_CA_BIN);
			SSL_UART_DBG("%s qapi_Net_SSL_Cert_Convert_And_Store: %d\n", SSL_CA_BIN, result);
			
			if (result == QAPI_OK)
			{
				if (qapi_Net_SSL_Cert_Load(ssl->sslCtx, QAPI_NET_SSL_CA_LIST_E, SSL_CA_BIN) < 0)
				{
					SSL_UART_DBG("ERROR: Unable to load CA from FLASH\n");
					//return QCLI_STATUS_ERROR_E;
				}
				else
				{
					tx_byte_release(calist_info[0].ca_Buf);
				}
			}

			break;
		}			
		default:
		{
			SSL_UART_DBG("Don't need to verify certifications\n");
			break;
		}
	}
	return result;
}

static int ssl_config(SSL_INST *ssl)
{
	int ret_val = 0;	
	qapi_Status_t result = QAPI_OK;
	qapi_Net_SSL_Role_t role = QAPI_NET_SSL_CLIENT_E;	/* TLS Client object creation. */

	memset(ssl, 0, sizeof(SSL_INST));	
	ssl->role = role;
	ssl->sslCtx = qapi_Net_SSL_Obj_New(role);
	if (ssl->sslCtx == QAPI_NET_SSL_INVALID_HANDLE)
	{
		SSL_UART_DBG("ERROR: Unable to create SSL context");
		return -1;
	}

	/* 
	 * allocate memory and read the certificate from certificate server or EFS.
	 * Once cert_data_buf filled with valid SSL certificate, Call QAPI to Store and Load 
	 */
	result = ssl_cert_load(ssl);
	if (result != QAPI_OK)
	{
		SSL_UART_DBG("ssl_cert_load return error");
		ret_val = -1;
	}
	
	SSL_UART_DBG("--->ssl config: New SSL Object ssl->sslCtx - %lu", ssl->sslCtx);

	/* TLS Configuration of a Connection Object */
	result = ssl_conn_obj_config(ssl);
	
	return ret_val;
}

static int start_ssl_session(void)
{
	int ret = 0;
	SSL_INST ssl;
	
	if (0 != ssl_config(&ssl))
	{
		SSL_UART_DBG("ERROR: Config ssl object and certificates error\n");
		return -1;
	}

	/* create a SSL session */
	ssl_obj_hdl = ssl.sslCtx;
	ssl.sslCon = qapi_Net_SSL_Con_New(ssl.sslCtx, QAPI_NET_SSL_TLS_E);
	if (ssl.sslCon == QAPI_NET_SSL_INVALID_HANDLE)
	{
		SSL_UART_DBG("ERROR: Unable to create SSL context\n");
		return -1;
	}

	if (QAPI_OK != qapi_Net_SSL_Configure(ssl.sslCon, &ssl.config))
	{
		SSL_UART_DBG("ERROR: SSL configure failed\n");
		return -2;
	}

	ret = start_tcp_session_with_tls(&ssl);
	if (ret == -1)
	{
		SSL_UART_DBG("ERROR: tcp connect error\n");
		return -3;
	}
	else if (ret == -2)
	{
		SSL_UART_DBG("ERROR: ssl connect error\n");
		qapi_Net_SSL_Shutdown(ssl.sslCon); 
		ssl.sslCon = QAPI_NET_SSL_INVALID_HANDLE;
		return -4;
	}
	
	return 0;
}

#ifdef NW_SIGNAL_ENABLE
void ssl_network_indication_cb(qapi_Device_Info_Hndl_t device_info_hndl, const qapi_Device_Info_t *dev_info)
{
	if(dev_info->id == QAPI_DEVICE_INFO_NETWORK_IND_E)
	{
		switch(dev_info->info_type)
		{
			case QAPI_DEVICE_INFO_TYPE_BOOLEAN_E:
				SSL_UART_DBG( "~type[b] id[%d] status[%d]\n", dev_info->id, dev_info->u.valuebool);
				if(dev_info->u.valuebool == true)
				{
					tx_event_flags_set(ssl_nw_signal_handle, NW_SIG_EVT_ENABLE, TX_OR);
				}
				else
				{
					tx_event_flags_set(ssl_nw_signal_handle, NW_SIG_EVT_UNENABLE, TX_OR);
				}
			break;

			case QAPI_DEVICE_INFO_TYPE_INTEGER_E:
				SSL_UART_DBG( "~type[i] id[%d] status[%d]\n", dev_info->id, dev_info->u.valueint);
			break;

			case QAPI_DEVICE_INFO_TYPE_BUFFER_E:
				SSL_UART_DBG( "~type[s] id[%d] status[%s]\n", dev_info->id, dev_info->u.valuebuf.buf);
			break;

			default:
				SSL_UART_DBG( "~[Invalid][%d]\n", dev_info->id);
			break;
		}
	}
}

void qt_ssl_init_device_info(void)
{
	qapi_Status_t status = 0;

	status = qapi_Device_Info_Init_v2(&device_info_hndl);
	if(status != QAPI_OK)
	{
		SSL_UART_DBG( "~ qapi_Device_Info_Init fail [%d]\n", status);
	}
	else
	{
		SSL_UART_DBG( "~ qapi_Device_Info_Init OK [%d]\n", status);
	}
}
#endif /* NW_SIGNAL_ENABLE */

/*
@func
	quectel_task_entry
@brief
	The entry of data service task.
*/
int quectel_task_entry(void)
{

	int     ret = 0;
	ULONG   dss_event = 0;
	int32   sig_mask;
	ULONG   nw_event = 0;
	int32   nw_sig_mask;
	qapi_Status_t status = QAPI_OK;

#ifndef NW_SIGNAL_ENABLE 
	/* wait 10sec(set according to the time required for network registration) for device startup.
	if checking network status by device_info, do not need timer sleep. */
	qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_SEC, true);
#endif

	/* Create a byte memory pool. */
	txm_module_object_allocate(&byte_pool_ssl, sizeof(TX_BYTE_POOL));
	tx_byte_pool_create(byte_pool_ssl, "ssl application pool", free_memory_ssl, SSL_BYTE_POOL_SIZE);

	/* Initial uart for debug */
	ssl_uart_dbg_init();
	SSL_UART_DBG("SSL Task Start...\n");

#ifdef NW_SIGNAL_ENABLE 
	/* waiting for registering the network */
	txm_module_object_allocate(&ssl_nw_signal_handle, sizeof(TX_EVENT_FLAGS_GROUP));
	tx_event_flags_create(ssl_nw_signal_handle, "nw_signal_event");
	tx_event_flags_set(ssl_nw_signal_handle, 0x0, TX_AND);
	
	qt_ssl_init_device_info();
	status = qapi_Device_Info_Pass_Pool_Ptr(device_info_hndl, byte_pool_ssl);
	SSL_UART_DBG("[dev_info] status, status %d\n", status);
	status = qapi_Device_Info_Set_Callback_v2(device_info_hndl, QAPI_DEVICE_INFO_NETWORK_IND_E, ssl_network_indication_cb);
	SSL_UART_DBG("[dev_info] qapi_Device_Info_Set_Callback, status %d\n", status);

	nw_sig_mask = NW_SIG_EVT_UNENABLE | NW_SIG_EVT_ENABLE;
	while(1)
	{
		tx_event_flags_get(ssl_nw_signal_handle, nw_sig_mask, TX_OR, &nw_event, TX_WAIT_FOREVER);

		if (nw_event & NW_SIG_EVT_UNENABLE)
		{
			SSL_UART_DBG("waiting for network registering...\n");
			tx_event_flags_set(ssl_nw_signal_handle, ~NW_SIG_EVT_UNENABLE, TX_AND);
		}
		else if (nw_event & NW_SIG_EVT_ENABLE)
		{
			SSL_UART_DBG("network registered!\n");
			tx_event_flags_set(ssl_nw_signal_handle, ~NW_SIG_EVT_ENABLE, TX_AND);
			break;
		}
	}
#endif /* NW_SIGNAL_ENABLE */
	
	/* Create event signal handle and clear signals */
	txm_module_object_allocate(&ssl_signal_handle, sizeof(TX_EVENT_FLAGS_GROUP));
	tx_event_flags_create(ssl_signal_handle, "dss_signal_event");
	tx_event_flags_set(ssl_signal_handle, 0x0, TX_AND);

	/* Start DSS thread, and detect iface status */
#ifdef QAPI_TXM_MODULE
	if (TX_SUCCESS != txm_module_object_allocate((VOID *)&dss_thread_handle, sizeof(TX_THREAD))) 
	{
		return -1;
	}
#endif
	ret = tx_thread_create(dss_thread_handle, "SSL DSS Thread", quec_dataservice_thread, NULL,
							ssl_dss_stack, THREAD_STACK_SIZE, THREAD_PRIORITY, THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);
	if (ret != TX_SUCCESS)
	{
		IOT_INFO("Thread creation failed\n");
	}

	sig_mask = DSS_SIG_EVT_INV_E | DSS_SIG_EVT_NO_CONN_E | DSS_SIG_EVT_CONN_E | DSS_SIG_EVT_EXIT_E;
	while (1)
	{
		/* SSLClient signal process */
		tx_event_flags_get(ssl_signal_handle, sig_mask, TX_OR, &dss_event, TX_WAIT_FOREVER);
		SSL_UART_DBG("SIGNAL EVENT IS [%d]\n", dss_event);
		
		if (dss_event & DSS_SIG_EVT_INV_E)
		{
			SSL_UART_DBG("DSS_SIG_EVT_INV_E Signal\n");
			tx_event_flags_set(ssl_signal_handle, ~DSS_SIG_EVT_INV_E, TX_AND);
		}
		else if (dss_event & DSS_SIG_EVT_NO_CONN_E)
		{
			SSL_UART_DBG("DSS_SIG_EVT_NO_CONN_E Signal\n");
			tx_event_flags_set(ssl_signal_handle, ~DSS_SIG_EVT_NO_CONN_E, TX_AND);
		}
		else if (dss_event & DSS_SIG_EVT_CONN_E)
		{
			SSL_UART_DBG("DSS_SIG_EVT_CONN_E Signal\n");
			/* Create a ssl context and connect with server */
			if(0 != start_ssl_session())
			{
				if(ssl_obj_hdl)
				{
					qapi_Net_SSL_Obj_Free(ssl_obj_hdl); 
					ssl_obj_hdl = QAPI_NET_SSL_INVALID_HANDLE;
				}
			}
          tx_event_flags_set(ssl_signal_handle, ~DSS_SIG_EVT_CONN_E, TX_AND);
		}
		else if (dss_event & DSS_SIG_EVT_EXIT_E)
		{
			SSL_UART_DBG("DSS_SIG_EVT_EXIT_E Signal\n");
			if(ssl_obj_hdl)
			{
				qapi_Net_SSL_Obj_Free(ssl_obj_hdl); 
				ssl_obj_hdl = QAPI_NET_SSL_INVALID_HANDLE;
			}
			tx_event_flags_set(ssl_signal_handle, ~DSS_SIG_EVT_EXIT_E, TX_AND);
			tx_event_flags_delete(&ssl_signal_handle);
			break;
		}
		else
		{
			SSL_UART_DBG("Unknown Signal\n");
		}

		/* Clear all signals and wait next notification */
		tx_event_flags_set(ssl_signal_handle, 0x0, TX_AND);	//@Fixme:maybe not need
	}
	
	SSL_UART_DBG("Quectel SSL Client Demo is Over!");
	return 0;
}
#endif /*__EXAMPLE_SSL__*/
/* End of Example_SSL.c */
