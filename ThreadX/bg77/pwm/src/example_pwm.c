/******************************************************************************
*@file    example_pwm.h
*@brief   example of pwm operation
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2019 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#if defined(__EXAMPLE_PWM__)
/**************************************************************************
*                                 INCLUDE
***************************************************************************/
#include <locale.h>

#include "txm_module.h"
#include "qapi_pwm.h"
#include "qapi_uart.h"
#include "qapi_timer.h"
#include "quectel_uart_apis.h"
#include "quectel_utils.h"
#include "example_pwm.h"
#include "quectel_gpio.h"
#include "qapi_tlmm.h"
#include "qapi_fs_types.h"

/**************************************************************************
*                                 GLOBAL
***************************************************************************/
TX_BYTE_POOL *byte_pool_uart = NULL;
#define UART_BYTE_POOL_SIZE 8*10*1024
UCHAR free_memory_uart[UART_BYTE_POOL_SIZE] = {0};

/*uart rx\tx buffer*/
static char *rx1_buff = NULL;	/*!!! should keep this buffer as 4K Bytes */
static char *tx1_buff = NULL;

/* uart config para*/
static QT_UART_CONF_PARA uart1_conf = {0};
	
/* Modify this pin num which you want to test */


/**************************************************************************
*                                 FUNCTION
***************************************************************************/
/*
@func
  quectel_dbg_uart_init
@brief
  Entry function for task. 
*/
/*=========================================================================*/
void quectel_dbg_uart_init(qapi_UART_Port_Id_e port_id)
{
	uart1_conf.hdlr	  = NULL;
	uart1_conf.port_id = port_id;
	uart1_conf.tx_buff = tx1_buff;
	uart1_conf.tx_len  = sizeof(tx1_buff);
	uart1_conf.rx_buff = rx1_buff;
	uart1_conf.rx_len  = sizeof(rx1_buff);
	uart1_conf.baudrate= 115200;

	/* uart 1 init */
	uart_init(&uart1_conf);

	/* start uart 1 receive */
	uart_recv(&uart1_conf);
}

/*
@func
  quectel_task_entry
@brief
  Entry function for task. 
*/
int quectel_task_entry(void)
{
	int 				ret = -1;
	qapi_Status_t 		status = QAPI_OK;
	qapi_PWM_ID_t 		pwm_id;
	uint32 				pwm_hz;

	//MUST SETTING,TBD
	setlocale(LC_ALL, "C");

	qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_SEC, true);
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
		IOT_DEBUG("tx_byte_allocate [rx_buff] failed, %d", ret);
		return ret;
	}

	ret = tx_byte_allocate(byte_pool_uart, (VOID *)&tx1_buff, 4*1024, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_allocate [tx_buff] failed, %d", ret);
		return ret;
	}

	quectel_dbg_uart_init(QT_QAPI_UART_PORT_01);
	
	qt_uart_dbg(uart1_conf.hdlr,"\r\n=== PWM START ===\r\n");

	status = qapi_PWM_Get_ID(QAPI_PWM_FRAME_0, QAPI_PWM_INSTANCE_0, &pwm_id);
	qt_uart_dbg(uart1_conf.hdlr,"qapi_PWM_Get_ID is succeed,status %d,pwm_id %lu",status,pwm_id);
	if (status == QAPI_OK)
	{
		status = qapi_PWM_Set_Frequency(pwm_id, 960000);
		if (status != QAPI_OK)
		{
			qt_uart_dbg(uart1_conf.hdlr,"qapi_PWM_Set_Frequency is failed,status %d",status);
		}
		qt_uart_dbg(uart1_conf.hdlr,"qapi_PWM_Set_Frequency is succeed,status %d",status);

		status = qapi_PWM_Set_Duty_Cycle(pwm_id, 50);
		if (status != QAPI_OK)
		{
		 	qt_uart_dbg(uart1_conf.hdlr,"qapi_PWM_Set_Duty_Cycle is failed,status %d",status);
		}
		qt_uart_dbg(uart1_conf.hdlr,"qapi_PWM_Set_Duty_Cycle is succeed,status %d",status);

		status = qapi_PWM_Enable(pwm_id, true);
		if (status != QAPI_OK)
		{
	 		qt_uart_dbg(uart1_conf.hdlr,"qapi_PWM_Enable is failed,status %d",status);
		}
		qt_uart_dbg(uart1_conf.hdlr,"qapi_PWM_Enable is succeed,status %d",status);
	}
	qt_uart_dbg(uart1_conf.hdlr,"\r\n=== PWM END ===\r\n");
	return 0;
}

#endif /*__EXAMPLE_PWM__*/
