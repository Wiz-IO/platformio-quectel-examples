/******************************************************************************
*@file    example_dnsclient.c
*@brief   Detection of network state and parser domain name via DNS server.
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/

#if defined(__EXAMPLE_DNSCLIENT__)
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

#include "qapi_dnsc.h"
#include "qapi_uart.h"
#include "qapi_timer.h"
#include "qapi_diag.h"
#include "quectel_uart_apis.h"
#include "quectel_utils.h"
#include "example_dnsclient.h"

/*===========================================================================
                             DEFINITION
===========================================================================*/
#define DSS_ADDR_INFO_SIZE	5
#define DSS_ADDR_SIZE       16

#define GET_ADDR_INFO_MIN(a, b) ((a) > (b) ? (b) : (a))

#define QUEC_DNS_UART_DBG		//enable debug feature

#ifdef QUEC_DNS_UART_DBG
#define DNS_UART_DBG(...)	\
{\
	dns_uart_debug_print(__VA_ARGS__);	\
}
#else
#define DNS_UART_DBG(...)
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

static unsigned char dns_dss_stack[THREAD_STACK_SIZE];

TX_EVENT_FLAGS_GROUP *dss_signal_handle;

qapi_DSS_Hndl_t dss_handle = NULL;	            /* Related to DSS netctrl */

/* @Note: If netctrl library fail to initialize, set this value will be 1,
 * We should not release library when it is 1. 
 */
signed char tcp_netctl_lib_status = DSS_LIB_STAT_INVALID_E;
unsigned char tcp_datacall_status = DSS_EVT_INVALID_E;

TX_BYTE_POOL *byte_pool_dns;
#define DNS_BYTE_POOL_SIZE		10*8*1024
UCHAR free_memory_dns[DNS_BYTE_POOL_SIZE];

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
void dss_uart_dbg_init(void)
{
  	if (TX_SUCCESS != tx_byte_allocate(byte_pool_dns, (VOID *)&uart_rx_buff, 4*1024, TX_NO_WAIT))
  	{
  		IOT_DEBUG("tx_byte_allocate [uart_rx_buff] failed!");
    	return;
  	}

  	if (TX_SUCCESS != tx_byte_allocate(byte_pool_dns, (VOID *)&uart_tx_buff, 4*1024, TX_NO_WAIT))
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

void dns_uart_debug_print(const char* fmt, ...) 
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
static void dss_net_event_cb
( 
	qapi_DSS_Hndl_t 		hndl,
	void 				   *user_data,
	qapi_DSS_Net_Evt_t 		evt,
	qapi_DSS_Evt_Payload_t *payload_ptr 
)
{
	qapi_Status_t status = QAPI_OK;
	
	DNS_UART_DBG("Data test event callback, event: %d\n", evt);

	switch (evt)
	{
		case QAPI_DSS_EVT_NET_IS_CONN_E:
		{
			DNS_UART_DBG("Data Call Connected.\n");
			dss_show_sysinfo();
			/* Signal main task */
  			tx_event_flags_set(dss_signal_handle, DSS_SIG_EVT_CONN_E, TX_OR);
			tcp_datacall_status = DSS_EVT_NET_IS_CONN_E;
			
			break;
		}
		case QAPI_DSS_EVT_NET_NO_NET_E:
		{
			DNS_UART_DBG("Data Call Disconnected.\n");
			
			if (DSS_EVT_NET_IS_CONN_E == tcp_datacall_status)
			{
				/* Release Data service handle and netctrl library */
				if (dss_handle)
				{
					status = qapi_DSS_Rel_Data_Srvc_Hndl(dss_handle);
					if (QAPI_OK == status)
					{
						DNS_UART_DBG("Release data service handle success\n");
						tx_event_flags_set(dss_signal_handle, DSS_SIG_EVT_EXIT_E, TX_OR);
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
				tx_event_flags_set(dss_signal_handle, DSS_SIG_EVT_NO_CONN_E, TX_OR);
			}

			break;
		}
		default:
		{
			DNS_UART_DBG("Data Call status is invalid.\n");
			
			/* Signal main task */
  			tx_event_flags_set(dss_signal_handle, DSS_SIG_EVT_INV_E, TX_OR);
			tcp_datacall_status = DSS_EVT_INVALID_E;
			break;
		}
	}
}

void dss_show_sysinfo(void)
{
	int i   = 0;
	int j 	= 0;
	unsigned int len = 0;
	uint8 buff[DSS_ADDR_SIZE] = {0}; 
	qapi_Status_t status;
	qapi_DSS_Addr_Info_t info_ptr[DSS_ADDR_INFO_SIZE];

	status = qapi_DSS_Get_IP_Addr_Count(dss_handle, &len);
	if (QAPI_ERROR == status)
	{
		DNS_UART_DBG("Get IP address count error\n");
		return;
	}
		
	status = qapi_DSS_Get_IP_Addr(dss_handle, info_ptr, len);
	if (QAPI_ERROR == status)
	{
		DNS_UART_DBG("Get IP address error\n");
		return;
	}
	
	j = GET_ADDR_INFO_MIN(len, DSS_ADDR_INFO_SIZE);
	
	for (i = 0; i < j; i++)
	{
		DNS_UART_DBG("<--- static IP address information --->\n");
		dss_inet_ntoa(info_ptr[i].iface_addr_s, buff, DSS_ADDR_SIZE);
		DNS_UART_DBG("static IP: %s\n", buff);

		memset(buff, 0, sizeof(buff));
		dss_inet_ntoa(info_ptr[i].gtwy_addr_s, buff, DSS_ADDR_SIZE);
		DNS_UART_DBG("Gateway IP: %s\n", buff);

		memset(buff, 0, sizeof(buff));
		dss_inet_ntoa(info_ptr[i].dnsp_addr_s, buff, DSS_ADDR_SIZE);
		DNS_UART_DBG("Primary DNS IP: %s\n", buff);

		memset(buff, 0, sizeof(buff));
		dss_inet_ntoa(info_ptr[i].dnss_addr_s, buff, DSS_ADDR_SIZE);
		DNS_UART_DBG("Second DNS IP: %s\n", buff);
	}

	DNS_UART_DBG("<--- End of system info --->\n");
}

void dns_parser(const char *hostname, const char *iface)
{
	int32 e = 0;
	char ip_str[48];
	struct ip46addr ipaddr;

	ipaddr.type = AF_INET;	//if you use IPv6, please set to AF_INET6

	DNS_UART_DBG("hostname: %s is resoling in interface: %s", hostname, iface);

	e = qapi_Net_DNSc_Reshost_on_iface(hostname, &ipaddr, iface);
	if (e)
	{
		DNS_UART_DBG("Unable to resolve %s\n", hostname);
	}
	else
	{
		DNS_UART_DBG("\n%s --> %s\n", hostname, inet_ntop(ipaddr.type, &ipaddr.a, ip_str, sizeof(ip_str)));
	}
}

/*
@func
	set_data_param
@brief
	Set the Parameter for Data Call, such as APN and network tech.
*/
static int set_data_param(void)
{
    qapi_DSS_Call_Param_Value_t param_info;
	
	/* Initial Data Call Parameter */

    if (NULL != dss_handle)
    {
        /* set data call param */
        param_info.buf_val = NULL;
        param_info.num_val = QAPI_DSS_RADIO_TECH_UNKNOWN;	//Automatic mode(or DSS_RADIO_TECH_LTE)
        DNS_UART_DBG("Setting tech to Automatic\n");
        qapi_DSS_Set_Data_Call_Param(dss_handle, QAPI_DSS_CALL_INFO_TECH_PREF_E, &param_info);

		/* set data call param for UMTS, this profile related to AT+CGDONT */
		param_info.buf_val = NULL;
		param_info.num_val = 1;
		DNS_UART_DBG("setting profile number to: %d", 1);
		qapi_DSS_Set_Data_Call_Param(dss_handle, QAPI_DSS_CALL_INFO_UMTS_PROFILE_IDX_E, &param_info);

		/* set IP version(IPv4 or IPv6) */
        param_info.buf_val = NULL;
        param_info.num_val = QAPI_DSS_IP_VERSION_4;
        DNS_UART_DBG("Setting family to IPv%d\n", param_info.num_val);
        qapi_DSS_Set_Data_Call_Param(dss_handle, QAPI_DSS_CALL_INFO_IP_VERSION_E, &param_info);
    }
    else
    {
        DNS_UART_DBG("Dss handler is NULL!!!\n");
		return -1;
    }
	
    return 0;
}

/*
@func
	dss_inet_ntoa
@brief
	utility interface to translate ip from "int" to x.x.x.x format.
*/
int32 dss_inet_ntoa
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
	dss_netctrl_init
@brief
	Initializes the DSS netctrl library for the specified operating mode.
*/
static int dss_netctrl_init(void)
{
	int ret_val = 0;
	qapi_Status_t status = QAPI_OK;

	DNS_UART_DBG("Initializes the DSS netctrl library\n");

	/* Initializes the DSS netctrl library */
	if (QAPI_OK == qapi_DSS_Init(QAPI_DSS_MODE_GENERAL))
	{
		tcp_netctl_lib_status = DSS_LIB_STAT_SUCCESS_E;
		DNS_UART_DBG("qapi_DSS_Init success\n");
	}
	else
	{
		/* @Note: netctrl library has been initialized */
		tcp_netctl_lib_status = DSS_LIB_STAT_FAIL_E;
		DNS_UART_DBG("DSS netctrl library has been initialized.\n");
	}
	
	/* Registering callback tcp_dss_handleR */
	do
	{
		DNS_UART_DBG("Registering Callback dss_handle\n");
		
		/* Obtain data service handle */
		status = qapi_DSS_Get_Data_Srvc_Hndl(dss_net_event_cb, NULL, &dss_handle);
		DNS_UART_DBG("dss_handle %d, status %d\n", dss_handle, status);
		
		if (NULL != dss_handle)
		{
			DNS_UART_DBG("Registed tcp_dss_handler success\n");
			break;
		}

		/* Obtain data service handle failure, try again after 10ms */
		qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_MSEC, true);
	} while(1);

	return ret_val;
}

/*
@func
	dss_netctrl_start
@brief
	Start the DSS netctrl library, and startup data call.
*/
int dss_netctrl_start(void)
{
	int rc = 0;
	qapi_Status_t status = QAPI_OK;
		
	rc = dss_netctrl_init();
	if (0 == rc)
	{
		/* Get valid DSS handler and set the data call parameter */
		set_data_param();
	}
	else
	{
		DNS_UART_DBG("quectel_dss_init fail.\n");
		return -1;
	}

	DNS_UART_DBG("qapi_DSS_Start_Data_Call start!!!.\n");
	status = qapi_DSS_Start_Data_Call(dss_handle);
	if (QAPI_OK == status)
	{
		DNS_UART_DBG("Start Data service success.\n");
		return 0;
	}
	else
	{
		return -1;
	}
}

/*
@func
	dss_netctrl_release
@brief
	Cleans up the DSS netctrl library and close data service.
*/
int dss_netctrl_stop(void)
{
	qapi_Status_t stat = QAPI_OK;
	
	if (dss_handle)
	{
		stat = qapi_DSS_Stop_Data_Call(dss_handle);
		if (QAPI_OK == stat)
		{
			DNS_UART_DBG("Stop data call success\n");
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
	dss_netctrl_start();

	while (1)
	{
		/* Wait disconnect signal */
		tx_event_flags_get(dss_signal_handle, DSS_SIG_EVT_DIS_E, TX_OR, &dss_event, TX_WAIT_FOREVER);
		if (dss_event & DSS_SIG_EVT_DIS_E)
		{
			/* Stop data call and release resource */
			dss_netctrl_stop();
			DNS_UART_DBG("Data service task exit.\n");
			break;
		}
	}

	DNS_UART_DBG("Data Service Thread Exit!\n");
	return;
}

static void dns_client_session()
{
	int e = 0;
	int i, j = 0;

	unsigned int len = 0;
	qapi_Status_t status;
	char iface[15] = {0};
	char first_dns[DSS_ADDR_SIZE] = {0};
	char second_dns[DSS_ADDR_SIZE] = {0};
	qapi_DSS_Addr_Info_t info_ptr[DSS_ADDR_INFO_SIZE];

	/* Get DNS server address */
	status = qapi_DSS_Get_IP_Addr_Count(dss_handle, &len);
	if (QAPI_ERROR == status)
	{
		DNS_UART_DBG("Get IP address count error\n");
		return;
	}
		
	status = qapi_DSS_Get_IP_Addr(dss_handle, info_ptr, len);
	if (QAPI_ERROR == status)
	{
		DNS_UART_DBG("Get IP address error\n");
		return;
	}
	
	j = GET_ADDR_INFO_MIN(len, DSS_ADDR_INFO_SIZE);
	
	for (i = 0; i < j; i++)
	{
		memset(first_dns, 0, sizeof(first_dns));
		dss_inet_ntoa(info_ptr[i].dnsp_addr_s, (uint8 *)first_dns, DSS_ADDR_SIZE);
		DNS_UART_DBG("Primary DNS IP: %s\n", first_dns);

		memset(second_dns, 0, sizeof(second_dns));
		dss_inet_ntoa(info_ptr[i].dnss_addr_s, (uint8 *)second_dns, DSS_ADDR_SIZE);
		DNS_UART_DBG("Second DNS IP: %s\n", second_dns);
	}

	/* Start DNS service */
	e = qapi_Net_DNSc_Command(QAPI_NET_DNS_START_E);
	DNS_UART_DBG("Start DNS service, ret: %d", e);

	/* Get current active iface */
	memset(iface, 0, sizeof(iface));
	qapi_DSS_Get_Device_Name(dss_handle, iface, 15);
	DNS_UART_DBG("device_name: %s\n", iface);
#if 0
	memset(iface, 0, sizeof(iface));
	qapi_DSS_Get_Qmi_Port_Name(dss_handle, iface, 15);
	DNS_UART_DBG("qmi_port_name: %s\n", iface);
#endif

	/* Add dns server into corresponding interface */
	qapi_Net_DNSc_Add_Server_on_iface(first_dns, QAPI_NET_DNS_V4_PRIMARY_SERVER_ID, iface);
	qapi_Net_DNSc_Add_Server_on_iface(second_dns, QAPI_NET_DNS_V4_SECONDARY_SERVER_ID, iface);

	/* URL parser */
	dns_parser("www.baidu.com", iface);
}

/*
@func
	quectel_task_entry
@brief
	The entry of data service task.
*/
int quectel_task_entry(void)
{

	int ret = 0;
	ULONG dss_event = 0;
	int32 sig_mask;

	/* wait 10sec for device startup */
	qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_SEC, true);

    /* Create a byte memory pool. */
    txm_module_object_allocate(&byte_pool_dns, sizeof(TX_BYTE_POOL));
    tx_byte_pool_create(byte_pool_dns, "dns application pool", free_memory_dns, DNS_BYTE_POOL_SIZE);

	/* Initial uart for debug */
	dss_uart_dbg_init();
    DNS_UART_DBG("DNSClient Task Start...\n");

	/* Create event signal handle and clear signals */
    txm_module_object_allocate(&dss_signal_handle, sizeof(TX_EVENT_FLAGS_GROUP));
	tx_event_flags_create(dss_signal_handle, "dss_signal_event");
	tx_event_flags_set(dss_signal_handle, 0x0, TX_AND);

	/* Start DSS thread, and detect iface status */
#ifdef QAPI_TXM_MODULE
	if (TX_SUCCESS != txm_module_object_allocate((VOID *)&dss_thread_handle, sizeof(TX_THREAD))) 
	{
		return -1;
	}
#endif
	ret = tx_thread_create(dss_thread_handle, "TCPCLINET DSS Thread", quec_dataservice_thread, NULL,
							dns_dss_stack, THREAD_STACK_SIZE, THREAD_PRIORITY, THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);
	if (ret != TX_SUCCESS)
	{
		IOT_INFO("Thread creation failed\n");
	}

	sig_mask = DSS_SIG_EVT_INV_E | DSS_SIG_EVT_NO_CONN_E | DSS_SIG_EVT_CONN_E | DSS_SIG_EVT_EXIT_E;
	
	while (1)
	{
		/* TCPClient signal process */
		tx_event_flags_get(dss_signal_handle, sig_mask, TX_OR, &dss_event, TX_WAIT_FOREVER);
		DNS_UART_DBG("SIGNAL EVENT IS [%d]\n", dss_event);
		
		if (dss_event & DSS_SIG_EVT_INV_E)
		{
			DNS_UART_DBG("DSS_SIG_EVT_INV_E Signal\n");
			tx_event_flags_set(dss_signal_handle, ~DSS_SIG_EVT_INV_E, TX_AND);
		}
		else if (dss_event & DSS_SIG_EVT_NO_CONN_E)
		{
			DNS_UART_DBG("DSS_SIG_EVT_NO_CONN_E Signal\n");
			tx_event_flags_set(dss_signal_handle, ~DSS_SIG_EVT_NO_CONN_E, TX_AND);
		}
		else if (dss_event & DSS_SIG_EVT_CONN_E)
		{
			DNS_UART_DBG("DSS_SIG_EVT_CONN_E Signal\n");
			dns_client_session();
			tx_event_flags_set(dss_signal_handle, ~DSS_SIG_EVT_CONN_E, TX_AND);
		}
		else if (dss_event & DSS_SIG_EVT_EXIT_E)
		{
			DNS_UART_DBG("DSS_SIG_EVT_EXIT_E Signal\n");
			tx_event_flags_set(dss_signal_handle, ~DSS_SIG_EVT_EXIT_E, TX_AND);
			tx_event_flags_delete(dss_signal_handle);
			break;
		}
		else
		{
			DNS_UART_DBG("Unkonw Signal\n");
		}

		/* Clear all signals and wait next notification */
		tx_event_flags_set(dss_signal_handle, 0x0, TX_AND);	//@Fixme:maybe not need
	}
	
	DNS_UART_DBG("Quectel TCP Client Demo is Over!");
	
	return 0;
}
#endif /*__EXAMPLE_DNSCLINET__*/
