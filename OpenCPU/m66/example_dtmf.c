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
 *   example_dtmf.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example demonstrates how to program dtmf in OpenCPU.
 *   All debug information will be output through DEBUG port.
 *
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_DTMF__" in gcc_makefile file. And compile the 
 *     app using "make clean/new".
 *     Download image bin to module to run.
 * 
 *   Operation:
 *            
 *     If input "open", that will open tone detection.
 *     If input "close", that will close tone detection.
 *     If input "send:<dtmfcode>,<continuancetime>,<mutetime>", that will send dtmf string with uplink/downlink volume 7.
 *     If input "set:<mode>,<prefixpause>,<lowthreshold>,<highthreshold>", that will configure detection threshold.
 *     If input "get:<mode>", that will get settings of current mode.
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
#ifdef __EXAMPLE_DTMF__
#include "custom_feature_def.h"
#include "ril.h"
#include "ril_util.h"
#include "ril_telephony.h"
#include "ql_stdlib.h"
#include "ql_error.h"
#include "ql_trace.h"
#include "ql_uart.h"
#include "ql_system.h"

#include "ril_dtmf.h"



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


#define SERIAL_RX_BUFFER_LEN  2048



// Define the UART port and the receive data buffer
static Enum_SerialPort m_myUartPort  = UART_PORT1;
static u8 m_RxBuf_Uart1[SERIAL_RX_BUFFER_LEN];
static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);
static s32 ATResponse_Handler(char* line, u32 len, void* userData);
static s32 DTMF_Program( u8 *data );
static void cb_WDTMF_hdl( s32 result );
static void cb_ToneDet_hdl( s32 dtmfCode, s32 timems );

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

    APP_DEBUG("OpenCPU: DTMF EXAMPLE Application\r\n");

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
        default:
            break;
        }
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
    switch (msg)
    {
    case EVENT_UART_READY_TO_READ:
        {
            if (m_myUartPort == port)
            {
                s32 totalBytes = ReadSerialPort(port, m_RxBuf_Uart1, sizeof(m_RxBuf_Uart1));
                if (totalBytes <= 0)
                {
                    APP_DEBUG("<-- No data in UART buffer! -->\r\n");
                    return;
                }
                {// Read data from UART
                    s32 ret;
                    char* pCh = NULL;
                    
                    // Echo
                    Ql_UART_Write(m_myUartPort, m_RxBuf_Uart1, totalBytes);

                    pCh = Ql_strstr((char*)m_RxBuf_Uart1, "\r\n");
                    if (pCh)
                    {
                        *(pCh + 0) = '\0';
                        *(pCh + 1) = '\0';
                    }

                    // No permission for single <cr><lf>
                    if (Ql_strlen((char*)m_RxBuf_Uart1) == 0)
                    {
                        return;
                    }
                    else if ( DTMF_Program(m_RxBuf_Uart1) )
                    {
                        ret = Ql_RIL_SendATCmd((char*)m_RxBuf_Uart1, totalBytes, ATResponse_Handler, NULL, 0);
                    }
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

s32 DTMF_Program( u8 *data )
{
    s32 ret;
    
    if ( NULL == data)
    {
        return -1;
    }
    else if ( Ql_strstr(data, "open") )
    {
        ret = RIL_ToneDet_Open( cb_ToneDet_hdl );
        if ( RIL_AT_SUCCESS == ret )
        {
            APP_DEBUG( "<-- ToneDet open success, ret = %d -->\r\n", ret );
        }
        else
        {
            APP_DEBUG( "<-- ToneDet open failed, ret = %d -->\r\n", ret );
        }
    }
    else if ( Ql_strstr(data, "close") )
    {
        ret = RIL_ToneDet_Close();
        if ( RIL_AT_SUCCESS == ret )
        {
            APP_DEBUG( "<-- ToneDet close success, ret = %d -->\r\n", ret );
        }
        else
        {
            APP_DEBUG( "<-- ToneDet close failed, ret = %d -->\r\n", ret );
        }
    }
    else if ( Ql_strstr(data, "send") )
    {
        u8 buff[50];
        Ql_sscanf( data, "%*[^:]:%[^\r\n]", buff );
        APP_DEBUG( "<-- Send: %s -->\r\n", buff );
        
        ret = RIL_WDTMF_Send( 7, 7, buff, cb_WDTMF_hdl );
        if ( RIL_AT_SUCCESS == ret )
        {
            APP_DEBUG( "<-- excute wdtmf success, ret = %d -->\r\n", ret );
        }
        else
        {
            APP_DEBUG( "<-- excute wdtmf failed, ret = %d -->\r\n", ret );
        }
    }
    else if ( Ql_strstr(data, "set") )
    {
        u32 mode, pause, low, high;
       
        Ql_sscanf( data, "%*[^:]:%d,%d,%d,%d", &mode, &pause, &low, &high );
        APP_DEBUG( "<-- Set: %d, %d, %d, %d -->\r\n", mode, pause, low, high );
        
        ret = RIL_ToneDet_Set( mode, pause, low, high );
        if ( RIL_AT_SUCCESS == ret )
        {
            APP_DEBUG( "<-- tonedet set success, ret = %d -->\r\n", ret );
        }
        else
        {
            APP_DEBUG( "<-- tonedet set failed, ret = %d -->\r\n", ret );
        }
    }
    else if ( Ql_strstr(data, "get") )
    {
        u32 mode, low, high;

        mode = low = high = 0;
        Ql_sscanf( data, "%*[^:]:%d", &mode );
        ret = RIL_ToneDet_Get( mode, &low, &high );
        if ( RIL_AT_SUCCESS == ret )
        {
            APP_DEBUG( "<-- tonedet get success: %d, %d, %d -->\r\n", mode, low, high );
        }
        else
        {
            APP_DEBUG( "<-- tonedet get failed, ret = %d -->\r\n", ret );
        }                    
    }
    else
    {
        return -1;
    }

    return 0;
}

static s32 ATResponse_Handler(char* line, u32 len, void* userData)
{
    Ql_UART_Write(m_myUartPort, (u8*)line, len);
    
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

/*****************************************************************
* Function:     cb_WDTMF_hdl
* 
* Description:
*               This function is used to handle the urc of wdtmf.
*
* Parameters:
*                result:
*                     [OUT] Indicate status of sending DTMF. 
*                                If <result> is 5, it means sending DTMF successfully;
*                                If <result> is not 5, it means sending DTMF unsuccessfully;
*
* Return:        
*
*****************************************************************/
void cb_WDTMF_hdl( s32 result )
{
    if ( 5 == result )
    {
        APP_DEBUG( "<-- wdtmf send success, ret = %d -->\r\n", result );
    }
    else
    {
        APP_DEBUG( "<-- wdtmf send failed, ret = %d -->\r\n", result );
    }
}

/*****************************************************************
* Function:     cb_ToneDet_hdl
* 
* Description:
*               This function is used to handle DTMF detected.
*
* Parameters:
*               dtmfCode:      
*                   [OUT] detected DTMF tone code corresponding ASSCII.
*               timems:      
*                   [OUT] persistence time of the tone, unit is ms.
*                             only when <dtmfCode> is 69(1400Hz) or 70(1400Hz) <timems> is available, otherwise it will always be -1.
*
* Return:        
*                            
*****************************************************************/
void cb_ToneDet_hdl( s32 dtmfCode, s32 timems )
{
    APP_DEBUG( "<-- TONE DTMF Detected: dtmfcode,timems[ %d, %d ]  -->\r\n", dtmfCode, timems );
}

#endif // __EXAMPLE_DTMF__