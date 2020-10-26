/******************************************************************************
*@file    example_sms.c
*@brief   example of SMS
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/

#if defined(__EXAMPLE_SMS__)
#include "txm_module.h"
#include "qapi_diag.h"
#include "qapi_timer.h"
#include "qapi_uart.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "qapi_device_info.h"
#include "qapi_quectel.h"
#include "qapi_status.h"
#include "example_sms.h"


#define QMI_CLNT_WAIT_SIG      (0x1 << 5)
#define QMI_CLNT_TIMER_SIG     (0x1 << 6)
#define MAX_QMI_ATTEMPTS  5
#define QMI_WMS_TIMEOUT_MS 10000
#define SEND_MSG_ADDR "08010"
#define SEND_MSG_DATA "hello"

TX_BYTE_POOL *byte_pool_sms;
#define SMS_BYTE_POOL_SIZE		16*8*1024
UCHAR free_memory_sms[SMS_BYTE_POOL_SIZE];

/* uart rx tx buffer */
static char *rx_buff = NULL; /*!!! should keep this buffer as 4K Bytes */
static char *tx_buff = NULL;

/* uart config para*/
QT_UART_CONF_PARA uart_conf = 
{
	NULL,
	QT_QAPI_UART_PORT_01,
	NULL,
	0,
	NULL,
	0,
	115200
};

/***************************************************************
 *						         FUNC										 *
 ***************************************************************/
/*
@func
	quectel_task_entry
@brief
	Entry function for task.
*/
int quectel_task_entry
(
    void
)
{
    int ret = -1;
    int i;
    qapi_Status_t   status = QAPI_ERROR;
    uint16_t        Service_state = 0;
    uint16_t        index = 0, index1 = 1;
    qapi_QT_SMS_Message_Rcvd_t msg_info;
    qapi_QT_SMS_Message_Content_t send_message;
	char            msg_buffer[128] = {0};
    uint16_t        used = 0;

    qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_SEC, true);

    IOT_DEBUG("QT# quectel_task_entry Start");

    ret = txm_module_object_allocate(&byte_pool_sms, sizeof(TX_BYTE_POOL));
  	if(ret != TX_SUCCESS)
  	{
  		IOT_DEBUG("txm_module_object_allocate [byte_pool_sensor] failed, %d", ret);
    	return ret;
  	}

	ret = tx_byte_pool_create(byte_pool_sms, "Sensor application pool", free_memory_sms, SMS_BYTE_POOL_SIZE);
  	if(ret != TX_SUCCESS)
  	{
  		IOT_DEBUG("tx_byte_pool_create [byte_pool_sensor] failed, %d", ret);
    	return ret;
  	}

  	ret = tx_byte_allocate(byte_pool_sms, (VOID *)&rx_buff, 4*1024, TX_NO_WAIT);
  	if(ret != TX_SUCCESS)
  	{
  		IOT_DEBUG("tx_byte_allocate [rx_buff] failed, %d", ret);
    	return ret;
  	}

  	ret = tx_byte_allocate(byte_pool_sms, (VOID *)&tx_buff, 4*1024, TX_NO_WAIT);
  	if(ret != TX_SUCCESS)
  	{
  		IOT_DEBUG("tx_byte_allocate [tx_buff] failed, %d", ret);
    	return ret;
  	}

	uart_conf.tx_buff = tx_buff;
	uart_conf.rx_buff = rx_buff;
	uart_conf.tx_len = 4096;
	uart_conf.rx_len = 4096;

	/* uart 1 init */
	uart_init(&uart_conf);
	/* start uart 1 receive */
	uart_recv(&uart_conf);
	/* prompt task running */
	qt_uart_dbg(uart_conf.hdlr,"SIM Example...\r\n");

/******Get SMS service status*****/
    //Must get sms service ready first, Service_state=1
	qt_uart_dbg(uart_conf.hdlr,"GET SMS service status...\r\n");

    do
    {
	    qapi_Timer_Sleep(3, QAPI_TIMER_UNIT_SEC, true);
        status = qapi_QT_SMS_Get_Service_Ready_Status(&Service_state);
        qt_uart_dbg(uart_conf.hdlr,"SMS service state %d",Service_state);
    }while( status != QAPI_OK || Service_state != 1 );

    qt_uart_dbg(uart_conf.hdlr,"#SMS service state %d \r\n",Service_state);

 /*******Read msg********/
 	qt_uart_dbg(uart_conf.hdlr,"Read SMS index:%d \r\n",index);

    status = qapi_QT_SMS_Message_Read(index, &msg_info);
    if( status != QAPI_OK )
    {
	    qt_uart_dbg(uart_conf.hdlr,"MSG read error \r\n");
    }
    else
    {
        qt_uart_dbg(uart_conf.hdlr,"SMS status %d, data_len %d, data %s \r\n",msg_info.status, msg_info.sms_info.data_len, msg_info.sms_info.data);
        IOT_DEBUG("SMS data_len %d, data %s", msg_info.sms_info.data_len,msg_info.sms_info.data);
    }

/*******Send msg*********/ 
    qt_uart_dbg(uart_conf.hdlr,"Send SMS...\r\n");
    memset(&send_message, 0, sizeof(qapi_QT_SMS_Message_Content_t));
    memcpy(send_message.address, SEND_MSG_ADDR, strlen(SEND_MSG_ADDR));
    memcpy(send_message.message, SEND_MSG_DATA, strlen(SEND_MSG_DATA));

    status = qapi_QT_SMS_Message_Send(&send_message);
    if( status != QAPI_OK )
    {
        IOT_DEBUG("Send sms fail");
        qt_uart_dbg(uart_conf.hdlr,"Send SMS fail\r\n");
    }
    {
        qt_uart_dbg(uart_conf.hdlr,"Send SMS success \r\n");
    }

/******Delete msg*******/
    qt_uart_dbg(uart_conf.hdlr,"Delete SMS index:%d \r\n",index1);

    status = qapi_QT_SMS_Message_Delete(index1);
    if( status != QAPI_OK )
    {
      IOT_DEBUG("Delete sms fail");
      qt_uart_dbg(uart_conf.hdlr,"Delete SMS fail \r\n");
    }
    
/*************************/
    qt_uart_dbg(uart_conf.hdlr,"SIM Example End \r\n");

    return 0;
}

#endif

