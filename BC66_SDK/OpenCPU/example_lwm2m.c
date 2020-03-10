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
    STATE_LwM2M_CONF,
    STATE_LwM2M_REG,
    STATE_LwM2M_ADDOBJ,
    STATE_LwM2M_UPDATE,
    STATE_LwM2M_OBSERVE_RSP,
    STATE_LwM2M_NOTIFY,
    STATE_LwM2M_WRITE_RSP,
    STATE_LwM2M_DEREG,
    STATE_LwM2M_TOTAL_NUM
}Enum_ONENETSTATE;
static u8 m_lwm2m_state = STATE_NW_QUERY_STATE;


/*****************************************************************
* LwM2M  timer param
******************************************************************/
#define LwM2M_TIMER_ID         TIMER_ID_USER_START
#define LwM2M_TIMER_PERIOD     (1000)

/*****************************************************************
* uart   param
******************************************************************/
static Enum_SerialPort m_myUartPort  = UART_PORT0;
#define SERIAL_RX_BUFFER_LEN  2048
static u8 m_RxBuf_Uart[SERIAL_RX_BUFFER_LEN];

/*****************************************************************
* Server Param
******************************************************************/
#define SRVADDR_BUFFER_LEN  (100)
#define SEND_BUFFER_LEN     (1024)
static u8 m_send_buf[SEND_BUFFER_LEN]={0};
static u8  m_SrvADDR[SRVADDR_BUFFER_LEN] = "180.101.147.115\0";
static u32 m_SrvPort = 5683;

/*****************************************************************
*  LwM2M Param
******************************************************************/
Lwm2m_Urc_Param_t*  lwm2m_urc_param_ptr = NULL;

#define   LWM2M_EndpointName_MAX   150  
ST_LwM2M_Param_t lwm2m_param_t;
static s32 ret;

/*****************************************************************
*  test Param
******************************************************************/
#define LWM2M_TEST_BUFFER_LENGTH    256
static u8 test_data[LWM2M_TEST_BUFFER_LENGTH] = "01F00035020056FFFFFFCD383633373033303333373033303337303330333337303330333337303330333337303330333337303330333337303330333337303330353132353436303131313137343830383738350000015FFB289A180100040200010097\0"; //send data

/*****************************************************************
* uart callback function
******************************************************************/
static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);

/*****************************************************************
* timer callback function
******************************************************************/
static void Callback_Timer(u32 timerId, void* param);

/*****************************************************************
* lwm2m recv callback function
******************************************************************/
static void Callback_Lwm2m_Write_Req(u8* buffer,u32 length);

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

    APP_DEBUG("<--OpenCPU: LwM2M Client.-->\r\n");

    //register 
    ret = Ql_Timer_Register(LwM2M_TIMER_ID, Callback_Timer, NULL);
	APP_DEBUG("<--Ql_Timer_Register(%d)-->\r\n",ret);
	
    //register write callback
    ret = Ql_Lwm2m_Write_Req_Register(Callback_Lwm2m_Write_Req);
    APP_DEBUG("<--register write request callback successful(%d)-->\r\n",ret);

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
    				if(SIM_STAT_READY == msg.param2)
    				{
    				   //ret = Ql_RIL_SendATCmd("AT+SM=LOCK\r\n",Ql_strlen("AT+SM=LOCK\r\n"),NULL,NULL,0);
                       //APP_DEBUG("AT+SM=LOCK(%d)\r\n", ret);
                       Ql_Timer_Start(LwM2M_TIMER_ID, LwM2M_TIMER_PERIOD, TRUE);
    				   APP_DEBUG("<--Ql_Timer_Start(%d)-->\r\n",ret);
    				}
					break;
    		    }
    			case URC_LWM2M_REG:
				{
					if(0 == msg.param2)
					{
						APP_DEBUG("<---urc_lwm2m_register successfully-->\r\n");
						m_lwm2m_state = STATE_LwM2M_ADDOBJ;
					}
					break;
				}
				case URC_LWM2M_UPDATE:
				{
					lwm2m_urc_param_ptr = msg.param2;
	
 					APP_DEBUG("<--urc_lwm2m_update,msgid(%d),update state(%d)-->\r\n", \
					lwm2m_urc_param_ptr->msgid,lwm2m_urc_param_ptr->update_status);
					
					break;
				}
				case URC_LWM2M_OBSERVE_REQ:
				{
					lwm2m_urc_param_ptr = msg.param2;
				
 					APP_DEBUG("<--urc_lwm2m_observe,msgid(%d),obj_id(%d),ins_id(%d),res_id(%d)-->\r\n", \
					lwm2m_urc_param_ptr->msgid,lwm2m_urc_param_ptr->obj_id,lwm2m_urc_param_ptr->ins_id, \
					lwm2m_urc_param_ptr->res_id);

					lwm2m_param_t.msgid = lwm2m_urc_param_ptr->msgid;
					lwm2m_param_t.objid = lwm2m_urc_param_ptr->obj_id;
					lwm2m_param_t.insid = lwm2m_urc_param_ptr->ins_id;
					lwm2m_param_t.resid= lwm2m_urc_param_ptr->res_id;

					m_lwm2m_state = STATE_LwM2M_OBSERVE_RSP;
					break;
				}
		        default:
    		    APP_DEBUG("<-- Other URC: type=%d\r\n", msg.param1);
    	        break;
			}
			break;
		}
	default:
         break;
        }
    }
}
 

static void Callback_Lwm2m_Write_Req(u8* buffer,u32 length)
{
    u8 strTmp[50]; 
	APP_DEBUG("<--buffer(%s),length(%d)-->\r\n",buffer,length);
	Ql_memset(strTmp, 0x0,	sizeof(strTmp));
    QSDK_Get_Str(buffer,strTmp,1);
    lwm2m_param_t.msgid= Ql_atoi(strTmp);
	m_lwm2m_state = STATE_LwM2M_WRITE_RSP;
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
                    m_lwm2m_state = STATE_LwM2M_CONF;
                }
                break;
            }
			case STATE_LwM2M_CONF:
            {
				ST_Lwm2m_Config_Param_t lwm2m_cfg_param_t;
				u8 strImei[30];
                Ql_memset(strImei, 0x0, sizeof(strImei));
				RIL_GetIMEI(strImei);
				
        		lwm2m_cfg_param_t.bs_mode = LWM2M_BS_MODE_DISABLE;
				
                Ql_memset(lwm2m_cfg_param_t.address, 0x0, MAX_IP_HOST_NAME);
				Ql_memcpy(lwm2m_cfg_param_t.address,m_SrvADDR,Ql_strlen(m_SrvADDR));
				lwm2m_cfg_param_t.remote_port = 5683;
				
				lwm2m_cfg_param_t.endpoint_name= (u8*)Ql_MEM_Alloc(sizeof(u8)*LWM2M_EndpointName_MAX);
                Ql_memset(lwm2m_cfg_param_t.endpoint_name, 0x0, LWM2M_EndpointName_MAX);
				Ql_memcpy(lwm2m_cfg_param_t.endpoint_name,strImei,Ql_strlen(strImei));
				
				lwm2m_cfg_param_t.lifetime = 300;
				lwm2m_cfg_param_t.security_mode = 3;

                ret =RIL_QLwM2M_Cfg(&lwm2m_cfg_param_t,FALSE);
				if(NULL != lwm2m_cfg_param_t.endpoint_name)
				{
                   Ql_MEM_Free(lwm2m_cfg_param_t.endpoint_name);
				   lwm2m_cfg_param_t.endpoint_name = NULL;
				}
                if (RIL_AT_SUCCESS == ret)
                {
                    APP_DEBUG("<-- Configure Parameters successfully\r\n");
                    m_lwm2m_state = STATE_LwM2M_REG;
                }else
                {
                    APP_DEBUG("<--Configure Parameters failure,error=%d.-->\r\n",ret);
                }
                break;
            }
			case STATE_LwM2M_REG:
            {

				ret = RIL_QLwM2M_Reg();
                if (RIL_AT_SUCCESS == ret)
                {
                    APP_DEBUG("<--send a deregister request to the IoT platform.\r\n");
                    m_lwm2m_state = STATE_LwM2M_TOTAL_NUM;
                }else
                {
                    APP_DEBUG("<--send a deregister request to the IoT platform failure,error=%d.-->\r\n",ret);
                }
                break;
            }
            case STATE_LwM2M_ADDOBJ:
            {
				ST_Lwm2m_object_Param_t lwm2m_object_param_t;
				lwm2m_object_param_t.obj_id = 19;
				lwm2m_object_param_t.ins_id = 0;
				lwm2m_object_param_t.res_num =2;
				lwm2m_object_param_t.res_id[0] = 0;
				lwm2m_object_param_t.res_id[1] = 1;
				
                ret = RIL_QLwM2M_Addobj(&lwm2m_object_param_t);
				if(RIL_AT_SUCCESS != ret)
                {
                    APP_DEBUG("<--Add object failure,ret=%d.-->\r\n",ret);
                }
                
				lwm2m_object_param_t.obj_id = 19;
				lwm2m_object_param_t.ins_id = 1;
				lwm2m_object_param_t.res_num =2;
				lwm2m_object_param_t.res_id[0] = 0;
				lwm2m_object_param_t.res_id[1] = 1;
				
                ret = RIL_QLwM2M_Addobj(&lwm2m_object_param_t);
				if (RIL_AT_SUCCESS == ret)
				{
                    m_lwm2m_state = STATE_LwM2M_UPDATE;
                    APP_DEBUG("<--Add object successfully->\r\n");
					
				    break;
				}
                else
                {
                    APP_DEBUG("<--Add object failure,ret=%d.-->\r\n",ret);
                }
                break;
            }

            case STATE_LwM2M_UPDATE:
            {
                ret = RIL_QLwM2M_Update(0,1000) ;
                if (RIL_AT_SUCCESS == ret)
                {
                    APP_DEBUG("<-- Send an Update Request\r\n");
                    m_lwm2m_state = STATE_LwM2M_TOTAL_NUM;
                }else
                {
                    APP_DEBUG("<--Send an Update Request failure,error=%d.-->\r\n",ret);
                }
                break;
            }
			case STATE_LwM2M_OBSERVE_RSP:
            {
				lwm2m_param_t.result_code= LWM2M_RESULT_CODE_1;
				lwm2m_param_t.value_type = LWM2M_VALUE_TYPE_STRING;
				lwm2m_param_t.len = 200;
				lwm2m_param_t.value= (u8*)Ql_MEM_Alloc(sizeof(u8)*LWM2M_TEST_BUFFER_LENGTH);
                Ql_memset(lwm2m_param_t.value, 0x0, LWM2M_TEST_BUFFER_LENGTH);
				Ql_memcpy(lwm2m_param_t.value,test_data,Ql_strlen(test_data));
				lwm2m_param_t.index = 0;//last message
				
				
                ret = RIL_QLwM2M_Observe_Rsp(&lwm2m_param_t);
				if(NULL != lwm2m_param_t.value)
				{
                   Ql_MEM_Free(lwm2m_param_t.value);
				   lwm2m_param_t.value = NULL;
				}

				if (RIL_AT_SUCCESS == ret)
                {
                    APP_DEBUG("<-- Respond to the Observe Request successfully\r\n");
                    m_lwm2m_state = STATE_LwM2M_TOTAL_NUM;
					
                }else
                {
                    APP_DEBUG("<--Respond to the Observe Request failure,error=%d.-->\r\n",ret);
					m_lwm2m_state = STATE_LwM2M_DEREG;
                }
                break;
            }
			case STATE_LwM2M_NOTIFY:
            {
				lwm2m_param_t.objid = 19;
				lwm2m_param_t.insid = 0;
				lwm2m_param_t.resid = 0;
				lwm2m_param_t.value_type = LWM2M_VALUE_TYPE_STRING;
				lwm2m_param_t.len = 200;
				lwm2m_param_t.value= (u8*)Ql_MEM_Alloc(sizeof(u8)*LWM2M_TEST_BUFFER_LENGTH);
                Ql_memset(lwm2m_param_t.value, 0x0, LWM2M_TEST_BUFFER_LENGTH);
				Ql_memcpy(lwm2m_param_t.value,test_data,Ql_strlen(test_data));
				lwm2m_param_t.index = 0;//last message
				
				lwm2m_param_t.ack= 0;
				lwm2m_param_t.rai= 0;
				
                ret =  RIL_LwM2M_Notify(&lwm2m_param_t,TRUE,TRUE);
				if(NULL != lwm2m_param_t.value)
				{
                   Ql_MEM_Free(lwm2m_param_t.value);
				   lwm2m_param_t.value = NULL;
				}

				if (RIL_AT_SUCCESS == ret)
                {
                    APP_DEBUG("<-- Notify the Data to Server successfully\r\n");
                    m_lwm2m_state = STATE_LwM2M_TOTAL_NUM;
					
                }else
                {
                    APP_DEBUG("<--Notify the Data to Server failure,error=%d.-->\r\n",ret);
					m_lwm2m_state = STATE_LwM2M_DEREG;
                }
                break;
            }
			case STATE_LwM2M_WRITE_RSP:
            {
            	ret = RIL_QLwM2M_Write_Rsp(lwm2m_param_t.msgid,LWM2M_RESULT_CODE_2);
            	APP_DEBUG("<--lwm2m write respond,ret(%d)-->\r\n",ret);
				
				m_lwm2m_state = STATE_LwM2M_NOTIFY;
                break;
            }
			case STATE_LwM2M_DEREG:
            {
                ret = RIL_QLwM2M_Dereg();
                if (RIL_AT_SUCCESS == ret)
                {
                    APP_DEBUG("<-- Send a Deregister Request successfully\r\n");
                    m_lwm2m_state = STATE_LwM2M_TOTAL_NUM;
                }else
                {
                    APP_DEBUG("<--Send a Deregister Request failure,error=%d.-->\r\n",ret);
                }
                break;
            }
			case STATE_LwM2M_TOTAL_NUM:
            {
              //APP_DEBUG("<--lwM2M test case-->\r\n");
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
	else if (Ql_RIL_FindString(line, len, "+CMEE ERROR"))
    {
        return  RIL_ATRSP_FAILED;
    }
    else if (Ql_RIL_FindString(line, len, "+CMS ERROR:"))
    {
        return  RIL_ATRSP_FAILED;
    }
    
    return RIL_ATRSP_CONTINUE; //continue wait
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


#endif // __EXAMPLE_LwM2M__

