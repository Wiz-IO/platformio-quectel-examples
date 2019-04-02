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
 *   example_transpass.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example gives an example for transpass setting.
 *   Through Uart port, input the special command, there will be given the response
 *   about transpass operation.
 *
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_TRANSPASS__" in gcc_makefile file. And compile the 
 *     app using "make clean/new".
 *     Download image bin to module to run.
 * 
 *   Operation:
 *            
 *     Send data from any port that will transmit the data to modem,and the response
 *     will be print out from debug port.
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
#ifdef __EXAMPLE_TRANSPASS__ 
#include "ql_type.h"
#include "ql_trace.h"
#include "ql_uart.h"
#include "ql_error.h"
#include "ql_stdlib.h"



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


static u8 m_Read_Buffer[1024];
static Enum_SerialPort m_myVirtualPort = VIRTUAL_PORT2;

static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);

void proc_main_task(s32 taskId)
{
    ST_MSG msg;  

    Ql_UART_Register(UART_PORT1, CallBack_UART_Hdlr, NULL);    
    Ql_UART_Register(UART_PORT2, CallBack_UART_Hdlr, NULL);
    Ql_UART_Register(UART_PORT3, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(UART_PORT1, 115200, FC_NONE);
    Ql_UART_Open(UART_PORT2, 115200, FC_NONE);
    Ql_UART_Open(UART_PORT3, 115200, FC_NONE);

    APP_DEBUG("\r\n<--OpenCPU: transpass test!-->\r\n");  

    // Register & open Modem port
    Ql_UART_Register(m_myVirtualPort, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(m_myVirtualPort, 0, 0);

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
static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{
    //APP_DEBUG("CallBack_UART_Hdlr: port=%d, event=%d, level=%d, p=%x\r\n", port, msg, level, customizedPara);
    u32 rdLen=0;
    if(m_myVirtualPort == port)
    {
        switch (msg)
        {
            case EVENT_UART_READY_TO_READ:
            {
                Ql_memset(m_Read_Buffer, 0x0, sizeof(m_Read_Buffer));
                rdLen = Ql_UART_Read(port, m_Read_Buffer, sizeof(m_Read_Buffer));

                APP_DEBUG("\r\nmodem data =%s\r\n",m_Read_Buffer);      
                break;
            }
            default:
                break;
        }
    }
    else if( UART_PORT1 == port | UART_PORT2 == port | UART_PORT3 == port)
    {
        switch (msg)
        {
            case EVENT_UART_READY_TO_READ:
            {
                Ql_memset(m_Read_Buffer, 0x0, sizeof(m_Read_Buffer));
                rdLen = Ql_UART_Read(port, m_Read_Buffer, sizeof(m_Read_Buffer));

                APP_DEBUG("\r\nuart%d data =%s\r\n",port-1,m_Read_Buffer);    
                Ql_UART_Write(m_myVirtualPort,(u8*)m_Read_Buffer,rdLen);    
                break;
            }
            default:
                break;
        }

    }
}

#endif // __EXAMPLE_TRANSPASS__

