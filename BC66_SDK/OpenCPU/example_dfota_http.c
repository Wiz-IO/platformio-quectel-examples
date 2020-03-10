
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
 *   example_dfota_http.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example demonstrates how to use dfota_http function with RIL APIs in OpenCPU.
 *   Input the specified command through any uart port and the result will be 
 *   output through the debug port.
 *   App bin or core must be put in server first.It will be used to upgrade data through the air.
 *
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_DFOTA_HTTP__" in gcc_makefile file. And compile the 
 *     app using "make clean/new".
 *     Download image bin to module to run.
 * 
 *   Operation:
 *  
 *     step 1: you must put your application bin in your server.
 *     step 2: replace the "APP_BIN_URL" with your own .
 *     step 3: input string : start dfota=XXXX, XXXX stands for URL.
 *
 *     The URL format for http is:   http://hostname:port/filePath/fileName                                
 *     NOTE:  if ":port" is be ignored, it means the port is http default port(80) 
 *
 *     eg1: http://23.11.67.89/file/xxx.bin 
 *     eg2: http://www.quectel.com:8080/file/xxx.bin
 * Author:
 * -------
 * -------
 *
 *============================================================================
 *             HISTORY
 *----------------------------------------------------------------------------
 * 
 ****************************************************************************/
#ifdef __EXAMPLE_DFOTA_HTTP__
#include "custom_feature_def.h"
#include "ril.h"
#include "ril_util.h"
#include "ql_error.h"
#include "ql_stdlib.h"
#include "ql_trace.h"
#include "ql_uart.h"
#include "ql_system.h"
#include "ril_dfota.h"



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



/****************************************************************************
* Define local param
****************************************************************************/
#define APP_BIN_URL   "http://124.74.41.170:5015/"

#define URL_LEN 255
#define READ_BUF_LEN 1024

static u8 m_URL_Buffer[URL_LEN];
static u8 m_Read_Buffer[READ_BUF_LEN];
static Enum_SerialPort m_myUartPort  = UART_PORT0;

/****************************************************************************
* Define local function
****************************************************************************/
static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);
static s32 ATResponse_Handler(char* line, u32 len, void* userData);

/****************************************************************************
* Main task
****************************************************************************/

void proc_main_task(s32 taskId)
{ 
	s32 ret;
	ST_MSG msg;

	// Register & open UART port
	ret = Ql_UART_Register(m_myUartPort, CallBack_UART_Hdlr, NULL);
	if (ret < QL_RET_OK)
	{
		Ql_Debug_Trace("Fail to register serial port[%d], ret=%d\r\n", m_myUartPort, ret);
	}
	ret = Ql_UART_Open(m_myUartPort, 115200, FC_NONE);
	if (ret < QL_RET_OK)
	{
		Ql_Debug_Trace("Fail to open serial port[%d], ret=%d\r\n", m_myUartPort, ret);
	}
	 APP_DEBUG("\r\n<-- OpenCPU: Dfota Example-->\r\n");

	// START MESSAGE LOOP OF THIS TASK
	while(TRUE)
	{
		Ql_OS_GetMessage(&msg);
		switch(msg.message)
		{
		case MSG_ID_RIL_READY:
			APP_DEBUG("<-- RIL is ready -->\r\n");
			Ql_RIL_Initialize();
			
			break;
		case MSG_ID_URC_INDICATION:
			switch (msg.param1)
			{
			case URC_SYS_INIT_STATE_IND:
				APP_DEBUG("<-- Sys Init Status %d -->\r\n", msg.param2);
				break;
			case URC_SIM_CARD_STATE_IND:
				APP_DEBUG("<-- SIM Card Status:%d -->\r\n", msg.param2);
				break;			  
			case URC_EGPRS_NW_STATE_IND:
				APP_DEBUG("<-- EGPRS Network Status:%d -->\r\n", msg.param2);
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


static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{
    u8 *p=NULL;
    u8 *q = NULL;

    if (m_myUartPort == port)
    {    
        switch (msg)
        {
            case EVENT_UART_READY_TO_READ:
            {
                Ql_memset(m_Read_Buffer, 0x0, sizeof(m_Read_Buffer));
                Ql_UART_Read(port, m_Read_Buffer, READ_BUF_LEN);

                //start dfota=xxx.bin
                p = Ql_strstr(m_Read_Buffer, "start dfota=");
                if(p)
                {
                    q = Ql_strstr(m_Read_Buffer, "\r\n");//delete\r\n
                    if(q)
                    {
                        *q = '\0';
                    }

                    p += Ql_strlen("start dfota=");
                    Ql_memset(m_URL_Buffer, 0, URL_LEN);
                    
                    //http://hostname:port/filePath/fileName
                    Ql_sprintf(m_URL_Buffer, "%s%s",APP_BIN_URL,p);
                    APP_DEBUG("\r\n<-- URL:%s-->\r\n",m_URL_Buffer);
                    
                    RIL_DFOTA_Upgrade(m_URL_Buffer);
                    break;
                }

			     // AT command
                Ql_UART_Write(m_myUartPort, m_Read_Buffer, READ_BUF_LEN);

                p = Ql_strstr((char*)m_Read_Buffer, "\r\n");
                if (p)
                {
                    *(p + 0) = '\0';
                    *(p + 1) = '\0';
                }
                if (Ql_strlen((char*)m_Read_Buffer) == 0)
                {
                    return;
                }
                Ql_RIL_SendATCmd((char*)m_Read_Buffer, Ql_strlen((char*)m_Read_Buffer), ATResponse_Handler, NULL, 0);
                break;
            }

            default:
            break;
        }
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


#endif // __EXAMPLE_DFOTA_HTTP__

