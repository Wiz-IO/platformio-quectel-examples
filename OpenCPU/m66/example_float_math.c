#ifdef __EXAMPLE_FLOAT_TEST__
 
#include <math.h>
#include "ql_common.h"
#include "ql_system.h"
#include "ql_type.h"
#include "ql_trace.h"
#include "ql_timer.h"
#include "ql_stdlib.h"
#include "ql_uart.h"
#include "ql_memory.h"
#include "ql_error.h"


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

static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{
     
}


static char m_strTemp[100];

#define PI                      3.1415926  
#define EARTH_RADIUS            6378.137


// Calculate radian
static float radian(float d)  
{  
     return d * PI / 180.0;
}  

static float get_distance(float lat1, float lng1, float lat2, float lng2)  
{  
     float radLat1 = radian(lat1);  
     float radLat2 = radian(lat2);  
     float a = radLat1 - radLat2;  
     float b = radian(lng1) - radian(lng2);  
	 
     float dst = 2 * asin((sqrt(pow(sin(a / 2), 2) + cos(radLat1) * cos(radLat2) * pow(sin(b / 2), 2) )));  
     dst = dst * EARTH_RADIUS;  
     dst= round(dst * 10000) / 10000;  
     return dst;  
} 

static void test_dis(void)
{  
     float lat1 = 39.90744;  
     float lng1 = 116.41615;
     float lat2 = 39.90744;  
     float lng2 = 116.30746;
     float dst ;

     // test: float as arguments
     dst = get_distance(lat1, lng1, lat2, lng2);  
     APP_DEBUG("dst = %0.3fkm\r\n", dst);  //dst = 9.281km  
} 

static void test_float(void)
{
    double a,b,s,pos;
    double radLat1 = 31.11;
    double radLat2 = 121.29;

    double lat = Ql_atof("58.12348976");
    double deg = Ql_atof("0.5678");
    double sec = 10.0;
    double sum = deg + sec;

    sec = round(lat);
    lat += sec;
    APP_DEBUG("sum=%f\r\n", lat+deg);
    APP_DEBUG("atof1=%.3f atof2=%f var=%f sum=%f\r\n", lat, deg, sec, sum);
    APP_DEBUG("atof1=%.3f atof2=%f var=%f\r\n", lat, deg, sec);    //Works OK, prints: "atof1=58.123 atof2=0.567800 var=58.000000"


    a = sin(45.0);
    b = cos(30.0);
    s = sqrt(81);
    pos = 2 * asin(sqrt(pow(sin(a/2),2) + cos(radLat1)*cos(radLat2)*pow(sin(b/2),2)));
    APP_DEBUG("Float test, a=%.2f,b=%.2f,radLat1=%.3f,radLat2=%.3f, s=%.5f\r\n", a,b,radLat1,radLat2,s);
    APP_DEBUG("Float test, pos=%g\r\n", pos);
    Ql_sprintf(m_strTemp, "Float test, a=%.2f,b=%.2f,radLat1=%.3f,radLat2=%.3f, s=%.5f\r\n", a,b,radLat1,radLat2,s);
    APP_DEBUG(m_strTemp);

}


static ST_MSG  msg; 
void proc_main_task(s32 taskId)
{
    s32 ret;
    u32 cnt = 0; 

    char Buff[10]="0.345";
    char* pStr;

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


    APP_DEBUG("OpenCPU: float test !\r\n\r\n");    /* Print out message through DEBUG port */

    test_float();
    test_dis();
    
    // Start message loop of this task
    while (TRUE)
    {
        Ql_OS_GetMessage(&msg);
        switch(msg.message)
        {
        case MSG_ID_USER_START:
            break;
        default:
            break;
        }
    }
    Ql_MEM_Free(pStr);
}

#endif // __EXAMPLE_FLOAT_TEST__

