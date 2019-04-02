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
 *   example_memory.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example demonstrates how to use memory function with APIs in OpenCPU.
 *   Input the specified command through any uart port and the result will be 
 *   output through the debug port.
 *
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_MEMORY__" in gcc_makefile file. And compile the 
 *     app using "make clean/new".
 *     Download image bin to module to run.
 * 
 *   Operation:
 *            
 *     If input "a", that will allocate memory, size is 1k bytes.
 *     If input "f", that will free memory.
 *     If input "w", that will write data to the allocated memory.
 *     If input "r", that will read data from the allocated memory.
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
#ifdef __EXAMPLE_MEMORY__ 
#include <string.h>
#include "ql_type.h"
#include "ql_trace.h"
#include "ql_uart.h"
#include "ql_memory.h"
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



#pragma arm section zidata = "NVZI",  rwdata = "NVRW" 
int nv1;
int nv2;
int arrNum[100];
#pragma arm section zidata, rwdata
static u32 k=11;

static u8 m_Read_Buffer[1024];

static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);

void proc_main_task(s32 taskId)
{
    s32 ret;
    ST_MSG msg;
    


    // Register & open UART port
    Ql_UART_Register(UART_PORT1, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(UART_PORT1, 115200, FC_NONE);

    Ql_UART_Register(UART_PORT2, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(UART_PORT2, 115200, FC_NONE);

    APP_DEBUG("\r\n<--OpenCPU: Memory TEST!-->\r\n");  
    APP_DEBUG("\r\n<-- nv1=[0x%08x,%d], nv2=[0x%08x,%d] -->\r\n", &nv1, ++nv1, &nv2, ++nv2);

/*
    APP_DEBUG("\r\n<-- arrNum[0x%x]: %s -->\r\n", &arrNum[0],(char*)arrNum);
    u16 i;
    u16 elementNum = sizeof(arrNum) / sizeof(arrNum[0]) - 1;
    for (i = 0; i < (elementNum-1); i++)
    {
        *(arrNum+i) = 0x44434241;
    }
    *(arrNum+i) = 0;
*/
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

void *pointer = 0;
static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{
    //APP_DEBUG("CallBack_UART_Hdlr: port=%d, event=%d, level=%d, p=%x\r\n", port, msg, level, customizedPara);
    u32 rdLen=0;
    s32 ret;
    char *pData;
    char *p=NULL;
    
    switch (msg)
    {
        case EVENT_UART_READY_TO_READ:
        {
            Ql_memset(m_Read_Buffer, 0x0, sizeof(m_Read_Buffer));
            rdLen = Ql_UART_Read(port, m_Read_Buffer, sizeof(m_Read_Buffer));
            pData = m_Read_Buffer;
            
            //command-->allocate memory, size is 1k bytes
            p = Ql_strstr(pData,"Allocate memory");
            if (p)
            {
                if (pointer != NULL)
                {
                    Ql_MEM_Free(pointer);
                    pointer = NULL;
                }
                pointer = Ql_MEM_Alloc(1024);
                APP_DEBUG("\r\n<---ALLOC,pointer=0x%08x--->\r\n",pointer);
                break;
            }
            
            //command-->free memory
            p = Ql_strstr(pData,"Free memory");
            if (p)
            {
                APP_DEBUG("\r\n<---Free pointer=0x%08x --->\r\n",pointer);
                Ql_MEM_Free(pointer); 
                pointer = NULL;
                break;
            }                        
            p = Ql_strstr(pData,"Write data");
            if (p)
            {
                nv1 = 43;
                nv2 = 27;
                APP_DEBUG("\r\n<---Write:nvdata1=%d nvdata2=%d--->\r\n",nv1,nv2);
                break;
            }                                 
            p = Ql_strstr(pData,"Read data");
            if (p)
            {
                APP_DEBUG("\r\n<---Read:nvdata1=%d nvdata2=%d k=%d--->\r\n",nv1,nv2,k);
                break;
            }
        }
        default:
            break;
        }
}


#endif // __EXAMPLE_MEMORY__

