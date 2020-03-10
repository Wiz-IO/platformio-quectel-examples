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
 *   example_tcpclient_api.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example demonstrates how to establish a TCP connection, when the module 
 *   is used for the client. Input the specified command through any serial port 
 *   and the result will be output through the debug port.We have adopted a timeout 
 *   mechanism,if in the process of connecting socket or getting the TCP socket ACK 
 *   number overtime 90s, the socket will be close and the network will be deactivated.
 *   In most of TCPIP functions,  return -22(SOC_WOULDBLOCK) doesn't indicate failed.
 *   It means app should wait, till the callback function is called.
 *   The app can get the information of success or failure in callback function.
 *   Get more info about return value. Please read the "OPEN_CPU_DGD" document.
 *
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_TCPCLIENT_API__" in gcc_makefile file. And compile the 
 *     app using "make clean/new".
 *     Download image bin to module to run.
 * 
 *   Operation:
 *            
 *            Command:Set_Srv_Param=<srv ip>,<srv port>
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
#ifdef __EXAMPLE_TCPCLIENT_API__  
#include "custom_feature_def.h"
#include "ql_common.h"
#include "ql_trace.h"
#include "ql_uart.h"
#include "ql_network.h"
#include "ql_socket.h"
#include "ql_timer.h"
#include "ril_sim.h"
#include "ril_network.h"
#include "ql_stdlib.h"

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
* define process state
******************************************************************/
typedef enum{
    TCP_STATE_NW_GET_SIMSTATE,
    TCP_STATE_NW_QUERY_STATE,
    TCP_STATE_GET_DNSADDRESS,
    TCP_STATE_GET_LOCALIP,
    TCP_STATE_CHACK_SRVADDR,
    TCP_STATE_SOC_REGISTER,
    TCP_STATE_SOC_CREATE,
    TCP_STATE_SOC_CONNECT,
    TCP_STATE_SOC_CONNECTING,
    TCP_STATE_SOC_SEND,
    TCP_STATE_SOC_SENDING,
    TCP_STATE_SOC_ACK,
    TCP_STATE_SOC_CLOSE,
    TCP_STATE_TOTAL_NUM
}Enum_TCPSTATE;
static u8 m_tcp_state = TCP_STATE_NW_GET_SIMSTATE;

/*****************************************************************
* UART Param
******************************************************************/
#define SERIAL_RX_BUFFER_LEN  2048
static u8 m_RxBuf_Uart[SERIAL_RX_BUFFER_LEN];

/*****************************************************************
* timer param
******************************************************************/
#define TCP_TIMER_ID              TIMER_ID_USER_START
#define TCP_TIMEOUT_90S_TIMER_ID (TIMER_ID_USER_START + 1)   //timeout

#define TCP_TIMER_PERIOD     800
#define TIMEOUT_90S_PERIOD   90000

static s32 timeout_90S_monitor = FALSE;

/*****************************************************************
* Server Param
******************************************************************/
#define SRVADDR_BUFFER_LEN  100
static u8  m_SrvADDR[SRVADDR_BUFFER_LEN] ="220.180.xxx.xxx\0";
static u32 m_SrvPort = 8057;
static u8  m_ipaddress[IP_ADDR_LEN];  //only save the number of server ip, remove the comma
/*****************************************************************
* Other Param
******************************************************************/
#define SEND_BUFFER_LEN     1400
#define RECV_BUFFER_LEN     1400
static u8 m_send_buf[SEND_BUFFER_LEN];
static u8 m_recv_buf[RECV_BUFFER_LEN];
static u32 m_nSentLen  = 0;      // Bytes of number sent data through current socket    

static s32 m_socketid = -1; 
static s32 m_remain_len = 0;     // record the remaining number of bytes in send buffer.
static char *m_pCurrentPos = NULL; 

/*****************************************************************
* GPRS and socket callback function
******************************************************************/
void callback_socket_connect(s32 socketId, s32 errCode, void* customParam );
void callback_socket_close(s32 socketId, s32 errCode, void* customParam );
void callback_socket_accept(s32 listenSocketId, s32 errCode, void* customParam );
void callback_socket_read(s32 socketId, s32 errCode, void* customParam );
void Callback_GetIpByName(u8 contexId,s32 errCode,  u32 ipAddrCnt, u8* ipAddr);



ST_SOC_Callback      callback_soc_func=
{
    callback_socket_connect,
    callback_socket_close,
    callback_socket_accept,
    callback_socket_read,    
};

/*****************************************************************
* uart callback function
******************************************************************/
static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);

/*****************************************************************
* timer callback function
******************************************************************/
static void TCP_Callback_Timer(u32 timerId, void* param);

/*****************************************************************
* other subroutines
******************************************************************/
extern s32 Analyse_Command(u8* src_str,s32 symbol_num,u8 symbol, u8* dest_buf);
static s32 ReadSerialPort(Enum_SerialPort port, /*[out]*/u8* pBuffer, /*[in]*/u32 bufLen);
static void proc_handle(u8* pData,s32 len);


static s32 ret;
void proc_main_task(s32 taskId)
{
    ST_MSG msg;

    // Register & open UART port
	Ql_UART_Register(UART_PORT0, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(UART_PORT0, 115200, FC_NONE);

    APP_DEBUG("<--OpenCPU: TCP Client.-->\r\n");

    //register & start timer 
    Ql_Timer_Register(TCP_TIMER_ID, TCP_Callback_Timer, NULL);
    Ql_Timer_Start(TCP_TIMER_ID, TCP_TIMER_PERIOD, TRUE);

    Ql_Timer_Register(TCP_TIMEOUT_90S_TIMER_ID, TCP_Callback_Timer, NULL);
    timeout_90S_monitor = FALSE;
    
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

static void proc_handle(u8* pData,s32 len)
{
    char *p = NULL;
    u8 srvport[10];
	
    //command: Set_Srv_Param=<srv ip>,<srv port>
    p = Ql_strstr(pData,"Set_Srv_Param=");
    if (p)
    {
        Ql_memset(m_SrvADDR, 0, SRVADDR_BUFFER_LEN);
        if (Analyse_Command(pData, 1, '>', m_SrvADDR))
        {
            APP_DEBUG("<--Server Address Parameter Error.-->\r\n");
            return;
        }
        Ql_memset(srvport, 0, 10);
        if (Analyse_Command(pData, 2, '>', srvport))
        {
            APP_DEBUG("<--Server Port Parameter Error.-->\r\n");
            return;
        }
        m_SrvPort = Ql_atoi(srvport);
        APP_DEBUG("<--Set TCP Server Parameter Successfully<%s>,<%d>.-->\r\n",m_SrvADDR,m_SrvPort);

        m_tcp_state = TCP_STATE_NW_GET_SIMSTATE;
        APP_DEBUG("<--Restart the TCP connection process.-->\r\n");

        return;
    }

    //if not command,send it to server
    m_pCurrentPos = m_send_buf;
    Ql_strcpy(m_pCurrentPos + m_remain_len, pData);
    
    m_remain_len = Ql_strlen(m_pCurrentPos);
    
}

static void TCP_Callback_Timer(u32 timerId, void* param)
{
    if (TCP_TIMEOUT_90S_TIMER_ID == timerId)
    {
        APP_DEBUG("<--90s time out!!!-->\r\n");
        APP_DEBUG("<-- Close socket.-->\r\n");
        
        Ql_SOC_Close(m_socketid);
        m_socketid = -1;

        m_tcp_state = TCP_STATE_NW_GET_SIMSTATE;

        timeout_90S_monitor = FALSE;
    }
    else if (TCP_TIMER_ID == timerId)
    {
        //APP_DEBUG("<--...........m_tcp_state=%d..................-->\r\n",m_tcp_state);
        switch (m_tcp_state)
        {
            case TCP_STATE_NW_GET_SIMSTATE:
            {
                s32 simStat = 0;
                RIL_SIM_GetSimState(&simStat);
                if (simStat == SIM_STAT_READY)
                {
                    m_tcp_state = TCP_STATE_NW_QUERY_STATE;
                    APP_DEBUG("<--SIM card status is normal!-->\r\n");
                }else
                {
                    APP_DEBUG("<--SIM card status is unnormal!-->\r\n");
                }
                break;
            }        
            case TCP_STATE_NW_QUERY_STATE:
            {
                s32 cgreg = 0;
                ret = RIL_NW_GetEGPRSState(&cgreg);
                APP_DEBUG("<--Network State:cgreg=%d-->\r\n",cgreg);
                if((cgreg == NW_STAT_REGISTERED)||(cgreg == NW_STAT_REGISTERED_ROAMING))
                {
                    m_tcp_state = TCP_STATE_GET_LOCALIP;
                }
                break;
            }
            case TCP_STATE_GET_LOCALIP:
            {
                ST_Addr_Info_t addr_info;

                Ql_memset(addr_info.addr, 0, IP_ADDR_LEN);
				addr_info.addr_len = IP_ADDR_LEN;
				
                ret = Ql_GetLocalIPAddress(0,&addr_info);
                if (ret == SOC_SUCCESS)
                {
                    APP_DEBUG("<--Get Local Ip successfully,Local Ip=%s-->\r\n",addr_info.addr);
                    m_tcp_state = TCP_STATE_GET_DNSADDRESS;
                }else
                {
                    APP_DEBUG("<--Get Local Ip failure,ret=%d.-->\r\n",ret);
                }
                break;
            }
			case TCP_STATE_GET_DNSADDRESS:
            {         
				ST_Dns_Info_t dns_info;
				
				dns_info.ip_type= IP_TYPE_IPV4;
				Ql_memset(dns_info.primaryAddr,0,IP_ADDR_LEN);
				Ql_memset(dns_info.bkAddr,0,IP_ADDR_LEN);
				dns_info.addr_len = IP_ADDR_LEN;
				
                ret =Ql_GetDNSAddress(0,&dns_info);
                if (ret == SOC_SUCCESS)
                {
                    APP_DEBUG("<--Get DNS address successfully,primaryAddr=%s,bkAddr=%s-->\r\n",dns_info.primaryAddr,dns_info.bkAddr);            
                    m_tcp_state = TCP_STATE_CHACK_SRVADDR;
                }else
                {
                    APP_DEBUG("<--Get DNS address failure,ret=%d.-->\r\n",ret);
                    m_tcp_state = TCP_STATE_CHACK_SRVADDR;
                }
                break;
            }
			case TCP_STATE_CHACK_SRVADDR:
            {
                ret = Ql_SocketCheckIp(m_SrvADDR);
                if(ret == TRUE) // ip address, xxx.xxx.xxx.xxx
                {
					Ql_memset(m_ipaddress, 0, IP_ADDR_LEN);
                    Ql_memcpy(m_ipaddress, m_SrvADDR, IP_ADDR_LEN);
                    APP_DEBUG("<--Convert Ip Address successfully,m_ipaddress=%s-->\r\n",m_ipaddress);
                    m_tcp_state = TCP_STATE_SOC_REGISTER;
                    
                }else  //domain name
                {
                    ret = Ql_IpHelper_GetIPByHostName(0, m_SrvADDR, Callback_GetIpByName);
                    if(ret == SOC_SUCCESS)
                    {
                        APP_DEBUG("<--Get ip by hostname successfully.-->\r\n");
                    }
                    else if(ret == SOC_NONBLOCK)
                    {
						m_tcp_state = TCP_STATE_TOTAL_NUM;
                        APP_DEBUG("<--Waiting for the result of Getting ip by hostname,ret=%d.-->\r\n",ret);
                        //waiting CallBack_getipbyname
                    }
                    else
                    {
						m_tcp_state = TCP_STATE_TOTAL_NUM;
                        APP_DEBUG("<--Get ip by hostname failure:ret=%d-->\r\n",ret); 
                    }
                }
                break;
            }
            case TCP_STATE_SOC_REGISTER:
            {
                ret = Ql_SOC_Register(callback_soc_func, NULL);
                if (SOC_SUCCESS == ret)
                {
                    APP_DEBUG("<--Register socket callback function successfully.-->\r\n");
                    m_tcp_state = TCP_STATE_SOC_CREATE;
                }else if (SOC_ERROR_ALREADY == ret)
                {
                    APP_DEBUG("<--Socket callback function has already been registered,ret=%d.-->\r\n",ret);
                    m_tcp_state = TCP_STATE_SOC_CREATE;
                }else
                {
                    APP_DEBUG("<--Register Socket callback function failure,ret=%d.-->\r\n",ret);
                }
                break;
            }
            case TCP_STATE_SOC_CREATE:
            {
                m_socketid = Ql_SOC_Create(0, SOC_TYPE_TCP);
                if (m_socketid >= 0)
                {
                    APP_DEBUG("<--Create socket id successfully,socketid=%d.-->\r\n",m_socketid);
                    m_tcp_state = TCP_STATE_SOC_CONNECT;
                }else
                {
                    APP_DEBUG("<--Create socket id failure,error=%d.-->\r\n",m_socketid);
                }
                break;
            }
            case TCP_STATE_SOC_CONNECT:
            {
                m_tcp_state = TCP_STATE_SOC_CONNECTING;
                ret = Ql_SOC_Connect(m_socketid,m_ipaddress, m_SrvPort);
                if(ret == SOC_SUCCESS)
                {
                    APP_DEBUG("<--The socket is already connected.-->\r\n");
                    m_tcp_state = TCP_STATE_SOC_SEND;
                    
                }else if(ret == SOC_NONBLOCK)
                {
                      if (!timeout_90S_monitor)//start timeout monitor
                      {
                        Ql_Timer_Start(TCP_TIMEOUT_90S_TIMER_ID, TIMEOUT_90S_PERIOD, FALSE);
                        timeout_90S_monitor = TRUE;
                      }
                      APP_DEBUG("<--Waiting for the result of socket connection,ret=%d.-->\r\n",ret);
                      //waiting CallBack_getipbyname
                      
                }else //error
                {
                    APP_DEBUG("<--Socket Connect failure,ret=%d.-->\r\n",ret);
                    APP_DEBUG("<-- Close socket.-->\r\n");
                    Ql_SOC_Close(m_socketid);
                    m_socketid = -1;
                    
                   m_tcp_state = TCP_STATE_SOC_CLOSE;
                }
                break;
            }
            case TCP_STATE_SOC_SEND:
            {
				
                if (!Ql_strlen(m_send_buf))//no data need to send
                {
					APP_DEBUG("<--Waiting to send data-->\r\n");
                    break;
				}
                m_tcp_state = TCP_STATE_SOC_SENDING;
				
                do
                {
                    ret = Ql_SOC_Send(m_socketid, m_pCurrentPos, m_remain_len);
                    APP_DEBUG("<--Send data,socketid=%d,number of bytes sent=%d-->\r\n",m_socketid,ret);
                    if(ret == m_remain_len)//send compelete
                    {
                        m_remain_len = 0;
                        m_pCurrentPos = NULL;
                        m_nSentLen += ret;
                        m_tcp_state = TCP_STATE_SOC_ACK;
                        break;
                    }
                    else if((ret <= 0) && (ret == SOC_NONBLOCK)) 
                    {
						APP_DEBUG("<--Wait for data send to finish-->\r\n");
                        break;
                    }
                    else if(ret <= 0)
                    {
                        APP_DEBUG("<--Send data failure,ret=%d.-->\r\n",ret); 
                        m_tcp_state = TCP_STATE_SOC_CLOSE;
						
                        break;
                    }
                    else if(ret < m_remain_len)//continue send, do not send all data
                    {
                        m_remain_len -= ret;
                        m_pCurrentPos += ret; 
                        m_nSentLen += ret;
                    }
                }while(1);
                break;
            }
            case TCP_STATE_SOC_ACK:
            {
                u32 ackedNumCurr;
                ret = Ql_SOC_GetAckNumber(m_socketid, &ackedNumCurr);
                if (ret < 0)
                {
                    APP_DEBUG("<--get socket ack failed-->\r\n");
                }
                if (m_nSentLen == ackedNumCurr)
                {
                    if (timeout_90S_monitor) //stop timeout monitor
                    {
                        Ql_Timer_Stop(TCP_TIMEOUT_90S_TIMER_ID);
                        timeout_90S_monitor = FALSE;
                    }
                    
                    APP_DEBUG("<-- ACK Number:%d/%d. Server has received all data. -->\r\n\r\n", m_nSentLen, ackedNumCurr);

                    Ql_memset(m_send_buf,0,SEND_BUFFER_LEN);
                    m_tcp_state = TCP_STATE_SOC_SEND;
                }
                else
                {
                    if (!timeout_90S_monitor)//start timeout monitor
                    {
                        Ql_Timer_Start(TCP_TIMEOUT_90S_TIMER_ID, TIMEOUT_90S_PERIOD, FALSE);
                        timeout_90S_monitor = TRUE;
                    }
                    
                    APP_DEBUG("<-- ACK Number:%d/%d from socket[%d] -->\r\n", ackedNumCurr, m_nSentLen, m_socketid);
                }
                break;
            }
            case TCP_STATE_SOC_CLOSE:
            {
				ret = Ql_SOC_Close(m_socketid);//error , Ql_SOC_Close
                APP_DEBUG("<--socket closed,ret(%d)-->\r\n",ret);
				
		        m_socketid = -1;
                m_remain_len = 0;
                m_pCurrentPos = NULL;
                break;
            }
            default:
                break;
        }    
    }
}

void Callback_GetIpByName(u8 contexId,s32 errCode,u32 ipAddrCnt,u8* ipAddr)
{
    
    if (errCode == SOC_SUCCESS)
    {
        Ql_memset(m_ipaddress, 0, IP_ADDR_LEN);
        Ql_memcpy(m_ipaddress, ipAddr, IP_ADDR_LEN);
		APP_DEBUG("<-- %s:contexid=%d,error=%d,num_entry=%d,m_ipaddress(%s) -->\r\n", __func__, contexId,errCode,ipAddrCnt,m_ipaddress);
        m_tcp_state = TCP_STATE_SOC_REGISTER;
    }
}

void callback_socket_connect(s32 socketId, s32 errCode, void* customParam )
{
    if (errCode == SOC_SUCCESS)
    {
        if (timeout_90S_monitor) //stop timeout monitor
        {
           Ql_Timer_Stop(TCP_TIMEOUT_90S_TIMER_ID);
           timeout_90S_monitor = FALSE;
        }
        APP_DEBUG("<--Callback: socket connect successfully.-->\r\n");
        m_tcp_state = TCP_STATE_SOC_SEND;
    }else
    {
        APP_DEBUG("<--Callback: socket connect failure,(socketId=%d),errCode=%d-->\r\n",socketId,errCode);
        Ql_SOC_Close(socketId);
        m_tcp_state = TCP_STATE_SOC_CREATE;
    }
}

void callback_socket_close(s32 socketId, s32 errCode, void* customParam )
{
    m_nSentLen  = 0;
    
    if (errCode == SOC_SUCCESS)
    {
        m_tcp_state = TCP_STATE_SOC_CREATE;
        APP_DEBUG("<--CallBack: close socket successfully.-->\r\n"); 
    }else
    {
        APP_DEBUG("<--CallBack: close socket failure,(socketId=%d,error_cause=%d)-->\r\n",socketId,errCode); 
    }
}

void callback_socket_read(s32 socketId, s32 errCode, void* customParam )
{
    s32 ret;
    if(errCode)
    {
        APP_DEBUG("<--CallBack: socket read failure,(sock=%d,error=%d)-->\r\n",socketId,errCode);
        APP_DEBUG("<-- Close socket.-->\r\n");
        Ql_SOC_Close(socketId);
        m_socketid = -1;
        m_tcp_state = TCP_STATE_TOTAL_NUM; 
        return;
    }


    Ql_memset(m_recv_buf, 0, RECV_BUFFER_LEN);
    do
    {
        ret = Ql_SOC_Recv(socketId, m_recv_buf, RECV_BUFFER_LEN);

        if(ret < 0)
        {
            APP_DEBUG("<-- Receive data failure,ret=%d.-->\r\n",ret);
            APP_DEBUG("<-- Close socket.-->\r\n");
            Ql_SOC_Close(socketId); //you can close this socket  
            m_socketid = -1;
            m_tcp_state = TCP_STATE_SOC_CREATE;
            break;
        }
        else if(ret < RECV_BUFFER_LEN)
        {
            APP_DEBUG("<--Receive data from sock(%d),len(%d):%s\r\n",socketId,ret,m_recv_buf);
            break;
        }else if(ret == RECV_BUFFER_LEN)
        {
            APP_DEBUG("<--Receive data from sock(%d),len(%d):%s\r\n",socketId,ret,m_recv_buf);
        }
    }while(1);
}

void callback_socket_accept(s32 listenSocketId, s32 errCode, void* customParam )
{  
}


#endif // __EXAMPLE_TCPCLIENT_API__

