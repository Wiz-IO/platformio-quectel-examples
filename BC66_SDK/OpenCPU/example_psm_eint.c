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
 *   example_psm_eint.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example demonstrates how to use psm_eint function with APIs in OpenCPU.
 *
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_PSM_EINT__" in gcc_makefile file. And compile the 
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
#ifdef __EXAMPLE_PSM_EINT__
#include "ql_type.h"
#include "ql_trace.h"
#include "ql_psm_eint.h"
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
* psm_eint param
******************************************************************/
static s32 m_param = 0;
volatile bool PSM_EINT_Flag = FALSE;

/*****************************************************************
* uart callback function
******************************************************************/
static Enum_SerialPort m_myUartPort  = UART_PORT0;
static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);
static s32 ATResponse_Handler(char* line, u32 len, void* userData);
#define SERIAL_RX_BUFFER_LEN  2048
static u8 m_RxBuf_Uart[SERIAL_RX_BUFFER_LEN];

// PSM_EINT callback
static void PSM_EINT_handler(void* param);

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
    
    APP_DEBUG("\r\n<--OpenCPU: psm_enit function demo-->\r\n"); 

    //rtc register
    ret = Ql_Psm_Eint_Register(PSM_EINT_handler,&m_param);
    if(ret != QL_RET_OK)
    {
	    APP_DEBUG("\r\n<--psm_eint register failed!ret(%d)-->\r\n",ret);          
    }
	else 
	{
        APP_DEBUG("\r\n<--psm_eint register successful-->\r\n"); 
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
				 
				 //enable  psm event. if the module enter psm mode	which is reported by URC(+QNBIOTEVENT:	"ENTER PSM").
				 RIL_QNbiotEvent_Enable(PSM_EVENT);
    			 if(TRUE == PSM_EINT_Flag)//deep sleep wake up by psm_eint
    			 {
	                 APP_DEBUG("\r\n<--The module wake up from deep sleep mode by psm_eint-->\r\n"); 
					 PSM_EINT_Flag = FALSE;
    			 }
    			 else 
    			 {
                   APP_DEBUG("\r\n<--The module reset or other ways to wake up->\r\n"); 
    			 }
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


// when the module wakes up from the deep sleep, calls psm_eint callback first,  then initializes the opencpu, 
//so the API interface of Ql cannot be called in here.
void PSM_EINT_handler(void* param)
{
	PSM_EINT_Flag = TRUE;
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
