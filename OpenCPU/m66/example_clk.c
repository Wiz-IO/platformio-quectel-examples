/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Quectel Co., Ltd. 2013
*
*****************************************************************************/
/*****************************************************************************
 *
 * Filename:
 * ---------
 *   example_clk.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example demonstrates how to program clock pin in OpenCPU.
 *   When this example program runs, 13MHz clock signal will be output through PINNAME_DCD.
 *   User may watch the waveform on signal analyzer.
 *
 *   All debug information will be output through DEBUG port.
 *
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_CLK__" in gcc_makefile file. And compile the 
 *     app using "make clean/new".
 *     Download image bin to module to run.
 *
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
#ifdef __EXAMPLE_CLK__
#include "ql_trace.h"
#include "ql_system.h"
#include "ql_gpio.h"
#include "ql_clock.h"
#include "ql_stdlib.h"
#include "ql_uart.h"
#include "ql_error.h"



#define DEBUG_ENABLE 1
#if DEBUG_ENABLE > 0
#define DEBUG_PORT  UART_PORT1
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

static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{
     
}




static void CLOCK_Program(void)
{
    Enum_PinName clkPin = PINNAME_DCD;
    Enum_ClockSource clkSrc = CLOCKSOURCE_13M;

    // Initialize clock pin and clock source
    APP_DEBUG("<-- Initialize clock pin (PINNAME_DCD) and clock source (CLOCKSOURCE_13M) -->\r\n");
    Ql_CLK_Init(clkPin, clkSrc);

    // Switch on clock output
    APP_DEBUG("<-- Switch on clock output -->\r\n");
    Ql_CLK_Output(clkPin, TRUE);

    // Switch off clock output
    //Ql_CLK_Output(clkPin, FALSE);
}

void proc_main_task(s32 taskId)
{
    s32 ret;
    ST_MSG msg;

    // Register & open UART port
    ret = Ql_UART_Register(UART_PORT1, CallBack_UART_Hdlr, NULL);
    if (ret < QL_RET_OK)
    {
        Ql_Debug_Trace("Fail to register serial port[%d], ret=%d\r\n", UART_PORT1, ret);
    }
    ret = Ql_UART_Open(UART_PORT1, 115200, FC_NONE);
    if (ret < QL_RET_OK)
    {
        Ql_Debug_Trace("Fail to open serial port[%d], ret=%d\r\n", UART_PORT1, ret);
    }
    APP_DEBUG("\r\n<-- OpenCPU: Clock Example -->\r\n");

    // Start to program clock
    CLOCK_Program();

    // Start message loop of this task
    while(TRUE)
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

#endif // __EXAMPLE_CLK__
