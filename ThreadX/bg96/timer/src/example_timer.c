/******************************************************************************
*@file    example_timer.c
*@brief   example of timer operation
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#if defined(__EXAMPLE_TIMER__)
#include "txm_module.h"
#include "qapi_diag.h"
#include "qapi_timer.h"
#include "qapi_uart.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "example_timer.h"

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

qapi_TIMER_handle_t timer_handle;
qapi_TIMER_define_attr_t timer_def_attr;
qapi_TIMER_set_attr_t timer_set_attr;

/**************************************************************************
*                           FUNCTION DECLARATION
***************************************************************************/

/**************************************************************************
*                                 FUNCTION
***************************************************************************/
void cb_timer(uint32_t data)
{
	static int cnt = 0;
	char buffer[64];
	memset(buffer, 0, 64);
	sprintf(buffer, "[Timer] %d\r\n", cnt);
	cnt++;
	cnt &= 0xffffffff;
	qapi_UART_Transmit(uart_conf.hdlr, buffer, strlen(buffer), NULL);
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
	uart_conf.baudrate= 921600;

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
	qapi_Status_t status = QAPI_OK;
	int ret = -1;

	
	qapi_Timer_Sleep(20, QAPI_TIMER_UNIT_SEC, true);


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


	quectel_dbg_uart_init(QT_UART_PORT_02);

	/* prompt task running */
	qt_uart_dbg(uart_conf.hdlr,"[task_create] start task ~");

	/* prompt task running */
	qt_uart_dbg(uart_conf.hdlr,"[TIMER] start task ~");

	memset(&timer_def_attr, 0, sizeof(timer_def_attr));
	timer_def_attr.cb_type	= QAPI_TIMER_FUNC1_CB_TYPE;
	timer_def_attr.deferrable = false;
	timer_def_attr.sigs_func_ptr = cb_timer;
	timer_def_attr.sigs_mask_data = 0x11;
	status = qapi_Timer_Def(&timer_handle, &timer_def_attr);
	qt_uart_dbg(uart_conf.hdlr,"[TIMER] status[%d]", status);

	memset(&timer_set_attr, 0, sizeof(timer_set_attr));
	timer_set_attr.reload = 100;
	timer_set_attr.time = 10;
	timer_set_attr.unit = QAPI_TIMER_UNIT_MSEC;
	status = qapi_Timer_Set(timer_handle, &timer_set_attr);
	qt_uart_dbg(uart_conf.hdlr,"[TIMER] status[%d]", status);

	while (1)
	{
		main_thread_run_couter ++;
		qt_uart_dbg(uart_conf.hdlr,"[TIMER] task run times : %d", main_thread_run_couter);
		IOT_INFO("[TIMER] task run times : %d", main_thread_run_couter);

		/* sleep 1 seconds */
		qapi_Timer_Sleep(1, QAPI_TIMER_UNIT_SEC, true);
	}
}

#endif /*__EXAMPLE_TIMER__*/

