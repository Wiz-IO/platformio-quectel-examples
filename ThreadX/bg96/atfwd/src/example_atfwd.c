/******************************************************************************
*@file    example_atfwd.c
*@brief   example of AT forward service
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#if defined(__EXAMPLE_ATFWD__)
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
#include "qapi_atfwd.h"

#include "qapi_uart.h"
#include "qapi_timer.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "example_atfwd.h"

/**************************************************************************
*								  GLOBAL
***************************************************************************/
TX_BYTE_POOL *byte_pool_uart;
TX_BYTE_POOL *byte_pool_at;

#define UART_BYTE_POOL_SIZE		10*8*1024
UCHAR free_memory_uart[UART_BYTE_POOL_SIZE];
UCHAR free_memory_at[UART_BYTE_POOL_SIZE];


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

static int qexample_val = 0;


TX_BYTE_POOL * byte_pool_qcli;

/**************************************************************************
*                                 FUNCTION
***************************************************************************/
static int qt_atoi(char *str)
{
    int res = 0, i;

    for (i = 0; str[i] != '\0' && str[i] != '.'; ++i)
    {
        res = res*10 + (str[i] - '0');
    }

    return res;
}
static int strncasecmp(const char * s1, const char * s2, size_t n)
{
  unsigned char c1, c2;
  int diff;

  if (n > 0)
  {
    do
    {
      c1 = (unsigned char)(*s1++);
      c2 = (unsigned char)(*s2++);
      if (('A' <= c1) && ('Z' >= c1))
      {
        c1 = c1 - 'A' + 'a';
      }
      if (('A' <= c2) && ('Z' >= c2))
      {
        c2 = c2 - 'A' + 'a';
      }
      diff = c1 - c2;

      if (0 != diff)
      {
        return diff;
      }

      if ('\0' == c1)
      {
        break;
      }
    } while (--n);
  }
  return 0;
}

void atfwd_cmd_handler_cb(boolean is_reg, char *atcmd_name,
                                 uint8* at_fwd_params, uint8 mask,
                                 uint32 at_handle)
{
    char buff[32] = {0};
    int  tmp_val = 0;
    qapi_Status_t ret = QAPI_ERROR;
    
    qt_uart_dbg(uart_conf.hdlr,"atfwd_cmd_handler_cb is called, atcmd_name:[%s] mask:[%d]\n", atcmd_name, mask);
	qt_uart_dbg(uart_conf.hdlr,"atfwd_cmd_handler_cb is called,  is_reg:[%d]\n", is_reg);

	if(is_reg)  //Registration Successful,is_reg return 1 
	{
		if(mask==QUEC_AT_MASK_EMPTY_V01)
		{
			qt_uart_dbg(uart_conf.hdlr,"Atcmd %s is registered\n",atcmd_name);
			return;

		}
    	if( !strncasecmp(atcmd_name, "+QEXAMPLE",strlen(atcmd_name)) )
	    {
	        //Execute Mode
	        if ((QUEC_AT_MASK_NA_V01) == mask)//AT+QEXAMPLE
	        {
	            ret = qapi_atfwd_send_resp(atcmd_name, "", QUEC_AT_RESULT_OK_V01);
	        }
	        //Read Mode
	        else if ((QUEC_AT_MASK_NA_V01 | QUEC_AT_MASK_QU_V01) == mask)//AT+QEXAMPLE?
	        {
	            snprintf(buff, sizeof(buff), "+QEXAMPLE: %d", qexample_val);
	            ret = qapi_atfwd_send_resp(atcmd_name, buff, QUEC_AT_RESULT_OK_V01);
	        }
	        //Test Mode
	        else if ((QUEC_AT_MASK_NA_V01 | QUEC_AT_MASK_EQ_V01 | QUEC_AT_MASK_QU_V01) == mask)//AT+QEXAMPLE=?
	        {
	            snprintf(buff, sizeof(buff), "+QEXAMPLE: (0-2)");
	            ret = qapi_atfwd_send_resp(atcmd_name, buff, QUEC_AT_RESULT_OK_V01);
	        }
	        //Write Mode
	        else if ((QUEC_AT_MASK_NA_V01 | QUEC_AT_MASK_EQ_V01 | QUEC_AT_MASK_AR_V01) == mask)//AT+QEXAMPLE=<value>
	        {
	            tmp_val = qt_atoi((char*)at_fwd_params);
	            if(tmp_val >= 0 && tmp_val <= 2)
	            {
	                qexample_val = tmp_val;
	                ret = qapi_atfwd_send_resp(atcmd_name, "", QUEC_AT_RESULT_OK_V01);
	            }
	            else
	            {
	                ret = qapi_atfwd_send_resp(atcmd_name, "", QUEC_AT_RESULT_ERROR_V01);
	            }  
	        }
	    }
	    else
	    {
	        ret = qapi_atfwd_send_resp(atcmd_name, "", QUEC_AT_RESULT_ERROR_V01);
	    }

    	qt_uart_dbg(uart_conf.hdlr,"[%s] send resp, ret = %d\n", atcmd_name, ret);
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
	qapi_Status_t retval = QAPI_ERROR;
	int ret = -1;

	/* wait 5sec for device startup */
	qapi_Timer_Sleep(5, QAPI_TIMER_UNIT_SEC, true);

	txm_module_object_allocate(&byte_pool_at, sizeof(TX_BYTE_POOL));
	tx_byte_pool_create(byte_pool_at, "byte pool 0", free_memory_at, 10*1024);

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
	/* uart init */
	uart_init(&uart_conf);
	/* start uart receive */
	uart_recv(&uart_conf);
	/* prompt task running */
    
	qt_uart_dbg(uart_conf.hdlr,"ATFWD Example entry...\n");

	if (qapi_atfwd_Pass_Pool_Ptr(atfwd_cmd_handler_cb, byte_pool_at) != QAPI_OK)
	{
		qt_uart_dbg(uart_conf.hdlr, "Unable to alloc User space memory fail state  %x" ,0);
	  										
	}
	
    retval = qapi_atfwd_reg("+QEXAMPLE", atfwd_cmd_handler_cb);
    if(retval != QAPI_OK)
    {
        qt_uart_dbg(uart_conf.hdlr,"qapi_atfwd_reg  fail\n");
    }
    else
    {
        qt_uart_dbg(uart_conf.hdlr,"qapi_atfwd_reg ok!\n");
    }
    while(1)
    {
	    /* wait 5sec */
	    qapi_Timer_Sleep(5, QAPI_TIMER_UNIT_SEC, true);
    }

    return 0;
}

#endif/*end of __EXAMPLE_ATFWD__*/


