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
 *   example_rtc.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example demonstrates how to use rtc function with APIs in OpenCPU.
 *
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_RTC__" in gcc_makefile file. And compile the 
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
#ifdef __EXAMPLE_RTC__
#include "ql_type.h"
#include "ql_trace.h"
#include "ql_rtc.h"
#include "ql_uart.h"
#include "ql_stdlib.h"
#include "ql_error.h"
#include "ql_power.h"
#include "ril_system.h"
#include "ril.h"

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
* rtc param
******************************************************************/
static u32 Rtc_id = 0x101; 
static u32 Rtc_Interval = 60*1000*5; //5min
static s32 m_param = 0;
Enum_Ql_Power_On_Result_t m_pwrOnReason;

/*****************************************************************
* uart callback function
******************************************************************/
static Enum_SerialPort m_myUartPort  = UART_PORT0;
static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);
static s32 ATResponse_Handler(char* line, u32 len, void* userData);
#define SERIAL_RX_BUFFER_LEN  2048
static u8 m_RxBuf_Uart[SERIAL_RX_BUFFER_LEN];

// RTC callback
static void Rtc_handler(u32 rtcId, void* param);

/*******************************************************************
*main task
********************************************************************/
void proc_main_task(s32 taskId)
{
    s32 ret;
    ST_MSG msg;

    // Register & open UART port
    ret = Ql_UART_Register(m_myUartPort, CallBack_UART_Hdlr, NULL);
    if (ret < QL_RET_OK)
    {
        Ql_Debug_Trace("Fail to register serial port[%d], ret=%d\r\n",m_myUartPort, ret);
    }
    ret = Ql_UART_Open(m_myUartPort, 115200, FC_NONE);
    if (ret < QL_RET_OK)
    {
        Ql_Debug_Trace("Fail to open serial port[%d], ret=%d\r\n",m_myUartPort, ret);
    }
    
    APP_DEBUG("\r\n<--OpenCPU: Rtc_timer demo-->\r\n"); 

    //rtc register
    ret = Ql_Rtc_RegisterFast(Rtc_id, Rtc_handler, &m_param);
    if(ret <0)
    {
		if(-4 == ret)
		{
			APP_DEBUG("\r\n<--the rtc is already register-->\r\n");	
		}
		else
		{
		  APP_DEBUG("\r\n<--rtc register failed!:RTC id(%d),ret(%d)-->\r\n",Rtc_id,ret);          
		}
    }
	else 
	{
        APP_DEBUG("\r\n<--rtc register successful:RTC id(%d),param(%d),ret(%d)-->\r\n",Rtc_id,m_param,ret); 
	}
    //start a rtc ,repeat=true;
    ret = Ql_Rtc_Start(Rtc_id,Rtc_Interval,TRUE);
    if(ret < 0)
    {   
		if(-4 == ret)
		{
			APP_DEBUG("\r\n<--the rtc is already start-->\r\n");	
		}
		else
		{
           APP_DEBUG("\r\n<--rtc start failed!!:ret=%d-->\r\n",ret);   
		}
    }
	else
	{
         APP_DEBUG("\r\n<--rtc start successful:RTC id(%d),interval(%d),ret(%d)-->\r\n",Rtc_id,Rtc_Interval,ret);
	}
    while (1)
    {
        Ql_OS_GetMessage(&msg);
        switch(msg.message)
        {
          case MSG_ID_RIL_READY:
		  	 {
                 APP_DEBUG("<-- RIL is ready -->\r\n");
                 Ql_RIL_Initialize();
				 
				 RIL_QNbiotEvent_Enable(PSM_EVENT);//enable  psm event. if the module enter psm mode  which is reported by URC(+QNBIOTEVENT:  "ENTER PSM").

				 m_pwrOnReason = Ql_GetPowerOnReason();
    			 if(QL_DEEP_SLEEP == m_pwrOnReason)//deep sleep wake up
    			 {
	                 APP_DEBUG("\r\n<--The module wake up from deep sleep mode -->\r\n"); 
    			 }
    			 else if (QL_SYS_RESET == m_pwrOnReason)// reset
    			 {
                   APP_DEBUG("\r\n<--The module reset or first power on-->\r\n"); 
    			 }
              }
              break;
		  case MSG_ID_APP_TEST:
		  {
		  	APP_DEBUG("\r\n<--recv the message of rtc.(%d)-->\r\n",msg.param1); 
             break;
		  }
          case MSG_ID_URC_INDICATION: 
	  	     switch (msg.param1)
             {            
                 case URC_EGPRS_NW_STATE_IND:
                     APP_DEBUG("<-- EGPRS Network Status:%d -->\r\n", msg.param2);
                     break;  
    			 case URC_PSM_EVENT:
				 	 if(ENTER_PSM== msg.param2)
				 	 {
                        APP_DEBUG("<-- ENTER PSM-->\r\n");//The modem enter PSM mode.
				 	 }
                     break;
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


// rtc callback function
void Rtc_handler(u32 rtcId, void* param)
{
	*((s32*)param) +=1;

    if(Rtc_id == rtcId)
    {
		//Ql_SleepDisable();
		
		//if you want  send message in ISR,please use the following API. 
		Ql_OS_SendMessageFromISR(main_task_id,MSG_ID_APP_TEST,*((s32*)param),0);
		
		APP_DEBUG("\r\n<--Rtc_handler(%d)->\r\n",*((s32*)param));
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
        APP_DEBUG("Fail to read from port[%d]\r\n", port);
        return -99;
    }
    return rdTotalLen;
}

static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{
    //APP_DEBUG("CallBack_UART_Hdlr: port=%d, event=%d, level=%d, p=%x\r\n", port, msg, level, customizedPara);
    switch (msg)
    {
    case EVENT_UART_READY_TO_READ:
        {
            if (m_myUartPort == port)
            {
                s32 totalBytes = ReadSerialPort(port, m_RxBuf_Uart, sizeof(m_RxBuf_Uart));
                if (totalBytes <= 0)
                {
                    APP_DEBUG("<-- No data in UART buffer! -->\r\n");
                    return;
                }
                {// Read data from UART
                    s32 ret;
                    char* pCh = NULL;
                    
                    // Echo
                    Ql_UART_Write(m_myUartPort, m_RxBuf_Uart, totalBytes);

                    pCh = Ql_strstr((char*)m_RxBuf_Uart, "\r\n");
                    if (pCh)
                    {
                        *(pCh + 0) = '\0';
                        *(pCh + 1) = '\0';
                    }

                    // No permission for single <cr><lf>
                    if (Ql_strlen((char*)m_RxBuf_Uart) == 0)
                    {
                        return;
                    }
                    ret = Ql_RIL_SendATCmd((char*)m_RxBuf_Uart, totalBytes, ATResponse_Handler, NULL, 0);
                }
            }
            break;
        }
    case EVENT_UART_READY_TO_WRITE:
        break;
    default:
        break;
    }
}

static s32 ATResponse_Handler(char* line, u32 len, void* userData)
{
    APP_DEBUG("[ATResponse_Handler] %s\r\n", (u8*)line);
    
    if (Ql_RIL_FindLine(line, len, "OK"))
    {  
        return  RIL_ATRSP_SUCCESS;
    }
    else if (Ql_RIL_FindLine(line, len, "ERROR"))
    {  
        return  RIL_ATRSP_FAILED;
    }
    else if (Ql_RIL_FindString(line, len, "+CME ERROR"))
    {
        return  RIL_ATRSP_FAILED;
    }
    else if (Ql_RIL_FindString(line, len, "+CMS ERROR:"))
    {
        return  RIL_ATRSP_FAILED;
    }
    
    return RIL_ATRSP_CONTINUE; //continue wait
}


#endif
