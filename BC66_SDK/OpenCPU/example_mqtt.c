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
 *   example_mqtt.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example demonstrates how to use MQTT function with APIs in OpenCPU.
 *
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_MQTT__" in gcc_makefile file. And compile the 
 *     app using "make clean/new".
 *     Download image bin to module to run.
 * 
 * note:
 *     
 *============================================================================
 *             HISTORY
 *----------------------------------------------------------------------------
 * 
 ****************************************************************************/
#ifdef __EXAMPLE_MQTT__
#include "custom_feature_def.h"
#include "ql_stdlib.h"
#include "ql_common.h"
#include "ql_type.h"
#include "ql_trace.h"
#include "ql_error.h"
#include "ql_uart.h"
#include "ql_timer.h"
#include "ril_network.h"
#include "ril_mqtt.h"
#include "ril.h"
#include "ril_util.h"
#include "ril_system.h"
#include "ql_urc_register.h"



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
    STATE_MQTT_CFG,
    STATE_MQTT_OPEN,
    STATE_MQTT_CONN,
    STATE_MQTT_SUB,
    STATE_MQTT_PUB,
    STATE_MQTT_TUNS,
    STATE_MQTT_CLOSE,
    STATE_MQTT_DISC,
    STATE_MQTT_TOTAL_NUM
}Enum_ONENETSTATE;
static u8 m_mqtt_state = STATE_NW_QUERY_STATE;


/*****************************************************************
* MQTT  timer param
******************************************************************/
#define MQTT_TIMER_ID         TIMER_ID_USER_START
#define MQTT_TIMER_PERIOD     500


/*****************************************************************
* Server Param
******************************************************************/
#define SRVADDR_BUFFER_LEN  100
static u8  m_SrvADDR[SRVADDR_BUFFER_LEN] = "iot-as-mqtt.cn-shanghai.aliyuncs.com\0";
static u32 m_SrvPort = 1883;

/*****************************************************************
* uart   param
******************************************************************/
static Enum_SerialPort m_myUartPort  = UART_PORT0;

/*****************************************************************
*  MQTT Param
******************************************************************/
MQTT_Urc_Param_t*	 mqtt_urc_param_ptr = NULL;
ST_MQTT_topic_info_t  mqtt_topic_info_t;
static s32 ret;

/*****************************************************************
*  TEST Param
******************************************************************/
u8 connect_id = 0;
u32 pub_message_id = 0;
u32 sub_message_id = 0;

u8 product_key[] = "a1B9uSmaO23\0";
u8 device_name[]= "dev1\0";
u8 device_secret[] = "GnCV3VPnE4aHDubNmUSwNV5jAV0bkMEZ\0";
u8 clientID[] = "/a1B9uSmaO23/dev1/get\0";

static u8 test_data[128] = "hello quectel!!!\0"; //send data
static u8 test_topic[128] = "/a1B9uSmaO23/dev1/get\0"; //topic

/*****************************************************************
* uart callback function
******************************************************************/
static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);

/*****************************************************************
* timer callback function
******************************************************************/
static void Callback_Timer(u32 timerId, void* param);

/*****************************************************************
* mqtt recv callback function
******************************************************************/
static void callback_mqtt_recv(u8* buffer,u32 length);

/*****************************************************************
* other subroutines
******************************************************************/
static s32 ReadSerialPort(Enum_SerialPort port, /*[out]*/u8* pBuffer, /*[in]*/u32 bufLen);
static void proc_handle(u8 *pData,s32 len);

void proc_main_task(s32 taskId)
{
    ST_MSG msg;


    // Register & open UART port
    Ql_UART_Register(m_myUartPort, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(m_myUartPort, 115200, FC_NONE);

    APP_DEBUG("<--OpenCPU: MQTT Client.-->\r\n");

    //register 
    ret = Ql_Timer_Register(MQTT_TIMER_ID, Callback_Timer, NULL);
	APP_DEBUG("<--Ql_Timer_Register(%d)-->\r\n",ret);

	//register mqtt callback
    ret = Ql_Mqtt_Recv_Register(callback_mqtt_recv);
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
    			APP_DEBUG("<-- SIM Card Status:%d -->\r\n", msg.param2);
				if(SIM_STAT_READY == msg.param2)
				{
                   Ql_Timer_Start(MQTT_TIMER_ID, MQTT_TIMER_PERIOD, TRUE);
				   APP_DEBUG("<--Ql_Timer_Start(%d)-->\r\n",ret);
				}
    			break;	
				case URC_MQTT_OPEN:
				{
					mqtt_urc_param_ptr = msg.param2;
					if(0 == mqtt_urc_param_ptr->result)
					{
     					APP_DEBUG("<--open a MQTT client successfully-->\r\n");
                        m_mqtt_state = STATE_MQTT_CONN;
					}
					else
					{
						APP_DEBUG("<--open a MQTT client failure,error=%d.-->\r\n",mqtt_urc_param_ptr->result);
					}
				}
				break;
    		    case URC_MQTT_CONN:
				{
					mqtt_urc_param_ptr = msg.param2;
					if(0 == mqtt_urc_param_ptr->result)
					{
        		        APP_DEBUG("<--connection to MQTT server successfully->\r\n");
						m_mqtt_state = STATE_MQTT_SUB;
					}
					else
					{
						APP_DEBUG("<--connection to MQTT failure,error=%d.-->\r\n",mqtt_urc_param_ptr->result);
					}
    		    }
    			break;
                case URC_MQTT_SUB:
				{
					mqtt_urc_param_ptr = msg.param2;
					if((0 == mqtt_urc_param_ptr->result)&&(128 != mqtt_urc_param_ptr->sub_value[0]))
					{
        		        APP_DEBUG("<--subscribe topics successfully->\r\n");
						m_mqtt_state = STATE_MQTT_PUB;
					}
					else
					{
						APP_DEBUG("<--subscribe topics failure,error=%d.-->\r\n",mqtt_urc_param_ptr->result);
					}
    		    }
    			break;
				case URC_MQTT_PUB:
				{
					mqtt_urc_param_ptr = msg.param2;
					if(0 == mqtt_urc_param_ptr->result)
					{
        		        APP_DEBUG("<--publish messages to ali server successfully->\r\n");
						m_mqtt_state = STATE_MQTT_TOTAL_NUM;
					}
					else
					{
						APP_DEBUG("<--publish messages to ali server failure,error=%d.-->\r\n",mqtt_urc_param_ptr->result);
					}
    		    }
    			break;
			    case URC_MQTT_CLOSE:
				{
					mqtt_urc_param_ptr = msg.param2;
					if(0 == mqtt_urc_param_ptr->result)
					{
        		        APP_DEBUG("<--closed mqtt socket successfully->\r\n");
					}
					else
					{
						APP_DEBUG("<--closed mqtt socket failure,error=%d.-->\r\n",mqtt_urc_param_ptr->result);
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
static void callback_mqtt_recv(u8* buffer,u32 length)
{
	APP_DEBUG("<--buffer(%s),length(%d)-->\r\n",buffer,length);
	//m_mqtt_state = STATE_MQTT_PUB;   //loop publish message to server.
}

static void Callback_Timer(u32 timerId, void* param)
{
    if (MQTT_TIMER_ID == timerId)
    {
        //APP_DEBUG("<--...........m_mqtt_state=%d..................-->\r\n",m_mqtt_state);
        switch (m_mqtt_state)
        {        
            case STATE_NW_QUERY_STATE:
            {
                s32 cgreg = 0;
                ret = RIL_NW_GetEGPRSState(&cgreg);
                APP_DEBUG("<--Network State:cgreg=%d-->\r\n",cgreg);
                if((cgreg == NW_STAT_REGISTERED)||(cgreg == NW_STAT_REGISTERED_ROAMING))
                {
                    m_mqtt_state = STATE_MQTT_CFG;
                }
                break;
            }

            case STATE_MQTT_CFG:
            {
                ret = RIL_MQTT_QMTCFG_Ali(connect_id,product_key,device_name,device_secret);
                if (RIL_AT_SUCCESS == ret)
                {
                    APP_DEBUG("<- Ali Platform configure successfully-->\r\n");
                    m_mqtt_state = STATE_MQTT_OPEN;
                }else
                {
                    APP_DEBUG("<--Ali Platform configure failure,error=%d.-->\r\n",ret);
                }
                break;
            }

			case STATE_MQTT_OPEN:
            {
                ret = RIL_MQTT_QMTOPEN(connect_id,m_SrvADDR,m_SrvPort);
                if (RIL_AT_SUCCESS == ret)
                {
                    APP_DEBUG("<--Start opening an MQTT client-->\r\n");
                    m_mqtt_state = STATE_MQTT_TOTAL_NUM;
                }else
                {
                    APP_DEBUG("<--open a MQTT client failure,error=%d.-->\r\n",ret);
                }
                break;
            }
            case STATE_MQTT_CONN:
            {
			    ret = RIL_MQTT_QMTCONN(connect_id,clientID,NULL,NULL);
	            if (RIL_AT_SUCCESS == ret)
                {
                    APP_DEBUG("<--Start connection to MQTT server-->\r\n");
                    m_mqtt_state = STATE_MQTT_TOTAL_NUM;
                }else
                {
                    APP_DEBUG("<--connection to MQTT server failure,error=%d.-->\r\n",ret);
                }
                break;
                break;
            }
			case STATE_MQTT_SUB:
            {				
                mqtt_topic_info_t.count = 1;
				mqtt_topic_info_t.topic[0] = (u8*)Ql_MEM_Alloc(sizeof(u8)*256);
				
				Ql_memset(mqtt_topic_info_t.topic[0],0,256);
				Ql_memcpy(mqtt_topic_info_t.topic[0],"/a1B9uSmaO23/dev1/get\0",Ql_strlen("/a1B9uSmaO23/dev1/get\0"));
                mqtt_topic_info_t.qos[0] = QOS1_AT_LEASET_ONCE;
				sub_message_id++;  // 1-65535.
				
				ret = RIL_MQTT_QMTSUB(connect_id,sub_message_id,&mqtt_topic_info_t);
				
				Ql_MEM_Free(mqtt_topic_info_t.topic[0]);
	            mqtt_topic_info_t.topic[0] = NULL;
                if (RIL_AT_SUCCESS == ret)
                {
                    APP_DEBUG("<--Start subscribe topics-->\r\n");
                    m_mqtt_state = STATE_MQTT_TOTAL_NUM;
                }else
                {
                    APP_DEBUG("<--subscribe topics failure,error=%d.-->\r\n",ret);
                }
                break;
            }
            case STATE_MQTT_PUB:
            {
				
				pub_message_id++;  // The range is 0-65535. It will be 0 only when<qos>=0.
				ret = RIL_MQTT_QMTPUB(connect_id,pub_message_id,QOS1_AT_LEASET_ONCE,0,test_topic,test_data);
                if (RIL_AT_SUCCESS == ret)
                {
                    APP_DEBUG("<--Start publish messages to ali server-->\r\n");
                    m_mqtt_state = STATE_MQTT_TOTAL_NUM;
                }else
                {
                    APP_DEBUG("<--publish messages to ali server failure,error=%d.-->\r\n",ret);
                }
                break;
            }
			case STATE_MQTT_CLOSE:
            {
				ret = RIL_MQTT_QMTCLOSE(connect_id);
                if (RIL_AT_SUCCESS == ret)
                {
					m_mqtt_state = STATE_MQTT_TOTAL_NUM;
                    APP_DEBUG("<--Start closed MQTT socket-->\r\n");
                }else
                {
                    APP_DEBUG("<--closed MQTT socket failure,error=%d.-->\r\n",ret);
			    }
                break;
            }
			case STATE_MQTT_DISC:
            {
				ret = RIL_MQTT_QMTDISC(connect_id);
                if (RIL_AT_SUCCESS == ret)
                {
                    APP_DEBUG("<--Start disconnect MQTT socket-->\r\n");
                    m_mqtt_state = STATE_MQTT_TOTAL_NUM;
                }else
                {
                    APP_DEBUG("<--disconnect MQTT socket failure,error=%d.-->\r\n",ret); 
                }
                break;
            }
			case STATE_MQTT_TOTAL_NUM:
            {
              //APP_DEBUG("<--mqtt process wait->\r\n");
			  m_mqtt_state = STATE_MQTT_TOTAL_NUM;
			  break;
            }
            default:
                break;
        }    
    }
}

#endif // __EXAMPLE_MQTT__

