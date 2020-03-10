/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Quectel Co., Ltd. 2019
*
*****************************************************************************/
/*****************************************************************************
*
* Filename:
* ---------
*   example_adc.c
*
* Project:
* --------
*   OpenCPU
*
* Description:
* ------------
*   This example demonstrates how to program ADC interface in OpenCPU.
*
*   All debug information will be output through MAIN port.
*
* Usage:
* ------
*   Compile & Run:
*
*     Set "C_PREDEF=-D __EXAMPLE_ADC__" in gcc_makefile file. And compile the 
*     app using "make clean/new".
*     Download image bin to module to run.
*
* Author:
* -------
* -------
*
*============================================================================
*             HISTORY
*----------------------------------------------------------------------------
* 
****************************************************************************/
#ifdef __EXAMPLE_ADC__
#include "ql_trace.h"
#include "ql_system.h"
#include "ql_adc.h"
#include "ql_uart.h"
#include "ql_stdlib.h"
#include "ql_error.h"
#include "ql_timer.h"

#define DEBUG_ENABLE 1
#if DEBUG_ENABLE > 0
#define DEBUG_PORT  UART_PORT0
#define DBG_BUF_LEN   512
static char DBG_BUFFER[DBG_BUF_LEN];
#define APP_DEBUG(FORMAT,...) {\
    Ql_memset(DBG_BUFFER, 0, DBG_BUF_LEN);\
    Ql_sprintf(DBG_BUFFER,FORMAT,##__VA_ARGS__); \
    if (UART_PORT2 == (DEBUG_PORT)) \
    {\
        Ql_Debug_Trace(DBG_BUFFER);\
    } else {\
        Ql_UART_Write((Enum_SerialPort)(DEBUG_PORT), (u8*)(DBG_BUFFER), Ql_strlen((const char *)(DBG_BUFFER)));\
    }\
}
#else
#define APP_DEBUG(FORMAT,...) 
#endif

static Enum_SerialPort m_myUartPort  = UART_PORT0;

//timer
static u32 Stack_timer = 0x101; 
static u32 ST_Interval = 1000;
static s32 m_param1 = 0;

static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{
     
}

static void Timer_handler(u32 timerId, void* param)
{
	 u32 value = 0;
	 
	 Ql_ADC_Read(PIN_ADC0,&value);
	 APP_DEBUG("<---read adc0 voltage%d(mV)--->\r\n",value);
}

void proc_main_task(s32 taskId)
{
    s32 ret;
    ST_MSG msg;

    // Register & open UART port
    ret = Ql_UART_Register(m_myUartPort, CallBack_UART_Hdlr, NULL);
    if (ret < QL_RET_OK)
    {
        Ql_Debug_Trace("Fail to register serial port[%d], ret=%d\r\n", UART_PORT1, ret);
    }
    ret = Ql_UART_Open(m_myUartPort, 115200, FC_NONE);
    if (ret < QL_RET_OK)
    {
        Ql_Debug_Trace("Fail to open serial port[%d], ret=%d\r\n", UART_PORT1, ret);
    }

    APP_DEBUG("\r\n<-- OpenCPU: ADC Example -->\r\n")
		
    Ql_Timer_Register(Stack_timer, Timer_handler, &m_param1);
    Ql_Timer_Start(Stack_timer,ST_Interval,TRUE);
    
    // Start message loop of this task
    while (TRUE)
    {
        Ql_OS_GetMessage(&msg);
        switch(msg.message)
        {
        case MSG_ID_USER_START:
            break;
        default:
            break;
        }
    }
}

#endif // __EXAMPLE_ADC__
