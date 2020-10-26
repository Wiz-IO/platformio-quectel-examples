/******************************************************************************
*@file    example_random.c
*@brief   example of random operation
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2019 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#if defined(__EXAMPLE_RANDOM__)
/**************************************************************************
*                                 INCLUDE
***************************************************************************/
#include "qapi_uart.h"
#include "qapi_timer.h"
#include "qapi_diag.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "example_random.h"
#include "qapi_device_info.h"
#include "qapi_quectel.h"
#include <locale.h>

/**************************************************************************
*                                 GLOBAL
***************************************************************************/
TX_BYTE_POOL *byte_pool_uart;
#define UART_BYTE_POOL_SIZE		10*8*1024
UCHAR free_memory_uart[UART_BYTE_POOL_SIZE];

/* uart1 rx tx buffer */
static char *rx1_buff = NULL;	/*!!! should keep this buffer as 4K Bytes */
static char *tx1_buff = NULL;

/* uart config para*/
static QT_UART_CONF_PARA uart1_conf =
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

/*
@func
  quectel_task_entry
@brief
  Entry function for task. 
*/
int quectel_task_entry(void)
{
	int ret = -1;
	qapi_Status_t status = QAPI_QT_ERR_OK;
	qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_SEC, true);
	uint8 random_arr[20] = {0};
	uint16 len = 20,i;
	uint32 cmd;
	IOT_DEBUG("QT# quectel_task_entry Start");
	setlocale(LC_ALL, "C");

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
	
  	ret = tx_byte_allocate(byte_pool_uart, (VOID *)&rx1_buff, 4*1024, TX_NO_WAIT);
  	if(ret != TX_SUCCESS)
  	{
  		IOT_DEBUG("tx_byte_allocate [rx2_buff] failed, %d", ret);
    	return ret;
  	}

  	ret = tx_byte_allocate(byte_pool_uart, (VOID *)&tx1_buff, 4*1024, TX_NO_WAIT);
  	if(ret != TX_SUCCESS)
  	{
  		IOT_DEBUG("tx_byte_allocate [tx2_buff] failed, %d", ret);
    	return ret;
  	}

	uart1_conf.tx_buff = tx1_buff;
	uart1_conf.rx_buff = rx1_buff;
	uart1_conf.tx_len = 4096;
	uart1_conf.rx_len = 4096;

	/* uart 1 init 			*/
	uart_init(&uart1_conf);
	/* start uart 1 receive */
	uart_recv(&uart1_conf);

	qt_uart_dbg(uart1_conf.hdlr,"\r\n=== GET RANDOM EXAMPLE START ===\r\n");
	memset(random_arr,0,len);
	for(i = 0;i < len;i ++)
	{
		status = qapi_QT_Random_Data_Get(len,random_arr);
		if(QAPI_QT_ERR_OK != status)
		{
			qt_uart_dbg(uart1_conf.hdlr,"Get Random is error,status is %d\r\n",status);
		}
		qt_uart_dbg(uart1_conf.hdlr,"random[%d] is %d",i,random_arr[i]);
	}
	qt_uart_dbg(uart1_conf.hdlr,"\r\n=== GET RANDOM EXAMPLE END ===\r\n");
	return 0;
}

#endif /*__EXAMPLE_RANDOM__*/

