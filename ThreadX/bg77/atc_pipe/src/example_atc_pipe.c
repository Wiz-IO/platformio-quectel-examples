/******************************************************************************
*@file    example_atc_pipe.c
*@brief   example of atc pipe operation
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#if defined(__EXAMPLE_ATC_PIPE__)
#include "txm_module.h"
#include "qapi_diag.h"
#include "qapi_timer.h"
#include "qapi_uart.h"
#include "qapi_quectel.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "example_atc_pipe.h"

/**************************************************************************
*                                 DEFINE
***************************************************************************/


/**************************************************************************
*                                 GLOBAL
***************************************************************************/
/* uart1 rx tx buffer */
static char tx1_buff[1024];

/* uart config para*/
static QT_UART_CONF_PARA uart1_conf =
{
	NULL,
	QAPI_UART_PORT_001_E,
	tx1_buff,
	sizeof(tx1_buff),
	NULL,
	0,
	115200
};

qapi_QT_AT_Stream_ID_t stream_id_1,stream_id_0;

//qapi_at_stream_id_t stream_id_0, stream_id_1;
static char at_cmd_rsp[3096] = {0};
TX_EVENT_FLAGS_GROUP* atc_pipe_sig_handle;
static qapi_QT_AT_Pipe_Data_t pipe_data[2];

TX_BYTE_POOL *byte_pool_atc_pipe_task;
#define ATC_PIPE_THREAD_PRIORITY   	180
#define ATC_PIPE_THREAD_STACK_SIZE 	(1024 * 16)
TX_THREAD* ATC_PIPE_thread_handle; 
char *ATC_PIPE_thread_stack = NULL;

#define ATC_PIPE_BYTE_POOL_SIZE		10*8*1024
UCHAR free_memory_atc_pipe[ATC_PIPE_BYTE_POOL_SIZE];

/*===========================================================================

                           Static & global Variable Declarations

===========================================================================*/
static void callback_atc_pipe_0(qapi_QT_AT_Pipe_Data_t *data)
{
	ULONG sig_event = 0;

	/* Must consider the URC report firstly */

	/* Normal ATC response handler */
	tx_event_flags_get(atc_pipe_sig_handle, SIG_EVG_ATPIPE_SENDING_E, TX_OR, &sig_event, TX_NO_WAIT);
	if(sig_event & SIG_EVG_ATPIPE_SENDING_E)
	{
		tx_event_flags_set(atc_pipe_sig_handle, ~SIG_EVG_ATPIPE_SENDING_E, TX_AND);
		tx_event_flags_set(atc_pipe_sig_handle, SIG_EVG_ATPIPE_RECING_E, TX_OR);
		memset(at_cmd_rsp, 0, sizeof(at_cmd_rsp));
	}
	
	IOT_DEBUG("[DBG][ATC-6]Current Data: %s, %d",data->data, data->len);
	strcat(at_cmd_rsp, data->data);
	//memset(data->data,0,data->len);
	if(strstr(data->data,"\r\nOK") != NULL)
	{
		tx_event_flags_set(atc_pipe_sig_handle, SIG_EVG_ATPIPE_REC_OK_E, TX_OR);
	}
}

static void callback_atc_pipe_1(qapi_QT_AT_Pipe_Data_t *data)
{
	ULONG sig_event = 0;

	/* Must consider the URC report firstly */

	/* Normal ATC response handler */
	tx_event_flags_get(atc_pipe_sig_handle, SIG_EVG_ATPIPE_SENDING_E, TX_OR, &sig_event, TX_NO_WAIT);
	if(sig_event & SIG_EVG_ATPIPE_SENDING_E)
	{
		tx_event_flags_set(atc_pipe_sig_handle, ~SIG_EVG_ATPIPE_SENDING_E, TX_AND);
		tx_event_flags_set(atc_pipe_sig_handle, SIG_EVG_ATPIPE_RECING_E, TX_OR);
		memset(at_cmd_rsp, 0, sizeof(at_cmd_rsp));
	}
	
	IOT_DEBUG("[DBG][ATC-7]Current Data: %s, %d",data->data, data->len);
	strcat(at_cmd_rsp, data->data);
	//memset(data->data,0,data->len);

	if(strstr(data->data,"\r\nOK") != NULL)
	{
		tx_event_flags_set(atc_pipe_sig_handle, SIG_EVG_ATPIPE_REC_OK_E, TX_OR);
	}
}

int quectel_atc_pipe_task_entry(void)
{
	qapi_Status_t status = QAPI_OK;
	ULONG sig_event = 0;
	qapi_QT_AT_Stream_ID_t stream_id = stream_id_0;
	
	qapi_Timer_Sleep(15, QAPI_TIMER_UNIT_SEC, true);
    txm_module_object_allocate(&atc_pipe_sig_handle, sizeof(TX_EVENT_FLAGS_GROUP));
	/* Create event signal handle and clear signals */
	tx_event_flags_create(atc_pipe_sig_handle, "atc_pipe_sig_handle");
	tx_event_flags_set(atc_pipe_sig_handle, 0, TX_AND);

	/* uart 1 init */
	uart_init(&uart1_conf);
	
	/* prompt task running */
	qt_uart_dbg(uart1_conf.hdlr,"[atc pipe] start task ~");

	/* Open first ATC port */
	status = qapi_QT_Apps_AT_Port_Open(QAPI_AT_PORT_0, &stream_id_0, callback_atc_pipe_0, &pipe_data[0]);
	if(status != QAPI_QT_ERR_OK)
	{
		qt_uart_dbg(uart1_conf.hdlr,"[atc pipe %d] status[0x%x]", stream_id_0,status);
	}
	tx_event_flags_set(atc_pipe_sig_handle, SIG_EVG_ATPIPE_SENDING_E, TX_OR);
	status = qapi_QT_Apps_Send_AT(stream_id_0, "ATI\r\n");
	if(status != QAPI_QT_ERR_OK)
	{
		qt_uart_dbg(uart1_conf.hdlr,"[atc pipe %d] status[0x%x]", stream_id_0,status);
	}
	tx_event_flags_get(atc_pipe_sig_handle, SIG_EVG_ATPIPE_REC_OK_E, TX_OR, &sig_event, TX_WAIT_FOREVER);
	if(sig_event & SIG_EVG_ATPIPE_REC_OK_E)
	{
		qt_uart_dbg(uart1_conf.hdlr,"[atc pipe %d] rsp[%s]", stream_id_0,at_cmd_rsp);
		tx_event_flags_set(atc_pipe_sig_handle, ~SIG_EVG_ATPIPE_REC_OK_E, TX_AND);
		tx_event_flags_set(atc_pipe_sig_handle, SIG_EVG_ATPIPE_STANDBY_E, TX_OR);
	}

    /* Open first ATC port */
 	status = qapi_QT_Apps_AT_Port_Open(QAPI_AT_PORT_1, &stream_id_1, callback_atc_pipe_1, &pipe_data[1]);
	if(status != QAPI_QT_ERR_OK)
	{
		qt_uart_dbg(uart1_conf.hdlr,"[atc pipe %d] status[0x%x]", stream_id_1,status);
	}
	tx_event_flags_set(atc_pipe_sig_handle, SIG_EVG_ATPIPE_SENDING_E, TX_OR);
	status = qapi_QT_Apps_Send_AT(stream_id_1, "ATI\r\n");
	if(status != QAPI_QT_ERR_OK)
	{
		qt_uart_dbg(uart1_conf.hdlr,"[atc pipe %d] status[0x%x]", stream_id_1,status);
	}
	tx_event_flags_get(atc_pipe_sig_handle, SIG_EVG_ATPIPE_REC_OK_E, TX_OR, &sig_event, TX_WAIT_FOREVER);
	if(sig_event & SIG_EVG_ATPIPE_REC_OK_E)
	{
		qt_uart_dbg(uart1_conf.hdlr,"[atc pipe %d] rsp[%s]", stream_id_1,at_cmd_rsp);
		tx_event_flags_set(atc_pipe_sig_handle, ~SIG_EVG_ATPIPE_REC_OK_E, TX_AND);
		tx_event_flags_set(atc_pipe_sig_handle, SIG_EVG_ATPIPE_STANDBY_E, TX_OR);
	}

	while(1)
	{
		tx_event_flags_set(atc_pipe_sig_handle, SIG_EVG_ATPIPE_SENDING_E, TX_OR);
		
		/* loop test */
		stream_id = (stream_id == stream_id_0) ? stream_id_1 : stream_id_0;
					
		status = qapi_QT_Apps_Send_AT(stream_id, "AT+CCLK?\r\n");
		if(status != QAPI_QT_ERR_OK)
		{
			qt_uart_dbg(uart1_conf.hdlr,"[atc pipe] status[0x%x]", status);
		}
		tx_event_flags_get(atc_pipe_sig_handle, SIG_EVG_ATPIPE_REC_OK_E, TX_OR, &sig_event, TX_WAIT_FOREVER);
		if(sig_event & SIG_EVG_ATPIPE_REC_OK_E)
		{
			qt_uart_dbg(uart1_conf.hdlr,"[atc pipe:%d] rsp[%s]", stream_id,at_cmd_rsp);
			tx_event_flags_set(atc_pipe_sig_handle, ~SIG_EVG_ATPIPE_REC_OK_E, TX_AND);
			tx_event_flags_set(atc_pipe_sig_handle, SIG_EVG_ATPIPE_STANDBY_E, TX_OR);
		}

		qapi_Timer_Sleep(5, QAPI_TIMER_UNIT_SEC, true);
	}
}


int quectel_task_entry
(
    void
)
{
	int ret = -1;

	ret = txm_module_object_allocate(&byte_pool_atc_pipe_task, sizeof(TX_BYTE_POOL));
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("txm_module_object_allocate [byte_pool_sensor] failed, %d", ret);
		return ret;
	}

	ret = tx_byte_pool_create(byte_pool_atc_pipe_task, "Sensor application pool", free_memory_atc_pipe, ATC_PIPE_BYTE_POOL_SIZE);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_pool_create [byte_pool_sensor] failed, %d", ret);
		return ret;
	}

	ret = tx_byte_allocate(byte_pool_atc_pipe_task, (VOID *)&ATC_PIPE_thread_stack, ATC_PIPE_THREAD_STACK_SIZE, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_allocate [rx_buff] failed, %d", ret);
		return ret;
	}

	if(TX_SUCCESS != txm_module_object_allocate((VOID *)&ATC_PIPE_thread_handle, sizeof(TX_THREAD))) 
	{
		IOT_INFO("[task] txm_module_object_allocate qt_sub1_thread_handle failed");
		return - 1;
	}

	/* create a new task : quectel_atc_pipe_task_entry */
	ret = tx_thread_create(ATC_PIPE_thread_handle,
						   "ATC_PIPE Task Thread",
						   quectel_atc_pipe_task_entry,
						   NULL,
						   ATC_PIPE_thread_stack,
						   ATC_PIPE_THREAD_STACK_SIZE,
						   ATC_PIPE_THREAD_PRIORITY,
						   ATC_PIPE_THREAD_PRIORITY,
						   TX_NO_TIME_SLICE,
						   TX_AUTO_START
						   );
		  
	if(ret != TX_SUCCESS)
	{
		IOT_INFO("[task] Thread creation failed");
	}

	return 0;

}


#endif /*__EXAMPLE_ATC_PIPE__*/

