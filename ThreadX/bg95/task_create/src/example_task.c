/******************************************************************************
*@file    example_task.c
*@brief   example of new task creation
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#if defined(__EXAMPLE_TASK_CREATE__)
#include "txm_module.h"
#include "qapi_diag.h"
#include "qapi_timer.h"
#include "qapi_uart.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "example_task.h"

/**************************************************************************
*                                 DEFINE
***************************************************************************/
#define QT_SUB1_THREAD_PRIORITY   	180
#define QT_SUB1_THREAD_STACK_SIZE 	(1024 * 16)

#define QT_SUB2_THREAD_PRIORITY   	180
#define QT_SUB2_THREAD_STACK_SIZE 	(1024 * 16)
/**************************************************************************
*                                 GLOBAL
***************************************************************************/
TX_BYTE_POOL *byte_pool_task;
#define TASK_BYTE_POOL_SIZE		16*8*1024
char free_memory_task[TASK_BYTE_POOL_SIZE];


/* uart rx tx buffer */
static char *rx_buff = NULL; /*!!! should keep this buffer as 4K Bytes */
static char *tx_buff = NULL;

/* uart config para*/
QT_UART_CONF_PARA uart_conf;

/* conter used to count the total run times for main task */
unsigned long main_thread_run_couter = 0;
unsigned long main_thread_run_couter1= 0;
/* conter used to count the total run times for sub1 task */
unsigned long sub1_thread_run_couter = 0;

/* thread handle */
TX_THREAD* qt_sub1_thread_handle; 
char *qt_sub1_thread_stack = NULL;

TX_THREAD* qt_sub2_thread_handle; 
char *qt_sub2_thread_stack = NULL;
/* TX QUEUE handle */
TX_QUEUE *tx_queue_handle = NULL;

/* TX QUEUE buffer */
void *task_comm = NULL;

/**************************************************************************
*                           FUNCTION DECLARATION
***************************************************************************/
void quectel_sub1_task_entry(ULONG para);

void quectel_sub2_task_entry(ULONG para);
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
	uart_conf.hdlr 	  = NULL;
	uart_conf.port_id = port_id;
	uart_conf.tx_buff = tx_buff;
	uart_conf.tx_len  = sizeof(tx_buff);
	uart_conf.rx_buff = rx_buff;
	uart_conf.rx_len  = sizeof(rx_buff);
	uart_conf.baudrate= 115200;

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
/*=========================================================================*/
int quectel_task_entry(void)
{
	int ret = -1;
	UINT status = 0;
    uint32 message_size;
	TASK_COMM qt_main_task_comm;

//	qapi_Timer_Sleep(20, QAPI_TIMER_UNIT_SEC, true);


	/* prompt task running */
	qt_uart_dbg(uart_conf.hdlr,"[task] start task ~");

	ret = txm_module_object_allocate(&byte_pool_task, sizeof(TX_BYTE_POOL));
  	if(ret != TX_SUCCESS)
  	{
  		IOT_DEBUG("[task] txm_module_object_allocate [byte_pool_task] failed, %d", ret);
    	return ret;
  	}

	ret = tx_byte_pool_create(byte_pool_task, "task application pool", free_memory_task, TASK_BYTE_POOL_SIZE);
  	if(ret != TX_SUCCESS)
  	{
  		IOT_DEBUG("[task] tx_byte_pool_create [byte_pool_task] failed, %d", ret);
    	return ret;
  	}

	ret = tx_byte_allocate(byte_pool_task, (VOID *)&rx_buff, 16*1024, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		return ret;
	}

	ret = tx_byte_allocate(byte_pool_task, (VOID *)&tx_buff, 16*1024, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		return ret;
	}
	quectel_dbg_uart_init(QT_QAPI_UART_PORT_01);

	ret = txm_module_object_allocate(&tx_queue_handle, sizeof(TX_QUEUE));
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("[task] txm_module_object_allocate tx_queue_handle failed, %d", ret);
		qt_uart_dbg(uart_conf.hdlr, "[task] txm_module_object_allocate tx_queue_handle failed, %d", ret);
		return ret;
	}

	ret = tx_byte_allocate(byte_pool_task, (void **)&task_comm, QT_Q_MAX_INFO_NUM * sizeof(TASK_COMM), TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("[task] tx_byte_allocate task_comm failed, %d", ret);
		qt_uart_dbg(uart_conf.hdlr,"[task] tx_byte_allocate task_comm failed");
		return ret;
	}

	message_size = sizeof(TASK_COMM)/sizeof(uint32);

	/* create a new queue : q_task_comm */
	status = tx_queue_create(tx_queue_handle, "q_task_comm", message_size, task_comm, QT_Q_MAX_INFO_NUM * sizeof(TASK_COMM));
	if (TX_SUCCESS != status)
	{
		qt_uart_dbg(uart_conf.hdlr, "tx_queue_create failed with status %d", status);
	}
	else
	{
		qt_uart_dbg(uart_conf.hdlr, "tx_queue_create ok with status %d", status);
	}
	/* create a new task1 */
	if(TX_SUCCESS != txm_module_object_allocate((VOID *)&qt_sub1_thread_handle, sizeof(TX_THREAD))) 
	{
		qt_uart_dbg(uart_conf.hdlr,"[task] txm_module_object_allocate qt_sub1_thread_handle failed");
		IOT_INFO("[task] txm_module_object_allocate qt_sub1_thread_handle failed");
		return - 1;
	}

	ret = tx_byte_allocate(byte_pool_task, (VOID **) &qt_sub1_thread_stack, QT_SUB1_THREAD_STACK_SIZE, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		qt_uart_dbg(uart_conf.hdlr,"[task] tx_byte_allocate qt_sub1_thread_stack failed");
		IOT_INFO("[task] txm_module_object_allocate qt_sub1_thread_stack failed");
		return ret;
	}

	/* create a new task : sub1 */
	ret = tx_thread_create(qt_sub1_thread_handle,
						   "Quectel Main Task Thread",
						   quectel_sub1_task_entry,
						   NULL,
						   qt_sub1_thread_stack,
						   QT_SUB1_THREAD_STACK_SIZE,
						   QT_SUB1_THREAD_PRIORITY,
						   QT_SUB1_THREAD_PRIORITY,
						   TX_NO_TIME_SLICE,
						   TX_AUTO_START
						   );
	      
	if(ret != TX_SUCCESS)
	{
		qt_uart_dbg(uart_conf.hdlr,"[task] : Thread creation failed");
		IOT_INFO("[task] Thread creation failed");
	}

/* create a new task2 */
	if(TX_SUCCESS != txm_module_object_allocate((VOID *)&qt_sub2_thread_handle, sizeof(TX_THREAD))) 
	{
		qt_uart_dbg(uart_conf.hdlr,"[task] txm_module_object_allocate qt_sub1_thread_handle failed");
		IOT_INFO("[task] txm_module_object_allocate qt_sub1_thread_handle failed");
		return - 1;
	}

	ret = tx_byte_allocate(byte_pool_task, (VOID **) &qt_sub2_thread_stack, QT_SUB2_THREAD_STACK_SIZE, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		qt_uart_dbg(uart_conf.hdlr,"[task] tx_byte_allocate qt_sub1_thread_stack failed");
		IOT_INFO("[task] txm_module_object_allocate qt_sub1_thread_stack failed");
		return ret;
	}

	/* create a new task : sub2 */
	ret = tx_thread_create(qt_sub2_thread_handle,
						   "Quectel Main Task Thread",
						   quectel_sub2_task_entry,
						   NULL,
						   qt_sub2_thread_stack,
						   QT_SUB2_THREAD_STACK_SIZE,
						   QT_SUB2_THREAD_PRIORITY,
						   QT_SUB2_THREAD_PRIORITY,
						   TX_NO_TIME_SLICE,
						   TX_AUTO_START
						   );
	      
	if(ret != TX_SUCCESS)
	{
		qt_uart_dbg(uart_conf.hdlr,"[task] : Thread creation failed");
		IOT_INFO("[task] Thread creation failed");
	}

	while (1)
	{
		memset(&qt_main_task_comm, 0, sizeof(qt_main_task_comm));

		/* rec data from main task by queue */
		status = tx_queue_receive(tx_queue_handle, &qt_main_task_comm, TX_WAIT_FOREVER);

		if(TX_SUCCESS != status)
		{
			IOT_INFO("[task] tx_queue_receive failed with status %d", status);
			qt_uart_dbg(uart_conf.hdlr, "[task] tx_queue_receive failed with status %d", status);
			sub1_thread_run_couter = 0;
		}
		else
		{
			IOT_INFO("[task]tx_queue_receive ok with status %d", status);
		   // qt_uart_dbg(uart_conf.hdlr, "[task]tx_queue_receive ok with status %d", status);
			sub1_thread_run_couter =  qt_main_task_comm.dat;
		}
		qt_uart_dbg(uart_conf.hdlr,"[uart 1] task run times : %lu,%d,%s,%s", qt_main_task_comm.dat,qt_main_task_comm.msg_id,qt_main_task_comm.buffer,qt_main_task_comm.name);
	}

	return 0;
}

/*
@func
  quectel_sub1_task_entry
@brief
  A sub task will create by Quectel task entry. 
*/
void quectel_sub1_task_entry(ULONG para)
{
  UINT status = 0;
  TASK_COMM qt_sub1_task_comm;
  qt_uart_dbg(uart_conf.hdlr,"[task] start sub1 task ~");

  while (1)
  {
	main_thread_run_couter ++;
	IOT_INFO("[task] task run times : %d", main_thread_run_couter);
	qt_uart_dbg(uart_conf.hdlr,"sub1 task	main_thread_run_couter1 %lu",main_thread_run_couter);
	qt_sub1_task_comm.msg_id=1;
	qt_sub1_task_comm.dat=main_thread_run_couter;
	strcpy(qt_sub1_task_comm.name,"Task1");
	strcpy(qt_sub1_task_comm.buffer,"Task1 send Successful");

	/* send data to sub1 task by queue */
	status = tx_queue_send(tx_queue_handle, &qt_sub1_task_comm, TX_WAIT_FOREVER);
	if (TX_SUCCESS != status)
	{
		IOT_INFO("[task] tx_queue_send failed with status %d", status);
		qt_uart_dbg(uart_conf.hdlr, "[task] tx_queue_send failed with status %d", status);
	}
	else
	{
		IOT_INFO("tx_queue_send ok with status %d", status);
		qt_uart_dbg(uart_conf.hdlr, "tx_queue_send1 ok with status %d", status);
	}
	qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);
  }
}

/*
@func
  quectel_sub2_task_entry
@brief
  A sub task will create by Quectel task entry. 
*/
void quectel_sub2_task_entry(ULONG para)
{
	UINT status = 0;
	TASK_COMM qt_sub2_task_comm;

	qt_uart_dbg(uart_conf.hdlr,"[task_create] start sub2 task ~");

	while (1)
	{
		main_thread_run_couter1 ++; 
		qt_uart_dbg(uart_conf.hdlr,"sub2 task  main_thread_run_couter1 %lu",main_thread_run_couter1);
		qt_sub2_task_comm.msg_id=2;
		qt_sub2_task_comm.dat=main_thread_run_couter1;
		strcpy(qt_sub2_task_comm.name,"Task2");
		strcpy(qt_sub2_task_comm.buffer,"Task2 send Successful");
		/* send data to sub1 task by queue */
		status = tx_queue_send(tx_queue_handle, &qt_sub2_task_comm,TX_WAIT_FOREVER);
		if (TX_SUCCESS != status)
		{
			qt_uart_dbg(uart_conf.hdlr, "[task_create 2] tx_queue_send2 failed with status %d", status);
		} 
		qapi_Timer_Sleep(3, QAPI_TIMER_UNIT_SEC, true);
	}
}

#endif /*__EXAMPLE_TASK_CREATE__*/


