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
 *   example_eint.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example demonstrates how to use eint function with APIs in OpenCPU.
 *   Only the following pins have the function of the external interrupt.  
 *   PINNAME_DTR                  //1
 *   PINNAME_CTS                  //4
 *
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_EINT__" in gcc_makefile file. And compile the 
 *     app using "make clean/new".
 *     Download image bin to module to run.
 *
 *   Operation:
 *     You can modify pinname to specify the ENIT pin.
 *     And you and undefine FAST_REGISTER to call  Ql_EINT_Register.
 *     You can call Ql_EINT_Uninit to release the specified EINT pin.
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
#ifdef __EXAMPLE_EINT__
#include "ql_stdlib.h"
//#include "ql_common.h"
#include "ql_type.h"
#include "ql_trace.h"
#include "ql_eint.h"
#include "ql_system.h"
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





#define FAST_REGISTER
Enum_PinName pinname = PINNAME_DTR;
/*****************************************************************
* callback function
******************************************************************/
static void callback_eint_handle(Enum_PinName eintPinName, Enum_PinLevel pinLevel, void* customParam);

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
    

    APP_DEBUG("<--OpenCPU: eint.-->\r\n"); 
    #ifdef FAST_REGISTER
    /*************************************************************
    * Registers an EINT I/O, and specify the interrupt handler. 
    *The EINT, that is registered by calling this function, is a lower-level interrupt. 
    *The response for interrupt request is more timely.
    *Please don't add any task schedule in the interrupt handler.
    *And the interrupt handler cannot consume much CPU time.
    *Or it causes system exception or reset.
    **************************************************************/
    ret = Ql_EINT_RegisterFast(pinname,callback_eint_handle, NULL);
    #else
    //Registers an EINT I/O, and specify the interrupt handler. 
    ret = Ql_EINT_Register(pinname,callback_eint_handle, NULL);    
    #endif
    if(ret != 0)
    {
        APP_DEBUG("<--OpenCPU: Ql_EINT_RegisterFast fail.-->\r\n"); 
        return;    
    }
    APP_DEBUG("<--OpenCPU: Ql_EINT_RegisterFast OK.-->\r\n"); 
    
    /*************************************************************
    *Initialize an external interrupt function. 
    *Parameters:
    *               eintPinName:
    *                   EINT pin name, one value of Enum_PinName that has 
    *                   the interrupt function.
    *               eintType:
    *                   Interrupt type, level-triggered or edge-triggered.
    *                   Now, only level-triggered interrupt is supported.
    *               hwDebounce:
    *                   Hardware debounce. Unit in 10ms. 
    *               swDebounce:
    *                   Software debounce. Unit in 10ms. The minimum value for 
    *                   this parameter is 5, which means the minimum software
    *                   debounce time is 5*10ms=50ms.
    *		   automask:
    *	              mask the Eint after the interrupt happened.
    **************************************************************/    
    ret = Ql_EINT_Init(pinname, EINT_LEVEL_TRIGGERED, 0, 5,0);
    if(ret != 0)
    {
        APP_DEBUG("<--OpenCPU: Ql_EINT_Init fail.-->\r\n"); 
        return;    
    }
    APP_DEBUG("<--OpenCPU: Ql_EINT_Init OK.-->\r\n"); 
    

    while(TRUE)
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

static void callback_eint_handle(Enum_PinName eintPinName, Enum_PinLevel pinLevel, void* customParam)
{
    s32 ret;
    //mask the specified EINT pin.
    Ql_EINT_Mask(pinname);
    
    APP_DEBUG("<--Eint callback: pin(%d), levle(%d)-->\r\n",eintPinName,pinLevel);
    ret = Ql_EINT_GetLevel(eintPinName);
    APP_DEBUG("<--Get Level, pin(%d), levle(%d)-->\r\n",eintPinName,ret);
    
    //unmask the specified EINT pin
    Ql_EINT_Unmask(pinname);
}

#endif // __EXAMPLE_EINT__

