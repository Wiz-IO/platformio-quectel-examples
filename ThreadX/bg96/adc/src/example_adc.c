/******************************************************************************
*@file    example_adc.c
*@brief   example of adc
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#if defined(__EXAMPLE_ADC__)
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
#include "qapi_adc.h"

#include "qapi_uart.h"
#include "qapi_timer.h"
#include "qapi_diag.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "example_adc.h"

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
qapi_Status_t adc_open_handle(void)
{
	qapi_Status_t status = QAPI_ERROR;
    status = qapi_ADC_Open(&adc_handle, 0);
    if (QAPI_OK != status) 
    {
        qt_uart_dbg(uart_conf.hdlr,"Get ADC Handle ERROR!\r\n");
    }
    return status;
}

/*
@func
	adc_get_properties
@brief
	Get properties of the ADC 
*/
qapi_Status_t adc_get_properties(const char *Channel_Name_Ptr,
				        qapi_Adc_Input_Properties_Type_t *Properties_Ptr
				        )
{
    qapi_Status_t status = QAPI_ERROR;
    uint32_t Channel_Name_Size = strlen(Channel_Name_Ptr) + 1;

    status = qapi_ADC_Get_Input_Properties(adc_handle, Channel_Name_Ptr, Channel_Name_Size, Properties_Ptr);
    if (QAPI_OK != status)
    {   
        qt_uart_dbg(uart_conf.hdlr,"ADC Get Channel-%s Properties ERROR!\r\n", Channel_Name_Ptr);
    }
    return status;
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
    qapi_ADC_Read_Result_t result;
	qapi_Status_t status = QAPI_ERROR;

    const char *Channel_Name_ADC0 = ADC_INPUT_ADC0;
    const char *Channel_Name_ADC1 = ADC_INPUT_ADC1;
    
    qapi_Adc_Input_Properties_Type_t Properties_ADC0;
    qapi_Adc_Input_Properties_Type_t Properties_ADC1;

	/* wait 10sec for device startup */
	qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_SEC, true);
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

	/* uart 1 init */
	uart_init(&uart_conf);
	/* start uart 1 receive */
	uart_recv(&uart_conf);
	/* prompt task running */
	qt_uart_dbg(uart_conf.hdlr,"ADC Example...\r\n");


    /*get an adc handle*/
    status = adc_open_handle();
    if(status != QAPI_OK)
    {
        IOT_DEBUG("Get ADC Handle ERROR!");
        return status;
    }

    /*get the adc channel configuration*/
    status = adc_get_properties(Channel_Name_ADC0, &Properties_ADC0);
    if(status != QAPI_OK)
    {
        IOT_DEBUG("Get ADC channel-%s Configuration ERROR!", Channel_Name_ADC0);
        return status;
    }

    status = adc_get_properties(Channel_Name_ADC1, &Properties_ADC1);
    if(status != QAPI_OK)
    {
        IOT_DEBUG("Get ADC channel-%s Configuration ERROR!", Channel_Name_ADC1);
        return status;
    }
    
    while(1)
    {
        /*read channel ADC0*/
        memset(&result, 0, sizeof(result));
        status = qapi_ADC_Read_Channel(adc_handle,  &Properties_ADC0, &result);
        if(QAPI_OK == status)
        {
            if(ADC_RESULT_VALID == result.eStatus)
            {  
                qt_uart_dbg(uart_conf.hdlr,"Input %s  Voltage: %d mV~\r\n",
                            Channel_Name_ADC0,
                            result.nMicrovolts/1000);
                IOT_DEBUG("Input %s  Voltage: %d mV~",
                            Channel_Name_ADC1,
                            result.nMicrovolts/1000);
            }
        }
        else
        {
            qt_uart_dbg(uart_conf.hdlr,"Read ADC %s Fail!\r\n", Channel_Name_ADC0);
            IOT_DEBUG("Read ADC %s Fail!", Channel_Name_ADC0);
        }

        /*read channel ADC1*/
        memset(&result, 0, sizeof(result));
        status = qapi_ADC_Read_Channel(adc_handle,  &Properties_ADC1, &result);
        if(QAPI_OK == status)
        {
            if(ADC_RESULT_VALID == result.eStatus)
            {  
                qt_uart_dbg(uart_conf.hdlr,"Input %s  Voltage: %d mV~\r\n",
                            Channel_Name_ADC1,
                            result.nMicrovolts/1000);
                IOT_DEBUG("Input %s  Voltage: %d mV~",
                            Channel_Name_ADC1,
                            result.nMicrovolts/1000);
            }            
        }
        else
        {
            qt_uart_dbg(uart_conf.hdlr, "Read ADC %s ERROR!\r\n", Channel_Name_ADC1);
            IOT_DEBUG("Read ADC %s ERROR!", Channel_Name_ADC0);
        }
	    /* wait 5sec for reading again */
	    qapi_Timer_Sleep(5, QAPI_TIMER_UNIT_SEC, true);
    }

    status = qapi_ADC_Close(adc_handle, false);
    if(QAPI_OK != status)
    {
        qt_uart_dbg(uart_conf.hdlr, "Free ADC Handle ERROR!\r\n");
        IOT_DEBUG("Free ADC Handle ERROR!");
    }

    return 0;
}

#endif/*end of __EXAMPLE_ADC__*/

