#ifdef __EXAMPLE_ALARM__
/***************************************************************************************************
*   Example:
*       
*           ALARM Routine
*
*   Description:
*
*           This example gives an example for ALARM operation.it use to start up module when time coming.
*           Through Uart port, input the special command, there will be given the response about module operation.
*
*   Usage:
*
*           Precondition:
*
*                   Use "make clean/new" to compile, and download bin image to module.
*           
*           Through Uart port:
*
*               Input "Ql_GetLocalTime"  to retrieve the current local date and time.
*               Input "Ql_SetLocalTime"  to set the current local date and time.
*                      ex: Ql_SetLocalTime=2012,06,06,18,01,00
*               Input "RIL_Alarm_Create=year,month,day,hour,minute,second#repeattype" 
*                      to set the alarm start time and repeat type. If -1 is returned, pleae check
*                      and ensure the set time is later that the current local time.
*                      ex: RIL_Alarm_Create=12,06,06,18,02,00#0
*               Input "Ql_PowerDown=1"   to switch off the power of the module.
*               Input "RIL_Alarm_Remove" to switch off alarm function.
*       
****************************************************************************************************/
#include "ril.h"
#include "ril_util.h"
#include "custom_feature_def.h"
#include "ql_type.h"
#include "ql_trace.h"
#include "ql_system.h"
#include "ql_uart.h"
#include "ql_stdlib.h"
#include "ql_time.h"
#include "ql_timer.h"
#include "ql_power.h"
#include "ril_alarm.h"


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
static u8 m_RxBuf_Uart[SERIAL_RX_BUFFER_LEN];
static void Callback_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* param);

static ST_Time m_sysTime_Alarm; 
static s32 m_pwrOnReason;
static s32 BuildAlarmTime( u8* strTime, ST_Time* alarmTime);

void proc_main_task(s32 taskId)
{
    ST_MSG msg;

    // Register & open UART port
    Ql_UART_Register(UART_PORT1, Callback_UART_Hdlr, NULL);
    Ql_UART_Open(UART_PORT1, 115200, FC_NONE);

    Ql_UART_Register(UART_PORT2, Callback_UART_Hdlr, NULL);
    Ql_UART_Open(UART_PORT2, 115200, FC_NONE);
    
	APP_DEBUG("\r\n<--OpenCPU: Alarm -->\r\n"); 
    m_pwrOnReason = Ql_GetPowerOnReason();
	APP_DEBUG("\r\n<--Power on reason:%d -->\r\n", m_pwrOnReason); 
    
    while (1)
    {
        Ql_OS_GetMessage(&msg);
        switch(msg.message)
        {
#ifdef __OCPU_RIL_SUPPORT__
        case MSG_ID_RIL_READY:
            APP_DEBUG("<-- RIL is ready -->\r\n");
            Ql_RIL_Initialize();

            // Set cfun (1) if power on alarm by sending AT+CFUN=1 through RIL
            if (RTCPWRON == m_pwrOnReason)
            {// Power on Alarm, need to set CFUN (1)
                s32 iRet = Ql_RIL_SendATCmd("AT+CFUN=1", 9, NULL, NULL, 0);
                APP_DEBUG("<-- Power on alarm, switch CFUN from 0 to 1, iRet=%d -->\r\n", iRet);

                // TODO: Do what you want on alarm, or put it in "URC_ALARM_RING_IND"
                // ...
            }
            break;
#endif
        case MSG_ID_URC_INDICATION:
        {
            switch (msg.param1)
            {
            case URC_CFUN_STATE_IND:
                APP_DEBUG("<-- CFUN Status:%d -->\r\n", msg.param2);
                break;
            case URC_ALARM_RING_IND:
                APP_DEBUG("<-- alarm is ringing -->\r\n");
                // TODO: Do what you want on alarm
                // ...
                break;
            default:
                //APP_DEBUG("<-- Other URC: type=%d, param=%d\r\n", msg.param1, msg.param2);
                break;
            }
            break;
        }
        default:
            //APP_DEBUG("<-- rcv msg, id=%d, p1=%d, p2=%d -->\r\n", msg.message, msg.param1, msg.param2);
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
        APP_DEBUG("<--Fail to read from port[%d]-->\r\n", port);
        return -99;
    }
    return rdTotalLen;
}
static void Callback_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* param)
{
    s32 iRet = 0;
    switch (msg)
    {
    case EVENT_UART_READY_TO_READ:
        {
            char* p = NULL;
            s32 totalBytes = ReadSerialPort(port, m_RxBuf_Uart, sizeof(m_RxBuf_Uart));
            if (totalBytes <= 0)
            {
                break;
            }
            
            // Command: Ql_SetLocalTime=year,month,day,hour,minute,second
            //          Ql_SetLocalTime=2012,06,06,18,04,00
			p = Ql_strstr(m_RxBuf_Uart, "Ql_SetLocalTime=");
            if (p)
            {
                ST_Time SysTime; 
				Ql_memset(&SysTime, 0, sizeof(SysTime));
                BuildAlarmTime(m_RxBuf_Uart, &SysTime);
				APP_DEBUG("\r\nNew Time :\r\n%02d/%02d/%02d %02d:%02d:%02d\r\n",SysTime.year, SysTime.month, SysTime.day, SysTime.hour, SysTime.minute, SysTime.second);

				iRet = Ql_SetLocalTime(&SysTime);
				APP_DEBUG("\r\nQl_SetLocalTime(iRet = %d)", iRet);
                break;
            }

            // Command: Ql_GetLocalTime
            p = Ql_strstr(m_RxBuf_Uart, "Ql_GetLocalTime");
            if (p)
            {
                ST_Time SysTime; 
				Ql_memset(&SysTime, 0, sizeof(SysTime));
				Ql_GetLocalTime(&SysTime);
				APP_DEBUG("\r\nCurrent Time :\r\n%02d/%02d/%02d %02d:%02d:%02d\r\n",SysTime.year, SysTime.month, SysTime.day, SysTime.hour, SysTime.minute, SysTime.second);
                break;
            }
            
            // Command: RIL_Alarm_Create=year,month,day,hour,minute,second#repeattype
            //          RIL_Alarm_Create=12,06,06,18,05,00#0
            p = Ql_strstr(m_RxBuf_Uart, "RIL_Alarm_Create=");
            if(p)
            {
                s32 recurr;
                s8 buffer[30];
                Ql_strcpy(buffer,m_RxBuf_Uart);

				Ql_memset(&m_sysTime_Alarm, 0, sizeof(m_sysTime_Alarm));
                BuildAlarmTime((u8 *)m_RxBuf_Uart, &m_sysTime_Alarm);

                p = Ql_strstr(buffer, "#");
                p = p + 1;
                recurr = Ql_atoi(p);
                iRet = RIL_Alarm_Create(&m_sysTime_Alarm,(u8)recurr);
                APP_DEBUG("\r\nSet Alarm :\r\n%02d/%02d/%02d %02d:%02d:%02d,recurr=%d,iRet=%d\r\n",
                    m_sysTime_Alarm.year, m_sysTime_Alarm.month,m_sysTime_Alarm.day, 
                    m_sysTime_Alarm.hour, m_sysTime_Alarm.minute, m_sysTime_Alarm.second,recurr,
                    iRet);
                break;
            }
            
            // Command: Ql_PowerDown
            p = Ql_strstr(m_RxBuf_Uart, "Ql_PowerDown");
            if(p)
            {
                APP_DEBUG("power down\r\n");
                Ql_PowerDown(1);
                break;
             }

            // Command: RIL_Alarm_Remove
            p = Ql_strstr(m_RxBuf_Uart, "RIL_Alarm_Remove");
            if(p)
            {
                iRet = RIL_Alarm_Remove(&m_sysTime_Alarm);
                APP_DEBUG("RIL_Alarm_Remove=%d\r\n",iRet);
                break;
            }
            
            break;
        }
    case EVENT_UART_DTR_IND:// DTR level changed, developer can wake up the module here
        {
            if(0 == level)
            {
                APP_DEBUG("DTR set to low =%d  wake !!\r\n", level);
                Ql_SleepDisable();
            } else {
                APP_DEBUG("DTR set to high =%d  Sleep \r\n", level);
                Ql_SleepEnable();
            }

        }
    case EVENT_UART_READY_TO_WRITE:
        break;
    default:
        break;
    }
}

static s32 BuildAlarmTime( u8* strTime, ST_Time* alarmTime)
{
    s8 *p = NULL;
    s8 *q = NULL;

    if ((NULL == strTime) || (NULL == alarmTime))
    {
        return -1;
    }
                        // Year
	p = Ql_strstr(strTime, "=");
	p++;
	q = Ql_strstr(p, ",");
	(*q) = 0;
	alarmTime->year = Ql_atoi(p);
						// Month
	q++;
	p = Ql_strstr(q, ",");
	(*p) = 0;
	alarmTime->month = Ql_atoi(q);
					   // Day
	p++;
	q = Ql_strstr(p, ",");
	(*q)=0;
	alarmTime->day = Ql_atoi(p);
	                   // Hour
	q++;
	p = Ql_strstr(q, ",");
	(*p)=0;
	alarmTime->hour = Ql_atoi(q);
					   // Minute
	p++;
	q = Ql_strstr(p, ",");
	(*q)=0;
	alarmTime->minute = Ql_atoi(p);
	                  // Second
    q++;
    p = Ql_strstr(q, "#");
    if(p)
    {
        (*p)=0;
        alarmTime->second = Ql_atoi(q);
    } else {
        alarmTime->second = Ql_atoi(q);
    }
    
    return 0;
}

#endif  //__EXAMPLE_ALARM__

