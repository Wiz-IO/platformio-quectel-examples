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
 *   example_udpserver.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example demonstrates how to establish a UDP server.Input the specified 
 *   command through any uart port and the result will be output through the debug port.
 *   In most of UDPIP functions,  return -2(QL_SOC_WOULDBLOCK) doesn't indicate failed.
 *   It means app should wait, till the callback function is called.
 *   The app can get the information of success or failure in callback function.
 *   Get more info about return value. Please read the "OPEN_CPU_DGD" document.
 *
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_UDPSERVER__" in gcc_makefile file. And compile the 
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
#ifdef __EXAMPLE_UDPSERVER__
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
    STATE_SOC_RECEIVE,
    STATE_SOC_SEND,
    STATE_SOC_SENDING,
    STATE_SOC_CLOSE,
    STATE_GPRS_DEACTIVATE,
    STATE_TOTAL_NUM
}Enum_UDPSrv_STATE;
static u8 m_udp_state = STATE_NW_GET_SIMSTATE;

/*****************************************************************
* UART Param
******************************************************************/
#define SERIAL_RX_BUFFER_LEN  2048
static u8 m_RxBuf_Uart[SERIAL_RX_BUFFER_LEN];

/*****************************************************************
* timer param
******************************************************************/
#define UDP_TIMER_ID          TIMER_ID_USER_START
#define UDP_TIMER_PERIOD      1000  //ms
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
#define MAXCLIENT_NUM        5
#define MAX_ADDR_LEN   5

static s8  m_socket = -1; //socket

static u8  m_Local_Addr[MAX_ADDR_LEN];   //local ip
static u32 m_Local_Port = 8010;

static u16 m_broadcast_cnt = 0;   //broadcast count time

static s32 m_index = 0;

/*****************************************************************
* define client param & function
******************************************************************/
#define SEND_BUFFER_LEN     1024
#define RECV_BUFFER_LEN     1024

typedef struct
{
    bool  index;
    s8    socketId;
    u8    addr[MAX_ADDR_LEN];
    u16   port;
    char  send_buf[SEND_BUFFER_LEN];
    s32   sendRemain_len;
    char  *pSendCurrentPos;
    s32  (*send_handle)(s32 index, char *PData);
}QlClient;

static QlClient m_client[MAXCLIENT_NUM];

static s32 func_send_handle(s32 index, char *PDtata);
static s32 func_read_handle_callback(s32 socket, s32 errCode, void* customParam);

void client_socketId_init()
{
    s32 i;
    for(i = 0; i < MAXCLIENT_NUM; i++)
    {
        m_client[i].index = FALSE;
    }
}

void client_init(s32 index, s8 socket, u8 flag, u8* addr, u16 port)
{
    m_client[index].index = flag;
    m_client[index].socketId = socket;
    
    Ql_memset(m_client[index].addr, '\0', MAX_ADDR_LEN); 
    Ql_memcpy(m_client[index].addr, addr,MAX_ADDR_LEN);
    m_client[index].port = port;

    Ql_memset(m_client[index].send_buf, '\0', SEND_BUFFER_LEN); 
    m_client[index].sendRemain_len = 0;
    m_client[index].pSendCurrentPos = m_client[index].send_buf;

    m_client[index].send_handle = func_send_handle;
}

void client_uninit(void)
{
    s32 i;
    for(i = 0; i < MAXCLIENT_NUM; i++)
    {
        m_client[i].index = FALSE;
        m_client[i].socketId = 0xFF;
        
        Ql_memset(m_client[i].addr, '\0', MAX_ADDR_LEN);
        m_client[i].port = 0;
        
        Ql_memset(m_client[i].send_buf, '\0', SEND_BUFFER_LEN); 
        m_client[i].sendRemain_len = 0;     
        m_client[i].pSendCurrentPos = NULL;

        m_client[i].send_handle = NULL;
    }
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

    APP_DEBUG("<--OpenCPU: UDP Server.-->\r\n");

    //register & start timer 
    Ql_Timer_Register(UDP_TIMER_ID, Callback_Timer, NULL);
    Ql_Timer_Start(UDP_TIMER_ID, UDP_TIMER_PERIOD, TRUE);

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
        APP_DEBUG("<--Set UDP Loc Parameter Successfully<%d>.-->\r\n",m_Local_Port);

        m_udp_state = STATE_NW_GET_SIMSTATE;
        APP_DEBUG("<--Restart the UDP connection process.-->\r\n");

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
    if (UDP_TIMER_ID == timerId)
    {
        switch (m_udp_state)
        {
            case STATE_NW_GET_SIMSTATE:
            {
                s32 simStat = 0;
                RIL_SIM_GetSimState(&simStat);
                if (simStat == SIM_STAT_READY)
                {
                    m_udp_state = STATE_NW_QUERY_STATE;
                    APP_DEBUG("<--SIM card status is normal!-->\r\n");
                }else
                {
                //    Ql_Timer_Stop(UDP_TIMER_ID);
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
                    m_udp_state = STATE_GPRS_REGISTER;
                }
                break;
            }
            case STATE_GPRS_REGISTER:
            {
                ret = Ql_GPRS_Register(0, &callback_gprs_func, NULL);
                if (GPRS_PDP_SUCCESS == ret)
                {
                    APP_DEBUG("<--Register GPRS callback function successfully.-->\r\n");
                    m_udp_state = STATE_GPRS_CONFIG;
                }else if (GPRS_PDP_ALREADY == ret)
                {
                    APP_DEBUG("<--GPRS callback function has already been registered,ret=%d.-->\r\n",ret);
                    m_udp_state = STATE_GPRS_CONFIG;
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
                
                m_udp_state = STATE_GPRS_ACTIVATE;
                break;
            }
            case STATE_GPRS_ACTIVATE:
            {
                m_udp_state = STATE_GPRS_ACTIVATING;
                ret = Ql_GPRS_Activate(0);
                if (ret == GPRS_PDP_SUCCESS)
                {
                    APP_DEBUG("<--Activate GPRS successfully.-->\r\n");
                    m_udp_state = STATE_GPRS_GET_DNSADDRESS;
                }else if (ret == GPRS_PDP_WOULDBLOCK)
                {
                    //waiting Callback_GPRS_Actived
                    APP_DEBUG("<--Waiting for the result of GPRS activated.,ret=%d.-->\r\n",ret);
                    
                }else if (ret == GPRS_PDP_ALREADY)
                {
                    APP_DEBUG("<--GPRS has already been activated,ret=%d.-->\r\n",ret);
                    m_udp_state = STATE_GPRS_GET_DNSADDRESS;
                }else//error
                {
                    APP_DEBUG("<--Activate GPRS failure,ret=%d.-->\r\n",ret);
                    m_udp_state = STATE_GPRS_ACTIVATE;
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
                    m_udp_state = STATE_GPRS_GET_LOCALIP;
                }else
                {
                     APP_DEBUG("<--Get DNS address failure,ret=%d.-->\r\n",ret);
                    m_udp_state = STATE_GPRS_DEACTIVATE;
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
                    m_udp_state = STATE_SOC_REGISTER;
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
                    m_udp_state = STATE_SOC_CREATE;
                }else if (SOC_ALREADY == ret)
                {
                    APP_DEBUG("<--Socket callback function has already been registered,ret=%d.-->\r\n",ret);
                    m_udp_state = STATE_SOC_CREATE;
                }else
                {
                    APP_DEBUG("<--Register Socket callback function failure,ret=%d.-->\r\n",ret);
                }
                break;
            }
            case STATE_SOC_CREATE:
            {
                m_socket = Ql_SOC_Create(0, SOC_TYPE_UDP);
                if (m_socket >= 0)
                {
                    APP_DEBUG("<--Create socket id successfully,socketid=%d.-->\r\n",m_socket);
                    m_udp_state = STATE_SOC_BIND;
                }else
                {
                    APP_DEBUG("<--Create socket id failure,error=%d.-->\r\n",m_socket);
                }
                break;
            }
            case STATE_SOC_BIND:
            {
                ret = Ql_SOC_Bind(m_socket, m_Local_Port);
                if (SOC_SUCCESS == ret)
                {
                    APP_DEBUG("<--Bind local port successfully.-->\r\n");
                    m_udp_state = STATE_SOC_RECEIVE;
                }else
                {
                    APP_DEBUG("<--Bind local port failure,error=%d.-->\r\n",ret); 
                    m_udp_state = STATE_SOC_BIND;
                }
               break;
            }
            case STATE_SOC_SEND:
            {
                s32 i;
                m_broadcast_cnt ++ ;

                if (m_broadcast_cnt*UDP_TIMER_PERIOD >= BROADCAST_PERIOD)// >= 10000ms
                {
                    for(i = 0; i < MAXCLIENT_NUM; i++)
                    {
                        if(m_client[i].index == TRUE)
                        {
                            m_client[i].send_handle(i, "\r\nThis is broadcast message~\r\n");
                        }
                    }
                    m_broadcast_cnt = 0;
                }
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
        m_udp_state = STATE_GPRS_GET_DNSADDRESS;
    }else
    {
        APP_DEBUG("<--CallBack: active GPRS successfully,errCode=%d-->\r\n",errCode);
        m_udp_state = STATE_GPRS_ACTIVATE;
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
    client_uninit();
        
    if(errCode == SOC_BEARER_FAIL)
    {   
        m_udp_state = STATE_GPRS_DEACTIVATE;
        APP_DEBUG("<--CallBack: close socket failure,(socketId=%d,error_cause=%d)-->\r\n",socketId,errCode); 
    }else
    {
        m_udp_state = STATE_SOC_CREATE;
        APP_DEBUG("<--CallBack: close socket,(socketId=%d,error_cause=%d)-->\r\n",socketId,errCode); 
    }
}

void callback_socket_accept(s32 listenSocketId, s32 errCode, void* customParam )
{  

}

void callback_socket_read(s32 socketId, s32 errCode, void* customParam )
{

    APP_DEBUG("<--callback_socket_read,(socketId=%d,error_cause=%d)-->\r\n",socketId,errCode); 

    func_read_handle_callback(socketId, errCode, customParam);
}

void callback_socket_write(s32 socketId, s32 errCode, void* customParam )
{
    if(errCode)
    {
        APP_DEBUG("<--CallBack: socket write failure,(sock=%d,error=%d)-->\r\n",socketId,errCode);
        APP_DEBUG("<-- Close socket[%d].-->\r\n",socketId);
        Ql_SOC_Close(socketId);//error , Ql_SOC_Close

        if(ret == SOC_BEARER_FAIL)  
        {
            m_udp_state = STATE_GPRS_DEACTIVATE;
        }
        else
        {
            m_udp_state = STATE_SOC_CREATE;
        }
        return;
    }
    m_client[m_index].send_handle(m_socket, m_client[m_index].pSendCurrentPos);

}


void CallBack_GPRS_Deactived(u8 contextId, s32 errCode, void* customParam )
{
    if (errCode == SOC_SUCCESS)
    {
        APP_DEBUG("<--CallBack: deactived GPRS successfully.-->\r\n"); 
        m_udp_state = STATE_NW_GET_SIMSTATE;
    }else
    {
        APP_DEBUG("<--CallBack: deactived GPRS failure,(contexid=%d,error_cause=%d)-->\r\n",contextId,errCode); 
    }
}

static s32 func_send_handle(s32 index, char *PDtata)
{
    s32 ret;
    
    if (!Ql_strlen(PDtata))//no data need to send
        return -1;

    m_udp_state = STATE_SOC_SENDING;

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
        ret = Ql_SOC_SendTo(m_client[index].socketId, m_client[index].pSendCurrentPos, m_client[index].sendRemain_len, (u32)m_client[index].addr, m_client[index].port);
        APP_DEBUG("<--Send data to[%d,%d,%d,%d],number of bytes=%d-->\r\n", m_client[index].addr[0],m_client[index].addr[1],m_client[index].addr[2],m_client[index].addr[3], ret);
        if(ret == m_client[index].sendRemain_len)//send compelete
        {
            m_client[index].sendRemain_len = 0;
            m_client[index].pSendCurrentPos = NULL;
            m_udp_state = STATE_SOC_SEND;
            break;
        }
        else if((ret <= 0) && (ret == SOC_WOULDBLOCK)) 
        {
            //waiting CallBack_socket_write, then send data; 
            break;
        }
        else if(ret <= 0)
        {
            APP_DEBUG("<--Send data failure,ret=%d.-->\r\n",ret);
            APP_DEBUG("<-- Close socket[%d].-->\r\n",m_client[index].socketId);

            Ql_SOC_Close(m_client[index].socketId);

            if(ret == SOC_BEARER_FAIL)  
            {
                m_udp_state = STATE_GPRS_DEACTIVATE;
            }
            else
            {
                m_udp_state = STATE_SOC_CREATE;
            }
            break;
        }
        else if(ret < m_client[index].sendRemain_len)//continue send
        {
            m_client[index].sendRemain_len -= ret;
            m_client[index].pSendCurrentPos += ret; 
        }
    }while(1);
}

static s32 func_read_handle_callback(s32 socket, s32 errCode, void* customParam)
{
    s32 i;
    s32 ret;
    u8  recv_buf[RECV_BUFFER_LEN];
    u8  client_addr[MAX_ADDR_LEN];
    u16 client_port;
    
    if(errCode)
    {
        APP_DEBUG("<--CallBack: socket read failure,(sock=%d,error=%d)-->\r\n",socket,errCode);
        APP_DEBUG("<--Close socket[%d].-->\r\n",socket);

        Ql_SOC_Close(socket);//error , Ql_SOC_Close

        if(errCode == SOC_BEARER_FAIL)
        {
            m_udp_state = STATE_GPRS_DEACTIVATE;
        }
        else
        {
            m_udp_state = STATE_SOC_CREATE;
        }
        return -1;
    }

    do
    {
        Ql_memset(recv_buf, '\0', RECV_BUFFER_LEN); 
        Ql_memset(client_addr, '\0', MAX_ADDR_LEN); 
        
        ret = Ql_SOC_RecvFrom(socket, recv_buf, RECV_BUFFER_LEN, (u32*)client_addr, &client_port);


        if((ret < 0) && (ret != SOC_WOULDBLOCK))
        {
            APP_DEBUG("<-- Receive data failure,ret=%d.-->\r\n",ret);
            APP_DEBUG("<--Close socket[%d].-->\r\n",socket);
            Ql_SOC_Close(socket);//error , Ql_SOC_Close

            if(ret == SOC_BEARER_FAIL)
            {
                m_udp_state = STATE_GPRS_DEACTIVATE;
            }
            else
            {
                m_udp_state = STATE_SOC_CREATE;
            }
            break;
        }
        else if(ret == SOC_WOULDBLOCK)
        {
            //wait next CallBack_socket_read
            break;
        }
        else   //ret >= 0
        {
            
            for(i = 0; i < MAXCLIENT_NUM; i++)
            {
                if(!Ql_strncmp(m_client[i].addr,client_addr, MAX_ADDR_LEN))//existed
                {
                    m_index = i;
                    break;
                }
            }	

            if (i == MAXCLIENT_NUM) //no exist
            {
                for(i = 0; i < MAXCLIENT_NUM; i++)
                {
                    if(m_client[i].index == FALSE)
                    {
                        client_init(i, socket, TRUE, client_addr, client_port);
                        m_index = i;
                        break;
                    }
                }	
            }

            if(ret < RECV_BUFFER_LEN)
            {
                APP_DEBUG("<--Receive data from[%d,%d,%d,%d],len(%d):%s\r\n",client_addr[0],client_addr[1],client_addr[2],client_addr[3],ret,recv_buf);

                m_udp_state = STATE_SOC_SENDING;
                m_client[m_index].send_handle(m_index, recv_buf);
                break;
            }else if(ret == RECV_BUFFER_LEN)
            {
                APP_DEBUG("<--Receive data from[%d,%d,%d,%d],len(%d):%s\r\n",client_addr[0],client_addr[1],client_addr[2],client_addr[3],ret,recv_buf);

                m_udp_state = STATE_SOC_SENDING;
                m_client[m_index].send_handle(m_index, recv_buf);
            }
            
        }
    }while(1);
    
}

#endif // __EXAMPLE_UDPSERVER__

