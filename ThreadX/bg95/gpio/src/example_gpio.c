/******************************************************************************
*@file    example_gpio.c
*@brief   example for gpio operation
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#if defined(__EXAMPLE_GPIO__)
#include "qapi_tlmm.h"
#include "qapi_timer.h"
#include "qapi_diag.h"
#include "qapi_uart.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "example_gpio.h"
#include "quectel_gpio.h"

/**************************************************************************
*                                 GLOBAL
***************************************************************************/
TX_BYTE_POOL *byte_pool_gpio;
#define GPIO_BYTE_POOL_SIZE		16*8*1024
UCHAR free_memory_gpio[GPIO_BYTE_POOL_SIZE];

/* uart rx tx buffer */
static char *rx_buff = NULL; /*!!! should keep this buffer as 4K Bytes */
static char *tx_buff = NULL;

/* uart config para*/
static QT_UART_CONF_PARA uart_conf;


/*  !!! This Pin Enumeration Only Applicable BG95-OPEN Project !!!
 */
GPIO_MAP_TBL gpio_map_tbl[PIN_E_GPIO_MAX] = {
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

/* gpio id table */
qapi_GPIO_ID_t gpio_id_tbl[PIN_E_GPIO_MAX];

/* gpio tlmm config table */
qapi_TLMM_Config_t tlmm_config[PIN_E_GPIO_MAX];
	
/* Modify this pin num which you want to test */
MODULE_PIN_ENUM  g_test_pin_num = PIN_E_GPIO_01;

/**************************************************************************
*                           FUNCTION DECLARATION
***************************************************************************/


/**************************************************************************
*                                 FUNCTION
***************************************************************************/
/*
@func
  gpio_config
@brief
  [in]  m_pin
  		MODULE_PIN_ENUM type; the GPIO pin which customer want used for operation;
  [in]  gpio_dir
  		qapi_GPIO_Direction_t type; GPIO pin direction.
  [in]  gpio_pull
  		qapi_GPIO_Pull_t type; GPIO pin pull type.
  [in]  gpio_drive
  		qapi_GPIO_Drive_t type; GPIO pin drive strength. 
*/
void gpio_config(MODULE_PIN_ENUM m_pin,
				 qapi_GPIO_Direction_t gpio_dir,
				 qapi_GPIO_Pull_t gpio_pull,
				 qapi_GPIO_Drive_t gpio_drive
				 )
{
	qapi_Status_t status = QAPI_OK;

	tlmm_config[m_pin].pin   = gpio_map_tbl[m_pin].gpio_id;
	tlmm_config[m_pin].func  = gpio_map_tbl[m_pin].gpio_func;
	tlmm_config[m_pin].dir   = gpio_dir;
	tlmm_config[m_pin].pull  = gpio_pull;
	tlmm_config[m_pin].drive = gpio_drive;

	// the default here
	status = qapi_TLMM_Get_Gpio_ID(&tlmm_config[m_pin], &gpio_id_tbl[m_pin]);
	IOT_DEBUG("[GPIO] gpio_id[%d] status = %d", gpio_map_tbl[m_pin].gpio_id, status);
	if (status == QAPI_OK)
	{
		status = qapi_TLMM_Config_Gpio(gpio_id_tbl[m_pin], &tlmm_config[m_pin]);
		IOT_DEBUG("[GPIO] gpio_id[%d] status = %d", gpio_map_tbl[m_pin].gpio_id, status);
		if (status != QAPI_OK)
		{
			IOT_DEBUG("[GPIO] gpio_config failed");
		}
	}
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
	qapi_Status_t status = QAPI_OK;
	int ret = -1;

	qapi_GPIO_Value_t level = QAPI_GPIO_HIGH_VALUE_E;

	qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_SEC, true);

	ret = txm_module_object_allocate(&byte_pool_gpio, sizeof(TX_BYTE_POOL));
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("txm_module_object_allocate [byte_pool_sensor] failed, %d", ret);
		return ret;
	}

	ret = tx_byte_pool_create(byte_pool_gpio, "Sensor application pool", free_memory_gpio, GPIO_BYTE_POOL_SIZE);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_pool_create [byte_pool_sensor] failed, %d", ret);
		return ret;
	}

	ret = tx_byte_allocate(byte_pool_gpio, (VOID *)&rx_buff, 4*1024, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_allocate [rx_buff] failed, %d", ret);
		return ret;
	}

	ret = tx_byte_allocate(byte_pool_gpio, (VOID *)&tx_buff, 4*1024, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_allocate [tx_buff] failed, %d", ret);
		return ret;
	}
	
	quectel_dbg_uart_init(QAPI_UART_PORT_001_E);

	/* prompt task running	*/
	qt_uart_dbg(uart_conf.hdlr,"\r\n[GPIO] GPIO Example %d", GPIO_BYTE_POOL_SIZE/1024);

	IOT_INFO("[GPIO] quectel_task_entry ~~~");
	IOT_INFO("[GPIO] ### Test pin is %d, gpio_id is %d ###", g_test_pin_num, gpio_map_tbl[g_test_pin_num].gpio_id);
	qt_uart_dbg(uart_conf.hdlr, "[GPIO] ### Test pin is %d, gpio_id is %d ###", g_test_pin_num, gpio_map_tbl[g_test_pin_num].gpio_id);

#if GPIO_TEST_MODE /* Test output type */
	
	while(1)
	{
		gpio_config(g_test_pin_num, QAPI_GPIO_OUTPUT_E, QAPI_GPIO_NO_PULL_E, QAPI_GPIO_2MA_E);	
		status = qapi_TLMM_Drive_Gpio(gpio_id_tbl[g_test_pin_num], gpio_map_tbl[g_test_pin_num].gpio_id, QAPI_GPIO_LOW_VALUE_E);
		IOT_DEBUG("[GPIO] Set %d QAPI_GPIO_LOW_VALUE_E status = %d", g_test_pin_num, status);
		qt_uart_dbg(uart_conf.hdlr,"[GPIO] Set %d QAPI_GPIO_LOW_VALUE_E status = %d", g_test_pin_num, status);
		qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);

		status = qapi_TLMM_Drive_Gpio(gpio_id_tbl[g_test_pin_num], gpio_map_tbl[g_test_pin_num].gpio_id, QAPI_GPIO_HIGH_VALUE_E);
		IOT_DEBUG("[GPIO] Set %d QAPI_GPIO_HIGH_VALUE_E status = %d", g_test_pin_num, status);
		qt_uart_dbg(uart_conf.hdlr,"[GPIO] Set %d QAPI_GPIO_HIGH_VALUE_E status = %d", g_test_pin_num, status);
		qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);

		status = qapi_TLMM_Release_Gpio_ID(&tlmm_config[g_test_pin_num], gpio_id_tbl[g_test_pin_num]);
		IOT_DEBUG("[GPIO] release %d status = %d", g_test_pin_num, status);
		qt_uart_dbg(uart_conf.hdlr,"[GPIO] release %d status = %d", g_test_pin_num, status);

		g_test_pin_num ++;
		if(g_test_pin_num >= PIN_E_GPIO_MAX)
		{
			g_test_pin_num = PIN_E_GPIO_01;
			qapi_Timer_Sleep(5, QAPI_TIMER_UNIT_SEC, true);
		}
	}

#else /* Test iutput type */

	while (1)
	{
		gpio_config(g_test_pin_num, QAPI_GPIO_INPUT_E, QAPI_GPIO_PULL_UP_E, QAPI_GPIO_2MA_E);	
		qapi_Timer_Sleep(3, QAPI_TIMER_UNIT_SEC, true);

		status = qapi_TLMM_Read_Gpio(gpio_id_tbl[g_test_pin_num], gpio_map_tbl[g_test_pin_num].gpio_id, &level);
		IOT_DEBUG("[GPIO] read %d status = %d, level=%d", g_test_pin_num, status, level);
		qt_uart_dbg(uart_conf.hdlr,"[GPIO] read %d status = %d, level=%d", g_test_pin_num, status, level);

		status = qapi_TLMM_Release_Gpio_ID(&tlmm_config[g_test_pin_num], gpio_id_tbl[g_test_pin_num]);
		IOT_DEBUG("[GPIO] release status = %d", status);
		qt_uart_dbg(uart_conf.hdlr,"[GPIO] release status = %d", status);

		g_test_pin_num ++;
		if(g_test_pin_num >= PIN_E_GPIO_MAX)
		{
			g_test_pin_num = PIN_E_GPIO_01;
			qapi_Timer_Sleep(5, QAPI_TIMER_UNIT_SEC, true);
		}
	}
#endif

	return 0;
}

#endif /*__EXAMPLE_GPIO__*/

