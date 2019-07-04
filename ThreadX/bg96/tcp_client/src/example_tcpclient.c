/******************************************************************************
*@file    example_tcpclient.c
*@brief   Detection of network state and notify the main task to create tcp client.
*         If server send "Exit" to client, client will exit immediately.
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/

#if defined(__EXAMPLE_TCPCLIENT__)
/*===========================================================================
						   Header file
===========================================================================*/
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include "qapi_fs_types.h"
#include "qapi_status.h"
#include "qapi_socket.h"
#include "qapi_dss.h"
#include "qapi_netservices.h"

#include "qapi_uart.h"
#include "qapi_timer.h"
#include "qapi_diag.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "example_tcpclient.h"

/*===========================================================================
                             DEFINITION
===========================================================================*/
#define QL_DEF_APN	        "CMNET"
#define DSS_ADDR_INFO_SIZE	5
#define DSS_ADDR_SIZE       16

#define GET_ADDR_INFO_MIN(a, b) ((a) > (b) ? (b) : (a))

#define QUEC_TCP_UART_DBG
#ifdef QUEC_TCP_UART_DBG
#define TCP_UART_DBG(...)	\
{\
	tcp_uart_debug_print(__VA_ARGS__);	\
}
#else
#define TCP_UART_DBG(...)
#endif

#define THREAD_STACK_SIZE    (1024 * 16)
#define THREAD_PRIORITY      (180)
#define BYTE_POOL_SIZE       (1024 * 16)

#define CMD_BUF_SIZE  100

#define DEFAULT_PUB_TIME 5

/*===========================================================================
                           Global variable
===========================================================================*/
/* TCPClient dss thread handle */
#ifdef QAPI_TXM_MODULE
	static TX_THREAD *dss_thread_handle; 
#else
	static TX_THREAD _dss_thread_handle;
	static TX_THREAD *ts_thread_handle = &_dss_thread_handle;
#endif

static unsigned char tcp_dss_stack[THREAD_STACK_SIZE];

TX_EVENT_FLAGS_GROUP *tcp_signal_handle;

qapi_DSS_Hndl_t tcp_dss_handle = NULL;	            /* Related to DSS netctrl */

static char apn[QUEC_APN_LEN];					/* APN */
static char apn_username[QUEC_APN_USER_LEN];	/* APN username */
static char apn_passwd[QUEC_APN_PASS_LEN];		/* APN password */

/* @Note: If netctrl library fail to initialize, set this value will be 1,
 * We should not release library when it is 1. 
 */
signed char tcp_netctl_lib_status = DSS_LIB_STAT_INVALID_E;
unsigned char tcp_datacall_status = DSS_EVT_INVALID_E;

TX_BYTE_POOL *byte_pool_tcp;
#define TCP_BYTE_POOL_SIZE		10*8*1024
UCHAR free_memory_tcp[TCP_BYTE_POOL_SIZE];

/* uart rx tx buffer */
static char *uart_rx_buff = NULL;	/*!!! should keep this buffer as 4K Bytes */
static char *uart_tx_buff = NULL;

/* uart config para*/
static QT_UART_CONF_PARA uart_conf =
{
	NULL,
	QT_UART_PORT_02,
	NULL,
	0,
	NULL,
	0,
	115200
};

/*===========================================================================
                               FUNCTION
===========================================================================*/
void tcp_uart_dbg_init()
{
  	if (TX_SUCCESS != tx_byte_allocate(byte_pool_tcp, (VOID *)&uart_rx_buff, 4*1024, TX_NO_WAIT))
  	{
  		IOT_DEBUG("tx_byte_allocate [uart_rx_buff] failed!");
    	return;
  	}

  	if (TX_SUCCESS != tx_byte_allocate(byte_pool_tcp, (VOID *)&uart_tx_buff, 4*1024, TX_NO_WAIT))
  	{
  		IOT_DEBUG("tx_byte_allocate [uart_tx_buff] failed!");
    	return;
  	}

    uart_conf.rx_buff = uart_rx_buff;
	uart_conf.tx_buff = uart_tx_buff;
	uart_conf.tx_len = 4096;
	uart_conf.rx_len = 4096;

	/* debug uart init 			*/
	uart_init(&uart_conf);
	/* start uart receive */
	uart_recv(&uart_conf);
}
void tcp_uart_debug_print(const char* fmt, ...) 
{
	va_list arg_list;
    char dbg_buffer[128] = {0};
    
	va_start(arg_list, fmt);
    vsnprintf((char *)(dbg_buffer), sizeof(dbg_buffer), (char *)fmt, arg_list);
    va_end(arg_list);
		
    qapi_UART_Transmit(uart_conf.hdlr, dbg_buffer, strlen(dbg_buffer), NULL);
    qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_MSEC, true);   //50
}


/*
@func
	dss_net_event_cb
@brief
	Initializes the DSS netctrl library for the specified operating mode.
*/
static void tcp_net_event_cb
( 
	qapi_DSS_Hndl_t 		hndl,
	void 				   *user_data,
	qapi_DSS_Net_Evt_t 		evt,
	qapi_DSS_Evt_Payload_t *payload_ptr 
)
{
	qapi_Status_t status = QAPI_OK;
	
	TCP_UART_DBG("Data test event callback, event: %d\n", evt);

	switch (evt)
	{
		case QAPI_DSS_EVT_NET_IS_CONN_E:
		{
			TCP_UART_DBG("Data Call Connected.\n");
			tcp_show_sysinfo();
			/* Signal main task */
  			tx_event_flags_set(tcp_signal_handle, DSS_SIG_EVT_CONN_E, TX_OR);
			tcp_datacall_status = DSS_EVT_NET_IS_CONN_E;
			
			break;
		}
		case QAPI_DSS_EVT_NET_NO_NET_E:
		{
			TCP_UART_DBG("Data Call Disconnected.\n");
			
			if (DSS_EVT_NET_IS_CONN_E == tcp_datacall_status)
			{
				/* Release Data service handle and netctrl library */
				if (tcp_dss_handle)
				{
					status = qapi_DSS_Rel_Data_Srvc_Hndl(tcp_dss_handle);
					if (QAPI_OK == status)
					{
						TCP_UART_DBG("Release data service handle success\n");
						tx_event_flags_set(tcp_signal_handle, DSS_SIG_EVT_EXIT_E, TX_OR);
					}
				}
				
				if (DSS_LIB_STAT_SUCCESS_E == tcp_netctl_lib_status)
				{
					qapi_DSS_Release(QAPI_DSS_MODE_GENERAL);
				}
			}
			else
			{
				/* DSS status maybe QAPI_DSS_EVT_NET_NO_NET_E before data call establishment */
				tx_event_flags_set(tcp_signal_handle, DSS_SIG_EVT_NO_CONN_E, TX_OR);
			}

			break;
		}
		default:
		{
			TCP_UART_DBG("Data Call status is invalid.\n");
			
			/* Signal main task */
  			tx_event_flags_set(tcp_signal_handle, DSS_SIG_EVT_INV_E, TX_OR);
			tcp_datacall_status = DSS_EVT_INVALID_E;
			break;
		}
	}
}

void tcp_show_sysinfo(void)
{
	int i   = 0;
	int j 	= 0;
	unsigned int len = 0;
	uint8 buff[DSS_ADDR_SIZE] = {0}; 
	qapi_Status_t status;
	qapi_DSS_Addr_Info_t info_ptr[DSS_ADDR_INFO_SIZE];

	status = qapi_DSS_Get_IP_Addr_Count(tcp_dss_handle, &len);
	if (QAPI_ERROR == status)
	{
		TCP_UART_DBG("Get IP address count error\n");
		return;
	}
		
	status = qapi_DSS_Get_IP_Addr(tcp_dss_handle, info_ptr, len);
	if (QAPI_ERROR == status)
	{
		TCP_UART_DBG("Get IP address error\n");
		return;
	}
	
	j = GET_ADDR_INFO_MIN(len, DSS_ADDR_INFO_SIZE);
	
	for (i = 0; i < j; i++)
	{
		TCP_UART_DBG("<--- static IP address information --->\n");
		tcp_inet_ntoa(info_ptr[i].iface_addr_s, buff, DSS_ADDR_SIZE);
		TCP_UART_DBG("static IP: %s\n", buff);

		memset(buff, 0, sizeof(buff));
		tcp_inet_ntoa(info_ptr[i].gtwy_addr_s, buff, DSS_ADDR_SIZE);
		TCP_UART_DBG("Gateway IP: %s\n", buff);

		memset(buff, 0, sizeof(buff));
		tcp_inet_ntoa(info_ptr[i].dnsp_addr_s, buff, DSS_ADDR_SIZE);
		TCP_UART_DBG("Primary DNS IP: %s\n", buff);

		memset(buff, 0, sizeof(buff));
		tcp_inet_ntoa(info_ptr[i].dnss_addr_s, buff, DSS_ADDR_SIZE);
		TCP_UART_DBG("Second DNS IP: %s\n", buff);
	}

	TCP_UART_DBG("<--- End of system info --->\n");
}

/*
@func
	tcp_set_data_param
@brief
	Set the Parameter for Data Call, such as APN and network tech.
*/
static int tcp_set_data_param(void)
{
    qapi_DSS_Call_Param_Value_t param_info;
	
	/* Initial Data Call Parameter */
	memset(apn, 0, sizeof(apn));
	memset(apn_username, 0, sizeof(apn_username));
	memset(apn_passwd, 0, sizeof(apn_passwd));
	strlcpy(apn, QL_DEF_APN, QAPI_DSS_CALL_INFO_APN_MAX_LEN);

    if (NULL != tcp_dss_handle)
    {
        /* set data call param */
        param_info.buf_val = NULL;
        param_info.num_val = QAPI_DSS_RADIO_TECH_UNKNOWN;	//Automatic mode(or DSS_RADIO_TECH_LTE)
        TCP_UART_DBG("Setting tech to Automatic\n");
        qapi_DSS_Set_Data_Call_Param(tcp_dss_handle, QAPI_DSS_CALL_INFO_TECH_PREF_E, &param_info);

		/* set apn */
        param_info.buf_val = apn;
        param_info.num_val = strlen(apn);
        TCP_UART_DBG("Setting APN - %s\n", apn);
        qapi_DSS_Set_Data_Call_Param(tcp_dss_handle, QAPI_DSS_CALL_INFO_APN_NAME_E, &param_info);
#ifdef QUEC_CUSTOM_APN
		/* set apn username */
		param_info.buf_val = apn_username;
        param_info.num_val = strlen(apn_username);
        TCP_UART_DBG("Setting APN USER - %s\n", apn_username);
        qapi_DSS_Set_Data_Call_Param(tcp_dss_handle, QAPI_DSS_CALL_INFO_USERNAME_E, &param_info);

		/* set apn password */
		param_info.buf_val = apn_passwd;
        param_info.num_val = strlen(apn_passwd);
        TCP_UART_DBG("Setting APN PASSWORD - %s\n", apn_passwd);
        qapi_DSS_Set_Data_Call_Param(tcp_dss_handle, QAPI_DSS_CALL_INFO_PASSWORD_E, &param_info);
#endif
		/* set IP version(IPv4 or IPv6) */
        param_info.buf_val = NULL;
        param_info.num_val = QAPI_DSS_IP_VERSION_4;
        TCP_UART_DBG("Setting family to IPv%d\n", param_info.num_val);
        qapi_DSS_Set_Data_Call_Param(tcp_dss_handle, QAPI_DSS_CALL_INFO_IP_VERSION_E, &param_info);
    }
    else
    {
        TCP_UART_DBG("Dss handler is NULL!!!\n");
		return -1;
    }
	
    return 0;
}

/*
@func
	tcp_inet_ntoa
@brief
	utility interface to translate ip from "int" to x.x.x.x format.
*/
int32 tcp_inet_ntoa
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
} /* tcp_inet_ntoa() */

/*
@func
	tcp_netctrl_init
@brief
	Initializes the DSS netctrl library for the specified operating mode.
*/
static int tcp_netctrl_init(void)
{
	int ret_val = 0;
	qapi_Status_t status = QAPI_OK;

	TCP_UART_DBG("Initializes the DSS netctrl library\n");

	/* Initializes the DSS netctrl library */
	if (QAPI_OK == qapi_DSS_Init(QAPI_DSS_MODE_GENERAL))
	{
		tcp_netctl_lib_status = DSS_LIB_STAT_SUCCESS_E;
		TCP_UART_DBG("qapi_DSS_Init success\n");
	}
	else
	{
		/* @Note: netctrl library has been initialized */
		tcp_netctl_lib_status = DSS_LIB_STAT_FAIL_E;
		TCP_UART_DBG("DSS netctrl library has been initialized.\n");
	}
	
	/* Registering callback tcp_dss_handleR */
	do
	{
		TCP_UART_DBG("Registering Callback tcp_dss_handle\n");
		
		/* Obtain data service handle */
		status = qapi_DSS_Get_Data_Srvc_Hndl(tcp_net_event_cb, NULL, &tcp_dss_handle);
		TCP_UART_DBG("tcp_dss_handle %d, status %d\n", tcp_dss_handle, status);
		
		if (NULL != tcp_dss_handle)
		{
			TCP_UART_DBG("Registed tcp_dss_handler success\n");
			break;
		}

		/* Obtain data service handle failure, try again after 10ms */
		qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_MSEC, true);
	} while(1);

	return ret_val;
}

/*
@func
	tcp_netctrl_start
@brief
	Start the DSS netctrl library, and startup data call.
*/
int tcp_netctrl_start(void)
{
	int rc = 0;
	qapi_Status_t status = QAPI_OK;
		
	rc = tcp_netctrl_init();
	if (0 == rc)
	{
		/* Get valid DSS handler and set the data call parameter */
		tcp_set_data_param();
	}
	else
	{
		TCP_UART_DBG("quectel_dss_init fail.\n");
		return -1;
	}

	TCP_UART_DBG("qapi_DSS_Start_Data_Call start!!!.\n");
	status = qapi_DSS_Start_Data_Call(tcp_dss_handle);
	if (QAPI_OK == status)
	{
		TCP_UART_DBG("Start Data service success.\n");
		return 0;
	}
	else
	{
		return -1;
	}
}

/*
@func
	tcp_netctrl_release
@brief
	Cleans up the DSS netctrl library and close data service.
*/
int tcp_netctrl_stop(void)
{
	qapi_Status_t stat = QAPI_OK;
	
	if (tcp_dss_handle)
	{
		stat = qapi_DSS_Stop_Data_Call(tcp_dss_handle);
		if (QAPI_OK == stat)
		{
			TCP_UART_DBG("Stop data call success\n");
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
	tcp_netctrl_start();

	while (1)
	{
		/* Wait disconnect signal */
		tx_event_flags_get(tcp_signal_handle, DSS_SIG_EVT_DIS_E, TX_OR, &dss_event, TX_WAIT_FOREVER);
		if (dss_event & DSS_SIG_EVT_DIS_E)
		{
			/* Stop data call and release resource */
			tcp_netctrl_stop();
			TCP_UART_DBG("Data service task exit.\n");
			break;
		}
	}

	TCP_UART_DBG("Data Service Thread Exit!\n");
	return;
}

static int start_tcp_session(void)
{
	int  sock_fd = -1;
	int  sent_len = 0;
	int  recv_len = 0;
	char buff[SENT_BUF_SIZE];
	struct sockaddr_in client_addr;

	do
	{
		sock_fd = qapi_socket(AF_INET, DEF_SRC_TYPE, 0);
		if (sock_fd < 0)
		{
			TCP_UART_DBG("Create socket error\n");			
			break;
		}
		
		TCP_UART_DBG("<-- Create socket[%d] success -->\n", sock_fd);
		memset(buff, 0, sizeof(buff));
		memset(&client_addr, 0, sizeof(client_addr));
		client_addr.sin_family = AF_INET;
		client_addr.sin_port = _htons(DEF_SRV_PORT);
		client_addr.sin_addr.s_addr = inet_addr(DEF_SRV_ADDR);

		/* Connect to TCP server */
		if (-1 == qapi_connect(sock_fd, (struct sockaddr *)&client_addr, sizeof(client_addr)))
		{
			TCP_UART_DBG("Connect to servert error\n");
			break;
		}
		
		TCP_UART_DBG("<-- Connect to server[%s][%d] success -->\n", DEF_SRV_ADDR, DEF_SRV_PORT);

		strcpy(buff, "Hello Quectel, Test TCP client Demo!");
		
		/* Start sending data to server after connecting server success */
		sent_len = qapi_send(sock_fd, buff, SENT_BUF_SIZE, 0);
		if (sent_len > 0)
		{
			TCP_UART_DBG("Client send data success, len: %d, data: %s\n", sent_len, buff);
		}

		/* Block and wait for respons */
		while (1)
		{
			memset(buff, 0, sizeof(buff));
			
			recv_len = qapi_recv(sock_fd, buff, RECV_BUF_SIZE, 0);
			if (recv_len > 0)
			{
				if (0 == strncmp(buff, "Exit", 4))
				{
					qapi_socketclose(sock_fd);
					sock_fd = -1;
					tx_event_flags_set(tcp_signal_handle, DSS_SIG_EVT_DIS_E, TX_OR);
					TCP_UART_DBG("TCP Client Exit!!!\n");
					break;
				}

				/* Reveive data */
				TCP_UART_DBG("[TCP Client]@len[%d], @Recv: %s\n", recv_len, buff);
			}

			qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_MSEC, true);
		}
	} while (0);

	if (sock_fd >= 0)
	{
		qapi_socketclose(sock_fd);
	}
	
	return 0;
}

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

	/* wait 10sec for device startup */
	qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_SEC, true);

    /* Create a byte memory pool. */
    txm_module_object_allocate(&byte_pool_tcp, sizeof(TX_BYTE_POOL));
    tx_byte_pool_create(byte_pool_tcp, "tcp application pool", free_memory_tcp, TCP_BYTE_POOL_SIZE);

	/* Initial uart for debug */
	tcp_uart_dbg_init();
    TCP_UART_DBG("TCPClient Task Start...\n");

    /* Create event signal handle and clear signals */
    txm_module_object_allocate(&tcp_signal_handle, sizeof(TX_EVENT_FLAGS_GROUP));
	tx_event_flags_create(tcp_signal_handle, "dss_signal_event");
	tx_event_flags_set(tcp_signal_handle, 0x0, TX_AND);

	/* Start DSS thread, and detect iface status */
#ifdef QAPI_TXM_MODULE
	if (TX_SUCCESS != txm_module_object_allocate((VOID *)&dss_thread_handle, sizeof(TX_THREAD))) 
	{
		return -1;
	}
#endif
	ret = tx_thread_create(dss_thread_handle, "TCPCLINET DSS Thread", quec_dataservice_thread, NULL,
							tcp_dss_stack, THREAD_STACK_SIZE, THREAD_PRIORITY, THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);
	if (ret != TX_SUCCESS)
	{
		IOT_INFO("Thread creation failed\n");
	}

	sig_mask = DSS_SIG_EVT_INV_E | DSS_SIG_EVT_NO_CONN_E | DSS_SIG_EVT_CONN_E | DSS_SIG_EVT_EXIT_E;
	while (1)
	{
		/* TCPClient signal process */
		tx_event_flags_get(tcp_signal_handle, sig_mask, TX_OR, &dss_event, TX_WAIT_FOREVER);
		TCP_UART_DBG("SIGNAL EVENT IS [%d]\n", dss_event);
		
		if (dss_event & DSS_SIG_EVT_INV_E)
		{
			TCP_UART_DBG("DSS_SIG_EVT_INV_E Signal\n");
			tx_event_flags_set(tcp_signal_handle, ~DSS_SIG_EVT_INV_E, TX_AND);
		}
		else if (dss_event & DSS_SIG_EVT_NO_CONN_E)
		{
			TCP_UART_DBG("DSS_SIG_EVT_NO_CONN_E Signal\n");
			tx_event_flags_set(tcp_signal_handle, ~DSS_SIG_EVT_NO_CONN_E, TX_AND);
		}
		else if (dss_event & DSS_SIG_EVT_CONN_E)
		{
			TCP_UART_DBG("DSS_SIG_EVT_CONN_E Signal\n");

			/* Create a tcp client and comminucate with server */
			start_tcp_session();
            tx_event_flags_set(tcp_signal_handle, ~DSS_SIG_EVT_CONN_E, TX_AND);
		}
		else if (dss_event & DSS_SIG_EVT_EXIT_E)
		{
			TCP_UART_DBG("DSS_SIG_EVT_EXIT_E Signal\n");
			tx_event_flags_set(tcp_signal_handle, ~DSS_SIG_EVT_EXIT_E, TX_AND);
			tx_event_flags_delete(&tcp_signal_handle);
			break;
		}
		else
		{
			TCP_UART_DBG("Unkonw Signal\n");
		}

		/* Clear all signals and wait next notification */
		tx_event_flags_set(tcp_signal_handle, 0x0, TX_AND);	//@Fixme:maybe not need
	}
	
	TCP_UART_DBG("Quectel TCP Client Demo is Over!");
	
	return 0;
}
#endif /*__EXAMPLE_TCPCLINET__*/
/* End of Example_network.c */
