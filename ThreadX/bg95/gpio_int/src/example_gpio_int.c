/******************************************************************************
*@file    example_gpio_int.c
*@brief   example for gpio interrupt operation
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#if defined(__EXAMPLE_GPIO_INT__)
#include "qapi_tlmm.h"
#include "qapi_timer.h"
#include "qapi_uart.h"
#include "qapi_diag.h"
#include "qapi_gpioint.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "example_gpio_int.h"
#include "quectel_gpio.h"

/**************************************************************************
*                                 DEFINE
***************************************************************************/

/**************************************************************************
*                                 GLOBAL
***************************************************************************/
TX_BYTE_POOL *byte_pool_gpioint;
#define GPIOINT_BYTE_POOL_SIZE		16*8*1024
UCHAR free_memory_gpioint[GPIOINT_BYTE_POOL_SIZE];

/* uart rx tx buffer */
static char *rx_buff = NULL; /*!!! should keep this buffer as 4K Bytes */
static char *tx_buff = NULL;

/* uart config para*/
static QT_UART_CONF_PARA uart_conf;

/*  !!! This Pin Enumeration Only Applicable BG95-OPEN Project !!!
 */
static GPIO_MAP_TBL gpio_map_tbl[PIN_E_GPIO_MAX] = {
/* PIN NUM,     PIN NAME,    GPIO ID  GPIO FUNC */
	{  4, 		"PCM_CLK",  	24, 	 0},
	{  5, 		"PCM_sync",  	21, 	 0},
	{  6, 		"PCM_IN",  		22, 	 0},
	{  7, 		"PCM_OUT",  	23, 	 0},
	{ 18, 		"W_disable",  	3, 	     0},  
	{ 19, 		"AP_READ",  	2, 	     0},
	{ 27, 		"GNSS_TXD",  	4, 	     0},
	{ 28, 		"GNSS_RXD",  	5, 	     0},
	{ 40, 		"I2C_SCL",  	15, 	 0},
	{ 41, 		"I2C_SDA",  	14, 	 0},
	{ 64, 		"GPIO21",  		12, 	 0},
};

static MODULE_PIN_ENUM test_pin = PIN_E_GPIO_05;
static qapi_Instance_Handle_t pGpioIntHdlr;
static int gpio_int_counter = 0;

/**************************************************************************
*                           FUNCTION DECLARATION
***************************************************************************/


/**************************************************************************
*                                 FUNCTION
***************************************************************************/
void pfnCallback(qapi_GPIOINT_Callback_Data_t data)
{
	//could not print log in this callback
	gpio_int_counter++;
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

	/* uart init */
	uart_init(&uart_conf);

	/* start uart receive */
	uart_recv(&uart_conf);
}

/*
@func
  quectel_task_demo_entry
@brief
  Entry function for task. 
*/
/*=========================================================================*/
int quectel_task_entry(void)
{
	int ret = 0;
	int cnt = 0;
	qapi_Status_t status;
	qbool_t is_pending = 0;

	qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_SEC, true);

	ret = txm_module_object_allocate(&byte_pool_gpioint, sizeof(TX_BYTE_POOL));
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("txm_module_object_allocate [byte_pool_sensor] failed, %d", ret);
		return ret;
	}

	ret = tx_byte_pool_create(byte_pool_gpioint, "Sensor application pool", free_memory_gpioint, GPIOINT_BYTE_POOL_SIZE);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_pool_create [byte_pool_sensor] failed, %d", ret);
		return ret;
	}

	ret = tx_byte_allocate(byte_pool_gpioint, (VOID *)&rx_buff, 4*1024, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_allocate [rx_buff] failed, %d", ret);
		return ret;
	}

	ret = tx_byte_allocate(byte_pool_gpioint, (VOID *)&tx_buff, 4*1024, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_allocate [tx_buff] failed, %d", ret);
		return ret;
	}

	quectel_dbg_uart_init(QAPI_UART_PORT_001_E);

	/* prompt task running	*/
	qt_uart_dbg(uart_conf.hdlr,"\r\n[GPIOINT] GPIOINT Example %d", GPIOINT_BYTE_POOL_SIZE/1024);
	IOT_INFO("[GPIOINT] GPIOINT Example %d", GPIOINT_BYTE_POOL_SIZE/1024);

	status = qapi_GPIOINT_Register_Interrupt(&pGpioIntHdlr,
											 gpio_map_tbl[test_pin].gpio_id,
											 pfnCallback,
											 NULL,
											 QAPI_GPIOINT_TRIGGER_EDGE_RISING_E,
											 QAPI_GPIOINT_PRIO_LOWEST_E,
											 false);
	qt_uart_dbg(uart_conf.hdlr,"[GPIO INT] status[%x]", status);
	IOT_DEBUG("[GPIO INT] status[%x]", status);

	status = qapi_GPIOINT_Is_Interrupt_Pending(&pGpioIntHdlr, gpio_map_tbl[test_pin].gpio_id, &is_pending);
	qt_uart_dbg(uart_conf.hdlr,"[GPIO INT] status[%x] is_pending[%x]", status, is_pending);
	IOT_DEBUG("[GPIO INT] status[%x] is_pending[%x]", status, is_pending);

	while(1)
	{
		qt_uart_dbg(uart_conf.hdlr,"[GPIO INT][loop %d]", ++cnt & 0xffffffff);
		qt_uart_dbg(uart_conf.hdlr,"[GPIO INT][int %d]", gpio_int_counter & 0xffffffff);
		IOT_DEBUG("[GPIO INT][loop %d]", ++cnt & 0xffffffff);
		IOT_DEBUG("[GPIO INT][int %d]", gpio_int_counter & 0xffffffff);
		qapi_Timer_Sleep(1, QAPI_TIMER_UNIT_SEC, true);
	}

	return 0;
}

#endif /*__EXAMPLE_GPIO_INT__*/

