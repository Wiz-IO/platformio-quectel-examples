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
 *   example_flash.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example demonstrates how to use read/write data to flash in OpenCPU.
 *
 *Note:
 *------------
 *    The module supports two blocks, each with a maximum storage of 2KB data, a total of 4KB.
 *these APIs and Ql_SecureData_xxx  operates on the same area.Users can choose one of them. 
 *If no data has been written before, the first reading returns -4(FLASH_ERROR_NOFOUND).
 *
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_FLASH__" in gcc_makefile file. And compile the 
 *     app using "make clean/new".
 *     Download image bin to module to run.
 * 
 *   Operation:
 *            
 *     If input "Flash_write", that will allocate memory, size is 1k bytes.
 *     If input "Flash_read", that will free memory.

 * Author:
 * -------
 * -------
 *
 *============================================================================
 *             HISTORY
 *----------------------------------------------------------------------------
 * 
 ****************************************************************************/
#ifdef __EXAMPLE_FLASH__ 
#include "ql_type.h"
#include "ql_trace.h"
#include "ql_uart.h"
#include "ql_flash.h"
#include "ql_stdlib.h"
#include "ql_error.h"


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

/*****************************************************************
* UART Param
******************************************************************/
// Define the UART port 
static Enum_SerialPort m_myUartPort  = UART_PORT0;
#define READ_BUFFER_LENGTH 1024
static u8 m_Read_Buffer[READ_BUFFER_LENGTH];
static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);


/*****************************************************************
* flash Param
******************************************************************/
#define FLASH_ADDR   (0x7f0)

typedef struct{
    u32 BaudRate;
    u8  DataBit;
    u8  StopBit;
    u8  Parity;
    u8  FlowCtrl;
}Uart_cfg;

typedef struct{
    u8  Period;
    u8  timeDelay;
}Reset_cfg;

typedef struct{
    u8 version[20];
    s16 Backup_Datasize;
    Uart_cfg Uart;
    u8 data[10];
    Reset_cfg reset;
}Global_SYS_Cfg;
Global_SYS_Cfg test=
{
    "TEST00V0.0.1\0",
     6345,
     {                        
        115200,      //baud rate 
        8,           //bytes ize
        1,           //stop bit
        0,           //parity
        0,           //flow control
    },
    "012345",
    {                       
        4,          //reset period, unit hour
        20,         //Max time delay when reset timer come in, unit minute
    }
};

Global_SYS_Cfg tempTest;

void proc_main_task(s32 taskId)
{
    s32 ret;
    ST_MSG msg;
    
    // Register & open UART port
    Ql_UART_Register(m_myUartPort, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(m_myUartPort, 115200, FC_NONE);

    APP_DEBUG("\r\n<--OpenCPU: Flash TEST!-->\r\n");  

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

void *pointer = NULL;
static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{
    //APP_DEBUG("CallBack_UART_Hdlr: port=%d, event=%d, level=%d, p=%x\r\n", port, msg, level, customizedPara);
    u32 rdLen=0;
    char *pData;
    char *p=NULL;
    char parambuf[100];
	
    switch (msg)
    {
        case EVENT_UART_READY_TO_READ:
        {
            Ql_memset(m_Read_Buffer, 0x0, sizeof(m_Read_Buffer));
            rdLen = Ql_UART_Read(port, m_Read_Buffer, sizeof(m_Read_Buffer));
            pData = m_Read_Buffer;
            
            //command:Flash_write=<n> , n=1~2.
            p= Ql_strstr(pData,"Flash_write=");
            if(p)
            {
                s32 ret;
                u8 index;
        
                Ql_memset(parambuf, 0, 100);
                if (Analyse_Command(pData, 1, '>', parambuf))
                {
                    APP_DEBUG("<--Parameter Error.-->\r\n");
                    return;
                }
                
                index = Ql_atoi(parambuf);
                ret = Ql_Flash_Write(index,FLASH_ADDR, (u8*)&test,sizeof(test));
                APP_DEBUG("<-- Ql_Flash_Write(index=%d, value=%d, data size=%d)=%d -->\r\n", index,test.reset.Period,sizeof(Global_SYS_Cfg),ret);   
                return;
            }
        
            //command:Flash_read=<n> , n=1~2.
            p= Ql_strstr(pData,"Flash_read=");
            if(p)
            {
                s32 ret;
                u8 index;
        
                Ql_memset(parambuf, 0, 100);
                if (Analyse_Command(pData, 1, '>', parambuf))
                {
                    APP_DEBUG("<--Parameter Error.-->\r\n");
                    return;
                }
                
                index = Ql_atoi(parambuf);
                ret = Ql_Flash_Read(index,FLASH_ADDR,(u8*)&tempTest,sizeof(Global_SYS_Cfg));
                APP_DEBUG("<-- Ql_SecureData_Read =%d \r\n test.version:%s test.data=%s  test.reset.timeDelay =%d-->\r\n",ret, tempTest.version,tempTest.data,tempTest.reset.timeDelay);   
                return;
            }
        }
        default:
            break;
        }
}


#endif // __EXAMPLE_Flash__

