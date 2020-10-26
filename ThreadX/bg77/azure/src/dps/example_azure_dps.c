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

#if defined(__EXAMPLE_AZURE_IOT__)
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

#include "qapi_dnsc.h"
#include "qapi_uart.h"
#include "qapi_timer.h"
#include "qapi_diag.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "qapi_quectel.h"
#include "example_azure_iot.h"

#include "qapi_fs.h"

#include "qapi_atfwd.h"



/**********************AT_UART GLOBAL**********************/
TX_BYTE_POOL *byte_pool_uart;
TX_BYTE_POOL *byte_pool_at;

#define UART_BYTE_POOL_SIZE		10*8*1024
UCHAR free_memory_uart[UART_BYTE_POOL_SIZE];
UCHAR free_memory_at[UART_BYTE_POOL_SIZE];


/* uart rx tx buffer */
static char *rx_buff = NULL;
static char *tx_buff = NULL;

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

static int dpsinfo_val = 0;


TX_BYTE_POOL * byte_pool_qcli;
/**********************************************************/

#define SYMM_AUTH_FILE "/datatx/symm_auth.json"
/**********************AT_UART FUNCTION**********************/
static int qt_atoi(char *str)
{
    int res = 0, i;

    for (i = 0; str[i] != '\0' && str[i] != '.'; ++i)
    {
        res = res*10 + (str[i] - '0');
    }

    return res;
}
static int strncasecmp(const char * s1, const char * s2, size_t n)
{
  unsigned char c1, c2;
  int diff;

  if (n > 0)
  {
    do
    {
      c1 = (unsigned char)(*s1++);
      c2 = (unsigned char)(*s2++);
      if (('A' <= c1) && ('Z' >= c1))
      {
        c1 = c1 - 'A' + 'a';
      }
      if (('A' <= c2) && ('Z' >= c2))
      {
        c2 = c2 - 'A' + 'a';
      }
      diff = c1 - c2;

      if (0 != diff)
      {
        return diff;
      }

      if ('\0' == c1)
      {
        break;
      }
    } while (--n);
  }
  return 0;
}


int sas_flag = 0 ,reg_flag = 0, scp_flag = 0;
char symm_auth_buf_w[1024] = {0};
char sas_buf[512] = {0};
char reg_buf[100] = {0};
char scp_buf[100] = {0};
int    fd = -1;
qapi_FS_Status_t status = QAPI_OK;
int   wr_bytes = 0;

void atfwd_cmd_handler_cb(boolean is_reg, char *atcmd_name,
                                 uint8* at_fwd_params, uint8 mask,
                                 uint32 at_handle)
{
    char buff[32] = {0};
    int  tmp_val = 0;
    qapi_Status_t ret = QAPI_ERROR;
    
    qt_uart_dbg(uart_conf.hdlr,"atfwd_cmd_handler_cb is called, atcmd_name:[%s] mask:[%d]\n", atcmd_name, mask);
	qt_uart_dbg(uart_conf.hdlr,"atfwd_cmd_handler_cb is called,  is_reg:[%d]\n", is_reg);

	if(is_reg)  //Registration Successful,is_reg return 1 
	{
		if(mask==QUEC_AT_MASK_EMPTY_V01)
		{
			qt_uart_dbg(uart_conf.hdlr,"Atcmd %s is registered\n",atcmd_name);
			return;

		}
    	if( !strncasecmp(atcmd_name, "+DPSINFO",strlen(atcmd_name)) )
	    {
	        //Execute Mode
	        if ((QUEC_AT_MASK_NA_V01) == mask)//AT+DPSINFO
	        {
	            ret = qapi_atfwd_send_resp(atcmd_name, "", QUEC_AT_RESULT_OK_V01);
	        }
	        //Read Mode
	        else if ((QUEC_AT_MASK_NA_V01 | QUEC_AT_MASK_QU_V01) == mask)//AT+DPSINFO?
	        {
	            snprintf(buff, sizeof(buff), "+DPSINFO: %d", dpsinfo_val);
	            ret = qapi_atfwd_send_resp(atcmd_name, buff, QUEC_AT_RESULT_OK_V01);
	        }
	        //Test Mode
	        else if ((QUEC_AT_MASK_NA_V01 | QUEC_AT_MASK_EQ_V01 | QUEC_AT_MASK_QU_V01) == mask)//AT+DPSINFO=?
	        {
				snprintf(buff, sizeof(buff), "+DPSINFO: (0-2)");
	            ret = qapi_atfwd_send_resp(atcmd_name, buff, QUEC_AT_RESULT_OK_V01);
	        }
	        //Write Mode
	        else if ((QUEC_AT_MASK_NA_V01 | QUEC_AT_MASK_EQ_V01 | QUEC_AT_MASK_AR_V01) == mask)//AT+DPSINFO=<value>
	        {
				if(strncmp((char*)at_fwd_params,"SASKEY=",7) == 0)
				{
					strncpy(sas_buf,(char*)at_fwd_params+7,strlen((char*)at_fwd_params) - 7);
					sas_flag = 1;					
					if(sas_flag == 1 && reg_flag == 1 && scp_flag == 1)
					{
						sprintf(symm_auth_buf_w,"{\"SasKey\":\"%s\",\"DpsIdScope\":\"%s\",\"RegisterId\":\"%s\"}",sas_buf,scp_buf,reg_buf);
							
						status = qapi_FS_Open(SYMM_AUTH_FILE, QAPI_FS_O_RDWR_E | QAPI_FS_O_CREAT_E | QAPI_FS_O_TRUNC_E, &fd);
	
						if((status == QAPI_OK) && (-1 != fd))
						{
							status = qapi_FS_Write(fd, (uint8*)&symm_auth_buf_w, strlen(symm_auth_buf_w), &wr_bytes);
							qt_uart_dbg(uart_conf.hdlr,"qapi_FS_Write %d %d", status, wr_bytes);		

							status = qapi_FS_Close(fd);
							qt_uart_dbg(uart_conf.hdlr,"qapi_FS_Close %d", status);	
						}

						ret = qapi_atfwd_send_resp(atcmd_name, symm_auth_buf_w, QUEC_AT_RESULT_OK_V01);
					}else
					{
						/* code */
						ret = qapi_atfwd_send_resp(atcmd_name, sas_buf, QUEC_AT_RESULT_OK_V01);
					}					
				}else if(strncmp((char*)at_fwd_params,"REGISTERID=",11) == 0)
				{
					strncpy(reg_buf,(char*)at_fwd_params+11,strlen((char*)at_fwd_params) - 11);
					reg_flag = 1;
					if(sas_flag == 1 && reg_flag == 1 && scp_flag == 1)
					{
						sprintf(symm_auth_buf_w,"{\"SasKey\":\"%s\",\"DpsIdScope\":\"%s\",\"RegisterId\":\"%s\"}",sas_buf,scp_buf,reg_buf);

						status = qapi_FS_Open(SYMM_AUTH_FILE, QAPI_FS_O_RDWR_E | QAPI_FS_O_CREAT_E | QAPI_FS_O_TRUNC_E, &fd);
	
						if((status == QAPI_OK) && (-1 != fd))
						{
							status = qapi_FS_Write(fd, (uint8*)&symm_auth_buf_w, strlen(symm_auth_buf_w), &wr_bytes);
							qt_uart_dbg(uart_conf.hdlr,"qapi_FS_Write %d %d", status, wr_bytes);		

							status = qapi_FS_Close(fd);
							qt_uart_dbg(uart_conf.hdlr,"qapi_FS_Close %d", status);	
						}

						ret = qapi_atfwd_send_resp(atcmd_name, symm_auth_buf_w, QUEC_AT_RESULT_OK_V01);
					}else
					{
						/* code */
						ret = qapi_atfwd_send_resp(atcmd_name, reg_buf, QUEC_AT_RESULT_OK_V01);
					}	
				}else if(strncmp((char*)at_fwd_params,"SCOPEID=",8) == 0)
				{
					strncpy(scp_buf,(char*)at_fwd_params+8,strlen((char*)at_fwd_params) - 8);
					scp_flag = 1;
					if(sas_flag == 1 && reg_flag == 1 && scp_flag == 1)
					{
						sprintf(symm_auth_buf_w,"{\"SasKey\":\"%s\",\"DpsIdScope\":\"%s\",\"RegisterId\":\"%s\"}",sas_buf,scp_buf,reg_buf);

						status = qapi_FS_Open(SYMM_AUTH_FILE, QAPI_FS_O_RDWR_E | QAPI_FS_O_CREAT_E | QAPI_FS_O_TRUNC_E, &fd);
	
						if((status == QAPI_OK) && (-1 != fd))
						{
							status = qapi_FS_Write(fd, (uint8*)&symm_auth_buf_w, strlen(symm_auth_buf_w), &wr_bytes);
							qt_uart_dbg(uart_conf.hdlr,"qapi_FS_Write %d %d", status, wr_bytes);		

							status = qapi_FS_Close(fd);
							qt_uart_dbg(uart_conf.hdlr,"qapi_FS_Close %d", status);	
						}

						ret = qapi_atfwd_send_resp(atcmd_name, symm_auth_buf_w, QUEC_AT_RESULT_OK_V01);
					}else
					{
						/* code */
						ret = qapi_atfwd_send_resp(atcmd_name, scp_buf, QUEC_AT_RESULT_OK_V01);
					}	
				}
	        }
	    }
	    else
	    {
	        ret = qapi_atfwd_send_resp(atcmd_name, "", QUEC_AT_RESULT_ERROR_V01);
	    }

    	qt_uart_dbg(uart_conf.hdlr,"[%s] send resp, ret = %d\n", atcmd_name, ret);
	}
}

/************************************************************/


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

//#define THREAD_STACK_SIZE    (1024 * 16)
#define THREAD_STACK_SIZE    (1024 * 24)
#define THREAD_PRIORITY      (180)
//#define BYTE_POOL_SIZE       (1024 * 16)
#define BYTE_POOL_SIZE       (1024 * 16)

#define CMD_BUF_SIZE  100

#define DEFAULT_PUB_TIME 5

#define MAX_LOG_MSG_SIZE 128
//#define MAX_LOG_MSG_SIZE 1024
#define GETGPSINTERVAL 10

#define NW_SIGNAL_ENABLE //for checking network status by device_info
#define TCP_IO_SELECT//I/O multiplexing for receiving data
#define DATA_TRANSFER_TIMER //timer for sending data every 5seconds
/*===========================================================================
						   Global variable
===========================================================================*/
/* TCPClient dss thread handle */
#ifdef QAPI_TXM_MODULE
static TX_THREAD* dss_thread_handle;
#else
static TX_THREAD _dss_thread_handle;
static TX_THREAD* ts_thread_handle = &_dss_thread_handle;
#endif

void* tcp_nw_signal_handle = NULL;	/* Related to nework indication */

qapi_Device_Info_Hndl_t device_info_hndl;

static unsigned char tcp_dss_stack[THREAD_STACK_SIZE];

TX_EVENT_FLAGS_GROUP* tcp_signal_handle;

qapi_DSS_Hndl_t tcp_dss_handle = NULL;	            /* Related to DSS netctrl */

static char apn[QUEC_APN_LEN];					/* APN */
static char apn_username[QUEC_APN_USER_LEN];	/* APN username */
static char apn_passwd[QUEC_APN_PASS_LEN];		/* APN password */

/* @Note: If netctrl library fail to initialize, set this value will be 1,
 * We should not release library when it is 1.
 */
signed char tcp_netctl_lib_status = DSS_LIB_STAT_INVALID_E;
unsigned char tcp_datacall_status = DSS_EVT_INVALID_E;

TX_BYTE_POOL* byte_pool_tcp;
#define TCP_BYTE_POOL_SIZE		10*8*1024
UCHAR free_memory_tcp[TCP_BYTE_POOL_SIZE];

/* uart rx tx buffer */
static char* uart_rx_buff = NULL;	/*!!! should keep this buffer as 4K Bytes */
static char* uart_tx_buff = NULL;

#ifdef DATA_TRANSFER_TIMER
static qapi_TIMER_handle_t data_notify_timer;
static qapi_TIMER_define_attr_t time_def_attr;
static qapi_TIMER_set_attr_t time_attr;
qbool_t obs_timer_state = false;
static int g_sock_fd = -1; // for tcp data sending timer
#endif

/*==========================================================================
  Definations for GPS service
===========================================================================*/
static bool pnp_GPS_get_flag = false;

typedef unsigned int	size_t;
typedef unsigned int 	uint32_t;

static uint32 gps_tracking_id;

#define LOCATION_TRACKING_FIXED_BIT (0x1 << 0)
#define LOCATION_TRACKING_RSP_OK_BIT (0x1 << 1)
#define LOCATION_TRACKING_RSP_FAIL_BIT (0x1 << 2)
#define LOCATION_NMEA_RSP_OK_BIT (0x1 << 3)

qapi_Location_t last_location;

#define GPS_USER_BUFFER_SIZE 4096
static uint8 GPSuserBuffer[GPS_USER_BUFFER_SIZE];
qapi_loc_client_id loc_clnt;

#define GPS_LOCATION_BUFFER_SIZE 128
uint8 last_location_buf[GPS_LOCATION_BUFFER_SIZE] = { 0 };

char m_lat[56] = { 0 };
char m_lon[56] = { 0 };


/*==========================================================================
LOCATION API REGISTERED CALLBACKS
===========================================================================*/

static void loc_gnss_nmea_callback(qapi_Gnss_Nmea_t gnssNmea)
{


}

static void location_capabilities_callback(qapi_Location_Capabilities_Mask_t capabilities)
{
	TCP_UART_DBG("Location mod has tracking capability:%d\r\n", capabilities);
}

static void location_response_callback(qapi_Location_Error_t err, uint32_t id)
{
	TCP_UART_DBG("LOCATION RESPONSE CALLBACK err=%u id=%u\r\n", err, id);
}
static void location_collective_response_cb(size_t count, qapi_Location_Error_t* err, uint32_t* ids)
{
	TCP_UART_DBG("LOCATION collective RESPONSE CALLBACK count = %d err=%u \r\n", count, err);
	for (int i = 0; i < count; i++)
	{
		TCP_UART_DBG("LOCATION collective RESPONSE CALLBACK id = %u \r\n", ids[i]);
	}
}

static void location_tracking_callback(qapi_Location_t location)
{
	//TCP_UART_DBG("location_tracking_callback start"); 
	memcpy(&last_location, &location, sizeof(qapi_Location_t));
	snprintf((char*)(last_location_buf), sizeof(last_location_buf),
		"LAT: %d.%d LON: %d.%d ACC: %d.%d",
		(int)location.latitude, (abs((int)(location.latitude * 100000))) % 100000,
		(int)location.longitude, (abs((int)(location.longitude * 100000))) % 100000,
		(int)location.accuracy, (abs((int)(location.accuracy * 100))) % 100);

	TCP_UART_DBG("location_tracking_callbacklast_location_buf = %s \r\n", last_location_buf);

	//int azure_sendLocation(char * lat, char * lon)
	memset(m_lat, 0x00, sizeof(char) * 56);
	memset(m_lon, 0x00, sizeof(char) * 56);

	//lat
	snprintf((char*)(m_lat), sizeof(m_lat),
		"%d.%d",
		(int)location.latitude, (abs((int)(location.latitude * 100000))) % 100000);
	//lon
	snprintf((char*)(m_lon), sizeof(m_lon),
		"%d.%d",
		(int)location.longitude, (abs((int)(location.longitude * 100000))) % 100000);
	//azure_sendLocation(lat, lon);
	//TCP_UART_DBG("location_tracking_callback end \r\n"); 
	pnp_GPS_get_flag = true;
	qapi_Loc_Stop_Tracking(loc_clnt, gps_tracking_id);
}






qapi_Location_Callbacks_t location_callbacks = {
	sizeof(qapi_Location_Callbacks_t),
	location_capabilities_callback,
	location_response_callback,
	location_collective_response_cb,
	location_tracking_callback,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,//loc_gnss_nmea_callback
	NULL
};

/*==========================================================================
LOCATION INIT / DEINIT APIs
===========================================================================*/
void location_init(void)
{
	qapi_Location_Error_t ret;

	ret = qapi_Loc_Init(&loc_clnt, &location_callbacks);
	if (ret == QAPI_LOCATION_ERROR_SUCCESS)
	{
		TCP_UART_DBG("LOCATION_INIT SUCCESS\r\n");
	}
	else
	{
		TCP_UART_DBG("LOCATION_INIT FAILED ! (ret %d)\r\n", ret);
	}
	ret = (qapi_Location_Error_t)qapi_Loc_Set_User_Buffer(loc_clnt, (uint8*)GPSuserBuffer, GPS_USER_BUFFER_SIZE);
	if (ret != QAPI_LOCATION_ERROR_SUCCESS) {
		TCP_UART_DBG("Set user buffer failed ! (ret %d)", ret);
	}
}

void location_deinit(void)
{

	qapi_Loc_Stop_Batching(loc_clnt, gps_tracking_id);
	qapi_Loc_Deinit(loc_clnt);
}



/*===========================================================================
							   FUNCTION
===========================================================================*/
static void tcp_uart_rx_cb(uint32_t num_bytes, void* cb_data)
{
	if (num_bytes == 0)
	{
		qapi_UART_Receive(uart_conf.hdlr, uart_conf.rx_buff, uart_conf.rx_len, NULL);
		return;
	}
	else if (num_bytes >= uart_conf.rx_len)
	{
		num_bytes = uart_conf.rx_len;
	}

	IOT_DEBUG("QT# uart_rx_cb data [%d][%s]", num_bytes, uart_conf.rx_buff);
	memset(uart_conf.rx_buff, 0, uart_conf.rx_len);

	qapi_UART_Receive(uart_conf.hdlr, uart_conf.rx_buff, uart_conf.rx_len, NULL);
}

static void tcp_uart_tx_cb(uint32_t num_bytes, void* cb_data)
{
	IOT_DEBUG("QT# uart_tx_cb, send [%d]", num_bytes);
}

void tcp_uart_dbg_init()
{
	qapi_Status_t status;
	qapi_UART_Open_Config_t uart_cfg;

	if (TX_SUCCESS != tx_byte_allocate(byte_pool_tcp, (VOID*)&uart_rx_buff, 4 * 1024, TX_NO_WAIT))
	{
		IOT_DEBUG("tx_byte_allocate [uart_rx_buff] failed!");
		return;
	}

	if (TX_SUCCESS != tx_byte_allocate(byte_pool_tcp, (VOID*)&uart_tx_buff, 4 * 1024, TX_NO_WAIT))
	{
		IOT_DEBUG("tx_byte_allocate [uart_tx_buff] failed!");
		return;
	}

	uart_conf.rx_buff = uart_rx_buff;
	uart_conf.tx_buff = uart_tx_buff;
	uart_conf.tx_len = 4096;
	uart_conf.rx_len = 4096;

	/* debug uart init 			*/
	uart_cfg.baud_Rate = 115200;
	uart_cfg.enable_Flow_Ctrl = QAPI_FCTL_OFF_E;
	uart_cfg.bits_Per_Char = QAPI_UART_8_BITS_PER_CHAR_E;
	uart_cfg.enable_Loopback = 0;
	uart_cfg.num_Stop_Bits = QAPI_UART_1_0_STOP_BITS_E;
	uart_cfg.parity_Mode = QAPI_UART_NO_PARITY_E;
	uart_cfg.rx_CB_ISR = (qapi_UART_Callback_Fn_t)&tcp_uart_rx_cb;
	uart_cfg.tx_CB_ISR = (qapi_UART_Callback_Fn_t)&tcp_uart_tx_cb;

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
void tcp_uart_debug_print(const char* fmt, ...)
{
	va_list arg_list;
	//char dbg_buffer[128] = {0};
	char dbg_buffer[1024] = { 0 };

	va_start(arg_list, fmt);
	vsnprintf((char*)(dbg_buffer), sizeof(dbg_buffer), (char*)fmt, arg_list);
	va_end(arg_list);

	qapi_UART_Transmit(uart_conf.hdlr, dbg_buffer, strlen(dbg_buffer), NULL);
	//qapi_Timer_Sleep(50, QAPI_TIMER_UNIT_MSEC, true);   //Do not add delay unless necessary
}

#ifdef DATA_TRANSFER_TIMER
/* data notification callback */
static void tcp_data_notify_signal_callback(uint32 userData)
{
	int sent_len = 0;
	int sock_fd = -1;
	const char* data_buffer = "1234567890";
	static uint32 cnt = 0;

	//TCP_UART_DBG("Data timer user data: %d, socket: %d\n", userData, g_sock_fd);

	sock_fd = g_sock_fd;

	/* Start sending data to server after connecting server success */
	sent_len = qapi_send(sock_fd, data_buffer, strlen(data_buffer), 0);

	if (sent_len > 0)
	{
		cnt++;
		TCP_UART_DBG("Client send data success, len: %d, cnt: %d\n", sent_len, cnt);
	}
	else
	{
		//TCP_UART_DBG("Send data failed: %d\n", sent_len);
	}



	return;
}

void tcp_data_transfer_timer_def(void)
{
	//if (!obs_timer_state)
	{
		time_def_attr.deferrable = false;
		time_def_attr.cb_type = QAPI_TIMER_FUNC1_CB_TYPE;
		time_def_attr.sigs_func_ptr = tcp_data_notify_signal_callback;
		time_def_attr.sigs_mask_data = 0;

		qapi_Timer_Def(&data_notify_timer, &time_def_attr);
	}
}

void tcp_data_transfer_timer_start()
{
	//qapi_TIMER_set_attr_t time_attr;
	time_attr.unit = QAPI_TIMER_UNIT_SEC;
	time_attr.reload = 5;
	time_attr.time = 5; /* 5 minutes */

	qapi_Timer_Set(data_notify_timer, &time_attr);
	obs_timer_state = true;
}

static void tcp_data_transfer_timer_stop(void)
{
	if (obs_timer_state)
	{
		obs_timer_state = false;
		qapi_Timer_Stop(data_notify_timer);
		qapi_Timer_Undef(data_notify_timer);
	}
}
#endif


/*
@func
	dss_net_event_cb
@brief
	Initializes the DSS netctrl library for the specified operating mode.
*/
static void tcp_net_event_cb
(
	qapi_DSS_Hndl_t 		hndl,
	void* user_data,
	qapi_DSS_Net_Evt_t 		evt,
	qapi_DSS_Evt_Payload_t* payload_ptr
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
	int i = 0;
	int j = 0;
	unsigned int len = 0;
	uint8 buff[DSS_ADDR_SIZE] = { 0 };
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
	qapi_Status_t status;
	unsigned char pdp_context_number = 1; //profile1
	qapi_QT_NW_DS_Profile_PDP_Context_t profile;
    qapi_DSS_Call_Param_Value_t param_info;
	
	/* Initial Data Call Parameter */
	memset(apn, 0, sizeof(apn));
	memset(apn_username, 0, sizeof(apn_username));
	memset(apn_passwd, 0, sizeof(apn_passwd));
	//strlcpy(apn, QL_DEF_APN, QAPI_DSS_CALL_INFO_APN_MAX_LEN);
	
	status = qapi_QT_NW_PDP_Cfg_Get(&pdp_context_number, &profile);
	if(QAPI_QT_ERR_OK == status)
	{
		TCP_UART_DBG("[DATA_CALL: PDP Profile %d] pdp_type=%d, apn=%s, user_name=%s, pass_word=%s, auth=%d.", pdp_context_number, profile.pdp_type, profile.apn, profile.user_name, profile.pass_word, profile.auth_type);
		if(strlen(profile.apn) > 0)
		{
			strlcpy(apn, profile.apn, strlen(profile.apn));
			strlcpy(apn_username, profile.user_name, strlen(profile.user_name));
			strlcpy(apn_passwd, profile.pass_word, strlen(profile.pass_word));
		}
		TCP_UART_DBG("[DATA_CALL: PDP Profile %d] context copied.", pdp_context_number);
	}
	else
	{
		TCP_UART_DBG("qapi_QT_NW_PDP_Cfg_Get() ERROR, %d.", status);
	}

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
	uint8* buf,    /* Buffer to hold the converted address */
	int32                    buflen  /* Length of buffer                     */
)
{
	uint8* paddr = (uint8*)&inaddr.addr.v4;
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
	} while (1);

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
#ifdef DATA_TRANSFER_TIMER
			tcp_data_transfer_timer_stop();
#endif
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
	char sent_buff[SENT_BUF_SIZE];
	char recv_buff[RECV_BUF_SIZE];
	fd_set  fdset;
	int  ret_val = 0;
	struct sockaddr_in server_addr;

	do
	{
		sock_fd = qapi_socket(AF_INET, DEF_SRC_TYPE, 0);
		if (sock_fd < 0)
		{
			TCP_UART_DBG("Create socket error\n");
			break;
		}

		TCP_UART_DBG("<-- Create socket[%d] success -->\n", sock_fd);
		memset(sent_buff, 0, sizeof(sent_buff));
		memset(&server_addr, 0, sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = _htons(DEF_SRV_PORT);
		server_addr.sin_addr.s_addr = inet_addr(DEF_SRV_ADDR);

		/* Connect to TCP server */
		if (-1 == qapi_connect(sock_fd, (struct sockaddr*) & server_addr, sizeof(server_addr)))
		{
			TCP_UART_DBG("Connect to servert error\n");
			break;
		}
		else
		{
			g_sock_fd = sock_fd; //for sending data by timer
		}

		TCP_UART_DBG("<-- Connect to server[%s][%d] success -->\n", DEF_SRV_ADDR, DEF_SRV_PORT);

		/* Start sending data to server after connecting server success */
		strcpy(sent_buff, "Hello Quectel, Test TCP client Demo!");
		sent_len = qapi_send(sock_fd, sent_buff, strlen(sent_buff), 0);
		if (sent_len > 0)
		{
			TCP_UART_DBG("Client send data success, len: %d, data: %s\n", sent_len, sent_buff);
		}

		/* Block and wait for response */
#ifdef TCP_IO_SELECT /* I/O multiplexing */
		while (1)
		{
			qapi_fd_zero(&fdset);
			qapi_fd_set(sock_fd, &fdset);
			ret_val = qapi_select(&fdset, NULL, NULL, TX_WAIT_FOREVER);

			if (ret_val > 0)
			{
				if (qapi_fd_isset(sock_fd, &fdset) != 0)
				{
					memset(recv_buff, 0, sizeof(recv_buff));

					recv_len = qapi_recv(sock_fd, recv_buff, RECV_BUF_SIZE, 0);
					if (recv_len > 0)
					{
						if (0 == strncmp(recv_buff, "Exit", 4))
						{
							qapi_socketclose(sock_fd);
							sock_fd = -1;
#ifdef DATA_TRANSFER_TIMER
							tcp_data_transfer_timer_stop();
#endif
							tx_event_flags_set(tcp_signal_handle, DSS_SIG_EVT_DIS_E, TX_OR);
							TCP_UART_DBG("TCP Client Exit!!!\n");
							break;
						}

						/* Reveive data */
						TCP_UART_DBG("[TCP Client]@len[%d], @Recv: %s\n", recv_len, recv_buff);
					}
					else if (recv_len == 0)
					{
						TCP_UART_DBG("TCP socket connection is closed.\n");
						qapi_socketclose(sock_fd);
						sock_fd = -1;
#ifdef DATA_TRANSFER_TIMER
						tcp_data_transfer_timer_stop();
#endif
						tx_event_flags_set(tcp_signal_handle, DSS_SIG_EVT_DIS_E, TX_OR);
						break;
					}
				}
			}
		}
#else /* Timer polling for receiving data */
		while (1)
		{
			memset(recv_buff, 0, sizeof(recv_buff));

			recv_len = qapi_recv(sock_fd, recv_buff, RECV_BUF_SIZE, 0);
			if (recv_len > 0)
			{
				if (0 == strncmp(recv_buff, "Exit", 4))
				{
					qapi_socketclose(sock_fd);
					sock_fd = -1;
#ifdef DATA_TRANSFER_TIMER
					tcp_data_transfer_timer_stop();
#endif
					tx_event_flags_set(tcp_signal_handle, DSS_SIG_EVT_DIS_E, TX_OR);
					TCP_UART_DBG("TCP Client Exit!!!\n");
					break;
				}

				/* Reveive data */
				TCP_UART_DBG("[TCP Client]@len[%d], @Recv: %s\n", recv_len, recv_buff);
			}

			qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_MSEC, true);
		}
#endif
	} while (0);

	if (sock_fd >= 0)
	{
		qapi_socketclose(sock_fd);
	}

	return 0;
}

#ifdef NW_SIGNAL_ENABLE
void tcp_network_indication_cb(qapi_Device_Info_Hndl_t device_info_hndl, const qapi_Device_Info_t* dev_info)
{
	if (dev_info->id == QAPI_DEVICE_INFO_NETWORK_IND_E)
	{
		switch (dev_info->info_type)
		{
		case QAPI_DEVICE_INFO_TYPE_BOOLEAN_E:
			TCP_UART_DBG("~type[b] id[%d] status[%d]\n", dev_info->id, dev_info->u.valuebool);
			if (dev_info->u.valuebool == true)
			{
				tx_event_flags_set(tcp_nw_signal_handle, NW_SIG_EVT_ENABLE, TX_OR);
			}
			else
			{
				tx_event_flags_set(tcp_nw_signal_handle, NW_SIG_EVT_UNENABLE, TX_OR);
			}
			break;

		case QAPI_DEVICE_INFO_TYPE_INTEGER_E:
			TCP_UART_DBG("~type[i] id[%d] status[%d]\n", dev_info->id, dev_info->u.valueint);
			break;

		case QAPI_DEVICE_INFO_TYPE_BUFFER_E:
			TCP_UART_DBG("~type[s] id[%d] status[%s]\n", dev_info->id, dev_info->u.valuebuf.buf);
			break;

		default:
			TCP_UART_DBG("~[Invalid][%d]\n", dev_info->id);
			break;
		}
	}
}

void qt_tcp_init_device_info(void)
{
	qapi_Status_t status = 0;

	status = qapi_Device_Info_Init_v2(&device_info_hndl);
	if (status != QAPI_OK)
	{
		TCP_UART_DBG("~ qapi_Device_Info_Init fail [%d]\n", status);
	}
	else
	{
		TCP_UART_DBG("~ qapi_Device_Info_Init OK [%d]\n", status);
	}
}
#endif /* NW_SIGNAL_ENABLE */

int qt_read_EFS_file(const char* name, void** buffer_ptr, size_t* buffer_size)
{
	int bytes_read;
	int efs_fd = -1;
	struct qapi_FS_Stat_Type_s stat;
	uint8* file_buf = NULL;
	stat.st_size = 0;

	if ((!name) || (!buffer_ptr) || (!buffer_size))
	{
		TCP_UART_DBG("Reading SSL from EFS file failed!\n");
		return QAPI_ERROR;
	}

	if (qapi_FS_Open(name, QAPI_FS_O_RDONLY_E, &efs_fd) < 0)
	{
		TCP_UART_DBG("Opening EFS EFS file %s failed\n", name);
		return QAPI_ERROR;
	}

	if (qapi_FS_Stat_With_Handle(efs_fd, &stat) < 0)
	{
		TCP_UART_DBG("Reading EFS file %s failed\n", name);
		return QAPI_ERROR;
	}

	TCP_UART_DBG("Reading EFS file size %d \n", stat.st_size);

	tx_byte_allocate(byte_pool_tcp, (VOID**)&file_buf, stat.st_size, TX_NO_WAIT);
	if (file_buf == NULL)
	{
		TCP_UART_DBG("SSL_cert_store: QAPI_ERR_NO_MEMORY \n");
		return QAPI_ERR_NO_MEMORY;
	}

	qapi_FS_Read(efs_fd, file_buf, stat.st_size, &bytes_read);
	if ((bytes_read < 0) || (bytes_read != stat.st_size))
	{
		tx_byte_release(file_buf);
		TCP_UART_DBG("SSL_cert_store: Reading EFS file error\n");
		return QAPI_ERROR;
	}

	*buffer_ptr = file_buf;
	*buffer_size = stat.st_size;

	qapi_FS_Close(efs_fd);

	return QAPI_OK;
}

int quec_convert_ssl_certs()
{
	uint8_t* cert_Buf = NULL;
	uint32_t cert_Size = 0;
	uint8_t* key_Buf = NULL;
	uint32_t key_Size = 0;
	int result;
	qapi_Net_SSL_Cert_Info_t cert_info;
	qapi_NET_SSL_CA_Info_t calist_info[QAPI_NET_SSL_MAX_CA_LIST];

	/* Read the CA certificate information */
	result = qt_read_EFS_file(AZURE_CA_CRT, (void**)&calist_info[0].ca_Buf, (size_t*)&calist_info[0].ca_Size);
	TCP_UART_DBG("Read %s, result %d\r\n", AZURE_CA_CRT, result);

	if (QAPI_OK != result)
	{
		TCP_UART_DBG("ERROR: Reading ca information from EFS failed!! \r\n");
		result = QAPI_ERROR;
	}

	cert_info.info.ca_List.ca_Info[0] = &calist_info[0];
	cert_info.info.ca_List.ca_Cnt = 1;
	cert_info.cert_Type = QAPI_NET_SSL_CA_LIST_E;

	result = qapi_Net_SSL_Cert_Convert_And_Store(&cert_info, AZURE_CALIST_BIN);
	TCP_UART_DBG("%s qapi_Net_SSL_Cert_Convert_And_Store: %d\r\n", AZURE_CALIST_BIN, result);

	/* Read the client certificate information */
	result = qt_read_EFS_file((char*)AZURE_CLI_CRT, (void**)&cert_Buf, (size_t*)&cert_Size);
	TCP_UART_DBG("Read %s, result %d\r\n", AZURE_CLI_CRT, result);
	if (QAPI_OK != result)
	{
		TCP_UART_DBG("ERROR: Reading client certificate from EFS failed!! \r\n");
		result = QAPI_ERROR;
	}

	/* Read the client key information */
	result = qt_read_EFS_file((char*)AZURE_CLI_KEY, (void**)&key_Buf, (size_t*)&key_Size);
	TCP_UART_DBG("Read %s, result %d\r\n", AZURE_CLI_KEY, result);
	if (QAPI_OK != result)
	{
		TCP_UART_DBG("ERROR: Reading key information from EFS failed!! \r\n");
		result = QAPI_ERROR;
	}

	/* Update the client certificate information */
	cert_info.cert_Type = QAPI_NET_SSL_CERTIFICATE_E;
	cert_info.info.cert.cert_Buf = cert_Buf;
	cert_info.info.cert.cert_Size = cert_Size;
	cert_info.info.cert.key_Buf = key_Buf;
	cert_info.info.cert.key_Size = key_Size;

	/* Convert and store the certificate */
	result = qapi_Net_SSL_Cert_Convert_And_Store(&cert_info, AZURE_CERT_BIN);
	TCP_UART_DBG("%s qapi_Net_SSL_Cert_Convert_And_Store: %d\r\n", AZURE_CERT_BIN, result);
	return result;
}

int32 dss_inet_ntoa
(
	const qapi_DSS_Addr_t    inaddr, /* IPv4 address to be converted         */
	uint8* buf,    /* Buffer to hold the converted address */
	int32                    buflen  /* Length of buffer                     */
)
{
	uint8* paddr = (uint8*)&inaddr.addr.v4;
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

void quec_config_dns_server()
{
	int e = 0;
	int i, j = 0;

	unsigned int len = 0;
	qapi_Status_t status;
	char iface[15] = { 0 };
	char first_dns[DSS_ADDR_SIZE] = { 0 };
	char second_dns[DSS_ADDR_SIZE] = { 0 };
	qapi_DSS_Addr_Info_t info_ptr[DSS_ADDR_INFO_SIZE];

	/* Get DNS server address */
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
		memset(first_dns, 0, sizeof(first_dns));
		dss_inet_ntoa(info_ptr[i].dnsp_addr_s, (uint8*)first_dns, DSS_ADDR_SIZE);
		TCP_UART_DBG("Primary DNS IP: %s\n", first_dns);

		memset(second_dns, 0, sizeof(second_dns));
		dss_inet_ntoa(info_ptr[i].dnss_addr_s, (uint8*)second_dns, DSS_ADDR_SIZE);
		TCP_UART_DBG("Second DNS IP: %s\n", second_dns);
	}

	/* Start DNS service */
	e = qapi_Net_DNSc_Command(QAPI_NET_DNS_START_E);
	TCP_UART_DBG("Start DNS service, ret: %d", e);

	/* Get current active iface */
	memset(iface, 0, sizeof(iface));
	qapi_DSS_Get_Device_Name(tcp_dss_handle, iface, 15);
	TCP_UART_DBG("device_name: %s\n", iface);
#if 0
	memset(iface, 0, sizeof(iface));
	qapi_DSS_Get_Qmi_Port_Name(dss_handle, iface, 15);
	DNS_UART_DBG("qmi_port_name: %s\n", iface);
#endif

	/* Add dns server into corresponding interface */
	qapi_Net_DNSc_Add_Server_on_iface(first_dns, QAPI_NET_DNS_V4_PRIMARY_SERVER_ID, iface);
	qapi_Net_DNSc_Add_Server_on_iface(second_dns, QAPI_NET_DNS_V4_SECONDARY_SERVER_ID, iface);

}

void quec_uart_debug(LOG_CATEGORY log_category, const char* file, const char* func, int line, unsigned int options, const char* format, ...)
{
    char log_buf[ MAX_LOG_MSG_SIZE ];
	unsigned int len = 0;
    va_list ap;
    va_start( ap, format );

	vsnprintf(log_buf, MAX_LOG_MSG_SIZE - 1, format, ap);

    va_end( ap );
    //TCP_UART_DBG("%s\r\n",log_buf);
    len = strlen(log_buf);
    if((log_buf[len - 1] != '\n') && (len + 2 < MAX_LOG_MSG_SIZE))
    {
    	log_buf[len] = '\n';
		log_buf[len + 1] = 0;
    }

	qapi_UART_Transmit(uart_conf.hdlr, log_buf, strlen(log_buf), NULL);
}

uint8_t * symm_auth_buf = NULL;
uint32_t  symm_auth_size = 0;

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

	int result_o;
	uint8_t * symm_auth_buf = NULL;
	uint32_t  symm_auth_size = 0;

#ifndef NW_SIGNAL_ENABLE 
	/* wait 10sec(set according to the time required for network registration) for device startup.
	if checking network status by device_info, do not need timer sleep. */
	qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_SEC, true);
#endif

	/* Create a byte memory pool. */
	txm_module_object_allocate(&byte_pool_tcp, sizeof(TX_BYTE_POOL));
	tx_byte_pool_create(byte_pool_tcp, "tcp application pool", free_memory_tcp, TCP_BYTE_POOL_SIZE);

	/* Initial uart for debug */
	tcp_uart_dbg_init();
	TCP_UART_DBG("TCPClient Task Start...\n");

#ifdef NW_SIGNAL_ENABLE 
	/* waiting for registering the network */
	txm_module_object_allocate(&tcp_nw_signal_handle, sizeof(TX_EVENT_FLAGS_GROUP));
	tx_event_flags_create(tcp_nw_signal_handle, "nw_signal_event");
	tx_event_flags_set(tcp_nw_signal_handle, 0x0, TX_AND);

	qt_tcp_init_device_info();
	status = qapi_Device_Info_Pass_Pool_Ptr(device_info_hndl, byte_pool_tcp);
	TCP_UART_DBG("[dev_info] status, status %d\n", status);
	status = qapi_Device_Info_Set_Callback_v2(device_info_hndl, QAPI_DEVICE_INFO_NETWORK_IND_E, tcp_network_indication_cb);
	TCP_UART_DBG("[dev_info] qapi_Device_Info_Set_Callback, status %d\n", status);

	nw_sig_mask = NW_SIG_EVT_UNENABLE | NW_SIG_EVT_ENABLE;
	while (1)
	{
		tx_event_flags_get(tcp_nw_signal_handle, nw_sig_mask, TX_OR, &nw_event, TX_WAIT_FOREVER);

		if (nw_event & NW_SIG_EVT_UNENABLE)
		{
			TCP_UART_DBG("waiting for network registering...\n");
			tx_event_flags_set(tcp_nw_signal_handle, ~NW_SIG_EVT_UNENABLE, TX_AND);
		}
		else if (nw_event & NW_SIG_EVT_ENABLE)
		{
			TCP_UART_DBG("network registered!\n");
			tx_event_flags_set(tcp_nw_signal_handle, ~NW_SIG_EVT_ENABLE, TX_AND);
			break;
		}
	}
#endif /* NW_SIGNAL_ENABLE */

#ifdef DATA_TRANSFER_TIMER
	/* Init timer for sending data every 5seconds */
	tcp_data_transfer_timer_def();
	tcp_data_transfer_timer_start();
#endif

	/* Create event signal handle and clear signals */
	txm_module_object_allocate(&tcp_signal_handle, sizeof(TX_EVENT_FLAGS_GROUP));
	tx_event_flags_create(tcp_signal_handle, "dss_signal_event");
	tx_event_flags_set(tcp_signal_handle, 0x0, TX_AND);

	/* Start DSS thread, and detect iface status */
#ifdef QAPI_TXM_MODULE
	if (TX_SUCCESS != txm_module_object_allocate((VOID*)&dss_thread_handle, sizeof(TX_THREAD)))
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

	/*******************AT_UART logic********************/
		qapi_Status_t retval = QAPI_ERROR;
	ret = -1;

	/* wait 5sec for device startup */
	//qapi_Timer_Sleep(5, QAPI_TIMER_UNIT_SEC, true);

	txm_module_object_allocate(&byte_pool_at, sizeof(TX_BYTE_POOL));
	tx_byte_pool_create(byte_pool_at, "byte pool 0", free_memory_at, 10*1024);

	ret = txm_module_object_allocate(&byte_pool_uart, sizeof(TX_BYTE_POOL));
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("txm_module_object_allocate [byte_pool_sensor] failed, %d", ret);
		return ret;
	}

	ret = tx_byte_pool_create(byte_pool_uart, "Sensor application pool", free_memory_uart, UART_BYTE_POOL_SIZE);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_pool_create [byte_pool_sensor] failed, %d", ret);
		return ret;
	}

	ret = tx_byte_allocate(byte_pool_uart, (VOID *)&rx_buff, 4*1024, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_allocate [rx_buff] failed, %d", ret);
		return ret;
	}

	ret = tx_byte_allocate(byte_pool_uart, (VOID *)&tx_buff, 4*1024, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_allocate [tx_buff] failed, %d", ret);
		return ret;
	}

	uart_conf.tx_buff = tx_buff;
	uart_conf.rx_buff = rx_buff;
	uart_conf.tx_len = 4096;
	uart_conf.rx_len = 4096;
	/* uart init */
	uart_init(&uart_conf);
	/* start uart receive */
	uart_recv(&uart_conf);
	/* prompt task running */
    

	if (qapi_atfwd_Pass_Pool_Ptr(atfwd_cmd_handler_cb, byte_pool_at) != QAPI_OK)
	{
		qt_uart_dbg(uart_conf.hdlr, "Unable to alloc User space memory fail state  %x" ,0);
	  										
	}
	
    retval = qapi_atfwd_reg("+DPSINFO", atfwd_cmd_handler_cb);
    if(retval != QAPI_OK)
    {
        qt_uart_dbg(uart_conf.hdlr,"qapi_atfwd_reg  fail\n");
    }
    else
    {
        qt_uart_dbg(uart_conf.hdlr,"qapi_atfwd_reg ok!\n");
    }

	/******************************************/


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
			//quec_convert_ssl_certs();
			quec_config_dns_server();

			xlogging_set_log_function(&quec_uart_debug);
			TCP_UART_DBG("\r\nlocation init\r\n");
			/* GPS options and signal*/
			qapi_Location_Options_t Location_Options = { sizeof(qapi_Location_Options_t),
														500,//update lat/lon frequency
														0 };

			location_init();

			/****************read symm_auth file***********/
			result_o = qt_read_EFS_file(SYMM_AUTH_FILE, (void **)&symm_auth_buf, (size_t *)&symm_auth_size);
    		TCP_UART_DBG("Read %s, result_o %d\r\n", SYMM_AUTH_FILE, result_o);
			/**********************************************/



			int setupRet = azure_setup(symm_auth_buf);
			int GetGPSintervals = 0;
			while (true)
			{
				if (setupRet == 0)
				{
					pnp_GPS_get_flag = false;

					if (GetGPSintervals <= 0)
					{
					
						ret = qapi_Loc_Start_Tracking(loc_clnt, &Location_Options, &gps_tracking_id);
						TCP_UART_DBG("location qapi_Loc_Start_Tracking started, ret = %d\r\n", ret);

						for (int i = 0; i < 16; i++)
						{
							if (pnp_GPS_get_flag == true)
							{

								break;
							}
							ThreadAPI_Sleep(2500);
							TCP_UART_DBG("Waiting for GPS data,time = %d x 2.5s \r\n", i + 1);
						
						}

						if (pnp_GPS_get_flag != true)
						{
							qapi_Loc_Stop_Tracking(loc_clnt, gps_tracking_id);
						}

						TCP_UART_DBG("qapi_Loc_Stop_Tracking() \r\n");
						GetGPSintervals = GETGPSINTERVAL;


					}
					else
					{
						GetGPSintervals--;
					}

				
					

					if (pnp_GPS_get_flag == true)
					{
						result_o = qt_read_EFS_file(SYMM_AUTH_FILE, (void **)&symm_auth_buf, (size_t *)&symm_auth_size);
    					TCP_UART_DBG("Read %s, result_o %d\r\n", SYMM_AUTH_FILE, result_o);

						azure_Task(m_lat, m_lon);
					}
					else
					{
						azure_Task(NULL, NULL);
					}

					

					for (int i = 0; i < 16; i++)
					{
						TCP_UART_DBG("Waiting for sending telemetry ,time = %d x 2.5s \r\n", i + 1);
						if (IsSendingEnds() == true)
						{

							break;
						}
						ThreadAPI_Sleep(2500);

					}

				}
				else
				{
					setupRet = azure_setup();
				
				}
				//ThreadAPI_Sleep(40000);
				
			}


			//azure_Task();
			//azure_iothub_Task
			/* Create a tcp client and comminucate with server */
			//start_tcp_session();
			tx_event_flags_set(tcp_signal_handle, ~DSS_SIG_EVT_CONN_E, TX_AND);
		}
		else if (dss_event & DSS_SIG_EVT_EXIT_E)
		{
			TCP_UART_DBG("DSS_SIG_EVT_EXIT_E Signal\n");
			tx_event_flags_set(tcp_signal_handle, ~DSS_SIG_EVT_EXIT_E, TX_AND);
			tx_event_flags_delete(tcp_signal_handle);
			break;
		}
		else
		{
			TCP_UART_DBG("Unknown Signal\n");
		}

		/* Clear all signals and wait next notification */
		tx_event_flags_set(tcp_signal_handle, 0x0, TX_AND);	//@Fixme:maybe not need
	}

	location_deinit();

	TCP_UART_DBG("Quectel TCP Client Demo is Over!");
	return 0;
}
#endif /*__EXAMPLE_AZURE_IOT__*/
/* End of Example_network.c */
