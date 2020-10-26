
/******************************************************************************
*@file    example_fota.c
*@brief   example of fota http download and update.
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2020 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#if defined(__EXAMPLE_FOTA__)
/**************************************************************************
*                                 INCLUDE
***************************************************************************/
#include "qapi_uart.h"
#include "qapi_timer.h"
#include "qapi_diag.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "qapi_quectel.h"
#include "qapi_fs_types.h"
#include <locale.h>



/**************************************************************************
*                                 GLOBAL
***************************************************************************/
TX_BYTE_POOL *byte_pool_uart;
TX_BYTE_POOL *byte_pool_at;

#define FOTA_BYTE_POOL_SIZE		10*8*1024
UCHAR free_memory_at[FOTA_BYTE_POOL_SIZE];

UCHAR free_memory_fota[FOTA_BYTE_POOL_SIZE];

/* fota uart rx tx buffer */
static char *fota_rx_buff = NULL; /*!!! should keep this buffer as 4K Bytes */
static char *fota_tx_buff = NULL;

/* uart config para*/
static QT_UART_CONF_PARA fota_uart_conf = 
{
	NULL,
	QT_QAPI_UART_PORT_01,
	NULL,
	0,
	NULL,
	0,
	115200
};

/* conter used to count the total run times*/
unsigned long task_run_couter = 0;


/**************************************************************************
*                                 FUNCTION
***************************************************************************/

void fota_uart_recv(QT_UART_CONF_PARA *uart_conf)
{
	qapi_Status_t status;
	status = qapi_UART_Receive(uart_conf->hdlr, uart_conf->rx_buff, uart_conf->rx_len, (void*)uart_conf);
	IOT_DEBUG("QT# qapi_UART_Receive [%d] status %d", (qapi_UART_Port_Id_e)uart_conf->port_id, status);
}

void fota_uart_dbg(qapi_UART_Handle_t uart_hdlr, const char* fmt, ...)
{
	char log_buf[256] = {0};

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(log_buf, sizeof(log_buf), fmt, ap);
	va_end( ap );

    qapi_UART_Transmit(uart_hdlr, log_buf, strlen(log_buf), NULL);
    qapi_UART_Transmit(uart_hdlr, "\r\n", strlen("\r\n"), NULL);
}

/*
@func
  quectel_FOTA_dl_cb
@brief
  This callback will come when download failed or success.
*/
void quectel_FOTA_dl_cb(int error_id)
{
    qapi_Status_t status = 0;
    if(error_id == 0)
    {
        fota_uart_dbg(fota_uart_conf.hdlr,"FOTA download success");
    }
    else
    {
        fota_uart_dbg(fota_uart_conf.hdlr,"FOTA download failed:%d", error_id); 
    }  
}

void quectel_FOTA_dl_process_cb(int percent)
{
	fota_uart_dbg(fota_uart_conf.hdlr,"FOTA download process:%d%%", percent); 
}

static void fota_uart_rx_cb(uint32_t num_bytes, void *cb_data)
{
	qapi_Status_t status = 0;
	QT_UART_CONF_PARA *uart_conf = (QT_UART_CONF_PARA*)cb_data;
	if(num_bytes == 0)
	{
		fota_uart_recv(uart_conf);
		return;
	}
	else if(num_bytes >= uart_conf->rx_len)
	{
		num_bytes = uart_conf->rx_len;
	}
    if(strncmp(uart_conf->rx_buff, "FOTAURL=HTTP", strlen("FOTAURL=HTTP")) == 0) // FOTA upgrade,for example:FOTAURL=HTTP://220.180.239.212:8005/BG95_112A_119.zip.
    {
        status = qapi_QT_FOTA_Download_Start(uart_conf->rx_buff+8, quectel_FOTA_dl_cb);
        if(status != QAPI_QT_ERR_OK)
        {
             fota_uart_dbg(fota_uart_conf.hdlr,"qapi_QT_FOTA_Download_Start failed:%d", status);  
        }
		else
		{
			fota_uart_dbg(fota_uart_conf.hdlr,"qapi_QT_FOTA_Download_Start success");
			status = qapi_QT_FOTA_DL_Process(quectel_FOTA_dl_process_cb);
        	if(status != QAPI_QT_ERR_OK)
        	{
            	IOT_DEBUG("qapi_QT_FOTA_DL_Process fialed:%d",status); 
        	}				
		}
    }
    else if(strncmp(uart_conf->rx_buff, "FOTAURL=COAP", strlen("FOTAURL=COAP")) == 0) // FOTA upgrade,for example:FOTAURL=COAP://220.180.239.212:8005/BG95_112A_119.zip.
    {
        status = qapi_QT_FOTA_Download_Start(uart_conf->rx_buff+8, quectel_FOTA_dl_cb);
        if(status != QAPI_QT_ERR_OK)
        {
             fota_uart_dbg(fota_uart_conf.hdlr,"qapi_QT_FOTA_Download_Start failed:%d", status);  
        }
		else
		{
			fota_uart_dbg(fota_uart_conf.hdlr,"qapi_QT_FOTA_Download_Start success");
		}
    }	
    /*
    fota upgrade by local. for example:FOTAURL=LCOAL. Before sending this command, the delta firmware package should be uploded to the flash first.
    */
    else if(strncmp(uart_conf->rx_buff, "FOTAURL=LOCAL",strlen("FOTAURL=LOCAL")) == 0)
    {
        fota_uart_dbg(fota_uart_conf.hdlr, "FOTA upgrade by local");
        status = qapi_QT_FOTA_Update_Start();
        if(status != QAPI_QT_ERR_OK)
        {
            IOT_DEBUG("qapi_QT_FOTA_Update_Start fialed:%d",status); 
        }
    }
    /*
    Get the FOTA package information
    */
    else if(strncmp(uart_conf->rx_buff, "FOTAINFO",strlen("FOTAINFO")) == 0)
    {
    	uint32 download_size = 0;
		uint32 total_size = 0;

		fota_uart_dbg(fota_uart_conf.hdlr, "FOTA Information");
        status = qapi_QT_FOTA_Get_Package_Info(&download_size, &total_size);
        if(status != QAPI_QT_ERR_OK)
        {
        	fota_uart_dbg(fota_uart_conf.hdlr, "qapi_QT_FOTA_Get_Package_Info fialed:%d",status); 
            IOT_DEBUG("qapi_QT_FOTA_Get_Package_Info fialed:%d",status); 
        }
		else
		{
			fota_uart_dbg(fota_uart_conf.hdlr, "qapi_QT_FOTA_Get_Package_Info %d %d", download_size, total_size); 
			IOT_DEBUG("qapi_QT_FOTA_Get_Package_Info %d %d", download_size, total_size); 
		}
    }
    /*
    Cancel the current download operation
    */
    else if(strncmp(uart_conf->rx_buff, "FOTADLCANCEL",strlen("FOTADLCANCEL")) == 0)
    {
        status = qapi_QT_FOTA_Download_Cancel();
		if(status != QAPI_QT_ERR_OK)
        {
            fota_uart_dbg(fota_uart_conf.hdlr,"qapi_QT_FOTA_Download_Cancel failed:%d", status);
			IOT_DEBUG("qapi_QT_FOTA_Download_Cancel fialed:%d",status); 
        }
		else
		{
			fota_uart_dbg(fota_uart_conf.hdlr,"qapi_QT_FOTA_Download_Cancel success");  
		}
		
    } 
	else if(strncmp(uart_conf->rx_buff, "FOTA_APN",strlen("FOTA_APN")) == 0)
	{
		qapi_QT_FOTA_APN_Struct_t tfota_struct;
		tfota_struct.iptype = QAPI_FOTA_DSS_IP_VERSION_NON_IP;
		memset(&tfota_struct,0,sizeof(qapi_QT_FOTA_APN_Struct_t));
		status = qapi_QT_FOTA_APN_Set(&tfota_struct);
		if(status != QAPI_QT_ERR_OK)
        {
            fota_uart_dbg(fota_uart_conf.hdlr,"qapi_QT_FOTA_APN_Set failed:%d", status);
			IOT_DEBUG("qapi_QT_FOTA_APN_Set fialed:%d",status); 
        }
		else
		{
			fota_uart_dbg(fota_uart_conf.hdlr,"qapi_QT_FOTA_APN_Set success");  
		}		
	}
	else if(strncmp(uart_conf->rx_buff,"REMOVEPKG",strlen("REMOVEPKG"))==0)
	{
		status = qapi_QT_Remove_FOTA_Package();	
		if(status != QAPI_QT_ERR_OK)
        {
             fota_uart_dbg(fota_uart_conf.hdlr,"qapi_QT_Remove_FOTA_Package failed:%d", status);  
        }
		else
		{
			fota_uart_dbg(fota_uart_conf.hdlr,"qapi_QT_Remove_FOTA_Package success");
		}
	}
    else
    {
        fota_uart_dbg(fota_uart_conf.hdlr, "Input command error");
    }
 	memset(uart_conf->rx_buff, 0, uart_conf->rx_len);
	fota_uart_recv(uart_conf);
;
}

static void fota_uart_tx_cb(uint32_t num_bytes, void *cb_data)
{
	IOT_DEBUG("QT# fota_uart_tx_cb, send [%d]", num_bytes);
}


void fota_uart_init(QT_UART_CONF_PARA *uart_conf)
{
	qapi_Status_t status;
	qapi_UART_Open_Config_t uart_cfg;
//	QAPI_Flow_Control_Type uart_fc_type = QAPI_FCTL_OFF_E;

	uart_cfg.baud_Rate			= uart_conf->baudrate;
	uart_cfg.enable_Flow_Ctrl	= QAPI_FCTL_OFF_E;
	uart_cfg.bits_Per_Char		= QAPI_UART_8_BITS_PER_CHAR_E;
	uart_cfg.enable_Loopback 	= 0;
	uart_cfg.num_Stop_Bits		= QAPI_UART_1_0_STOP_BITS_E;
	uart_cfg.parity_Mode 		= QAPI_UART_NO_PARITY_E;
	uart_cfg.rx_CB_ISR			= (qapi_UART_Callback_Fn_t)&fota_uart_rx_cb;
	uart_cfg.tx_CB_ISR			= (qapi_UART_Callback_Fn_t)&fota_uart_tx_cb;

	status = qapi_UART_Open(&uart_conf->hdlr, uart_conf->port_id, &uart_cfg);
	IOT_DEBUG("QT# qapi_UART_Open [%d] status %d", uart_conf->port_id, status);

	status = qapi_UART_Power_On(uart_conf->hdlr);
	IOT_DEBUG("QT# qapi_UART_Power_On [%d] status %d", uart_conf->port_id, status);
#if 0
	status = qapi_UART_Ioctl(uart_conf->hdlr, QAPI_SET_FLOW_CTRL_E, &uart_fc_type);
	IOT_DEBUG("QT# qapi_UART_Ioctl [%d] status %d", uart_conf->port_id, status);	
#endif
}


/*
@func
  quectel_task_entry
@brief
  Entry function for task. 
*/
int quectel_task_entry(void)
{
	int ret = -1;
	
	// MUST SETTING,TBD
	setlocale(LC_ALL, "C");

//	qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_SEC, true);
	IOT_DEBUG("QT# quectel_task_entry Start");
	txm_module_object_allocate(&byte_pool_at, sizeof(TX_BYTE_POOL));
	tx_byte_pool_create(byte_pool_at, "byte pool 0", free_memory_at, 10*1024);



	ret = txm_module_object_allocate(&byte_pool_uart, sizeof(TX_BYTE_POOL));
  	if(ret != TX_SUCCESS)
  	{
  		IOT_DEBUG("txm_module_object_allocate [byte_pool_sensor] failed, %d", ret);
    	return ret;
  	}

	ret = tx_byte_pool_create(byte_pool_uart, "Sensor application pool", free_memory_fota, FOTA_BYTE_POOL_SIZE);
  	if(ret != TX_SUCCESS)
  	{
  		IOT_DEBUG("tx_byte_pool_create [byte_pool_sensor] failed, %d", ret);
    	return ret;
  	}

  	ret = tx_byte_allocate(byte_pool_uart, (VOID *)&fota_rx_buff, 4*1024, TX_NO_WAIT);
  	if(ret != TX_SUCCESS)
  	{
  		IOT_DEBUG("tx_byte_allocate [fota_rx_buff] failed, %d", ret);
    	return ret;
  	}

  	ret = tx_byte_allocate(byte_pool_uart, (VOID *)&fota_tx_buff, 4*1024, TX_NO_WAIT);
  	if(ret != TX_SUCCESS)
  	{
  		IOT_DEBUG("tx_byte_allocate [fota_tx_buff] failed, %d", ret);
    	return ret;
  	}

	fota_uart_conf.tx_buff = fota_tx_buff;
	fota_uart_conf.rx_buff = fota_rx_buff;
	fota_uart_conf.tx_len = 4096;
	fota_uart_conf.rx_len = 4096;

	/* uart init 			*/
	fota_uart_init(&fota_uart_conf);
	/* start uart receive */
	fota_uart_recv(&fota_uart_conf);
	/* prompt task running 	*/
	qt_uart_dbg(fota_uart_conf.hdlr,"\r\nFOTA Example Start");

	while (1)
	{
		return ret;
	}
}

#endif /*__EXAMPLE_FOTA__*/

