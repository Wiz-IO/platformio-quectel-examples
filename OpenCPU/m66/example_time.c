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
 *   example_time.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example demonstrates how to use time function with APIs in OpenCPU.
 *   It display how to set and get local time.You can modify struct ST_time to set diffrent local time.
 *   All debug trace will be output through DEBUG port.
 *
 *
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_TIME__" in gcc_makefile file. And compile the 
 *     app using "make clean/new".
 *     Download image bin to module to run.
 * 
 *   Operation:
 *            
 *     
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
#ifdef __EXAMPLE_TIME__
#include "ql_type.h"
#include "ql_trace.h"
#include "ql_time.h"
#include "ql_system.h"
#include "ql_stdlib.h"
#include "ql_error.h"
#include "ql_uart.h"


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



ST_MSG  msg;
void proc_main_task(s32 taskId)
{
    ST_Time time;
    ST_Time* pTime = NULL;
    s32 ret;
    u64 totalSeconds;

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
    
    APP_DEBUG("\r\n<--OpenCPU: Time TEST!-->\r\n");  
    //set local time
    time.year = 2015;
    time.month = 12;
    time.day = 10;
    time.hour = 12;
    time.minute = 30;
    time.second = 18;
    time.timezone = 22; // +05:30, one digit expresses a quarter of an hour
    ret = Ql_SetLocalTime(&time);
    APP_DEBUG("<-- Ql_SetLocalTime(%d.%02d.%02d %02d:%02d:%02d timezone=%02d)=%d -->\n\r", 
        time.year, time.month, time.day, time.hour, time.minute, time.second, time.timezone, ret);

    //get local time
    if((Ql_GetLocalTime(&time)))
    {
        APP_DEBUG("\r\n<--Local time successfuly determined: %i.%i.%i %i:%i:%i timezone=%i-->\r\n", time.day, time.month, time.year, time.hour, time.minute, time.second,time.timezone);

        //This function get total seconds elapsed   since 1970.01.01 00:00:00.             
        totalSeconds= Ql_Mktime(&time);
        APP_DEBUG("\r\n<-- Mktime is %lld -->\r\n", totalSeconds);
        
        //This function convert the seconds elapsed since 1970.01.01 00:00:00 to local date and time.                  
        pTime = Ql_MKTime2CalendarTime(totalSeconds, &time);
        if(NULL == pTime)
        {
            APP_DEBUG("<--Ql_MKTime2CalendarTime failed !!-->\r\n");
        }
        else
        {
            APP_DEBUG("\r\n<--Ql_MKTime2CalendarTime: (%i.%i.%i %i:%i:%i timezone=%i)-->\r\n", time.day, time.month, time.year, time.hour, time.minute, time.second,time.timezone);
        }
    }
    else
    {
        APP_DEBUG("\r\n<--failed !! Local time not determined -->\r\n");
    }

    while (1)
    {
         Ql_OS_GetMessage(&msg);
        switch(msg.message)
        {
        case 0:
            break;
        default:
            break;
        }
        
    }
    
}
#endif

