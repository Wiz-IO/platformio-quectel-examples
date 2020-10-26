/******************************************************************************
*@file    example_rtc.c
*@brief   example of rtc operation
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#if defined(__EXAMPLE_RTC__)
#include "txm_module.h"
#include "qapi_diag.h"
#include "qapi_timer.h"
#include "qapi_pmapp_rtc.h"
#include "qapi_uart.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "example_rtc.h"
#include "qapi_diag.h"

/**************************************************************************
*                                 DEFINE
***************************************************************************/
TX_BYTE_POOL *byte_pool_gpio;
#define GPIO_BYTE_POOL_SIZE		16*8*1024
UCHAR free_memory_Uart[GPIO_BYTE_POOL_SIZE];
UCHAR free_memory_rtc[GPIO_BYTE_POOL_SIZE];

TX_BYTE_POOL *byte_pool_rtc_task;
#define RTC_THREAD_PRIORITY   	180
#define RTC_THREAD_STACK_SIZE 	(1024 * 16)
TX_THREAD* RTC_thread_handle; 
char *RTC_thread_stack = NULL;

/* uart rx tx buffer */
static char *rx_buff = NULL; /*!!! should keep this buffer as 4K Bytes */
static char *tx_buff = NULL;

/* uart config para*/
static QT_UART_CONF_PARA uart_conf;


/* conter used to count the total run times for main task */
static unsigned long main_thread_run_couter = 0;



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

/**************************************************************************
*                           FUNCTION DECLARATION
***************************************************************************/

/**************************************************************************
*                                 FUNCTION
***************************************************************************/
/*
@func
  qt_rtc_init
@brief
  Init rtc and set the display mode.
*/
void qt_rtc_init(void)
{
	qapi_Status_t status = QAPI_OK;

	status = qapi_PM_Rtc_Init();
	if(QAPI_OK != status)
	{
		qt_uart_dbg(uart_conf.hdlr,"[rtc] qapi_PM_Rtc_Init failed [%d]", status);
		return;
	}

	status = qapi_PM_Set_Rtc_Display_Mode(QAPI_PM_RTC_24HR_MODE_E);
	if(QAPI_OK != status)
	{
		qt_uart_dbg(uart_conf.hdlr,"[rtc] qapi_PM_Set_Rtc_Display_Mode failed [%d]", status);
		return;
	}
}

/*
@func
  qt_rtc_show_time
@brief
  Show rtc time. 
*/
void qt_rtc_show_time(qapi_PM_Rtc_Julian_Type_t *rtc_date)
{
	qt_uart_dbg(uart_conf.hdlr,"[rtc] %04d/%02d/%02d-%02d:%02d:%02d",
								 rtc_date->year,
								 rtc_date->month,
								 rtc_date->day,
								 rtc_date->hour,
								 rtc_date->minute,
								 rtc_date->second
								 );
}

/*
@func
  qt_rtc_get_time
@brief
  Get rtc time. 
*/
void qt_rtc_get_time(qapi_PM_Rtc_Julian_Type_t *rtc_date)
{
	qapi_Status_t status = QAPI_OK;

	memset(rtc_date, 0, sizeof(qapi_PM_Rtc_Julian_Type_t));
	status = qapi_PM_Rtc_Read_Cmd(rtc_date);
	if(QAPI_OK != status)
	{
		qt_uart_dbg(uart_conf.hdlr,"[rtc] qapi_PM_Rtc_Read_Cmd failed [%d]", status);
		return;
	}

	qt_rtc_show_time(rtc_date);
}

/*
@func
  qt_rtc_rw_alarm_time
@brief
  Set/get rtc alarm time. 
*/
void qt_rtc_rw_alarm_time(qapi_PM_Rtc_Cmd_Type_t cmd,
					 qapi_PM_Rtc_Alarm_Type_t what_alarm,
					 qapi_PM_Rtc_Julian_Type_t *rtc_date)
{
	qapi_Status_t status = QAPI_OK;

	status = qapi_PM_Rtc_Alarm_RW_Cmd(cmd, what_alarm, rtc_date);
	if(QAPI_OK != status)
	{
		qt_uart_dbg(uart_conf.hdlr,"[rtc] qapi_PM_Rtc_Alarm_RW_Cmd failed [%d]", status);
	}

	if(QAPI_PM_RTC_GET_CMD_E == cmd)
	{
		qt_rtc_show_time(rtc_date);
	}
}

int quectel_rtc_task_entry(void)
{
	 qapi_PM_Rtc_Julian_Type_t rtc_date;
	 int ret = -1;
	 qapi_Timer_Sleep(1, QAPI_TIMER_UNIT_SEC, true);


	 ret = txm_module_object_allocate(&byte_pool_gpio, sizeof(TX_BYTE_POOL));
	 if(ret != TX_SUCCESS)
	 {
		 IOT_DEBUG("txm_module_object_allocate [byte_pool_sensor] failed, %d", ret);
		 return ret;
	 }

	 ret = tx_byte_pool_create(byte_pool_gpio, "rtc task pool",free_memory_Uart, GPIO_BYTE_POOL_SIZE);
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

	 /* prompt task running */
	 qt_uart_dbg(uart_conf.hdlr,"[RTC] start task ~");

	 /* initial RTC */
	 qt_rtc_init();

	 while (1)
	 {
		 main_thread_run_couter ++;
		 qt_uart_dbg(uart_conf.hdlr,"[task_create] task run times : %d", main_thread_run_couter);
		 IOT_INFO("[task_create] task run times : %d", main_thread_run_couter);

		 qt_rtc_get_time(&rtc_date);

		 /* sleep 2 seconds */
		 qapi_Timer_Sleep(1, QAPI_TIMER_UNIT_SEC, true);
	 }
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

	ret = txm_module_object_allocate(&byte_pool_rtc_task, sizeof(TX_BYTE_POOL));
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("txm_module_object_allocate [byte_pool_sensor] failed, %d", ret);
		return ret;
	}

	ret = tx_byte_pool_create(byte_pool_rtc_task, "Sensor application pool", free_memory_rtc, GPIO_BYTE_POOL_SIZE);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_pool_create [byte_pool_sensor] failed, %d", ret);
		return ret;
	}

	ret = tx_byte_allocate(byte_pool_rtc_task, (VOID **) &RTC_thread_stack, RTC_THREAD_STACK_SIZE, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_pool_create [byte_pool_sensor] failed, %d", ret);
		return ret;
	}

	ret = txm_module_object_allocate((VOID *)&RTC_thread_handle, sizeof(TX_THREAD));
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("txm_module_object_allocate [byte_pool_sensor] failed, %d", ret);
		return ret;
	}

	/* create a new task : quectel_time_task_entry */
	ret = tx_thread_create(RTC_thread_handle,
						   "TIME Task Thread",
						   quectel_rtc_task_entry,
						   NULL,
						   RTC_thread_stack,
						   RTC_THREAD_STACK_SIZE,
						   RTC_THREAD_PRIORITY,
						   RTC_THREAD_PRIORITY,
						   TX_NO_TIME_SLICE,
						   TX_AUTO_START
						   );
		  
	if(ret != TX_SUCCESS)
	{
		IOT_INFO("[task] Thread creation failed");
	}

return 0;

}

#endif /*__EXAMPLE_RTC__*/

