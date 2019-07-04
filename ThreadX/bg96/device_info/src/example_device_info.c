/******************************************************************************
*@file    example_device_info.c
*@brief   example of get device information
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#if defined(__EXAMPLE_DEVICE_INFO__)
#include <stdbool.h>
#include "qapi_uart.h"
#include "qapi_timer.h"
#include "qapi_diag.h"
#include "qapi_device_info.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "example_device_info.h"

/**************************************************************************
*                                 DEFINE
***************************************************************************/

/**************************************************************************
*                                 GLOBAL
***************************************************************************/
TX_BYTE_POOL *byte_pool_dev_info;
#define DEV_INFO_BYTE_POOL_SIZE		16*8*1024
UCHAR free_memory_dev_info[DEV_INFO_BYTE_POOL_SIZE];

/* uart rx tx buffer */
static char *rx_buff = NULL; /*!!! should keep this buffer as 4K Bytes */
static char *tx_buff = NULL;

/* uart config para*/
QT_UART_CONF_PARA uart_conf = 
{
	NULL,
	QT_UART_PORT_02,
	NULL,
	0,
	NULL,
	0,
	115200
};

/* conter used to count the total run times*/
unsigned long task_run_couter = 0;

/* device info */
qapi_Device_Info_t dev_info;

/**************************************************************************
*                           FUNCTION DECLARATION
***************************************************************************/

/**************************************************************************
*                                 FUNCTION
***************************************************************************/
/*
@func
  qt_device_info_data_output
@brief
  Output the device information data
*/
void qt_device_info_data_output(qapi_Device_Info_ID_t dev_info_id)
{
	switch(dev_info.info_type)
	{
		case QAPI_DEVICE_INFO_TYPE_BOOLEAN_E:
			qt_uart_dbg(uart_conf.hdlr, "~[b][%d][%d]", dev_info_id, dev_info.u.valuebool);
		break;

		case QAPI_DEVICE_INFO_TYPE_INTEGER_E:
			qt_uart_dbg(uart_conf.hdlr, "~[i][%d][%d]", dev_info_id, dev_info.u.valueint);
		break;

		case QAPI_DEVICE_INFO_TYPE_BUFFER_E:
			qt_uart_dbg(uart_conf.hdlr, "~[s][%d][%s]", dev_info_id, dev_info.u.valuebuf.buf);
		break;

		default:
			qt_uart_dbg(uart_conf.hdlr, "~[Invalid][%d]", dev_info_id);
		break;
	}
}

/*
@func
  qt_get_defice_info_item
@brief
  get the device information data
*/
void qt_get_defice_info_item(qapi_Device_Info_ID_t dev_info_id)
{
	qapi_Status_t status = 0;

	memset(&dev_info, 0, sizeof(dev_info));
	status = qapi_Device_Info_Get(dev_info_id, &dev_info);
	if(status != QAPI_OK)
	{
		qt_uart_dbg(uart_conf.hdlr, "! Get %d Fail, status %d", dev_info_id, status);
	}
	else
	{
		qt_device_info_data_output(dev_info_id);
	}
}

/*
@func
  qt_get_defice_info_all
@brief
  get the device ALL information data
*/
void qt_get_defice_info_all(void)
{
	qapi_Device_Info_ID_t dev_info_id;

	for(dev_info_id = QAPI_DEVICE_INFO_BUILD_ID_E;
		dev_info_id <= QAPI_DEVICE_INFO_SIM_STATE_E;
		dev_info_id ++)
	{
		qt_get_defice_info_item(dev_info_id);
		qapi_Timer_Sleep(200, QAPI_TIMER_UNIT_MSEC, true);
	}
}

/*
@func
  qt_init_device_info
@brief
  initial the device informaiton
*/
void qt_init_device_info(void)
{
	qapi_Status_t status = 0;

	status = qapi_Device_Info_Init();
	if(status != QAPI_OK)
	{
		qt_uart_dbg(uart_conf.hdlr, "~ qapi_Device_Info_Init fail [%d]", status);
	}
	else
	{
		qt_uart_dbg(uart_conf.hdlr, "~ qapi_Device_Info_Init OK [%d]", status);
	}
}

/*
@func
  qt_qapi_callback
@brief
  device info callback
*/
void qt_qapi_callback(const qapi_Device_Info_t *dev_info)
{
	qt_uart_dbg(uart_conf.hdlr, "~ qt_qapi_callback OK [%d]", dev_info->id);

	switch(dev_info->info_type)
	{
		case QAPI_DEVICE_INFO_TYPE_BOOLEAN_E:
			qt_uart_dbg(uart_conf.hdlr, "~[b][%d][%d]", dev_info->id, dev_info->u.valuebool);
		break;

		case QAPI_DEVICE_INFO_TYPE_INTEGER_E:
			qt_uart_dbg(uart_conf.hdlr, "~[i][%d][%d]", dev_info->id, dev_info->u.valueint);
		break;

		case QAPI_DEVICE_INFO_TYPE_BUFFER_E:
			qt_uart_dbg(uart_conf.hdlr, "~[s][%d][%s]", dev_info->id, dev_info->u.valuebuf.buf);
		break;

		default:
			qt_uart_dbg(uart_conf.hdlr, "~[Invalid][%d]", dev_info->id);
		break;
	} 
}

/*
@func
  quectel_task_entry
@brief
  Entry function for task. 
*/
/*=========================================================================*/
int quectel_task_entry(void)
{
	qapi_Status_t status = QAPI_OK;
	int ret = -1;

//	qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_SEC, true);
	IOT_DEBUG("QT# quectel_task_entry Start");

	ret = txm_module_object_allocate(&byte_pool_dev_info, sizeof(TX_BYTE_POOL));
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("txm_module_object_allocate [byte_pool_sensor] failed, %d", ret);
		return ret;
	}

	ret = tx_byte_pool_create(byte_pool_dev_info, "Sensor application pool", free_memory_dev_info, DEV_INFO_BYTE_POOL_SIZE);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_pool_create [byte_pool_sensor] failed, %d", ret);
		return ret;
	}

	ret = tx_byte_allocate(byte_pool_dev_info, (VOID *)&rx_buff, 4*1024, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_allocate [rx_buff] failed, %d", ret);
		return ret;
	}

	ret = tx_byte_allocate(byte_pool_dev_info, (VOID *)&tx_buff, 4*1024, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_allocate [tx_buff] failed, %d", ret);
		return ret;
	}

	uart_conf.tx_buff = tx_buff;
	uart_conf.rx_buff = rx_buff;
	uart_conf.tx_len = 4096;
	uart_conf.rx_len = 4096;

	/* uart 1 init			*/
	uart_init(&uart_conf);
	/* start uart 1 receive */
	uart_recv(&uart_conf);
	/* prompt task running	*/
	qt_uart_dbg(uart_conf.hdlr,"\r\n[dev_info] Dev_Info Example %d", DEV_INFO_BYTE_POOL_SIZE/1024);

	qt_init_device_info();

	while (1)
	{
		qt_get_defice_info_all();

		task_run_couter ++;

	    qt_uart_dbg(uart_conf.hdlr,"[dev_info] task run times : %d", task_run_couter);
		IOT_INFO("[dev_info] task_run_couter %d", task_run_couter);
		/* sleep 2 seconds */
		qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);
	}
}

#endif /*__EXAMPLE_DEVICE_INFO__*/

