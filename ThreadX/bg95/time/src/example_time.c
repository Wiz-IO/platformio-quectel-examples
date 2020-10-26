/******************************************************************************
*@file    example_time.c
*@brief   example of real time operation
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#if defined(__EXAMPLE_TIME__)
#include "txm_module.h"
#include "qapi_diag.h"
#include "qapi_timer.h"
#include "qapi_uart.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "example_time.h"

/**************************************************************************
*                                 DEFINE
***************************************************************************/


/**************************************************************************
*                                 GLOBAL
***************************************************************************/
TX_BYTE_POOL *byte_pool_uart;
#define UART_BYTE_POOL_SIZE		10*8*1024
UCHAR free_memory_uart[UART_BYTE_POOL_SIZE];

/* uart rx tx buffer */
static char *rx_buff = NULL;
static char *tx_buff = NULL;


/* uart config para*/
static QT_UART_CONF_PARA uart_conf;

/* conter used to count the total run times for main task */
static unsigned long main_thread_run_couter = 0;

/**************************************************************************
*                           FUNCTION DECLARATION
***************************************************************************/

/**************************************************************************
*                                 FUNCTION
***************************************************************************/

/*
@func
  qt_show_real_time
@brief
  Show real time. 
*/
void qt_show_real_time(qapi_time_get_t *time_info)
{
	qt_uart_dbg(uart_conf.hdlr,
				"[time][ %04d/%02d/%02d %02d:%02d:%02d ]",
				time_info->time_julian.year,
				time_info->time_julian.month,
				time_info->time_julian.day,
				time_info->time_julian.hour,
				time_info->time_julian.minute,
				time_info->time_julian.second
				);
}

/*
@func
  qt_get_real_time
@brief
  Get real time. 
*/
void qt_get_real_time(qapi_time_get_t *time_info)
{
	qapi_Status_t status = QAPI_OK;

	memset(time_info, 0, sizeof(qapi_time_get_t));

	status =  qapi_time_get(QAPI_TIME_JULIAN, time_info);
	if(QAPI_OK != status)
	{
		qt_uart_dbg(uart_conf.hdlr,"[time] qapi_time_get failed [%d]", status);
		return;
	}

	qt_show_real_time(time_info);
}

/*
@func
  quectel_dbg_uart_init
@brief
  Entry function for task. 
*/
/*=========================================================================*/
void quectel_dbg_uart_init(qapi_UART_Port_Id_e port_id)
{
	uart_conf.hdlr 	  = NULL;
	uart_conf.port_id = port_id;
	uart_conf.tx_buff = tx_buff;
	uart_conf.tx_len  = sizeof(tx_buff);
	uart_conf.rx_buff = rx_buff;
	uart_conf.rx_len  = sizeof(rx_buff);
	uart_conf.baudrate= 115200;

	/* uart 1 init */
	uart_init(&uart_conf);

	/* start uart 1 receive */
	uart_recv(&uart_conf);
}


/*
@func
  quectel_task_entry
@brief
  Entry function for task. 
*/
int quectel_task_entry(void)
{
	qapi_time_get_t time_info;
	int ret = -1;

//	qapi_Timer_Sleep(20, QAPI_TIMER_UNIT_SEC, true);
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


	quectel_dbg_uart_init(QT_QAPI_UART_PORT_01);

	/* prompt task running */
	qt_uart_dbg(uart_conf.hdlr,"[TIME] start task ~");


	while (1)
	{
		main_thread_run_couter ++;
		qt_uart_dbg(uart_conf.hdlr,"[task_create] task run times : %d", main_thread_run_couter);
		IOT_INFO("[task_create] task run times : %d", main_thread_run_couter);

		qt_get_real_time(&time_info);

		/* sleep 1 seconds */
		qapi_Timer_Sleep(1, QAPI_TIMER_UNIT_SEC, true);
	}
}

#endif /*__EXAMPLE_TIME__*/

