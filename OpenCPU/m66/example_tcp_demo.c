
/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Quectel Co., Ltd. 2014
*
*****************************************************************************/
/*****************************************************************************
 *
 * Filename:
 * ---------
 *   example_tcp_demo.c 
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 * This example demonstrates how to program GPRS,TCP, and implement the TCP socket connection 
 * establishment, and send a package of data to server via TCP socket.
 * 
 * ---------------------
 * Usage:
 * -----------
 * If customer wants to run this example, the following steps should be followed:
 *     1. Change APN name and access account in variable "m_GprsConfig".
 *     2. Change server IP address and port number in variables "m_SrvADDR" and "m_SrvPort".
 *     3. Set "C_PREDEF=-D __EXAMPLE_TCP_DEMO__" in gcc_makefile file. 
 *     4. Compile this example using the "make clean/new" in command-line.
 *     5. Download the image bin to module to run.
 *    
 *     Note:
 *     All trace information will be output through DEBUG port.
 *
 ****************************************************************************/
#ifdef __EXAMPLE_TCP_DEMO__
#include "custom_feature_def.h"
#include "ril.h"
#include "ril_util.h"
#include "ril_network.h"
#include "ql_stdlib.h"
#include "ql_error.h"
#include "ql_trace.h"
#include "ql_system.h"
#include "ql_timer.h"
#include "ql_gprs.h"
#include "ql_socket.h"
#include "ql_uart.h"


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


/************************************************************************/
/* Definition for GPRS PDP context                                      */
/************************************************************************/
static ST_GprsConfig m_GprsConfig = {
    "CMNET",    // APN name
    "",         // User name for APN
    "",         // Password for APN
    0,
    NULL,
    NULL,
};

/************************************************************************/
/* Definition for Server IP Address and Socket Port Number              */
/************************************************************************/
static u8  m_SrvADDR[20] = "116.247.104.27\0";
static u32 m_SrvPort = 6005;

#define  SOC_RECV_BUFFER_LEN  1460
static s32 m_GprsActState    = 0;   // GPRS PDP activation state, 0= not activated, 1=activated
static s32 m_SocketId        = -1;  // Store socket Id that returned by Ql_SOC_Create()
static s32 m_SocketConnState = 0;   // Socket connection state, 0= disconnected, 1=connected
static u8  m_SocketRcvBuf[SOC_RECV_BUFFER_LEN];


/************************************************************************/
/* Declarations for GPRS and TCP socket callback                        */
/************************************************************************/
//
// This callback function is invoked when GPRS drops down.
static void Callback_GPRS_Deactived(u8 contextId, s32 errCode, void* customParam );
//
// This callback function is invoked when the socket connection is disconnected by server or network.
static void Callback_Socket_Close(s32 socketId, s32 errCode, void* customParam );
//
// This callback function is invoked when socket data arrives.
static void Callback_Socket_Read(s32 socketId, s32 errCode, void* customParam );
//
// This callback function is invoked in the following case:
// The return value is less than the data length to send when calling Ql_SOC_Send(), which indicates
// the socket buffer is full. Application should stop sending socket data till this callback function
// is invoked, which indicates application can continue to send data to socket.
static void Callback_Socket_Write(s32 socketId, s32 errCode, void* customParam );

static void GPRS_TCP_Program(void);

static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{
     
}



/************************************************************************/
/*                                                                      */
/* The entrance to application, which is called by bottom system.       */
/*                                                                      */
/************************************************************************/
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

    APP_DEBUG("OpenCPU: TCP Example Demo\r\n");

    // Start message loop of this task
    while (TRUE)
    {
        // Retrieve a message from the message queue of this task. 
        // This task will pend here if no message in the message queue, till a new message arrives.
        Ql_OS_GetMessage(&msg);

        // Handle the received message
        switch (msg.message)
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
                // URC for module initialization state
            case URC_SYS_INIT_STATE_IND:
                APP_DEBUG("<-- Sys Init Status %d -->\r\n", msg.param2);
                if (SYS_STATE_SMSOK == msg.param2)
                {
                    // SMS option has been initialized, and application can program SMS
                    APP_DEBUG("<-- Application can program SMS -->\r\n");
                }
                break;

                // URC for SIM card state(change)
            case URC_SIM_CARD_STATE_IND:
                if (SIM_STAT_READY == msg.param2)
                {
                    APP_DEBUG("<-- SIM card is ready -->\r\n");
                }else{
                    APP_DEBUG("<-- SIM card is not available, cause:%d -->\r\n", msg.param2);
                    /* cause: 0 = SIM card not inserted
                     *        2 = Need to input PIN code
                     *        3 = Need to input PUK code
                     *        9 = SIM card is not recognized
                     */
                }
                break;

                // URC for GSM network state(change).
                // Application receives this URC message when GSM network state changes, such as register to 
                // GSM network during booting, GSM drops down.
            case URC_GSM_NW_STATE_IND:
                if (NW_STAT_REGISTERED == msg.param2 || NW_STAT_REGISTERED_ROAMING == msg.param2)
                {
                    APP_DEBUG("<-- Module has registered to GSM network -->\r\n");
                }else{
                    APP_DEBUG("<-- GSM network status:%d -->\r\n", msg.param2);
                    /* status: 0 = Not registered, module not currently search a new operator
                     *         2 = Not registered, but module is currently searching a new operator
                     *         3 = Registration denied 
                     */
                }
                break;

                // URC for GPRS network state(change).
                // Application receives this URC message when GPRS network state changes, such as register to 
                // GPRS network during booting, GSM drops down.
            case URC_GPRS_NW_STATE_IND:
                if (NW_STAT_REGISTERED == msg.param2 || NW_STAT_REGISTERED_ROAMING == msg.param2)
                {
                    APP_DEBUG("<-- Module has registered to GPRS network -->\r\n");
                    
                    // Module has registered to GPRS network, and app may start to activate PDP and program TCP
                    GPRS_TCP_Program();
                }else{
                    APP_DEBUG("<-- GPRS network status:%d -->\r\n", msg.param2);
                    /* status: 0 = Not registered, module not currently search a new operator
                     *         2 = Not registered, but module is currently searching a new operator
                     *         3 = Registration denied 
                     */

                    // If GPRS drops down and currently socket connection is on line, app should close socket
                    // and check signal strength. And try to reset the module.
                    if (NW_STAT_NOT_REGISTERED == msg.param2 && m_GprsActState)
                    {// GPRS drops down
                        u32 rssi;
                        u32 ber;
                        s32 nRet = RIL_NW_GetSignalQuality(&rssi, &ber);
                        APP_DEBUG("<-- Signal strength:%d, BER:%d -->\r\n", rssi, ber);
                    }
                }
                break;
            default:
                APP_DEBUG("<-- Other URC: type=%d\r\n", msg.param1);
                break;
            }
            break;
        default:
            break;
        }
    }
}
//
//
// Activate GPRS and program TCP.
//
static void GPRS_TCP_Program(void)
{
    s32 ret;
    s32 pdpCntxtId;
    ST_PDPContxt_Callback callback_gprs_func = {
        NULL,
        Callback_GPRS_Deactived
    };
    ST_SOC_Callback callback_soc_func = {
        NULL,
        Callback_Socket_Close,
        NULL,
        Callback_Socket_Read,    
        Callback_Socket_Write
    };

    //1. Register GPRS callback
    pdpCntxtId = Ql_GPRS_GetPDPContextId();
    if (GPRS_PDP_ERROR == pdpCntxtId)
    {
        APP_DEBUG("No PDP context is available\r\n");
        return ret;
    }
    ret = Ql_GPRS_Register(pdpCntxtId, &callback_gprs_func, NULL);
    if (GPRS_PDP_SUCCESS == ret)
    {
        APP_DEBUG("<-- Register GPRS callback function -->\r\n");
    }else{
        APP_DEBUG("<-- Fail to register GPRS, cause=%d. -->\r\n", ret);
        return;
    }

    //2. Configure PDP
    ret = Ql_GPRS_Config(pdpCntxtId, &m_GprsConfig);
    if (GPRS_PDP_SUCCESS == ret)
    {
        APP_DEBUG("<-- Configure PDP context -->\r\n");
    }else{
        APP_DEBUG("<-- Fail to configure GPRS PDP, cause=%d. -->\r\n", ret);
        return;
    }

    //3. Activate GPRS PDP context
    APP_DEBUG("<-- Activating GPRS... -->\r\n");
    ret = Ql_GPRS_ActivateEx(pdpCntxtId, TRUE);
    if (ret == GPRS_PDP_SUCCESS)
    {
        m_GprsActState = 1;
        APP_DEBUG("<-- Activate GPRS successfully. -->\r\n\r\n");
    }else{
        APP_DEBUG("<-- Fail to activate GPRS, cause=%d. -->\r\n\r\n", ret);
        return;
    }

    //4. Register Socket callback
    ret = Ql_SOC_Register(callback_soc_func, NULL);
    if (SOC_SUCCESS == ret)
    {
        APP_DEBUG("<-- Register socket callback function -->\r\n");
    }else{
        APP_DEBUG("<-- Fail to register socket callback, cause=%d. -->\r\n", ret);
        return;
    }

    //5. Create socket
    m_SocketId = Ql_SOC_Create(pdpCntxtId, SOC_TYPE_TCP);
    if (m_SocketId >= 0)
    {
        APP_DEBUG("<-- Create socket successfully, socket id=%d. -->\r\n", m_SocketId);
    }else{
        APP_DEBUG("<-- Fail to create socket, cause=%d. -->\r\n", m_SocketId);
        return;
    }		

    //6. Connect to server
    {
        //6.1 Convert IP format
        u8 m_ipAddress[4]; 
        Ql_memset(m_ipAddress,0,5);
        ret = Ql_IpHelper_ConvertIpAddr(m_SrvADDR, (u32 *)m_ipAddress);
        if (SOC_SUCCESS == ret) // ip address is xxx.xxx.xxx.xxx
        {
            APP_DEBUG("<-- Convert Ip Address successfully,m_ipaddress=%d,%d,%d,%d -->\r\n",m_ipAddress[0],m_ipAddress[1],m_ipAddress[2],m_ipAddress[3]);
        }else{
            APP_DEBUG("<-- Fail to convert IP Address --> \r\n");
            return;
        }

        //6.2 Connect to server
        APP_DEBUG("<-- Connecting to server(IP:%d.%d.%d.%d, port:%d)... -->\r\n", m_ipAddress[0],m_ipAddress[1],m_ipAddress[2],m_ipAddress[3], m_SrvPort);
        ret = Ql_SOC_ConnectEx(m_SocketId,(u32) m_ipAddress, m_SrvPort, TRUE);
        if (SOC_SUCCESS == ret)
        {
            m_SocketConnState = 1;
            APP_DEBUG("<-- Connect to server successfully -->\r\n");
        }else{
            APP_DEBUG("<-- Fail to connect to server, cause=%d -->\r\n", ret);
            APP_DEBUG("<-- Close socket.-->\r\n");
            Ql_SOC_Close(m_SocketId);
            m_SocketId = -1;
            return;
        }
    }

    //7. Send data to socket
    {
        //7.1 Send data
        char pchData[200];
        s32  dataLen = 0;
        u64  ackNum = 0;
        Ql_memset(pchData, 0x0, sizeof(pchData));
        dataLen = Ql_sprintf(pchData + dataLen, "%s", "A B C D E F G");
        APP_DEBUG("<-- Sending data(len=%d): %s-->\r\n", dataLen, pchData);
        ret = Ql_SOC_Send(m_SocketId, (u8*)pchData, dataLen);
        if (ret == dataLen)
        {
            APP_DEBUG("<-- Send socket data successfully. --> \r\n");
        }else{
            APP_DEBUG("<-- Fail to send socket data. --> \r\n");
            Ql_SOC_Close(m_SocketId);
            return;
        }

        //7.2 Check ACK number
        do 
        {
            ret = Ql_SOC_GetAckNumber(m_SocketId, &ackNum);
            APP_DEBUG("<-- Current ACK Number:%llu/%d --> \r\n", ackNum, dataLen);
            Ql_Sleep(500);
        } while (ackNum != dataLen);
        APP_DEBUG("<-- Server has received all data --> \r\n");
    }
/*
    //8. Close socket
    ret = Ql_SOC_Close(m_SocketId);
    APP_DEBUG("<-- Close socket[%d], cause=%d --> \r\n", m_SocketId, ret);

    //9. Deactivate GPRS
    APP_DEBUG("<-- Deactivating GPRS... -->\r\n");
    ret = Ql_GPRS_DeactivateEx(pdpCntxtId, TRUE);
    APP_DEBUG("<-- Deactivated GPRS, cause=%d -->\r\n\r\n", ret);
*/
    APP_DEBUG("< Finish >\r\n");
}
//
//
// This callback function is invoked when GPRS drops down. The cause is in "errCode".
//
static void Callback_GPRS_Deactived(u8 contextId, s32 errCode, void* customParam )
{
    if (errCode == SOC_SUCCESS)
    {
        APP_DEBUG("<--CallBack: deactivated GPRS successfully.-->\r\n"); 
    }else{
        APP_DEBUG("<--CallBack: fail to deactivate GPRS, cause=%d)-->\r\n", errCode); 
    }
    if (1 == m_GprsActState)
    {
        m_GprsActState = 0;
        APP_DEBUG("<-- GPRS drops down -->\r\n"); 
    }
}
//
//
// This callback function is invoked when the socket connection is disconnected by server or network.
//
static void Callback_Socket_Close(s32 socketId, s32 errCode, void* customParam )
{
    if (errCode == SOC_SUCCESS)
    {
        APP_DEBUG("<--CallBack: close socket successfully.-->\r\n"); 
    }
    else if(errCode == SOC_BEARER_FAIL)
    {   
        APP_DEBUG("<--CallBack: fail to close socket,(socketId=%d,error_cause=%d)-->\r\n", socketId, errCode); 
    }else{
        APP_DEBUG("<--CallBack: close socket failure,(socketId=%d,error_cause=%d)-->\r\n", socketId, errCode); 
    }
    if (1 == m_SocketConnState)
    {
        APP_DEBUG("<-- Socket connection is disconnected -->\r\n"); 
        APP_DEBUG("<-- Close socket at module side -->\r\n"); 
        Ql_SOC_Close(socketId);
        m_SocketConnState = 0;
    }
}
//
//
// This callback function is invoked when socket data arrives.
// The program should call Ql_SOC_Recv to read all data out of the socket buffer.
//
static void Callback_Socket_Read(s32 socketId, s32 errCode, void* customParam )
{
    s32 ret;
    s32 offset = 0;
    if (errCode)
    {
        APP_DEBUG("<-- Close socket -->\r\n");
        Ql_SOC_Close(socketId);
        m_SocketId = -1;
        return;
    }

    Ql_memset(m_SocketRcvBuf, 0, SOC_RECV_BUFFER_LEN);
    do
    {
        ret = Ql_SOC_Recv(socketId, m_SocketRcvBuf + offset, SOC_RECV_BUFFER_LEN - offset);
        if((ret < SOC_SUCCESS) && (ret != SOC_WOULDBLOCK))
        {
            APP_DEBUG("<-- Fail to receive data, cause=%d.-->\r\n",ret);
            APP_DEBUG("<-- Close socket.-->\r\n");
            Ql_SOC_Close(socketId);
            m_SocketId = -1;
            break;
        }
        else if(SOC_WOULDBLOCK == ret)  // Read finish
        {
            APP_DEBUG("<-- Receive data from server,len(%d):%s\r\n", offset, m_SocketRcvBuf);
            break;
        }
        else // Continue to read...
        {
            if (SOC_RECV_BUFFER_LEN == offset)  // buffer if full
            {
                APP_DEBUG("<-- Receive data from server,len(%d):%s\r\n", offset, m_SocketRcvBuf);
                Ql_memset(m_SocketRcvBuf, 0, SOC_RECV_BUFFER_LEN);
                offset = 0;
            }else{
                offset += ret;
            }
            continue;
        }
    } while (TRUE);
}
//
//
// This callback function is invoked in the following case:
// The return value is less than the data length to send when calling Ql_SOC_Send(), which indicates
// the socket buffer is full. Application should stop sending socket data till this callback function
// is invoked, which indicates application can continue to send data to socket.
static void Callback_Socket_Write(s32 socketId, s32 errCode, void* customParam)
{
    if (errCode < 0)
    {
        APP_DEBUG("<-- Socket error(error code:%d), close socket.-->\r\n", errCode);
        Ql_SOC_Close(socketId);
        m_SocketId = -1;        
    }else{
        APP_DEBUG("<-- You can continue to send data to socket -->\r\n");
    }
}

#endif  //__EXAMPLE_TCP_DEMO__
