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
 *   example_pwm.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example demonstrates how to use PWM function with APIs in OpenCPU.
 *   Input the specified command through any uart port and the result will be 
 *   output through the debug port.
 *   The gpio for PWM is PINNAME_NETLIGHT.
 *
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_PWM__" in gcc_makefile file. And compile the 
 *     app using "make clean/new".
 *     Download image bin to module to run.
 * 
 *   Operation:
 *            
 *     Input "Init PWM=<pwmSrcClk>,<pwmDiv>,<lowPulseNum>,<highPulseNum>" to initialize
 *     the parameters of PWM.
 *     Input "Output PWM=<pwmOnOff>" to control PWM on or off.
 *     Input "Uninit PWM" to release the PWM pin.
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
#ifdef __EXAMPLE_PWM__  
#include "ql_type.h"
#include "ql_stdlib.h"
#include "ql_trace.h"
#include "ql_gpio.h"
#include "ql_pwm.h"
#include "ql_uart.h"
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



/*****************************************************************
* UART Param
******************************************************************/
static u8 m_Read_Buffer[512];

/*****************************************************************
* callback function
******************************************************************/
static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);

/*****************************************************************
* other subroutines
******************************************************************/
extern s32 Analyse_Command(u8* src_str,s32 symbol_num,u8 symbol, u8* dest_buf);


void proc_main_task(void)
{
    s32 ret;
    ST_MSG msg;
    bool keepGoing = TRUE;

    
    // Register & open UART port for send the test command
    Ql_UART_Register(UART_PORT1, CallBack_UART_Hdlr, NULL);
    Ql_UART_Register(UART_PORT2, CallBack_UART_Hdlr, NULL);
    Ql_UART_Register(UART_PORT3, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(UART_PORT1, 115200, FC_NONE);
    Ql_UART_Open(UART_PORT2, 115200, FC_NONE);   
    Ql_UART_Open(UART_PORT3, 115200, FC_NONE); 

    
    APP_DEBUG("\r\n<--OpenCPU: PWM TEST!-->\r\n");  
    
    while (keepGoing)
    {
         Ql_OS_GetMessage(&msg);
        switch(msg.message)
        {
        default:
            break;
        }       
    }

}

static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{
    //APP_DEBUG("CallBack_UART_Hdlr: port=%d, event=%d, level=%d, p=%x\r\n", port, msg, level, customizedPara);
    u32 rdLen=0;
    s32 ret;
    char *p = NULL;
    
    switch (msg)
    {
    
        case EVENT_UART_READY_TO_READ:
        {
            Ql_memset(m_Read_Buffer, 0x0, sizeof(m_Read_Buffer));
            rdLen = Ql_UART_Read(port, m_Read_Buffer, sizeof(m_Read_Buffer));

            //command: Init PWM=<pwmSrcClk>,<pwmDiv>,<lowPulseNum>,<highPulseNum>
            p = Ql_strstr(m_Read_Buffer,"Init PWM=");
            if(p)
            {
                u8 pwmSrcClk[10] = {0};
                u8 pwmDiv[10] = {0};
                u8 lowPulseNum[10] = {0};
                u8 highPulseNum[10] = {0};

                if (Analyse_Command(m_Read_Buffer, 1, '>', pwmSrcClk))
                {
                    APP_DEBUG("<--Parameter I Error.-->\r\n");
                    break;
                }
                if (Analyse_Command(m_Read_Buffer, 2, '>', pwmDiv))
                {
                    APP_DEBUG("<--Parameter II Error.-->\r\n");
                    break;
                }
                if (Analyse_Command(m_Read_Buffer, 3, '>', lowPulseNum))
                {
                    APP_DEBUG("<--Parameter III Error.-->\r\n");
                    break;
                }
                if (Analyse_Command(m_Read_Buffer, 4, '>', highPulseNum))
                {
                    APP_DEBUG("<--Parameter IV Error.-->\r\n");
                    break;
                }
                
                ret = Ql_PWM_Init(PINNAME_NETLIGHT,Ql_atoi(pwmSrcClk),Ql_atoi(pwmDiv),Ql_atoi(lowPulseNum),Ql_atoi(highPulseNum));
                if(ret < 0 )
                {
                    APP_DEBUG("<--Init PWM faliure,pin=%d,pwmSrcClk=%d,pwmDiv=%d,lowPulseNum=%d,highPulseNum=%d,ret=%d-->\r\n",PINNAME_NETLIGHT,Ql_atoi(pwmSrcClk), Ql_atoi(pwmDiv), Ql_atoi(lowPulseNum),Ql_atoi(highPulseNum), ret);
                }else
                {
                    APP_DEBUG("<--Init PWM successfully,pin=%d,pwmSrcClk=%d,pwmDiv=%d,lowPulseNum=%d,highPulseNum=%d-->\r\n",PINNAME_NETLIGHT,Ql_atoi(pwmSrcClk), Ql_atoi(pwmDiv), Ql_atoi(lowPulseNum),Ql_atoi(highPulseNum));
                }
                break;
            }

             //command: Output PWM=<pwmOnOff>
            p = Ql_strstr(m_Read_Buffer,"Output PWM=");
            if(p)
            {
                u8 pwmOnOff[10] = {0};

                if (Analyse_Command(m_Read_Buffer, 1, '>', pwmOnOff))
                {
                    APP_DEBUG("<--Parameter I Error.-->\r\n");
                    break;
                }
                ret = Ql_PWM_Output(PINNAME_NETLIGHT,Ql_atoi(pwmOnOff));
                if(ret < 0 )
                {
                    APP_DEBUG("<--Output PWM faliure,pin=%d,pwmOnOff=%d,ret=%d-->\r\n",PINNAME_NETLIGHT,Ql_atoi(pwmOnOff), ret);
                }else
                {
                    APP_DEBUG("<--Output PWM successfully,pin=%d,pwmOnOff=%d-->\r\n",PINNAME_NETLIGHT,Ql_atoi(pwmOnOff));
                }
                break;
            }  

             //command: Uninit PWM
            p = (Ql_strstr(m_Read_Buffer,"Uninit PWM"));
            if(p)
            {
                ret = Ql_PWM_Uninit(PINNAME_NETLIGHT);
                if(ret < 0 )
                {
                    APP_DEBUG("<--Output PWM faliure,pin=%d,ret=%d-->\r\n",PINNAME_NETLIGHT,ret);
                }else
                {
                    APP_DEBUG("<--Output PWM successfully,pin=%d-->\r\n",PINNAME_NETLIGHT);
                }
                break;
            }
        }
        default:
            break;
    }
}



#endif // __EXAMPLE_PWM__

