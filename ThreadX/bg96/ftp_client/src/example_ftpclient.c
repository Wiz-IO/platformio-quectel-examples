/******************************************************************************
*@file    example_tcpclient.c
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

#if defined(__EXAMPLE_FTPCLIENT__)
/*===========================================================================
						   Header file
===========================================================================*/
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include "qapi_fs.h"
#include "qapi_status.h"
#include "qapi_socket.h"
#include "qapi_dss.h"
#include "qapi_netservices.h"
#include "qapi_device_info.h"

#include "qapi_uart.h"
#include "qapi_timer.h"
#include "qapi_diag.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "Example_ftpclient.h"

/*===========================================================================
                             DEFINITION
===========================================================================*/
#define QL_DEF_APN	        "CMNET"
#define DSS_ADDR_INFO_SIZE  5
#define DSS_ADDR_SIZE       16

#define GET_ADDR_INFO_MIN(a, b) ((a) > (b) ? (b) : (a))

#define QUEC_FTP_UART_DBG
#ifdef QUEC_FTP_UART_DBG
#define FTP_UART_DBG(...)	\
{\
	ftp_uart_debug_print(__VA_ARGS__);	\
}
#else
#define FTP_UART_DBG(...)
#endif

#define THREAD_STACK_SIZE    (1024 * 16)
#define THREAD_PRIORITY      (180)
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

TX_BYTE_POOL *byte_pool_ftp;
#define FTP_BYTE_POOL_SIZE		10*8*1024
UCHAR free_memory_ftp[FTP_BYTE_POOL_SIZE];

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
void ftp_uart_dbg_init(void)
{
  	if (TX_SUCCESS != tx_byte_allocate(byte_pool_ftp, (VOID *)&uart_rx_buff, 4*1024, TX_NO_WAIT))
  	{
  		IOT_DEBUG("tx_byte_allocate [uart_rx_buff] failed!");
    	return;
  	}

  	if (TX_SUCCESS != tx_byte_allocate(byte_pool_ftp, (VOID *)&uart_tx_buff, 4*1024, TX_NO_WAIT))
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

void ftp_uart_debug_print(const char* fmt, ...) 
{
	va_list arg_list;
    char dbg_buffer[128] = {0};
    
	va_start(arg_list, fmt);
    vsnprintf((char *)(dbg_buffer), sizeof(dbg_buffer), (char *)fmt, arg_list);
    va_end(arg_list);
		
    qapi_UART_Transmit(uart_conf.hdlr, dbg_buffer, strlen(dbg_buffer), NULL);
    qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_MSEC, true);   //50
}

void ftp_control_log_print(char *log)
{
    ftp_uart_debug_print("FTP> %s", log);
}
static int util_atoi(char *str)
{
    int i=0,j=0;
    char temp[32];
    int num=0;

    for(i=0; str[i]!='\0'; i++)
	{
        if(str[i]>='0' && str[i]<='9')
		{
            temp[j++] = str[i];
        }
    }

    i=0;
    while(i<j)
	{
        num = num*10+temp[i]-'0';
	    i++;
    }
	return num;
}
qbool_t  util_isdigit(char ch)
{
    return (ch >= '0' && ch<= '9') ? true:false;
}

int util_replace_oldstr_with_newstr(char *srcstr, char *oldstr, char *newstr)
{
	int   found_count = 0;
	char *p_foo = NULL;
	char *p_bar = srcstr;

	
	if ((srcstr == NULL) || (oldstr == NULL) || (newstr == NULL))
		return -1;

	p_foo = strstr(p_bar, oldstr);
	
	while (p_foo)
	{
		p_bar = p_foo + strlen(newstr);
		memmove(p_bar, p_foo + strlen(oldstr), strlen(p_foo + strlen(oldstr)) + 1);
		memcpy(p_foo, newstr, strlen(newstr));
		found_count ++;
		p_foo = strstr(p_bar, oldstr);
	}

	return found_count;
}

int qt_get_respcode(const char* resp_ptr, int resp_len)
{
    int    i = 0;
    int    resp_code = 0;
    int    line_len = 0;
    char   line_buf[512];
    char  *temp_ptr = NULL;
    
    memset(line_buf, 0, sizeof(line_buf));
    temp_ptr = strstr(resp_ptr, "\r\n");
    if(temp_ptr == NULL)
    {
        return -1;
    }
    line_len = temp_ptr - resp_ptr;

    memcpy(line_buf, resp_ptr, resp_len);
    line_buf[line_len] = '\0';
    
    while(util_isdigit(line_buf[i]))
    {
        resp_code = resp_code*10 + (resp_ptr[i] - '0');
        i ++;
    }
    
    return resp_code;
}

int qt_process_pasv_resp(const char *resp_ptr, char *remote_ip, int *remote_port)
{
    int         i = 0;
    char        temp_str[10] = {0};
    int         resp_code = 0;
    int         addr_ip[4] = {0};
    int         addr_port[2] = {0};
    const char *addr_ptr = NULL;
    const char *temp_ptr = resp_ptr;
    
    if(temp_ptr == NULL)
        return -1;

    while(util_isdigit(*temp_ptr)
            &&(*temp_ptr != '\0'))
    {
        resp_code = resp_code*10 + (*temp_ptr - '0');
        temp_ptr ++;
    }
    
    if(resp_code != FTP_RESPONSE_ENTER_PASV)
    {
        return -1;
    }
    
    addr_ptr = (char*)strstr(resp_ptr, "(");
    addr_ptr += 1;

    for(i = 0;i < 4;i ++)
    {
        temp_ptr = (char*)strstr(addr_ptr, ",");
        memset(temp_str, 0, sizeof(temp_str));
        memcpy(temp_str, addr_ptr, (temp_ptr - addr_ptr));
        addr_ip[i] = util_atoi(temp_str);
        addr_ptr = temp_ptr + 1;
    }
    sprintf(remote_ip, "%d.%d.%d.%d", addr_ip[0], addr_ip[1], addr_ip[2], addr_ip[3]);

    temp_ptr = (char*)strstr(addr_ptr, ",");
    memset(temp_str, 0, sizeof(temp_str));
    memcpy(temp_str, addr_ptr, (temp_ptr - addr_ptr));
    addr_port[0] = util_atoi(temp_str);
    addr_ptr = temp_ptr + 1;

    temp_ptr = (char*)strstr(addr_ptr, ")");
    memset(temp_str, 0, sizeof(temp_str));
    memcpy(temp_str, addr_ptr, (temp_ptr - addr_ptr));
    addr_port[1] = util_atoi(temp_str);

    *remote_port |= addr_port[0] << 8;
    *remote_port |= addr_port[1];
    
    return resp_code;
}
int ftp_socket_conn(const char *servip, const int port)
{
	int sock_fd = 0;
	struct sockaddr_in client_addr;

    sock_fd = qapi_socket(AF_INET, DEF_SRC_TYPE, 0);
    if (sock_fd < 0)
    {
        FTP_UART_DBG("Create socket error\n");			
		return -1;
    }

    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = _htons(port);
    client_addr.sin_addr.s_addr = inet_addr(servip);

    /* Connect to FTP server */
    if (-1 == qapi_connect(sock_fd, (struct sockaddr *)&client_addr, sizeof(client_addr)))
    {
        FTP_UART_DBG("Connect to servert error\n");
		return -1;
    }
		
    FTP_UART_DBG("\n<---- Connect to server[%s][%d] success ---->\n", servip, port);

	return sock_fd;
}

int ftp_socket_send(int sockfd, void *send_buf, int bytes)
{
	int			nleft = -1;
	int 		nwritten = -1;
	const char *data_ptr = (const char*)send_buf;

    if(data_ptr == NULL)
    {
		FTP_UART_DBG("send buffer is NULL!\n");
        return -1;
    }

	nleft = bytes;
	while (nleft > 0)
	{
		nwritten = qapi_send(sockfd, data_ptr, nleft, 0);
		if (nwritten <= 0)
		{
			return(nwritten);		/* error */
		}

		nleft    -= nwritten;
		data_ptr += nwritten;
	}
	
	return(bytes - nleft);
}

int ftp_send_cmd(int sockfd, const char *cmd, const char *buf, int flag)
{
	int     ret_val = -1;
	char    request[512];
	char    response[512];


	memset(request, 0, sizeof(request));
    memset(response, 0, sizeof(response));

    if((cmd == NULL) ||
        (buf == NULL && flag == 1))
    {
    	FTP_UART_DBG("invalid parameter!\n");
        return -1;
    }
    
	if (flag == 0)
		snprintf(request, sizeof(request), "%s\r\n", cmd);
	else
		snprintf(request, sizeof(request), "%s %s\r\n", cmd, buf);

	ret_val = ftp_socket_send(sockfd, request, strlen(request));
	if (ret_val == -1)
		return -1;
    ftp_control_log_print(request);

	return 0;
}
int ftp_process_list_cmd(int data_sock)
{
    char data_buff[120] = {0};

    while ((qapi_recv(data_sock, data_buff, sizeof(data_buff)-1,0)) > 0)
    {
        FTP_UART_DBG(data_buff);
        memset(data_buff, 0, sizeof(data_buff));
    }
    qapi_socketclose(data_sock);
    return 0;
}

int ftp_process_retr_cmd(int data_sock)
{
    int  open_flag = 0;
    int  write_fd = -1;
    char data_buff[120] = {0};
    int  data_written = 0;

    snprintf(data_buff, sizeof(data_buff), "/datatx/%s", FTP_DOWNLOAD_FILE);
    open_flag = QAPI_FS_O_WRONLY_E | QAPI_FS_O_CREAT_E | QAPI_FS_O_TRUNC_E;
    if(QAPI_OK != qapi_FS_Open(data_buff, open_flag, &write_fd))
    {
    	FTP_UART_DBG("open file [%s] fail!\n", data_buff);
        return -1;
    }

    memset(data_buff, 0, sizeof(data_buff));
    while (qapi_recv(data_sock, data_buff, sizeof(data_buff)-1, 0) > 0)
    {
        if(write_fd >= 0)
        {
            if(QAPI_OK != qapi_FS_Write(write_fd, data_buff, strlen(data_buff), &data_written))
            {
                FTP_UART_DBG("write file fail!\n");
                return -1;
            }
        }
        
        FTP_UART_DBG(data_buff);
        memset(data_buff, 0, sizeof(data_buff));
    }
    FTP_UART_DBG("\n");

    if(write_fd >= 0)
    {
        qapi_FS_Close(write_fd);
    }
    qapi_socketclose(data_sock);
    return 0;
}
int ftp_process_stor_cmd(int data_sock)
{
    int  ret_val = -1;
    int  open_flag = 0;
    int  read_fd = -1;
    char data_buff[120] = {0};
    int  data_read = 0;

    snprintf(data_buff, sizeof(data_buff), "/datatx/%s", FTP_UPLOAD_FILE);

    open_flag = QAPI_FS_O_RDONLY_E;
    if(QAPI_OK != qapi_FS_Open(data_buff, open_flag, &read_fd))
    {
    	FTP_UART_DBG("open file [%s] fail!\n", data_buff);
        return -1;
    }
    if(data_sock >= 0)
    {   
        memset(data_buff, 0, sizeof(data_buff));
        while((QAPI_OK == qapi_FS_Read(read_fd, data_buff, sizeof(data_buff)-1, &data_read))
                    && (data_read > 0))
        {
    		FTP_UART_DBG(data_buff);
            ret_val = ftp_socket_send(data_sock, data_buff, strlen(data_buff));
            if(ret_val <= 0)
            {
            	qapi_socketclose(data_sock);
                qapi_FS_Close(read_fd);
                return -1;
            }
            memset(data_buff, 0, sizeof(data_buff));
        }
        qapi_FS_Close(read_fd);
    }
    FTP_UART_DBG("\n");
    
	qapi_socketclose(data_sock);
    return 0;
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
	
	FTP_UART_DBG("Data test event callback, event: %d\n", evt);

	switch (evt)
	{
		case QAPI_DSS_EVT_NET_IS_CONN_E:
		{
			FTP_UART_DBG("Data Call Connected.\n");
			tcp_show_sysinfo();
			/* Signal main task */
  			tx_event_flags_set(tcp_signal_handle, DSS_SIG_EVT_CONN_E, TX_OR);
			tcp_datacall_status = DSS_EVT_NET_IS_CONN_E;
			
			break;
		}
		case QAPI_DSS_EVT_NET_NO_NET_E:
		{
			FTP_UART_DBG("Data Call Disconnected.\n");
			
			if (DSS_EVT_NET_IS_CONN_E == tcp_datacall_status)
			{
				/* Release Data service handle and netctrl library */
				if (tcp_dss_handle)
				{
					status = qapi_DSS_Rel_Data_Srvc_Hndl(tcp_dss_handle);
					if (QAPI_OK == status)
					{
						FTP_UART_DBG("Release data service handle success\n");
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
			FTP_UART_DBG("Data Call status is invalid.\n");
			
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
		FTP_UART_DBG("Get IP address count error\n");
		return;
	}
		
	status = qapi_DSS_Get_IP_Addr(tcp_dss_handle, info_ptr, len);
	if (QAPI_ERROR == status)
	{
		FTP_UART_DBG("Get IP address error\n");
		return;
	}
	
	j = GET_ADDR_INFO_MIN(len, DSS_ADDR_INFO_SIZE);
	
	for (i = 0; i < j; i++)
	{
		FTP_UART_DBG("<--- static IP address information --->\n");
		tcp_inet_ntoa(info_ptr[i].iface_addr_s, buff, DSS_ADDR_SIZE);
		FTP_UART_DBG("static IP: %s\n", buff);
        
		memset(buff, 0, sizeof(buff));
		tcp_inet_ntoa(info_ptr[i].gtwy_addr_s, buff, DSS_ADDR_SIZE);
		FTP_UART_DBG("Gateway IP: %s\n", buff);

		memset(buff, 0, sizeof(buff));
		tcp_inet_ntoa(info_ptr[i].dnsp_addr_s, buff, DSS_ADDR_SIZE);
		FTP_UART_DBG("Primary DNS IP: %s\n", buff);

		memset(buff, 0, sizeof(buff));
		tcp_inet_ntoa(info_ptr[i].dnss_addr_s, buff, DSS_ADDR_SIZE);
		FTP_UART_DBG("Second DNS IP: %s\n", buff);
	}

	FTP_UART_DBG("<--- End of system info --->\n");
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
        FTP_UART_DBG("Setting tech to Automatic\n");
        qapi_DSS_Set_Data_Call_Param(tcp_dss_handle, QAPI_DSS_CALL_INFO_TECH_PREF_E, &param_info);

		/* set apn */
        param_info.buf_val = apn;
        param_info.num_val = strlen(apn);
        FTP_UART_DBG("Setting APN - %s\n", apn);
        qapi_DSS_Set_Data_Call_Param(tcp_dss_handle, QAPI_DSS_CALL_INFO_APN_NAME_E, &param_info);
#ifdef QUEC_CUSTOM_APN
		/* set apn username */
		param_info.buf_val = apn_username;
        param_info.num_val = strlen(apn_username);
        FTP_UART_DBG("Setting APN USER - %s\n", apn_username);
        qapi_DSS_Set_Data_Call_Param(tcp_dss_handle, QAPI_DSS_CALL_INFO_USERNAME_E, &param_info);

		/* set apn password */
		param_info.buf_val = apn_passwd;
        param_info.num_val = strlen(apn_passwd);
        FTP_UART_DBG("Setting APN PASSWORD - %s\n", apn_passwd);
        qapi_DSS_Set_Data_Call_Param(tcp_dss_handle, QAPI_DSS_CALL_INFO_PASSWORD_E, &param_info);
#endif
		/* set IP version(IPv4 or IPv6) */
        param_info.buf_val = NULL;
        param_info.num_val = QAPI_DSS_IP_VERSION_4;
        FTP_UART_DBG("Setting family to IPv%d\n", param_info.num_val);
        qapi_DSS_Set_Data_Call_Param(tcp_dss_handle, QAPI_DSS_CALL_INFO_IP_VERSION_E, &param_info);
    }
    else
    {
        FTP_UART_DBG("Dss handler is NULL!!!\n");
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

	FTP_UART_DBG("Initializes the DSS netctrl library\n");

	/* Initializes the DSS netctrl library */
	if (QAPI_OK == qapi_DSS_Init(QAPI_DSS_MODE_GENERAL))
	{
		tcp_netctl_lib_status = DSS_LIB_STAT_SUCCESS_E;
		FTP_UART_DBG("qapi_DSS_Init success\n");
	}
	else
	{
		/* @Note: netctrl library has been initialized */
		tcp_netctl_lib_status = DSS_LIB_STAT_FAIL_E;
		FTP_UART_DBG("DSS netctrl library has been initialized.\n");
	}
	
	/* Registering callback tcp_dss_handleR */
	do
	{
		FTP_UART_DBG("Registering Callback tcp_dss_handle\n");
		
		/* Obtain data service handle */
		status = qapi_DSS_Get_Data_Srvc_Hndl(tcp_net_event_cb, NULL, &tcp_dss_handle);
		FTP_UART_DBG("tcp_dss_handle %d, status %d\n", tcp_dss_handle, status);
		
		if (NULL != tcp_dss_handle)
		{
			FTP_UART_DBG("Registed tcp_dss_handler success\n");
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
		FTP_UART_DBG("quectel_dss_init fail.\n");
		return -1;
	}

	FTP_UART_DBG("qapi_DSS_Start_Data_Call start!!!.\n");
	status = qapi_DSS_Start_Data_Call(tcp_dss_handle);
	if (QAPI_OK == status)
	{
		FTP_UART_DBG("Start Data service success.\n");
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
			FTP_UART_DBG("Stop data call success\n");
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
			FTP_UART_DBG("Data service task exit.\n");
			break;
		}
	}

	FTP_UART_DBG("Data Service Thread Exit!\n");
	return;
}
static int start_ftp_session(void)
{
    int     ctrl_sock = -1;
    int     data_sock = -1;
    int     ret_val = 0;
    int     resp_code = -1;
    char    response[512];
    fd_set  fdset;
    ftp_command_enum last_cmd = FTP_COMMAND_NONE;
    ftp_operation_enum curr_oper = FTP_OPER_NONE;
    
    ctrl_sock = ftp_socket_conn(DEF_SRV_ADDR, DEF_SRV_PORT);

    qapi_fd_zero(&fdset);
    qapi_fd_set(ctrl_sock, &fdset);

    while(1)
    {
        ret_val = qapi_select(&fdset, NULL, NULL, CLIENT_WAIT_TIME);
        if(ret_val > 0)
        {
            memset(response, 0, sizeof(response));
            ret_val = qapi_recv(ctrl_sock, (char *)response, sizeof(response), 0);
            resp_code = qt_get_respcode(response, strlen(response));
            ftp_control_log_print(response);
            
            switch(last_cmd)
            {
                case FTP_COMMAND_NONE:
                    if(resp_code == FTP_RESPONSE_READY_NEW)
                    {
                        curr_oper = FTP_OPER_LOGIN;
                        ftp_send_cmd(ctrl_sock, "USER", "test", 1);
                        last_cmd = FTP_COMMAND_USER;
                    }
                    break;
                case FTP_COMMAND_USER:
                    if(resp_code == FTP_RESPONSE_NEED_PWD)
                    {
                        ftp_send_cmd(ctrl_sock, "PASS", "test", 1);
                        last_cmd = FTP_COMMAND_PASS;
                    }
                    break;
                case FTP_COMMAND_PASS:
                    if(resp_code == FTP_RESPONSE_USER_LOGIN)
                    {
                    	FTP_UART_DBG("<---- FTP client login in success! ---->\n\n");

                        //login ftp server success, then perform change dir operation
                        curr_oper = FTP_OPER_CD;   
                        ftp_send_cmd(ctrl_sock, "CWD", "/quectel", 1);
                        last_cmd = FTP_COMMAND_CWD;
                    }
                    break;
                case FTP_COMMAND_CWD:
                    if(resp_code == FTP_RESPONSE_DAT_FINISH)
                    {
                    	FTP_UART_DBG("<---- FTP client change dir success! ---->\n\n");

                        //change dir success, then perform list operation
                        curr_oper = FTP_OPER_LIST; 
                        ftp_send_cmd(ctrl_sock, "TYPE", "I", 1);
                        last_cmd = FTP_COMMAND_TYPE;
                    }
                case FTP_COMMAND_TYPE:
                    if(resp_code == FTP_RESPONSE_CMD_OK)
                    {
                        if(curr_oper == FTP_OPER_LIST || curr_oper == FTP_OPER_GET || 
                            curr_oper == FTP_OPER_PUT )
                        {
                            ftp_send_cmd(ctrl_sock, "PASV", NULL, 0);
                            last_cmd = FTP_COMMAND_PASV;
                        }
                    }
                case FTP_COMMAND_PASV:
                    if(resp_code == FTP_RESPONSE_ENTER_PASV)
                    {
                        int     remote_port = 0;
                        char    remote_ip[16] = {0};
                        
                        qt_process_pasv_resp(response, remote_ip, &remote_port);
                        data_sock = ftp_socket_conn(remote_ip, remote_port);
                        if(data_sock == -1)
                        {
                			FTP_UART_DBG("ftp_socket_conn fail!\n");
                            break;
                        }
                        if(curr_oper == FTP_OPER_LIST )
                        {
                            ftp_send_cmd(ctrl_sock, "LIST", ".", 1);
                            last_cmd = FTP_COMMAND_LIST;
                        }
                        else if(curr_oper == FTP_OPER_GET)
                        {
                            ftp_send_cmd(ctrl_sock, "RETR", FTP_DOWNLOAD_FILE, 1);
                            last_cmd = FTP_COMMAND_RETR;
                        }
                        else if(curr_oper == FTP_OPER_PUT)
                        {
                            ftp_send_cmd(ctrl_sock, "STOR", FTP_UPLOAD_FILE, 1);
                            last_cmd = FTP_COMMAND_STOR;
                        }
                    }
                    break;
                case FTP_COMMAND_LIST:
                    if(resp_code == FTP_RESPONSE_OPEN_DATA && data_sock != -1)
                    {
                        ftp_process_list_cmd(data_sock);
                        data_sock = -1;
                    }
                    else if(resp_code == FTP_RESPONSE_DAT_SUCCESS)
                    {
                    	FTP_UART_DBG("<---- FTP client list success! ---->\n\n");
                        
                        //LIST success, then perform get file operation
                        curr_oper = FTP_OPER_GET;
                        ftp_send_cmd(ctrl_sock, "TYPE", "I", 1);
                        last_cmd = FTP_COMMAND_TYPE;
                    }
                    break;
                case FTP_COMMAND_RETR:
                    if(resp_code == FTP_RESPONSE_OPEN_DATA && data_sock != -1)
                    {
                        ftp_process_retr_cmd(data_sock);
                        data_sock = -1;
                    }
                    else if(resp_code == FTP_RESPONSE_DAT_SUCCESS)
                    {
                    	FTP_UART_DBG("<---- FTP client get file success! ---->\n\n");

                        //GET file success, then perform put file operation
                        curr_oper = FTP_OPER_PUT;
                        ftp_send_cmd(ctrl_sock, "TYPE", "I", 1);
                        last_cmd = FTP_COMMAND_TYPE;
                    }
                    break;
                case FTP_COMMAND_STOR:
                    if(resp_code == FTP_RESPONSE_OPEN_DATA && data_sock != -1)
                    {
                        ftp_process_stor_cmd(data_sock);
                        data_sock = -1;
                    }
                    else if(resp_code == FTP_RESPONSE_DAT_SUCCESS)
                    {
                    	FTP_UART_DBG("<---- FTP client put file success! ---->\n\n");
                        
                        //PUT file success, then log out ftp server
                        ftp_send_cmd(ctrl_sock, "QUIT", NULL, 0);
                        last_cmd = FTP_COMMAND_QUIT;
                    }
                    break;
                case FTP_COMMAND_QUIT:
                    if(resp_code == FTP_RESPONSE_BYE)
                    {
                    	FTP_UART_DBG("<---- FTP client log out success! ---->\n\n");
                        qapi_socketclose(ctrl_sock);
                        return 0;
                    }
                    break;
                default:
                    FTP_UART_DBG("invalid ftp command\r\n");
                    break;
            }
        }
    }
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
	bool network_enabled = false;
	qapi_Device_Info_t info;

    /* Create a byte memory pool. */
    txm_module_object_allocate(&byte_pool_ftp, sizeof(TX_BYTE_POOL));
    tx_byte_pool_create(byte_pool_ftp, "ftp application pool", free_memory_ftp, FTP_BYTE_POOL_SIZE);

	/* Initial uart for debug */
	ftp_uart_dbg_init();

	FTP_UART_DBG("FTP Task Start...\n");
	qapi_Device_Info_Init();

	do
	{
		qapi_Timer_Sleep(200, QAPI_TIMER_UNIT_MSEC, true);
		memset(&info,  0x00, sizeof(qapi_Device_Info_t));
		qapi_Device_Info_Get(QAPI_DEVICE_INFO_SERVICE_STATE_E, &info);
		if(info.u.valueint == 0)
		{
			network_enabled = false;
		}
		else
		{
			network_enabled = true;
		}
	}while(!network_enabled);

	FTP_UART_DBG("network_enabled %d\n", network_enabled);
	//qapi_Device_Info_Release();
	
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
		FTP_UART_DBG("SIGNAL EVENT IS [%d]\n", dss_event);
		
		if (dss_event & DSS_SIG_EVT_INV_E)
		{
			FTP_UART_DBG("DSS_SIG_EVT_INV_E Signal\n");
			tx_event_flags_set(tcp_signal_handle, ~DSS_SIG_EVT_INV_E, TX_AND);
		}
		else if (dss_event & DSS_SIG_EVT_NO_CONN_E)
		{
			FTP_UART_DBG("DSS_SIG_EVT_NO_CONN_E Signal\n");
			tx_event_flags_set(tcp_signal_handle, ~DSS_SIG_EVT_NO_CONN_E, TX_AND);
		}
		else if (dss_event & DSS_SIG_EVT_CONN_E)
		{
			FTP_UART_DBG("DSS_SIG_EVT_CONN_E Signal\n");

			/* Create a ftp client and comminucate with server */
			start_ftp_session();
			tx_event_flags_set(tcp_signal_handle, ~DSS_SIG_EVT_CONN_E, TX_AND);
		}
		else if (dss_event & DSS_SIG_EVT_EXIT_E)
		{
			FTP_UART_DBG("DSS_SIG_EVT_EXIT_E Signal\n");
			tx_event_flags_set(tcp_signal_handle, ~DSS_SIG_EVT_EXIT_E, TX_AND);
			tx_event_flags_delete(tcp_signal_handle);
			break;
		}
		else
		{
			FTP_UART_DBG("Unkonw Signal\n");
		}

		/* Clear all signals and wait next notification */
		tx_event_flags_set(tcp_signal_handle, 0x0, TX_AND);	//@Fixme:maybe not need
	}
	
	FTP_UART_DBG("Quectel TCP Client Demo is Over!");
	
	return 0;
}
#endif /*__EXAMPLE_FTPCLIENT__*/
/* End of Example_ftpclient.c */
