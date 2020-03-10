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
 *   example_udpclient.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example demonstrates how to establish a UDP connection, when the module
 *   is used for the client.
 *
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_UDPCLIENT__" in gcc_makefile file. And compile the 
 *     app using "make clean/new".
 *     Download image bin to module to run.
 * 
 *   Operation:
 *            set server parameter, which is you want to connect.
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
#ifdef __EXAMPLE_UDPCLIENT__  
#include "custom_feature_def.h"
#include "ql_stdlib.h"
#include "ql_common.h"
#include "ql_type.h"
#include "ql_trace.h"
#include "ql_error.h"
#include "ql_uart.h"
#include "ql_timer.h"
#include "ql_urc_register.h"
#include "ril_network.h"
#include "ril_socket.h"
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
* UART Param
******************************************************************/
static Enum_SerialPort m_myUartPort  = UART_PORT0;
#define SERIAL_RX_BUFFER_LEN  2048
static u8 m_RxBuf_Uart[SERIAL_RX_BUFFER_LEN];

/*****************************************************************
* sever Param
******************************************************************/
#define SRVADDR_LEN  100
static u8  m_SrvADDR[SRVADDR_LEN] = "220.180.239.212\0";

/*****************************************************************
* socket Param
******************************************************************/
ST_Socket_Param_t  socket_param_t = {0,0,0,NULL,0,0,0,0};
Socket_Urc_Param_t* socket_urc_param_ptr = NULL;

#define SEND_BUFFER_LEN     1024
static u8 m_send_buf[SEND_BUFFER_LEN];
static s32 m_socketid = -1; 

#define TEMP_BUFFER_LENGTH  100
u8 temp_buffer[TEMP_BUFFER_LENGTH] = {0};

/*****************************************************************
* uart callback function
******************************************************************/
static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);

/*****************************************************************
* lwm2m recv callback function
******************************************************************/
static void callback_socket_recv(u8* buffer,u32 length);

/*****************************************************************
* other subroutines
******************************************************************/
extern s32 Analyse_Command(u8* src_str,s32 symbol_num,u8 symbol, u8* dest_buf);
static s32 ReadSerialPort(Enum_SerialPort port, /*[out]*/u8* pBuffer, /*[in]*/u32 bufLen);
static void proc_handle(u8 *pData,s32 len);
static s32 ATResponse_Handler(char* line, u32 len, void* userData);

void proc_main_task(s32 taskId)
{
    ST_MSG msg;
	s32 ret;

    // Register & open UART port
    Ql_UART_Register(m_myUartPort, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(m_myUartPort, 115200, FC_NONE);

    APP_DEBUG("<--OpenCPU: UDP Client.-->\r\n");
	
	//register  recv callback
    ret = Ql_Socket_Recv_Register(callback_socket_recv);
	APP_DEBUG("<--register recv callback successful(%d)-->\r\n",ret);
	
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
		case MSG_ID_URC_INDICATION:
		{	  
			switch (msg.param1)
			{
			   case URC_SIM_CARD_STATE_IND:
			   {
					APP_DEBUG("<-- SIM Card Status:%d -->\r\n", msg.param2);
					break;	
				}
				case URC_SOCKET_OPEN:
				{
					socket_urc_param_ptr = msg.param2;
					if(0 == socket_urc_param_ptr->error_no)
					{
						m_socketid = socket_urc_param_ptr->connectID;
						APP_DEBUG("<--Open socket successfully,socketid=%d.-->\r\n",m_socketid);
					}
					else 
					{
						APP_DEBUG("<--Open socket failed,error=%d.-->\r\n",socket_urc_param_ptr->error_no);
					}
					break;
				}
				case URC_SOCKET_CLOSE:
				{
					APP_DEBUG("<-- close socket id(%d )-->\r\n", msg.param2);	
					ret = RIL_SOC_QICLOSE(msg.param2);
					if (ret == 0)
					{
						APP_DEBUG("<-- closed socket successfully\r\n");
					}else
					{
						APP_DEBUG("<--closed socket id failure,error=%d.-->\r\n",ret);
					}
					break;
				}
				default:
				//APP_DEBUG("<-- Other URC: type=%d\r\n", msg.param1);
				break;
			}
		}
		break;
	default:
		 break;
		}
	}
}


static void callback_socket_recv(u8* buffer,u32 length)
{
	APP_DEBUG("<--tcp buffer(%s),length(%d)-->\r\n",buffer,length);
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

static void proc_handle(u8 *pData,s32 len)
{
    u8 srvport[10];
	u8 *p = NULL;
	s32 ret;

	/***************************************param congfig********************************************/
    //command: Set_Srv_Param=<srv ip>,<srv port>
    p = Ql_strstr(pData,"Set_Srv_Param=");
    if (p)
    {
        Ql_memset(m_SrvADDR, 0, SRVADDR_LEN);
        if (Analyse_Command(pData, 1, '>', m_SrvADDR))
        {
            APP_DEBUG("SOCKET Address Parameter Error.\r\n");
            return;
        }
        Ql_memset(srvport, 0, 10);
        if (Analyse_Command(pData, 2, '>', srvport))
        {
            APP_DEBUG("SOCKET Port Parameter Error.\r\n");
            return;
        }

    	socket_param_t.address = Ql_MEM_Alloc(sizeof(u8)*SRVADDR_LEN);
    	if(socket_param_t.address!=NULL)
    	{
    		Ql_memset(socket_param_t.address,0,SRVADDR_LEN);
            Ql_memcpy(socket_param_t.address,m_SrvADDR,sizeof(m_SrvADDR));
    	}
				
        socket_param_t.remote_port= Ql_atoi(srvport);
        APP_DEBUG("Set Server Parameter Successfully<%s>,<%d>\r\n",socket_param_t.address,socket_param_t.remote_port);
        return;
    }
    //command:SOCKET_ContextID=<contextID>
    p = Ql_strstr(pData,"SOCKET_ContextID=");
    if (p)
    {
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 1, '>', temp_buffer))
        {
    		APP_DEBUG("SOCKET Contextid Parameter Error.\r\n");
            return;
        }
		socket_param_t.contextID = Ql_atoi(temp_buffer);
		APP_DEBUG("Set Contextid Parameter Successfully<%d>\r\n",socket_param_t.contextID);
        return;
    }
	
	
    //command:SOCKET_ServiceType=<service_type>
    p = Ql_strstr(pData,"SOCKET_ServiceType=");
    if (p)
    {
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 1, '>', temp_buffer))
        {
    		APP_DEBUG("SOCKET service type Parameter Error.\r\n");
            return;
        }
		socket_param_t.service_type = Ql_atoi(temp_buffer);
		APP_DEBUG("Set service type Parameter Successfully<%d>\r\n",socket_param_t.service_type);
        return;
    }

	//command:SOCKET_LocalPort=<local_port>
    p = Ql_strstr(pData,"SOCKET_LocalPort=");
    if (p)
    {
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 1, '>', temp_buffer))
        {
    		APP_DEBUG("SOCKET local port Parameter Error.\r\n");
            return;
        }
		socket_param_t.local_port = Ql_atoi(temp_buffer);
		APP_DEBUG("Set local port Parameter Successfully<%d>\r\n",socket_param_t.local_port);
        return;
    }

    //command:SOCKET_ProtocolType=<protocol_type>
    p = Ql_strstr(pData,"SOCKET_ProtocolType=");
    if (p)
    {
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 1, '>', temp_buffer))
        {
    		APP_DEBUG("SOCKET protocol type Parameter Error.\r\n");
            return;
        }
		socket_param_t.protocol_type = Ql_atoi(temp_buffer);
		APP_DEBUG("Set protocol type Parameter Successfully<%d>\r\n",socket_param_t.protocol_type);
        return;
    }
		
	//command:SOCKET_Open=<connectid>
	p = Ql_strstr(pData,"SOCKET_Open=");
	if (p)
	{
		Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
		if (Analyse_Command(pData, 1, '>', temp_buffer))
		{
			APP_DEBUG("SOCKET Open Error.\r\n");
			return;
		}
		socket_param_t.connectID = Ql_atoi(temp_buffer);


        ret = RIL_SOC_QIOPEN(&socket_param_t);
		if(ret == 0)
        {				
            APP_DEBUG("<--start to create socket,ret =%d -->\r\n",ret);
        }
		else
        {
            APP_DEBUG("<--Open socket failure,ret=%d.-->\r\n",ret);
		}
		return;
	}


	//command: SOCKET_SENDDATA=<connectid>,<data>
    p = Ql_strstr(pData,"SOCKET_SENDDATA=");
    if (p)
    {
        Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 1, '>', temp_buffer))
        {
            APP_DEBUG("SOCKET Connectid Parameter Error.\r\n");
            return;
        }
		Ql_memset(m_send_buf, 0, SEND_BUFFER_LEN);
        if (Analyse_Command(pData, 2, '>', m_send_buf))
        {
            APP_DEBUG("SOCKET Send Data Parameter Error.\r\n");
            return;
        }
       m_socketid = Ql_atoi(temp_buffer);
       ret = RIL_SOC_QISEND(m_socketid,Ql_strlen(m_send_buf),m_send_buf);
        if (ret == 0)
        {
            APP_DEBUG("<--Send data successfully->\r\n");
        }else
        {
            APP_DEBUG("<--Send data failure,ret=%d.-->\r\n",ret);
        }
		return;
      }

	//command: SOCKET_SENDDATAHEX=<connectid>,<data>
    p = Ql_strstr(pData,"SOCKET_SENDDATAHEX=");
    if (p)
    {
        
        Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
        if (Analyse_Command(pData, 1, '>', temp_buffer))
        {
            APP_DEBUG("SOCKET Connectid Parameter Error.\r\n");
            return;
        }
		Ql_memset(m_send_buf, 0, SEND_BUFFER_LEN);
        if (Analyse_Command(pData, 2, '>', m_send_buf))
        {
            APP_DEBUG("SOCKET Send hex Data Parameter Error.\r\n");
            return;
        }
       m_socketid = Ql_atoi(temp_buffer);
       ret = RIL_SOC_QISENDEX(m_socketid,Ql_strlen(m_send_buf)/2,m_send_buf);
        if (ret == 0)
        {
            APP_DEBUG("<--Send data successfully->\r\n"); 
        }else
        {
            APP_DEBUG("<--Send data failure,ret=%d.-->\r\n",ret);
        }
		return;
      }

     //command: SOCKET_CLOSE=<connectid>
     p = Ql_strstr(pData,"SOCKET_CLOSE=");
     if (p)
     {
   
         Ql_memset(temp_buffer, 0, TEMP_BUFFER_LENGTH);
         if (Analyse_Command(pData, 1, '>', temp_buffer))
         {
             APP_DEBUG("<--SOCKET Port Parameter Error.-->\r\n");
             return;
         }
    
         ret = RIL_SOC_QICLOSE(m_socketid);
         if (ret == 0)
         {    
	        APP_DEBUG("<-- closed socketid successfully\r\n");          	        
         }else
         {
              APP_DEBUG("<--closed socketid failure,error=%d.-->\r\n",ret);
         }
		 return;
     }

	      //command: Ql_SleepEnable
     p = Ql_strstr(pData,"Ql_SleepEnable");
     if (p)
     {
	 	 Ql_SleepEnable();
         APP_DEBUG("Ql_SleepEnable Successful\\r\n");
		 return;
     }   

	 //command: Ql_SleepDisable
     p = Ql_strstr(pData,"Ql_SleepDisable");
     if (p)
     {
	 	 Ql_SleepDisable();
         APP_DEBUG("Ql_SleepDisable Successful\r\n");
		 return;
     } 


	 {// Read data from UART
  
       // Echo
       Ql_UART_Write(m_myUartPort, pData, len);
   
       p = Ql_strstr((char*)pData, "\r\n");
       if (p)
       {
           *(p + 0) = '\0';
           *(p + 1) = '\0';
       }
   
       // No permission for single <cr><lf>
       if (Ql_strlen((char*)pData) == 0)
       {
           return;
       }
       ret = Ql_RIL_SendATCmd((char*)pData, len, ATResponse_Handler, NULL, 0);
      }
}


static s32 ATResponse_Handler(char* line, u32 len, void* userData)
{
    APP_DEBUG("[ATResponse_Handler] %s\r\n", (u8*)line);
    
    if(Ql_RIL_FindLine(line, len, "OK"))
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


#endif // __EXAMPLE_TCPCLIENT__

