/******************************************************************************
*@file    example_sim_info.c
*@brief   example of get sim info
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/

#if defined(__EXAMPLE_SIM_INFO__)
#include "txm_module.h"
#include "qapi_diag.h"
#include "qapi_timer.h"
#include "qapi_uart.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "example_sim_info.h"
#include "qapi_device_info.h"
#include "qapi_quectel.h"


TX_BYTE_POOL *byte_pool_sim_info;
#define SIM_INFO_BYTE_POOL_SIZE		16*8*1024
UCHAR free_memory_sim_info[SIM_INFO_BYTE_POOL_SIZE];


#define MSISDN_LEN 20
#define ICCID_LEN  40
#define IMSI_LEN   20

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
    char SIM_STATE[20] = {0};
    char ICCID[ICCID_LEN] = {0};
    char IMSI[IMSI_LEN] = {0};
    char MSISDN[MSISDN_LEN] = {0};
    qapi_Status_t status = QAPI_ERROR;
    /* wait 10sec for device startup */
	qapi_Timer_Sleep(5, QAPI_TIMER_UNIT_SEC, true);

	IOT_DEBUG("QT# quectel_task_entry Start");

    ret = txm_module_object_allocate(&byte_pool_sim_info, sizeof(TX_BYTE_POOL));
  	if(ret != TX_SUCCESS)
  	{
  		IOT_DEBUG("txm_module_object_allocate [byte_pool_sensor] failed, %d", ret);
    	return ret;
  	}

	ret = tx_byte_pool_create(byte_pool_sim_info, "Sensor application pool", free_memory_sim_info, SIM_INFO_BYTE_POOL_SIZE);
  	if(ret != TX_SUCCESS)
  	{
  		IOT_DEBUG("tx_byte_pool_create [byte_pool_sensor] failed, %d", ret);
    	return ret;
  	}

  	ret = tx_byte_allocate(byte_pool_sim_info, (VOID *)&rx_buff, 4*1024, TX_NO_WAIT);
  	if(ret != TX_SUCCESS)
  	{
  		IOT_DEBUG("tx_byte_allocate [rx_buff] failed, %d", ret);
    	return ret;
  	}

  	ret = tx_byte_allocate(byte_pool_sim_info, (VOID *)&tx_buff, 4*1024, TX_NO_WAIT);
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

    status = qapi_QT_SIM_RDY_Check(SIM_STATE);
    if( status != QAPI_OK )
    {
        qt_uart_dbg(uart_conf.hdlr,"Check SIM RDY error");
        return 0;
    }
    qt_uart_dbg(uart_conf.hdlr,"SIM_STATE %s \r\n",SIM_STATE);
    
    status = qapi_QT_SIM_CCID_Get(ICCID);
    if(status != QAPI_OK )
    {
        qt_uart_dbg(uart_conf.hdlr,"Get ICCID error");
        return 0;
    }
    qt_uart_dbg(uart_conf.hdlr,"ICCID %s \r\n",ICCID);

    status = qapi_QT_SIM_IMSI_Get(IMSI);
    if(status != QAPI_OK )
    {
        qt_uart_dbg(uart_conf.hdlr,"Get IMSI error");
        return 0;
    }
    qt_uart_dbg(uart_conf.hdlr,"IMSI %s \r\n",IMSI);

    qapi_QT_SIM_MSISDN_Get(MSISDN);
    if(status != QAPI_OK )
    {
        qt_uart_dbg(uart_conf.hdlr,"Get MSISDN error");
        return 0;
    }
    qt_uart_dbg(uart_conf.hdlr,"MSISDN %s \r\n",MSISDN);

    qt_uart_dbg(uart_conf.hdlr,"QT# quectel_task_entry End");
    return 0;

}
#endif/*__EXAMPLE_SIM_INFO__*/
