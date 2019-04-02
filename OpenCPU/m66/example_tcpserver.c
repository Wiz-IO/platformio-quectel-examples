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
 *   example_tcpserver.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example demonstrates how to establish a TCP server.
 *   Input the specified command through any serial port and the result will be 
 *   output through the debug port.
 *   In most of TCPIP functions,  return -2(QL_SOC_WOULDBLOCK) doesn't indicate failed.
 *   It means app should wait, till the callback function is called.
 *   The app can get the information of success or failure in callback function.
 *   Get more info about return value. Please read the "OPEN_CPU_DGD" document.
 *
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_TCPSERVER__" in gcc_makefile file. And compile the 
 *     app using "make clean/new".
 *     Download image bin to module to run.
 * 
 *   Operation:
 *            
 *     step1: set APN parameter.
 *            Command: Set_APN_Param=<APN>,<username>,<password>
 *     step2: set local parameter.
 *            Command:Set_Loc_Port=<loc port>
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
#ifdef __EXAMPLE_TCPSERVER__  
#include "custom_feature_def.h"
#include "ql_stdlib.h"
#include "ql_common.h"
#include "ql_type.h"
#include "ql_trace.h"
#include "ql_error.h"
#include "ql_uart.h"
#include "ql_uart.h"
#include "ql_gprs.h"
#include "ql_socket.h"
#include "ql_timer.h"
#include "ril_sim.h"
#include "ril_network.h"
#include "ril.h"
#include "ril_util.h"
#include "ril_telephony.h"

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


/*****************************************************************
* define process state
******************************************************************/
typedef enum{
    STATE_NW_GET_SIMSTATE,
    STATE_NW_QUERY_STATE,
    STATE_GPRS_REGISTER,
    STATE_GPRS_CONFIG,
    STATE_GPRS_ACTIVATE,
    STATE_GPRS_ACTIVATING,
    STATE_GPRS_GET_DNSADDRESS,
    STATE_GPRS_GET_LOCALIP,
    STATE_SOC_REGISTER,
    STATE_SOC_CREATE,
    STATE_SOC_BIND,
    STATE_SOC_LISTEN,
    STATE_SOC_LISTENING,
    STATE_SOC_ACCEPT,
    STATE_SOC_SEND,
    STATE_SOC_SENDING,
    STATE_SOC_ACK,
    STATE_SOC_CLOSE,
    STATE_GPRS_DEACTIVATE,
    STATE_TOTAL_NUM
}Enum_TCPSrv_STATE;
static u8 m_tcp_state = STATE_NW_GET_SIMSTATE;

/*****************************************************************
* UART Param
******************************************************************/
#define SERIAL_RX_BUFFER_LEN  2048
static u8 m_RxBuf_Uart[SERIAL_RX_BUFFER_LEN];

/*****************************************************************
* timer param
******************************************************************/
#define TCP_TIMER_ID          TIMER_ID_USER_START
#define TCP_TIMER_PERIOD      1000  //ms
#define BROADCAST_PERIOD      10000 //ms

/*****************************************************************
* APN Param
******************************************************************/
static u8 m_apn[10] = "cmnet";
static u8 m_userid[10] = "";
static u8 m_passwd[10] = "";

static ST_GprsConfig  m_gprsCfg;

/*****************************************************************
* Local Param
******************************************************************/
#define MAXCLIENT_NUM   5
#define MAX_ADDR_LEN    5

static s8  m_Listen_Socket = -1; //listen socket
static s8  m_Accept_Socket = -1; //accept socket
static s32 m_Index_Ack;

static u8  m_Local_Addr[MAX_ADDR_LEN];   //local ip
static u32 m_Local_Port = 8010;

static u8  m_Client_Addr[MAX_ADDR_LEN];//client ip
static u16 m_Client_Port;   //client port

static u16 m_broadcast_cnt[MAXCLIENT_NUM] ={0,0,0,0,0};   //broadcast count time

/*****************************************************************
* define client param & function
******************************************************************/
#define SEND_BUFFER_LEN     1024
#define RECV_BUFFER_LEN     1024

typedef struct
{
    s8    socketId;
    bool  IsGreeting;  // Make a mark, whether to send a greeting, when the client connected to the server.
    u64   nSentLen;    // Bytes of number sent data through current socket    
    char  send_buf[SEND_BUFFER_LEN];
    char  recv_buf[RECV_BUFFER_LEN];
    s32   sendRemain_len;
    s32   recvRemain_len;
    char  *pSendCurrentPos;
    char  *pRecvCurrentPos;
    s32  (*send_handle)(s32 sock, char *PData);
    s32  (*send_ack)(s8 sock);
    s32  (*read_handle_callback)(s32 socket, s32 errCode, void* customParam, s32 index);
}QlClient;

static QlClient m_client[MAXCLIENT_NUM];

static s32 func_send_handle(s8 sock, char *PDtata);  
static s32 func_send_ack(s8 sock);
static s32 func_read_handle_callback(s32 socket, s32 errCode, void* customParam, s32 index);

s32 findClientBySockid(s8 sockid)
{
    s32 i;
    s32 index = -1;
    for( i = 0; i < MAXCLIENT_NUM; i++)
    {
        if(sockid == m_client[i].socketId)
        {
            index = i;
            break;
        }
    }
    return index;	
}

void client_socketId_init()
{
    s32 i;
    for(i = 0; i < MAXCLIENT_NUM; i++)
    {
        m_client[i].socketId = 0x7F;
    }
}

void client_init(s32 index, s8 sockid)
{
    m_client[index].socketId = sockid;
    m_client[index].IsGreeting = FALSE;
    m_client[index].nSentLen = 0;
        
    Ql_memset(m_client[index].send_buf, '\0', SEND_BUFFER_LEN); 
    Ql_memset(m_client[index].recv_buf, '\0', RECV_BUFFER_LEN); 

    m_client[index].sendRemain_len = 0;     
    m_client[index].recvRemain_len = RECV_BUFFER_LEN; 
    
    m_client[index].pSendCurrentPos = m_client[index].send_buf;
    m_client[index].pRecvCurrentPos = m_client[index].recv_buf;

    m_client[index].send_handle = func_send_handle;
    m_client[index].send_ack = func_send_ack;
    m_client[index].read_handle_callback = func_read_handle_callback;
}

void client_uninit(s32 index)
{
    m_client[index].socketId = 0x7F;
    m_client[index].IsGreeting = FALSE;
    m_client[index].nSentLen = 0;
        
    Ql_memset(m_client[index].send_buf, '\0', SEND_BUFFER_LEN); 
    Ql_memset(m_client[index].recv_buf, '\0', RECV_BUFFER_LEN); 

    m_client[index].sendRemain_len = 0;     
    m_client[index].recvRemain_len = 0;
    
    m_client[index].pSendCurrentPos = NULL;
    m_client[index].pRecvCurrentPos = NULL;

    m_client[index].send_handle = NULL;
    m_client[index].send_ack = NULL;
    m_client[index].read_handle_callback = NULL;
}

bool Check_IsAllSoc_Close()
{
    s32 i;

	for(i = 0; i < MAXCLIENT_NUM; i++)
	{
		if(m_client[i].socketId != 0x7F)
		{
			break;
		}
	}
    
	if(i == MAXCLIENT_NUM)
	{
      return TRUE;
	}

    return FALSE;
}


/*****************************************************************
* GPRS and socket callback function
******************************************************************/
void callback_socket_connect(s32 socketId, s32 errCode, void* customParam );
void callback_socket_close(s32 socketId, s32 errCode, void* customParam );
void callback_socket_accept(s32 listenSocketId, s32 errCode, void* customParam );
void callback_socket_read(s32 socketId, s32 errCode, void* customParam );
void callback_socket_write(s32 socketId, s32 errCode, void* customParam );

void Callback_GPRS_Actived(u8 contexId, s32 errCode, void* customParam);
void CallBack_GPRS_Deactived(u8 contextId, s32 errCode, void* customParam );
void Callback_GetIpByName(u8 contexId, u8 requestId, s32 errCode,  u32 ipAddrCnt, u32* ipAddr);


ST_PDPContxt_Callback     callback_gprs_func = 
{
    Callback_GPRS_Actived,
    CallBack_GPRS_Deactived
};
ST_SOC_Callback      callback_soc_func=
{
    callback_socket_connect,
    callback_socket_close,
    callback_socket_accept,
    callback_socket_read,    
    callback_socket_write
};


/*****************************************************************
* uart callback function
******************************************************************/
static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);

/*****************************************************************
* timer callback function
******************************************************************/
static void Callback_Timer(u32 timerId, void* param);

/*****************************************************************
* other subroutines
******************************************************************/
extern s32 Analyse_Command(u8* src_str,s32 symbol_num,u8 symbol, u8* dest_buf);
static s32 ReadSerialPort(Enum_SerialPort port, /*[out]*/u8* pBuffer, /*[in]*/u32 bufLen);
static void proc_handle(char *pData,s32 len);


static s32 ret;
void proc_main_task(s32 taskId)
{
    s32 ret;
    ST_MSG msg;



    // Register & open UART port
    Ql_UART_Register(UART_PORT1, CallBack_UART_Hdlr, NULL);
    Ql_UART_Register(UART_PORT2, CallBack_UART_Hdlr, NULL);
    Ql_UART_Register(UART_PORT3, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(UART_PORT1, 115200, FC_NONE);
    Ql_UART_Open(UART_PORT2, 115200, FC_NONE);
    Ql_UART_Open(UART_PORT3, 115200, FC_NONE);

    APP_DEBUG("<--OpenCPU: TCP Server.-->\r\n");

    //register & start timer 
    Ql_Timer_Register(TCP_TIMER_ID, Callback_Timer, NULL);
    Ql_Timer_Start(TCP_TIMER_ID, TCP_TIMER_PERIOD, TRUE);

    client_socketId_init();
    
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
    s32 iret;
    u8 srvport[10];

    //command: Set_APN_Param=<APN>,<username>,<password>
    p = Ql_strstr(pData,"Set_APN_Param=");
    if (p)
    {
        Ql_memset(m_apn, 0, 10);
        if (Analyse_Command(pData, 1, '>', m_apn))
        {
            APP_DEBUG("<--APN Parameter Error.-->\r\n");
            return;
        }
        Ql_memset(m_userid, 0, 10);
        if (Analyse_Command(pData, 2, '>', m_userid))
        {
            APP_DEBUG("<--APN Username Parameter Error.-->\r\n");
            return;
        }
        Ql_memset(m_passwd, 0, 10);
        if (Analyse_Command(pData, 3, '>', m_passwd))
        {
            APP_DEBUG("<--APN Password Parameter Error.-->\r\n");
            return;
        }
        
        APP_DEBUG("<--Set APN Parameter Successfully<%s>,<%s>,<%s>.-->\r\n",m_apn,m_userid,m_passwd);

        return;
    }
    
    //command: Set_Loc_Port=<loc port>
    p = Ql_strstr(pData,"Set_Loc_Port=");
    if (p)
    {
        Ql_memset(srvport, 0, 10);
        if (Analyse_Command(pData, 1, '>', srvport))
        {
            APP_DEBUG("<--Server Port Parameter Error.-->\r\n");
            return;
        }
        m_Local_Port = Ql_atoi(srvport);
        APP_DEBUG("<--Set TCP Loc Parameter Successfully<%d>.-->\r\n",m_Local_Port);

        m_tcp_state = STATE_NW_GET_SIMSTATE;
        APP_DEBUG("<--Restart the TCP connection process.-->\r\n");

        return;
    }
}

static void checkErr_AckNumber(s32 err_code)
{
    if(SOC_INVALID_SOCKET == err_code)
    {
        APP_DEBUG("<-- Invalid socket ID -->\r\n");
    }
    else if(SOC_INVAL == err_code)
    {
        APP_DEBUG("<-- Invalid parameters for ACK number -->\r\n");
    }
    else if(SOC_ERROR == err_code)
    {
        APP_DEBUG("<-- Unspecified error for ACK number -->\r\n");
    }
    else
    {
        // get the socket option successfully
    }
}

static void Callback_Timer(u32 timerId, void* param)
{
    if (TCP_TIMER_ID == timerId)
    {
        switch (m_tcp_state)
        {
            case STATE_NW_GET_SIMSTATE:
            {
                s32 simStat = 0;
                RIL_SIM_GetSimState(&simStat);
                if (simStat == SIM_STAT_READY)
                {
                    m_tcp_state = STATE_NW_QUERY_STATE;
                    APP_DEBUG("<--SIM card status is normal!-->\r\n");
                }else
                {
                //    Ql_Timer_Stop(TCP_TIMER_ID);
                    APP_DEBUG("<--SIM card status is unnormal!-->\r\n");
                }
                break;
            }        
            case STATE_NW_QUERY_STATE:
            {
                s32 creg = 0;
                s32 cgreg = 0;
                //Ql_NW_GetNetworkState(&creg, &cgreg);
                ret = RIL_NW_GetGSMState(&creg);
                ret = RIL_NW_GetGPRSState(&cgreg);
                APP_DEBUG("<--Network State:creg=%d,cgreg=%d-->\r\n",creg,cgreg);
                if((cgreg == NW_STAT_REGISTERED)||(cgreg == NW_STAT_REGISTERED_ROAMING))
                {
                    m_tcp_state = STATE_GPRS_REGISTER;
                }
                break;
            }
            case STATE_GPRS_REGISTER:
            {
                ret = Ql_GPRS_Register(0, &callback_gprs_func, NULL);
                if (GPRS_PDP_SUCCESS == ret)
                {
                    APP_DEBUG("<--Register GPRS callback function successfully.-->\r\n");
                    m_tcp_state = STATE_GPRS_CONFIG;
                }else if (GPRS_PDP_ALREADY == ret)
                {
                    APP_DEBUG("<--GPRS callback function has already been registered,ret=%d.-->\r\n",ret);
                    m_tcp_state = STATE_GPRS_CONFIG;
                }else
                {
                    APP_DEBUG("<--Register GPRS callback function failure,ret=%d.-->\r\n",ret);
                }
                break;
            }
            case STATE_GPRS_CONFIG:
            {
                Ql_strcpy(m_gprsCfg.apnName, m_apn);
                Ql_strcpy(m_gprsCfg.apnUserId, m_userid);
                Ql_strcpy(m_gprsCfg.apnPasswd, m_passwd);
                m_gprsCfg.authtype = 0;
                ret = Ql_GPRS_Config(0, &m_gprsCfg);
                if (GPRS_PDP_SUCCESS == ret)
                {
                    APP_DEBUG("<--configure GPRS param successfully.-->\r\n");
                }else
                {
                    APP_DEBUG("<--configure GPRS param failure,ret=%d.-->\r\n",ret);
                }
                
                m_tcp_state = STATE_GPRS_ACTIVATE;
                break;
            }
            case STATE_GPRS_ACTIVATE:
            {
                m_tcp_state = STATE_GPRS_ACTIVATING;
                ret = Ql_GPRS_Activate(0);
                if (ret == GPRS_PDP_SUCCESS)
                {
                    APP_DEBUG("<--Activate GPRS successfully.-->\r\n");
                    m_tcp_state = STATE_GPRS_GET_DNSADDRESS;
                }else if (ret == GPRS_PDP_WOULDBLOCK)
                {
                    //waiting Callback_GPRS_Actived
                    APP_DEBUG("<--Waiting for the result of GPRS activated.,ret=%d.-->\r\n",ret);
                    
                }else if (ret == GPRS_PDP_ALREADY)
                {
                    APP_DEBUG("<--GPRS has already been activated,ret=%d.-->\r\n",ret);
                    m_tcp_state = STATE_GPRS_GET_DNSADDRESS;
                }else//error
                {
                    APP_DEBUG("<--Activate GPRS failure,ret=%d.-->\r\n",ret);
                    m_tcp_state = STATE_GPRS_ACTIVATE;
                }
                break;
            }
            case STATE_GPRS_GET_DNSADDRESS:
            {            
                u8 primaryAddr[16];
                u8 bkAddr[16];
                ret =Ql_GPRS_GetDNSAddress(0, (u32*)primaryAddr,  (u32*)bkAddr);
                if (ret == GPRS_PDP_SUCCESS)
                {
                    APP_DEBUG("<--Get DNS address successfully,primaryAddr=%d.%d.%d.%d,bkAddr=%d.%d.%d.%d-->\r\n",primaryAddr[0],primaryAddr[1],primaryAddr[2],primaryAddr[3],bkAddr[0],bkAddr[1],bkAddr[2],bkAddr[3]);            
                    m_tcp_state = STATE_GPRS_GET_LOCALIP;
                }else
                {
                     APP_DEBUG("<--Get DNS address failure,ret=%d.-->\r\n",ret);
                    m_tcp_state = STATE_GPRS_DEACTIVATE;
                }
                break;
            }
            case STATE_GPRS_GET_LOCALIP:
            {
                Ql_memset(m_Local_Addr, 0, MAX_ADDR_LEN);
                ret = Ql_GPRS_GetLocalIPAddress(0, (u32 *)m_Local_Addr);
                if (ret == GPRS_PDP_SUCCESS)
                {
                    APP_DEBUG("<--Get Local Ip successfully,Local Ip=%d.%d.%d.%d,port=%d-->\r\n",m_Local_Addr[0],m_Local_Addr[1],m_Local_Addr[2],m_Local_Addr[3],m_Local_Port);
                    m_tcp_state = STATE_SOC_REGISTER;
                }else
                {
                    APP_DEBUG("<--Get Local Ip failure,ret=%d.-->\r\n",ret);
                }
                break;
            }
            case STATE_SOC_REGISTER:
            {
                ret = Ql_SOC_Register(callback_soc_func, NULL);
                if (SOC_SUCCESS == ret)
                {
                    APP_DEBUG("<--Register socket callback function successfully.-->\r\n");
                    m_tcp_state = STATE_SOC_CREATE;
                }else if (SOC_ALREADY == ret)
                {
                    APP_DEBUG("<--Socket callback function has already been registered,ret=%d.-->\r\n",ret);
                    m_tcp_state = STATE_SOC_CREATE;
                }else
                {
                    APP_DEBUG("<--Register Socket callback function failure,ret=%d.-->\r\n",ret);
                }
                break;
            }
            case STATE_SOC_CREATE:
            {
                m_Listen_Socket = Ql_SOC_Create(0, SOC_TYPE_TCP);
                if (m_Listen_Socket >= 0)
                {
                    APP_DEBUG("<--Create socket id successfully,socketid=%d.-->\r\n",m_Listen_Socket);
                    m_tcp_state = STATE_SOC_BIND;
                }else
                {
                    APP_DEBUG("<--Create socket id failure,error=%d.-->\r\n",m_Listen_Socket);
                }
                break;
            }
            case STATE_SOC_BIND:
            {
                ret = Ql_SOC_Bind(m_Listen_Socket, m_Local_Port);
                if (SOC_SUCCESS == ret)
                {
                    APP_DEBUG("<--Bind local port successfully.-->\r\n");
                    m_tcp_state = STATE_SOC_LISTEN;
                }else
                {
                    APP_DEBUG("<--Bind local port failure,error=%d.-->\r\n",ret); 
                    m_tcp_state = STATE_SOC_BIND;
                }
               break;
            }
            case STATE_SOC_LISTEN:
            {
                ret = Ql_SOC_Listen(m_Listen_Socket, MAXCLIENT_NUM);
                if(ret < 0)
                {
                    m_tcp_state = STATE_SOC_LISTEN;
                    APP_DEBUG("<--Set listen socket failure, ret=%d-->\r\n", ret);
                }else
                {
                    m_tcp_state = STATE_SOC_LISTENING;
                    APP_DEBUG("<--Set listen socket successfully.-->\r\n");
                }
                break;
            }
            case STATE_SOC_ACCEPT:
            {
                s32 i;
                m_Accept_Socket = Ql_SOC_Accept(m_Listen_Socket, (u32*)m_Client_Addr, &m_Client_Port);       
                if(m_Accept_Socket >= 0)
                {
                    APP_DEBUG("<--Accept a client, socket=%d, ip=%d.%d.%d.%d, port=%d.-->\r\n", 
                        m_Accept_Socket,m_Client_Addr[0],m_Client_Addr[1],m_Client_Addr[2],m_Client_Addr[3], m_Client_Port);

                    for(i = 0; i < MAXCLIENT_NUM; i++)
                    {
                        if(m_client[i].socketId == 0x7F)
                        {
                            client_init(i,m_Accept_Socket);
                            break;
                        }
                    }					
                    m_tcp_state = STATE_SOC_SEND;
                }
                else
                {
                    m_tcp_state = STATE_SOC_ACCEPT;
                    APP_DEBUG("<--Accept client failure, ret=%d-->\r\n", m_Accept_Socket);
                }
                break;
            }
            case STATE_SOC_SEND:
            {
                s32 i; 

                for(i = 0; i < MAXCLIENT_NUM; i++)
                {
                    m_broadcast_cnt[i] ++ ;

                    if(m_client[i].socketId != 0x7F)
                    {
                        if (!m_client[i].IsGreeting) //send Greeting
                        {
                            m_client[i].IsGreeting = TRUE;
                            m_client[i].send_handle(m_client[i].socketId, "\r\nwelcome to connect server~\r\n");
                        }else //send broadcast message
                        {
                            if (m_broadcast_cnt[i]*TCP_TIMER_PERIOD >= BROADCAST_PERIOD)// >= 10000ms
                            {
                                m_broadcast_cnt[i] = 0;
                                m_client[i].send_handle(m_client[i].socketId, "\r\nThis is broadcast message~\r\n");
                            }
                        }
                    }
                }
                break;
            }
            case STATE_SOC_ACK:
            {
                func_send_ack(m_client[m_Index_Ack].socketId);
                break;
            }
            case STATE_GPRS_DEACTIVATE:
            {
                APP_DEBUG("<--Deactivate GPRS.-->\r\n");
                Ql_GPRS_Deactivate(0);
                break;
            }
            default:
                break;
        }    
    }
}

void Callback_GPRS_Actived(u8 contexId, s32 errCode, void* customParam)
{
    if(errCode == SOC_SUCCESS)
    {
        APP_DEBUG("<--CallBack: active GPRS successfully.-->\r\n");
        m_tcp_state = STATE_GPRS_GET_DNSADDRESS;
    }else
    {
        APP_DEBUG("<--CallBack: active GPRS successfully,errCode=%d-->\r\n",errCode);
        m_tcp_state = STATE_GPRS_ACTIVATE;
    }
}

void Callback_GetIpByName(u8 contexId, u8 requestId, s32 errCode,  u32 ipAddrCnt, u32* ipAddr)
{
}

void callback_socket_connect(s32 socketId, s32 errCode, void* customParam )
{
}

void callback_socket_close(s32 socketId, s32 errCode, void* customParam )
{
    s32 index;
    index = findClientBySockid(socketId);
    client_uninit(index);
        
    if (errCode == SOC_SUCCESS)
    {
        APP_DEBUG("<--CallBack: close socket successfully.-->\r\n"); 
    }else if(errCode == SOC_BEARER_FAIL)
    {   
        m_tcp_state = STATE_GPRS_DEACTIVATE;
        APP_DEBUG("<--CallBack: close socket failure,(socketId=%d,error_cause=%d)-->\r\n",socketId,errCode); 
    }else
    {
        m_tcp_state = STATE_SOC_LISTENING;
        APP_DEBUG("<--CallBack: close socket failure,(socketId=%d,error_cause=%d)-->\r\n",socketId,errCode); 
    }
}

void callback_socket_accept(s32 listenSocketId, s32 errCode, void* customParam )
{  
    if (errCode == SOC_SUCCESS)
    {
        m_tcp_state = STATE_SOC_ACCEPT;
        
    }else if(errCode == SOC_BEARER_FAIL)
    {   
        m_tcp_state = STATE_GPRS_DEACTIVATE;
    }else
    {
        m_tcp_state = STATE_SOC_LISTENING;
    }
}

void callback_socket_read(s32 socketId, s32 errCode, void* customParam )
{
	s32 index;
	index = findClientBySockid(socketId);
	if(index >= 0)
	{
        func_read_handle_callback(socketId, errCode, customParam, index);
	}
}

void callback_socket_write(s32 socketId, s32 errCode, void* customParam )
{
    s32 ret;
    s32 index;

    index = findClientBySockid(socketId);
    
    if(errCode)
    {
        APP_DEBUG("<--CallBack: socket write failure,(sock=%d,error=%d)-->\r\n",socketId,errCode);
        APP_DEBUG("<-- Close socket[%d].-->\r\n",m_client[index].socketId);
        Ql_SOC_Close(m_client[index].socketId);//error , Ql_SOC_Close

        client_uninit(index);

        if(ret == SOC_BEARER_FAIL)  
        {
            if (Check_IsAllSoc_Close())// no socket connection
            {
                m_tcp_state = STATE_GPRS_DEACTIVATE;
            }else
            {
                m_tcp_state = STATE_SOC_LISTENING;
            }
        }
        else
        {
            m_tcp_state = STATE_SOC_LISTENING;
        }
        return;
    }

    m_client[index].send_handle(m_client[index].socketId, m_client[index].pSendCurrentPos);

}


void CallBack_GPRS_Deactived(u8 contextId, s32 errCode, void* customParam )
{
    if (errCode == SOC_SUCCESS)
    {
        APP_DEBUG("<--CallBack: deactived GPRS successfully.-->\r\n"); 
        m_tcp_state = STATE_NW_GET_SIMSTATE;
    }else
    {
        APP_DEBUG("<--CallBack: deactived GPRS failure,(contexid=%d,error_cause=%d)-->\r\n",contextId,errCode); 
    }
}

static s32 func_send_handle(s8 sock, char *PDtata)
{
    s32 ret;
    s32 index;
    if (!Ql_strlen(PDtata))//no data need to send
        return -1;

    m_tcp_state = STATE_SOC_SENDING;

    index = findClientBySockid(sock);

    Ql_memset(m_client[index].send_buf, '\0', SEND_BUFFER_LEN); 
    if (Ql_strlen(PDtata) > SEND_BUFFER_LEN)
    {
        Ql_strncpy(m_client[index].send_buf,PDtata,SEND_BUFFER_LEN);
    }else
    {
        Ql_strncpy(m_client[index].send_buf,PDtata,Ql_strlen(PDtata));
    }
    m_client[index].sendRemain_len = Ql_strlen(m_client[index].send_buf);
	m_client[index].pSendCurrentPos = m_client[index].send_buf;


    do
    {
        ret = Ql_SOC_Send(m_client[index].socketId, m_client[index].pSendCurrentPos, m_client[index].sendRemain_len);
        APP_DEBUG("<--Send data to socket[%d],number of bytes=%d-->\r\n",m_client[index].socketId,ret);
        if(ret == m_client[index].sendRemain_len)//send compelete
        {
            m_client[index].sendRemain_len = 0;
            m_client[index].pSendCurrentPos = NULL;
            m_client[index].nSentLen += ret;
            m_Index_Ack = index;

            m_tcp_state = STATE_SOC_ACK;
            
            break;
        }
        else if((ret <= 0) && (ret == SOC_WOULDBLOCK)) 
        {
            //waiting CallBack_socket_write, then send data; 
            m_Index_Ack = index;
            break;
        }
        else if(ret <= 0)
        {
            APP_DEBUG("<--Send data failure,ret=%d.-->\r\n",ret);
            APP_DEBUG("<-- Close socket[%d].-->\r\n",m_client[index].socketId);
            Ql_SOC_Close(m_client[index].socketId);//error , Ql_SOC_Close

            client_uninit(index);

            if(ret == SOC_BEARER_FAIL)  
            {
                if (Check_IsAllSoc_Close())// no socket connection
                {
                    m_tcp_state = STATE_GPRS_DEACTIVATE;
                }else
                {
                    m_tcp_state = STATE_SOC_LISTENING;
                }
            }
            else
            {
                m_tcp_state = STATE_SOC_LISTENING;
            }
            break;
        }
        else if(ret < m_client[index].sendRemain_len)//continue send, do not send all data
        {
            m_client[index].sendRemain_len -= ret;
            m_client[index].pSendCurrentPos += ret; 
            m_client[index].nSentLen += ret;
        }
    }while(1);
}

static s32 func_send_ack(s8 sock)
{
    s32 index;
    u64 ackedNumCurr;
    
    index = findClientBySockid(sock);
    
    ret = Ql_SOC_GetAckNumber(sock, &ackedNumCurr);
    if (ret < 0)
    {
        checkErr_AckNumber(ret);
    }
    if (m_client[index].nSentLen == ackedNumCurr)
    {
        APP_DEBUG("<-- ACK Number:%llu/%llu from socket[%d].Client has received all data. -->\r\n\r\n", m_client[index].nSentLen, ackedNumCurr,sock);

        Ql_memset(m_client[index].send_buf, '\0', SEND_BUFFER_LEN); 
        m_tcp_state = STATE_SOC_SEND;
    }
    else
    {
        APP_DEBUG("<-- ACK Number:%llu/%llu from socket[%d] -->\r\n", ackedNumCurr, m_client[index].nSentLen, sock);
    }
}
static s32 func_read_handle_callback(s32 socket, s32 errCode, void* customParam, s32 index)
{
    s32 ret;

    if(errCode)
    {
        APP_DEBUG("<--CallBack: socket read failure,(sock=%d,error=%d)-->\r\n",socket,errCode);
        APP_DEBUG("<--Close socket[%d].-->\r\n",m_client[index].socketId);
        Ql_SOC_Close(m_client[index].socketId);//error , Ql_SOC_Close

        client_uninit(index);

        if(errCode == SOC_BEARER_FAIL)
        {
            if (Check_IsAllSoc_Close())// no socket connection
            {
                m_tcp_state = STATE_GPRS_DEACTIVATE;
            }else
            {
                m_tcp_state = STATE_SOC_LISTENING;
            }
        }
        else
        {
            m_tcp_state = STATE_SOC_LISTENING;
        }
        return -1;
    }


    do
    {
        Ql_memset(m_client[index].recv_buf, '\0', RECV_BUFFER_LEN); 
        ret = Ql_SOC_Recv(m_client[index].socketId, m_client[index].recv_buf, RECV_BUFFER_LEN);

        if((ret < 0) && (ret != SOC_WOULDBLOCK))
        {
            APP_DEBUG("<-- Receive data failure,ret=%d.-->\r\n",ret);
            APP_DEBUG("<--Close socket[%d].-->\r\n",m_client[index].socketId);
            Ql_SOC_Close(m_client[index].socketId);//error , Ql_SOC_Close

            client_uninit(index);

            if(ret == SOC_BEARER_FAIL)
            {
                if (Check_IsAllSoc_Close())// no socket connection
                {
                    m_tcp_state = STATE_GPRS_DEACTIVATE;
                }else
                {
                    m_tcp_state = STATE_SOC_LISTENING;
                }
            }
            else
            {
                m_tcp_state = STATE_SOC_LISTENING;
            }
            break;
        }
        else if(ret == SOC_WOULDBLOCK)
        {
            //wait next CallBack_socket_read
            break;
        }
        else if(ret < RECV_BUFFER_LEN)
        {
            APP_DEBUG("<--Receive data from sock(%d),len(%d):%s\r\n",socket,ret,m_client[index].recv_buf);

            m_tcp_state = STATE_SOC_SENDING;
            m_client[index].send_handle(m_client[index].socketId, m_client[index].recv_buf);//send back to client.
            break;
        }else if(ret == RECV_BUFFER_LEN)
        {
            APP_DEBUG("<--Receive data from sock(%d),len(%d):%s\r\n",socket,ret,m_client[index].recv_buf);
            m_tcp_state = STATE_SOC_SENDING;
            m_client[index].send_handle(m_client[index].socketId, m_client[index].recv_buf);//send back to client.
        }
        
    }while(1);
}

#endif // __EXAMPLE_TCPSERVER__

