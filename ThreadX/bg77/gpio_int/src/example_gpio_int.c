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
#include "quectel_project_switch.h"

/**************************************************************************
*                                 DEFINE
***************************************************************************/

/**************************************************************************
*                                 GLOBAL
***************************************************************************/
TX_BYTE_POOL *byte_pool_gpioint;
#define GPIOINT_BYTE_POOL_SIZE		16*8*1024
UCHAR free_memory_uart[GPIOINT_BYTE_POOL_SIZE];
UCHAR free_memory_gpioint[GPIOINT_BYTE_POOL_SIZE];

TX_BYTE_POOL *byte_pool_gpio_int_task;
#define GPIO_INT_THREAD_PRIORITY   	180
#define GPIO_INT_THREAD_STACK_SIZE 	(1024 * 16)
TX_THREAD* GPIO_INT_thread_handle; 
char *GPIO_INT_thread_stack = NULL;

/* uart rx tx buffer */
static char *rx_buff = NULL; /*!!! should keep this buffer as 4K Bytes */
static char *tx_buff = NULL;

/* uart config para*/
static QT_UART_CONF_PARA uart_conf;

/*  !!! This Pin Enumeration Only Applicable BG95-OPEN Project !!!
 */
#if defined(_QUECTEL_PROJECT_BG95) 
static GPIO_MAP_TBL gpio_map_tbl[PIN_E_GPIO_MAX] = {
/* PIN NUM,     PIN NAME,    GPIO ID  GPIO FUNC */
	{  4, 		"GPIO_01",  	24, 	 0},
	{  5, 		"GPIO_02",  	21, 	 0},
	{  6, 		"GPIO_03",  	22, 	 0},
	{  7, 		"GPIO_04",  	23, 	 0},
	{ 18, 		"GPIO_05",  	3, 	     0},  
	{ 19, 		"GPIO_06",  	2, 	     0},
	{ 22, 		"GPIO_07",  	1, 	     0},
	{ 23, 		"GPIO_08",  	0, 	     0},
	{ 25, 		"GPIO_09",  	6, 	     0},
	{ 26, 		"GPIO_10",  	7, 	     0},
	{ 27, 		"GPIO_11",  	4, 	     0},
	{ 28, 		"GPIO_12",  	5, 	     0},
	{ 40, 		"GPIO_13",  	15, 	 0},
	{ 41, 		"GPIO_14",  	14, 	 0},
	{ 64, 		"GPIO_15",  	12, 	 0},
	{ 65, 		"GPIO_16",  	13, 	 0},
	{ 66, 		"GPIO_17",  	50, 	 0},
	{ 85, 		"GPIO_18",  	52, 	 0},
	{ 87, 		"GPIO_20",  	40, 	 0},
	{ 88, 		"GPIO_21",  	41, 	 0},

};
#elif defined(_QUECTEL_PROJECT_BG77)
static GPIO_MAP_TBL gpio_map_tbl[PIN_E_GPIO_MAX] = {
/* PIN NUM,     PIN NAME,    GPIO ID  GPIO FUNC */
	{  1, 		"GPIO_01",  	14, 	 0},
	{  2, 		"GPIO_02",  	22, 	 0},
	{  3, 		"GPIO_03",  	24, 	 0},
	{  4, 		"GPIO_04",  	13, 	 0},
	{  5, 		"GPIO_05",  	2, 	     0}, 
	{  8, 		"GPIO_06",  	5, 	     0},
	{  9, 		"GPIO_07",  	7, 	     0},
	{ 33, 		"GPIO_08",  	52, 	 0},
	{ 34, 		"GPIO_09",  	23, 	 0},
	{ 35, 		"GPIO_10",  	21, 	 0},
	{ 36, 		"GPIO_11",  	12, 	 0},
	{ 37, 		"GPIO_12",  	3, 	     0},
	{ 40, 		"GPIO_13",  	4, 	 	 0},
	{ 41, 		"GPIO_14",  	27, 	 0},
	{ 48, 		"GPIO_15",  	33, 	 0},  
	{ 49, 		"GPIO_16",  	34, 	 0},
	{ 50, 		"GPIO_17",  	36, 	 0},
	{ 51, 		"GPIO_18",  	37, 	 0},
	{ 57, 		"GPIO_19",  	15, 	 0},
	{ 60, 		"GPIO_20",  	0, 	 	 0},
	{ 61, 		"GPIO_21",  	1, 	 	 0},
	{ 63, 		"GPIO_22",  	6, 	 	 0},
	{ 67, 		"GPIO_23",  	31, 	 0}, 
	{ 68, 		"GPIO_24",  	38, 	 0}, 
	{ 69, 		"GPIO_25",  	51, 	 0},
	{ 70, 		"GPIO_26",  	35, 	 0},
	{ 71, 		"GPIO_27",  	42, 	 0},
	{ 77, 		"GPIO_28",  	28, 	 0},
	{ 80, 		"GPIO_29",  	32, 	 0},
	{ 81, 		"GPIO_30",  	40, 	 0},
	{ 82, 		"GPIO_31",  	47, 	 0},
	{ 90,		"GPIO_32",		45, 	 0},
	{ 91,		"GPIO_33",		46, 	 0},
	{ 93,		"GPIO_34",		50, 	 0},	
};
#elif defined(_QUECTEL_PROJECT_BG600LM3)
GPIO_MAP_TBL gpio_map_tbl[PIN_E_GPIO_MAX] = {
/* PIN NUM,     PIN NAME,    GPIO ID  GPIO FUNC */
	{ 9,		"GPIO_01",		4, 	     0},
	{ 10,		"GPIO_02",		5, 	     0}, 
	{ 11,		"GPIO_03",		6, 	     0}, 
	{ 23,		"GPIO_04",		13, 	 0}, 
	{ 29,		"GPIO_05",		0,		 0},
	{ 30,		"GPIO_06",		1, 	     0},
	{ 54,		"GPIO_07",		14, 	 0},	
	{ 57,		"GPIO_08",		2, 	     0},
	{ 61,		"GPIO_09",		21, 	 0},
	{ 62,		"GPIO_10",		22, 	 0},
};
#endif

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
int quectel_gpio_int_task_entry(void)
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

	ret = tx_byte_pool_create(byte_pool_gpioint, "gpioint task pool", free_memory_uart, GPIOINT_BYTE_POOL_SIZE);
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

/*
@func
  quectel_task_demo_entry
@brief
  Entry function for task. 
*/
/*=========================================================================*/
int quectel_task_entry(void)
{
	int ret = -1;

	ret = txm_module_object_allocate(&byte_pool_gpio_int_task, sizeof(TX_BYTE_POOL));
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("txm_module_object_allocate [byte_pool_sensor] failed, %d", ret);
		return ret;
	}

	ret = tx_byte_pool_create(byte_pool_gpio_int_task, "Sensor application pool", free_memory_gpioint, GPIOINT_BYTE_POOL_SIZE);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_pool_create [byte_pool_sensor] failed, %d", ret);
		return ret;
	}

	ret = tx_byte_allocate(byte_pool_gpio_int_task, (VOID *)&GPIO_INT_thread_stack, GPIO_INT_THREAD_STACK_SIZE, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_allocate [rx_buff] failed, %d", ret);
		return ret;
	}

	if(TX_SUCCESS != txm_module_object_allocate((VOID *)&GPIO_INT_thread_handle, sizeof(TX_THREAD))) 
	{
		IOT_INFO("[task] txm_module_object_allocate qt_sub1_thread_handle failed");
		return - 1;
	}

	/* create a new task : quectel_gpio_int_task_entry */
	ret = tx_thread_create(GPIO_INT_thread_handle,
						   "GPIO INTTask Thread",
						   quectel_gpio_int_task_entry,
						   NULL,
						   GPIO_INT_thread_stack,
						   GPIO_INT_THREAD_STACK_SIZE,
						   GPIO_INT_THREAD_PRIORITY,
						   GPIO_INT_THREAD_PRIORITY,
						   TX_NO_TIME_SLICE,
						   TX_AUTO_START
						   );
		  
	if(ret != TX_SUCCESS)
	{
		IOT_INFO("[task] Thread creation failed");
	}

	return 0;

}


#endif /*__EXAMPLE_GPIO_INT__*/

