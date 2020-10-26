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
#if defined(__EXAMPLE_SAHARA__)
/**************************************************************************
*                                 INCLUDE
***************************************************************************/
#include "qapi_uart.h"
#include "qapi_timer.h"
#include "qapi_diag.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "example_sahara.h"
#include "qapi_device_info.h"
#include "qapi_quectel.h"
#include <locale.h>

/**************************************************************************
*                                 GLOBAL
***************************************************************************/
TX_BYTE_POOL *byte_pool_uart;
#define UART_BYTE_POOL_SIZE		10*8*1024
UCHAR free_memory_sahara[UART_BYTE_POOL_SIZE];


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
static int qt_uart1_init(void)
{
   	int ret = -1;

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
	return ret;
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
    qapi_Status_t status = QAPI_OK;
    qapi_QT_FATAL_ERR_Mode_e mode = QAPI_FATAL_ERR_MAX;

	IOT_DEBUG("QT# quectel_task_entry Start");
	setlocale(LC_ALL, "C");

	ret = txm_module_object_allocate(&byte_pool_uart, sizeof(TX_BYTE_POOL));
  	if(ret != TX_SUCCESS)
  	{
  		IOT_DEBUG("txm_module_object_allocate [byte_pool_sensor] failed, %d", ret);
    	return ret;
  	}

	ret = tx_byte_pool_create(byte_pool_uart, "Sensor application pool", free_memory_sahara, UART_BYTE_POOL_SIZE);
  	if(ret != TX_SUCCESS)
  	{
  		IOT_DEBUG("tx_byte_pool_create [byte_pool_sensor] failed, %d", ret);
    	return ret;
  	}

    qt_uart1_init();

	/* prompt task running 	*/
	qt_uart_dbg(uart1_conf.hdlr,"\r\n[uart 1] SAHARA Example Running...");

    status = qapi_QT_Sahara_Mode_Get(&mode);
    if(status != QAPI_QT_ERR_OK)
    {
       	qt_uart_dbg(uart1_conf.hdlr,"\r\n[uart 1] get sahara mode failed, 0x%x", status);
        return status;
    }
    qt_uart_dbg(uart1_conf.hdlr,"\r\n[uart 1] sahara mode is, %d %s", mode, mode == QAPI_FATAL_ERR_RESET ? "QAPI_FATAL_ERR_RESET" : "QAPI_FATAL_ERR_SAHARA");
    qapi_Timer_Sleep(200, QAPI_TIMER_UNIT_MSEC, true);

    if(mode == QAPI_FATAL_ERR_SAHARA)
    {
        status = qapi_QT_Sahara_Mode_Set(QAPI_FATAL_ERR_RESET);
        if(status != QAPI_QT_ERR_OK)
        {
           	qt_uart_dbg(uart1_conf.hdlr,"\r\n[uart 1] set sahara mode failed, 0x%x.", status);
            qapi_Timer_Sleep(200, QAPI_TIMER_UNIT_MSEC, true);
        }
        else
        {
            qt_uart_dbg(uart1_conf.hdlr,"\r\n[uart 1] set sahara mode OK.");
            qapi_Timer_Sleep(200, QAPI_TIMER_UNIT_MSEC, true);
        }
    }

	while (1)
	{
		task_run_couter ++;

		/* print log to uart */
		qt_uart_dbg(uart1_conf.hdlr,"[uart 1] task run times : %d", task_run_couter);

		IOT_DEBUG("QT# quectel_task_entry [%d]", task_run_couter);

		/* sleep 1 seconds */
		qapi_Timer_Sleep(1, QAPI_TIMER_UNIT_SEC, true);

		//After 30 seconds, force module enter into while loop for crash to check module is reset or enter into dump mode.
		if(task_run_couter >= 30)
		{
            while(1);
		}
	}
}

#endif /*__EXAMPLE_SAHARA__*/

