/******************************************************************************
*@file    example_uart.c
*@brief   example of uart operation
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#if defined(__EXAMPLE_UART__)
/**************************************************************************
*                                 INCLUDE
***************************************************************************/
#include "qapi_uart.h"
#include "qapi_timer.h"
#include "qapi_diag.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "example_uart.h"
#include "qapi_device_info.h"


/**************************************************************************
*                                 GLOBAL
***************************************************************************/
TX_BYTE_POOL *byte_pool_uart;
#define UART_BYTE_POOL_SIZE		10*8*1024
UCHAR free_memory_uart[UART_BYTE_POOL_SIZE];


#ifdef QT_UART_ENABLE_1ST
/* uart1 rx tx buffer */
static char *rx1_buff = NULL;	/*!!! should keep this buffer as 4K Bytes */
static char *tx1_buff = NULL;

/* uart config para*/
static QT_UART_CONF_PARA uart1_conf =
{
	NULL,
	QT_UART_PORT_01,
	NULL,
	0,
	NULL,
	0,
	115200
};
#endif

#ifdef QT_UART_ENABLE_2ND
/* uart2 rx tx buffer */
static char *rx2_buff = NULL; /*!!! should keep this buffer as 4K Bytes */
static char *tx2_buff = NULL;

/* uart config para*/
QT_UART_CONF_PARA uart2_conf = 
{
	NULL,
	QT_UART_PORT_02,
	NULL,
	0,
	NULL,
	0,
	115200
};
#endif

#ifdef QT_UART_ENABLE_3RD
/* uart3 rx tx buffer */
static char *rx3_buff = NULL; /*!!! should keep this buffer as 4K Bytes */
static char *tx3_buff = NULL;

/* uart config para*/
static QT_UART_CONF_PARA uart3_conf =
{
	NULL,
	QT_UART_PORT_03,
	NULL,
	0,
	NULL,
	0,
	115200
};
#endif

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

//	qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_SEC, true);
	IOT_DEBUG("QT# quectel_task_entry Start");

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

#ifdef QT_UART_ENABLE_1ST
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
	/* prompt task running 	*/
	qt_uart_dbg(uart1_conf.hdlr,"\r\n[uart 1] UART Example %d", UART_BYTE_POOL_SIZE/1024);
#endif

#ifdef QT_UART_ENABLE_2ND
  	ret = tx_byte_allocate(byte_pool_uart, (VOID *)&rx2_buff, 4*1024, TX_NO_WAIT);
  	if(ret != TX_SUCCESS)
  	{
  		IOT_DEBUG("tx_byte_allocate [rx2_buff] failed, %d", ret);
    	return ret;
  	}

  	ret = tx_byte_allocate(byte_pool_uart, (VOID *)&tx2_buff, 4*1024, TX_NO_WAIT);
  	if(ret != TX_SUCCESS)
  	{
  		IOT_DEBUG("tx_byte_allocate [tx2_buff] failed, %d", ret);
    	return ret;
  	}

	uart2_conf.tx_buff = tx2_buff;
	uart2_conf.rx_buff = rx2_buff;
	uart2_conf.tx_len = 4096;
	uart2_conf.rx_len = 4096;

	/* uart 2 init 			*/
	uart_init(&uart2_conf);
	/* start uart 2 receive */
	uart_recv(&uart2_conf);
	/* prompt task running 	*/
	qt_uart_dbg(uart2_conf.hdlr,"\r\n[uart 2] UART Example %d", UART_BYTE_POOL_SIZE/1024);
#endif

#ifdef QT_UART_ENABLE_3RD
	ret = tx_byte_allocate(byte_pool_uart, (VOID *)&rx3_buff, 4*1024, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_allocate [rx3_buff] failed, %d", ret);
		return ret;
	}

	ret = tx_byte_allocate(byte_pool_uart, (VOID *)&tx3_buff, 4*1024, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_allocate [tx2_buff] failed, %d", ret);
		return ret;
	}

	uart3_conf.tx_buff = tx3_buff;
	uart3_conf.rx_buff = rx3_buff;
	uart3_conf.tx_len = 4096;
	uart3_conf.rx_len = 4096;

	/* uart 3 init 			*/
	uart_init(&uart3_conf);
	/* start uart 3 receive */
	uart_recv(&uart3_conf);
	/* prompt task running 	*/
	qt_uart_dbg(uart3_conf.hdlr,"\r\n[uart 3] UART Example %d", UART_BYTE_POOL_SIZE/1024);
#endif

	while (1)
	{
		task_run_couter ++;

#ifdef QT_UART_ENABLE_1ST
		/* print log to uart */
		qt_uart_dbg(uart1_conf.hdlr,"[uart 1] task run times : %d", task_run_couter);
#endif

#ifdef QT_UART_ENABLE_2ND
		qt_uart_dbg(uart2_conf.hdlr,"[uart 2] task run times : %d", task_run_couter);
#endif

#ifdef QT_UART_ENABLE_3RD
		qt_uart_dbg(uart3_conf.hdlr,"[uart 3] task run times : %d", task_run_couter);
#endif

		IOT_DEBUG("QT# quectel_task_entry [%d]", task_run_couter);
		/* sleep 1 seconds */
		qapi_Timer_Sleep(1, QAPI_TIMER_UNIT_SEC, true);

		//After 24 hour, reset the module;
		if(task_run_couter >= 86400)
		{
			//qapi_Device_Info_Reset();
		}
	}
}

#endif /*__EXAMPLE_UART__*/

