/******************************************************************************
*@file    quectel_adc.c
*@brief   Use the API(qapi_QT_ADC_Read) to read the values of ADC0 and ADC1
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#if defined(__EXAMPLE_QT_ADC__)
/*===========================================================================
						   Header file
===========================================================================*/
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include "qapi_fs_types.h"
#include "qapi_status.h"
#include "qapi_dss.h"
#include "qapi_adc.h"

#include "qapi_uart.h"
#include "qapi_timer.h"
#include "qapi_diag.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "example_qt_adc.h"
#include "qapi_quectel.h"

/**************************************************************************
*								  GLOBAL
***************************************************************************/
TX_BYTE_POOL *byte_pool_uart;
#define UART_BYTE_POOL_SIZE		10*8*1024
UCHAR free_memory_uart[UART_BYTE_POOL_SIZE];

/* uart rx tx buffer */
static char *rx_buff = NULL;
static char *tx_buff = NULL;

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


/*ADC handle*/
qapi_ADC_Handle_t adc_handle;


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
    const char *Channel_Name_ADC0 = ADC_INPUT_ADC0;
    const char *Channel_Name_ADC1 = ADC_INPUT_ADC1;

    qapi_ADC_Read_Result_t result;
	qapi_Status_t status = QAPI_ERROR;
	int ret = -1;

	/* wait 10sec for device startup */
	qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_MSEC, true);

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

	/* uart  init */
	uart_init(&uart_conf);
	/* start uart  receive */
	uart_recv(&uart_conf);
	qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_MSEC, true);
	
	/* prompt task running */
	qt_uart_dbg(uart_conf.hdlr,"[uart 1] QT ADC Example");

    while(1)
    {
        /*read channel ADC0*/
        memset(&result, 0, sizeof(result));
        status = qapi_QT_ADC_Read(Channel_Name_ADC0, &result);
        if (QAPI_OK == status)
        {
            if(ADC_RESULT_VALID == result.eStatus)
            {  
                qt_uart_dbg(uart_conf.hdlr,"Input %s Voltage: %d mV\n",
                            Channel_Name_ADC0,
                            result.nMicrovolts/1000);
            }
        }

        /*read channel ADC1*/
        memset(&result, 0, sizeof(result));
        status = qapi_QT_ADC_Read(Channel_Name_ADC1, &result);
        if (QAPI_OK == status)
        {
            if(ADC_RESULT_VALID == result.eStatus)
            {  
                qt_uart_dbg(uart_conf.hdlr,"Input %s Voltage: %d mV\n",
                            Channel_Name_ADC1,
                            result.nMicrovolts/1000);
            }
        }

	    /* wait 5sec for reading again */
	    qapi_Timer_Sleep(5, QAPI_TIMER_UNIT_SEC, true);
    }

    return 0;
}
#endif/*end of __EXAMPLE_QT_ADC__*/

