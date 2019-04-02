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
 *   example_watchdog.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example demonstrates how to use watchdog function with APIs in OpenCPU.
 *   In main task ,it start a GPTimer and set reset mode .
 *   In proc_subtask1,it start a watchdog and start a timer to feed the dog .If you do not feed 
 *   the dog ,the modem will reset when the dog overflow.
 *   All debug information will be output through DEBUG port.
 *   
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_WATCHDOG__" in gcc_makefile file. And compile the 
 *     app using "make clean/new".
 *     Download image bin to module to run.
 * 
 *   Operation:
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
#ifdef __EXAMPLE_WATCHDOG__
#include "ql_type.h"
#include "ql_trace.h"
#include "ql_system.h"
#include "ql_timer.h"
#include "ql_wtd.h"
#include "ril.h"
#include "ril_util.h"
#include "ril_telephony.h"
#include "ql_uart.h"
#include "ql_stdlib.h"
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



#define LOGIC_WTD1_TMR_ID  (TIMER_ID_USER_START + 1)
#define LOGIC_WTD2_TMR_ID  (TIMER_ID_USER_START + 2)
static void callback_onTimer(u32 timerId, void* param);


static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{
     
}


/**************************************************************
* main task
***************************************************************/
void proc_main_task(s32 TaskId)
{
    ST_MSG msg;
    s32 wtdid;
    s32 ret;

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

    APP_DEBUG("\r\n<--OpenCPU: watchdog TEST!-->\r\n");  

    // Initialize external watchdog:
    // specify the GPIO pin (PINNAME_NETLIGHT) and the overflow time is 600ms.
    ret = Ql_WTD_Init(0, PINNAME_NETLIGHT, 600);
    if (0 == ret)
    {
        APP_DEBUG("\r\n<--OpenCPU: watchdog init OK!-->\r\n");         
    }

    // Create a logic watchdog, the interval is 3s
    wtdid = Ql_WTD_Start(3000);

    // Register & start a timer to feed the logic watchdog.
    // The watchdog id will be passed into callback function as parameter.
    ret = Ql_Timer_Register(LOGIC_WTD1_TMR_ID, callback_onTimer, &wtdid);
    if(ret < 0)
    {
        APP_DEBUG("<--main task: register fail ret=%d-->\r\n",ret);
        return;
    }
    // The real feeding interval is 2s
    ret = Ql_Timer_Start(LOGIC_WTD1_TMR_ID, 2000,TRUE);
    if(ret < 0)
    {
        APP_DEBUG("<--main task: start timer fail ret=%d-->\r\n",ret);        
        return;
    }
    APP_DEBUG("<--main task: start timer OK  ret=%d-->\r\n",ret);

    while (TRUE)
    {
        Ql_OS_GetMessage(&msg);
        switch(msg.message)
        {
            // Application will receive this message when OpenCPU RIL starts up.
            // Then application needs to call Ql_RIL_Initialize to launch the initialization of RIL.
        case MSG_ID_RIL_READY:
            APP_DEBUG("<-- RIL is ready -->\r\n");
            Ql_RIL_Initialize();
            //
            // After RIL initialization, developer may call RIL-related APIs in the .h files in the directory of SDK\ril\inc
            // RIL-related APIs may simplify programming, and quicken the development.
            //
            break;

            // Handle URC messages.
            // URC messages include "module init state", "CFUN state", "SIM card state(change)",
            // "GSM network state(change)", "GPRS network state(change)" and other user customized URC.
        case MSG_ID_URC_INDICATION:
            switch (msg.param1)
            {
            case URC_COMING_CALL_IND:
                {
                    ST_ComingCall* pComingCall = (ST_ComingCall*)msg.param2;
                    APP_DEBUG("<-- Coming call, number:%s, type:%d -->\r\n", pComingCall->phoneNumber, pComingCall->type);

                    // Enable module sleep mode
                    ret = Ql_SleepEnable();
                    APP_DEBUG("\r\n<-- Request sleep mode, ret=%d -->\r\n\r\n", ret);            
                    break;
                }
            
            default:
                //APP_DEBUG("<-- Other URC: type=%d\r\n", msg.param1);
                break;
            }
            break;

        default:
            break;
        }        
    }
}


/**************************************************************
* the 1st sub task
***************************************************************/
static void callback_onTimer(u32 timerId, void* param)
{
    s32* wtdid = (s32*)param;
    Ql_WTD_Feed(*wtdid);
    APP_DEBUG("<-- time to feed logic watchdog (wtdId=%d) -->\r\n",*wtdid);                
}

void proc_subtask1(s32 TaskId)
{
    ST_MSG subtask1_msg;
    s32 wtdid;
    s32 ret;
    
    Ql_Debug_Trace("<-- subtask1: enter ->\r\n");
    
    // Create a logic watchdog, the interval is 1.2s
    wtdid = Ql_WTD_Start(1200);

    // Register & start a timer to feed the logic watchdog.
    // The watchdog id will be passed into callback function as parameter.
    ret = Ql_Timer_Register(LOGIC_WTD2_TMR_ID, callback_onTimer, &wtdid);
    if(ret < 0)
    {
        Ql_Debug_Trace("<--subtask1: register fail ret=%d-->\r\n",ret);
        return;
    }
    // The real feeding interval is 0.8s
    ret = Ql_Timer_Start(LOGIC_WTD2_TMR_ID, 800, TRUE);
    if(ret < 0)
    {
        Ql_Debug_Trace("<--subtask1: start timer fail ret=%d-->\r\n",ret);        
        return;
    }
    Ql_Debug_Trace("<--subtask1: start timer OK  ret=%d-->\r\n",ret);
    
    while (TRUE)
    {
        Ql_OS_GetMessage(&subtask1_msg);
        switch (subtask1_msg.message)
        {
            default:
                break;
        }
    }    
}

#endif // __EXAMPLE_WATCHDOG__

