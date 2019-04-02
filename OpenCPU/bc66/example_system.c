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
 *   example_system.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example demonstrates how to use system function with APIs in OpenCPU.
 *   Input the specified command through any uart port and the result will be 
 *   output through the MAIN port.
 *
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_SYSTEM__" in gcc_makefile file. And compile the 
 *     app using "make clean/new".
 *     Download image bin to module to run.
 * 
 *   Operation:
 *            
 *     If input "Ql_Reset=<xxx>", that will reset the module.
 *     If input "Ql_Sleep=<xxx>", that will suspends the execution of the current
 *     task until the time-out interval elapses.
 *     If input "Ql_GetSDKVer", that will get the version ID of the SDK.
 * Author:
 * -------
 * -------
 *
 *============================================================================
 *             HISTORY
 *----------------------------------------------------------------------------
 * 
 ****************************************************************************/
#ifdef __EXAMPLE_SYSTEM__
#include "custom_feature_def.h"
#include "ql_stdlib.h"
#include "ql_common.h"
#include "ql_type.h"
#include "ql_trace.h"
#include "ql_error.h"
#include "ql_time.h"
#include "ql_system.h"
#include "ql_power.h"
#include "ql_uart.h"

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
#define SERIAL_RX_BUFFER_LEN  2048
static u8 m_RxBuf_Uart[SERIAL_RX_BUFFER_LEN];
static Enum_SerialPort m_myUartPort  = UART_PORT0;
/*****************************************************************
* uart callback function
******************************************************************/
static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);

/*****************************************************************
* other subroutines
******************************************************************/
extern s32 Analyse_Command(u8* src_str,s32 symbol_num,u8 symbol, u8* dest_buf);
static s32 ReadSerialPort(Enum_SerialPort port, /*[out]*/u8* pBuffer, /*[in]*/u32 bufLen);
static void proc_handle(char *pData,s32 len);

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
Global_SYS_Cfg tempTest;
Global_SYS_Cfg test=
{
    "TEST00V0.0.1\0",
     6345,
     {                        
        115200,            //baud rate 
        8,                 //bytes ize
        1,            //stop bit
        0,           //parity
        0,           //flow control
    },
    "012345",
    {                       
        4,                 //reset period, unit hour
        20,                //Max time delay when reset timer come in, unit minute
    }
};

void proc_main_task(s32 taskId)
{
    ST_MSG msg;
	s32 ret;
    s32 m_pwrOnReason;
	
    // Register & open UART port
    Ql_UART_Register(m_myUartPort, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(m_myUartPort, 115200, FC_NONE);
    APP_DEBUG("<--OpenCPU: system.-->\r\n");
    m_pwrOnReason = Ql_GetPowerOnReason();
	APP_DEBUG("\r\n<--Power on reason:%d -->\r\n", m_pwrOnReason); 
	
    while(TRUE)
    {
        Ql_OS_GetMessage(&msg);
        switch(msg.message)
        {
#ifdef __OCPU_RIL_SUPPORT__
        case MSG_ID_RIL_READY:
            APP_DEBUG("<-- RIL is ready -->\r\n");
            Ql_RIL_Initialize();
            break;
#endif
        default:
            break;
        }
    }
}

static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{
    switch (msg)
    {
    case EVENT_UART_READY_TO_READ:
        {

           s32 totalBytes = ReadSerialPort(port, m_RxBuf_Uart, sizeof(m_RxBuf_Uart));
           if (totalBytes > 0)
           {
               proc_handle(m_RxBuf_Uart,sizeof(m_RxBuf_Uart));
           }
           break;
        }
    case EVENT_UART_READY_TO_WRITE:
        break;
    default:
        break;
    }
}

static s32 ReadSerialPort(Enum_SerialPort port, /*[out]*/u8* pBuffer, /*[in]*/u32 bufLen)
{
    s32 rdLen = 0;
    s32 rdTotalLen = 0;
    if (NULL == pBuffer || 0 == bufLen)
    {
        return -1;
    }
    Ql_memset(pBuffer, 0x0, bufLen);
    while (1)
    {
        rdLen = Ql_UART_Read(port, pBuffer + rdTotalLen, bufLen - rdTotalLen);
        if (rdLen <= 0)  // All data is read out, or Serial Port Error!
        {
            break;
        }
        rdTotalLen += rdLen;
        // Continue to read...
    }
    if (rdLen < 0) // Serial Port Error!
    {
        APP_DEBUG("<--Fail to read from port[%d]-->\r\n", port);
        return -99;
    }
    return rdTotalLen;
}

static void proc_handle(char *pData,s32 len)
{
    char *p = NULL;
    char parambuf[100];
    s32 iRet = 0;

    //command: Reset=<type>
    p = Ql_strstr(pData,"Reset=");
    if (p)
    {
        Ql_memset(parambuf, 0, 100);
        if (Analyse_Command(pData, 1, '>', parambuf))
        {
            APP_DEBUG("<--Parameter Error.-->\r\n");
            return;
        }
        
        if (Ql_atoi(parambuf) != 0)//must be 0
        {
            APP_DEBUG("<--Parameter Error.-->\r\n");
            return;
        }

        APP_DEBUG("<--Set Reset Parameter Successfully<%d>.-->\r\n",Ql_atoi(parambuf));
        Ql_Sleep(100);
        Ql_Reset(Ql_atoi(parambuf));
        return;
    }

    //command: Sleep=<msec> 
    p = Ql_strstr(pData,"Sleep=");
    if (p)
    {
        s32 ret;
        ST_Time time,*tm;
        Ql_memset(parambuf, 0, 100);
        if (Analyse_Command(pData, 1, '>', parambuf))
        {
            APP_DEBUG("<--Parameter Error.-->\r\n");
            return;
        }
        
        APP_DEBUG("<--Set Sleep Parameter Successfully<%d>.-->\r\n",Ql_atoi(parambuf));
        APP_DEBUG("<--Start sleeping-->\r\n");

        Ql_Sleep(Ql_atoi(parambuf));
        APP_DEBUG("<--sleeping over-->\r\n");

        return;
    }


    //command: Ql_GetSDKVer
    p = Ql_strstr(pData,"Ql_GetSDKVer");
    if (p)
    {
        s32 ret;
        u8 tmpbuf[100];
        
        Ql_memset(tmpbuf, 0, sizeof(tmpbuf));
        ret = Ql_GetSDKVer((u8*)tmpbuf, sizeof(tmpbuf));
        if(ret > 0)
        {
            APP_DEBUG("<--SDK Version:%s.-->\r\n", tmpbuf);
        }else
        {
            APP_DEBUG("<--Get SDK Version Failure.-->\r\n");
        }
        return;
    }
	    //command: Ql_SleepEnable  set the module into sleep mod 
    p= Ql_strstr(pData,"Ql_SleepEnable");
    if(p)
    {
        s32 ret;
        ret = Ql_SleepEnable();
        APP_DEBUG("<-- Ql_SleepEnable=%d -->\r\n", ret);
	   return;
    } 

    //command: Ql_SleepDisable
    p= Ql_strstr(pData,"Ql_SleepDisable");
    if(p)
    {
        s32 ret;
	    ret = Ql_SleepDisable();
        APP_DEBUG("<-- Ql_SleepDisable=%d -->\r\n", ret);
        return;
    } 
	
    //command:Ql_SecureData_Store=<n> , n=1~2.
    p= Ql_strstr(pData,"Ql_SecureData_Store=");
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
        if(index <1 || index >2)
        {
            APP_DEBUG("<--Parameter n Error.-->\r\n");
            return;
        }
        ret = Ql_SecureData_Store(index, (u8*)&test,sizeof(test));
        APP_DEBUG("<-- Ql_SecureData_Store(index=%d, value=%d, data size=%d)=%d -->\r\n", index,test.reset.Period,sizeof(Global_SYS_Cfg),ret);   
        return;
    }

    //command:Ql_SecureData_Read=<n> , n=1~2.
    p= Ql_strstr(pData,"Ql_SecureData_Read=");
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
        if(index <1 || index >2)
        {
            APP_DEBUG("<--Parameter n Error.-->\r\n");
            return;
        }
        ret = Ql_SecureData_Read(index, (u8*)&tempTest,sizeof(Global_SYS_Cfg));
        APP_DEBUG("<-- Ql_SecureData_Read =%d \r\n test.version:%s test.data=%s  test.reset.timeDelay =%d-->\r\n",ret, tempTest.version,tempTest.data,tempTest.reset.timeDelay);   
        return;
    }
}

#endif
