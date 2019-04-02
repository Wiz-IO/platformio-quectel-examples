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
 *   example_LwM2M.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example demonstrates how to use LwM2M function with APIs in OpenCPU.
 *
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_LwM2M__" in gcc_makefile file. And compile the 
 *     app using "make clean/new".
 *     Download image bin to module to run.
 * 
 * note:
 *     This example shows how to send and receive hex data using direct push mode
 *============================================================================
 *             HISTORY
 *----------------------------------------------------------------------------
 * 
 ****************************************************************************/
#ifdef __EXAMPLE_LwM2M__
#include "custom_feature_def.h"
#include "ql_stdlib.h"
#include "ql_common.h"
#include "ql_type.h"
#include "ql_trace.h"
#include "ql_error.h"
#include "ql_uart.h"
#include "ql_timer.h"
#include "ril_network.h"
#include "ril_LwM2M.h"
#include "ril.h"
#include "ril_util.h"
#include "ril_system.h"



#define DEBUG_ENABLE 1
#if DEBUG_ENABLE > 0
#define DEBUG_PORT  UART_PORT0
#define DBG_BUF_LEN   1024
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
    STATE_NW_QUERY_STATE,
    STATE_LwM2M_SERV,
    STATE_LwM2M_CONF,
    STATE_LwM2M_ADDOBJ,
    STATE_LwM2M_OPEN,
    STATE_LwM2M_UPDATE,
    STATE_LwM2M_CFG,
    STATE_LwM2M_SEND,
    //STATE_LwM2M_RD,
    STATE_LwM2M_CLOSE,
    STATE_LwM2M_DELETE,
    STATE_TOTAL_NUM
}Enum_ONENETSTATE;
static u8 m_lwm2m_state = STATE_NW_QUERY_STATE;


/*****************************************************************
* LwM2M  timer param
******************************************************************/
#define LwM2M_TIMER_ID         TIMER_ID_USER_START
#define LwM2M_TIMER_PERIOD     1000


/*****************************************************************
* Server Param
******************************************************************/
static Enum_SerialPort m_myUartPort  = UART_PORT0;

#define SRVADDR_BUFFER_LEN  100
#define SEND_BUFFER_LEN     1024
#define RECV_BUFFER_LEN     1024

static u8 m_send_buf[SEND_BUFFER_LEN]={0};
static u8 m_recv_buf[RECV_BUFFER_LEN]={0};
static u8  m_SrvADDR[SRVADDR_BUFFER_LEN] = "180.101.147.115\0";
static u32 m_SrvPort = 5683;


/*****************************************************************
*  LwM2M Param
******************************************************************/
ST_Lwm2m_Send_Param_t  lwm2m_send_param_t = {0,0,0,0,NULL,0};
Lwm2m_Urc_Param_t*  lwm2m_urc_param_ptr = NULL;


static u8 test_data[128] = "011111\0"; //send data
u8 res_id[5] = "0\0";    // Resources id.

bool lwm2m_access_mode =  LWM2M_ACCESS_MODE_DIRECT;
//s32 recv_actual_length = 0;
//s32 recv_remain_length = 0;


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
static s32 ReadSerialPort(Enum_SerialPort port, /*[out]*/u8* pBuffer, /*[in]*/u32 bufLen);
static void proc_handle(u8 *pData,s32 len);

static s32 ret;
void proc_main_task(s32 taskId)
{
    ST_MSG msg;


    // Register & open UART port
    Ql_UART_Register(m_myUartPort, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(m_myUartPort, 115200, FC_NONE);

    APP_DEBUG("<--OpenCPU: LwM2M Client.-->\r\n");

    //register 
    ret = Ql_Timer_Register(LwM2M_TIMER_ID, Callback_Timer, NULL);
	APP_DEBUG("<--Ql_Timer_Register(%d)-->\r\n",ret);

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
    			APP_DEBUG("<-- SIM Card Status:%d -->\r\n", msg.param2);
				if(SIM_STAT_READY == msg.param2)
				{
				   //ret = Ql_RIL_SendATCmd("AT+SM=LOCK\r\n",Ql_strlen("AT+SM=LOCK\r\n"),NULL,NULL,0);
                   //APP_DEBUG("AT+SM=LOCK(%d)\r\n", ret);
                   Ql_Timer_Start(LwM2M_TIMER_ID, LwM2M_TIMER_PERIOD, TRUE);
				   APP_DEBUG("<--Ql_Timer_Start(%d)-->\r\n",ret);
				}
    			break;	
				case URC_LwM2M_OBSERVE:
				{
					lwm2m_urc_param_ptr = msg.param2;
					if(0 == lwm2m_urc_param_ptr->observe_flag && lwm2m_send_param_t.obj_id == lwm2m_urc_param_ptr->obj_id)
					{
     					APP_DEBUG("<--urc_lwm2m_observer,obj_id(%d),ins_id(%d),res_id(%d)-->\r\n", \
						lwm2m_urc_param_ptr->obj_id,lwm2m_urc_param_ptr->ins_id,lwm2m_urc_param_ptr->res_num);
						
     					m_lwm2m_state = STATE_LwM2M_SEND;
     					Ql_Timer_Start(LwM2M_TIMER_ID, LwM2M_TIMER_PERIOD, TRUE);
					}
				}
				break;
    		    case URC_LwM2M_RECV_DATA:
				{
					lwm2m_urc_param_ptr = msg.param2;
					if(lwm2m_send_param_t.obj_id == lwm2m_urc_param_ptr->obj_id)
					{
						if(lwm2m_urc_param_ptr->access_mode == LWM2M_ACCESS_MODE_DIRECT)
						{
        		          APP_DEBUG("<--lwm2m recv length(%d),recv data(%s)-->\r\n",lwm2m_urc_param_ptr->recv_length,lwm2m_urc_param_ptr->recv_buffer);
						  m_lwm2m_state = STATE_LwM2M_SEND;
						}
						Ql_Timer_Start(LwM2M_TIMER_ID, LwM2M_TIMER_PERIOD, FALSE);

					}
    		    }
    			break;
	
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
 
static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{
    switch (msg)
    {
    case EVENT_UART_READY_TO_READ:
        {
           break;
        }
    case EVENT_UART_READY_TO_WRITE:
        break;
    default:
        break;
    }
}

static void Callback_Timer(u32 timerId, void* param)
{
    if (LwM2M_TIMER_ID == timerId)
    {
        //APP_DEBUG("<--...........m_lwm2m_state=%d..................-->\r\n",m_lwm2m_state);
        switch (m_lwm2m_state)
        {        
            case STATE_NW_QUERY_STATE:
            {
                s32 cgreg = 0;
                ret = RIL_NW_GetEGPRSState(&cgreg);
                APP_DEBUG("<--Network State:cgreg=%d-->\r\n",cgreg);
                if((cgreg == NW_STAT_REGISTERED)||(cgreg == NW_STAT_REGISTERED_ROAMING))
                {
                    m_lwm2m_state = STATE_LwM2M_SERV;
                }
                break;
            }

            case STATE_LwM2M_SERV:
            {
                ret = RIL_QLwM2M_Serv(m_SrvADDR,m_SrvPort);
                if (RIL_AT_SUCCESS == ret)
                {
                    APP_DEBUG("<-configure address and port successfully-->\r\n");
                    m_lwm2m_state = STATE_LwM2M_CONF;
                }else
                {
                    APP_DEBUG("<--configure address and port failure, error=%d.-->\r\n",ret);
                }
                break;
            }

			case STATE_LwM2M_CONF:
            {
				char strImei[30];
               Ql_memset(strImei, 0x0, sizeof(strImei));
        		
        	    ret = RIL_GetIMEI(strImei);
                APP_DEBUG("<-- IMEI:%s,ret=%d -->\r\n", strImei,ret);
                
                ret = RIL_QLwM2M_Conf(strImei);
                if (RIL_AT_SUCCESS == ret)
                {
                    APP_DEBUG("<-- Configure Parameters successfully\r\n");
                    m_lwm2m_state = STATE_LwM2M_ADDOBJ;
                }else
                {
                    APP_DEBUG("<--Configure Parameters failure,error=%d.-->\r\n",ret);
                }
                break;
            }
            case STATE_LwM2M_ADDOBJ:
            {

				lwm2m_send_param_t.obj_id = 19;  //Object id. The max object id number is 65535
				lwm2m_send_param_t.ins_id = 0;	 //Instance id,uplink
				lwm2m_send_param_t.res_num = 1;   //Resources id number

                ret = RIL_QLwM2M_Addobj(lwm2m_send_param_t.obj_id,lwm2m_send_param_t.ins_id,lwm2m_send_param_t.res_num,res_id);
				if(RIL_AT_SUCCESS != ret)
                {
                    APP_DEBUG("<--Add object failure,ret=%d.-->\r\n",ret);
                }
                
				lwm2m_send_param_t.ins_id= 1;//Instance id,downlink
                ret = RIL_QLwM2M_Addobj(lwm2m_send_param_t.obj_id,lwm2m_send_param_t.ins_id,lwm2m_send_param_t.res_num,res_id);
				if (RIL_AT_SUCCESS == ret)
				{
                    m_lwm2m_state = STATE_LwM2M_CFG;
                    APP_DEBUG("<--Add object successfully->\r\n");
					
				    break;
				}
                else if(ret <0)
                {
                    APP_DEBUG("<--Add object failure,ret=%d.-->\r\n",ret);
                }
                break;
            }
			case STATE_LwM2M_CFG:
            {

				ret = RIL_QLwM2M_Cfg(LWM2M_DATA_FORMAT_HEX,LWM2M_DATA_FORMAT_HEX);
                if (RIL_AT_SUCCESS == ret)
                {
                    APP_DEBUG("<--Configure Optional Parameters successfully\r\n");
                    m_lwm2m_state = STATE_LwM2M_OPEN;
                }else
                {
                    APP_DEBUG("<--Configure Optional Parameters failure,error=%d.-->\r\n",ret);
                }
                break;
            }
            case STATE_LwM2M_OPEN:
            {
				Ql_Timer_Stop(LwM2M_TIMER_ID);
                ret = RIL_QLwM2M_Open(lwm2m_access_mode);
                if (RIL_AT_SUCCESS == ret)
                {
                    APP_DEBUG("<-- Open a Register Request successfully\r\n");
                }else
                {
                    APP_DEBUG("<--Open a Register Request failure,error=%d.-->\r\n",ret);
                }
                break;
            }
			
			 case STATE_LwM2M_SEND:
            {
				lwm2m_send_param_t.lwm2m_send_mode = LWM2M_SEND_MODE_NON;
				
				lwm2m_send_param_t.send_length = Ql_strlen(test_data);
				lwm2m_send_param_t.buffer = (u8*)Ql_MEM_Alloc(sizeof(u8)*SEND_BUFFER_LEN);
				if(lwm2m_send_param_t.buffer!=NULL)
				{
					Ql_memset(lwm2m_send_param_t.buffer,0,SEND_BUFFER_LEN);
                    Ql_memcpy(lwm2m_send_param_t.buffer,test_data,lwm2m_send_param_t.send_length);
				}

				lwm2m_send_param_t.send_length /=2;
                ret = RIL_QLwM2M_Send(&lwm2m_send_param_t) ;
                if (RIL_AT_SUCCESS == ret)
                {
					Ql_Timer_Stop(LwM2M_TIMER_ID);
                    APP_DEBUG("<-- send data successfully\r\n");
                    m_lwm2m_state = STATE_TOTAL_NUM;
					
                }else
                {
                    APP_DEBUG("<--send data failure,error=%d.-->\r\n",ret);
					m_lwm2m_state = STATE_LwM2M_CLOSE;
                }
                break;
            }
#if 0//Direct push mode does not need this case
			case STATE_LwM2M_RD:
            {
    			Ql_memset(m_recv_buf,0,RECV_BUFFER_LEN);

				ret = RIL_QLwM2M_RD(lwm2m_urc_param_ptr->recv_length,&recv_actual_length,&recv_remain_length,m_recv_buf);
				if (RIL_AT_SUCCESS == ret)
				{
                   if(recv_actual_length == 0)
                   {
     	              APP_DEBUG("<--The buffer has been read empty\r\n");
     				  return;
     			   }
     			   APP_DEBUG("<-- read data successfully,recv data(%s)\r\n",m_recv_buf);
    			   if(recv_remain_length == 0)
    			   {
    					m_lwm2m_state = STATE_LwM2M_SEND;
    			   }
    			   else
    			   {
    				    m_lwm2m_state = STATE_LwM2M_RD;//continue read buffer until the reamin length returns zero.
    			   }	
				}
				else 
     			{
                   m_lwm2m_state = STATE_LwM2M_CLOSE;
     			}
				Ql_Timer_Stop(LwM2M_TIMER_ID);
				Ql_Timer_Start(LwM2M_TIMER_ID, LwM2M_TIMER_PERIOD, FALSE);
				break;
            }
#endif 
			case STATE_LwM2M_CLOSE:
            {
                ret = RIL_QLwM2M_Close();
                if (RIL_AT_SUCCESS == ret)
                {
                    APP_DEBUG("<-- Send a Deregister Request successfully\r\n");
                    m_lwm2m_state = STATE_TOTAL_NUM;
                }else
                {
                    APP_DEBUG("<--Send a Deregister Request failure,error=%d.-->\r\n",ret);
                }
                break;
            }
			case STATE_TOTAL_NUM:
            {
  
              APP_DEBUG("<--lwM2M test case-->\r\n",ret);
			  m_lwm2m_state = STATE_TOTAL_NUM;
            }
            default:
                break;
        }    
    }
}

#endif // __EXAMPLE_LwM2M__

