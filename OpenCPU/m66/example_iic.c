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
 *   example_iic.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example demonstrates how to use iic function with APIs in OpenCPU.
 *   Input the specified command through any uart port and the result will be 
 *   output through the debug port.
 *   you can use any of two pins for the simultion IIC.And we have a IIC Controller
 *   interface ,PINNAME_GPIO8 for IIC SCL pin and PINNAME_GPIO9 for IIC SDA pin.
 *
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_IIC__" in gcc_makefile file. And compile the 
 *     app using "make clean/new".
 *     Download image bin to module to run.
 * 
 *   Operation:
 *            
 *     If input "Ql_IIC_Init=", that will initialize the IIC channel.
 *     If input "Ql_IIC_Config=", that will configure the IIC parameters.
 *     If input "Ql_IIC_Write=", that will write bytes to slave equipment through IIC interface.
 *     If input "Ql_IIC_Read=", that will read bytes from slave equipment through IIC interface.
 *     If input "Ql_IIC_Write_Read=", that will read and write bytes through IIC interface.
 *     If input "Ql_IIC_Uninit=", that will release the IIC pins.
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
#ifdef __EXAMPLE_IIC__
#include "ql_type.h"
#include "ql_stdlib.h"
#include "ql_trace.h"
#include "ql_gpio.h"
#include "ql_uart.h"
#include "ql_iic.h"
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




static u8 m_Read_Buffer[1024];
static u8 m_buffer[100];

static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);

void proc_main_task(s32 taskId)
{
    s32 ret;
    ST_MSG msg;

    // Register & open UART port
    Ql_UART_Register(UART_PORT1, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(UART_PORT1, 115200, FC_NONE);

    Ql_UART_Register(UART_PORT2, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(UART_PORT2, 115200, FC_NONE);

    APP_DEBUG("\r\n<--OpenCPU: IIC TEST!-->\r\n"); 

    while (1)
    {
        Ql_OS_GetMessage(&msg);
        switch(msg.message)
        {
        case 0:
            break;
        default:
            break;
        }
    }
}

static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{
    u32 rdLen=0;
    s32 ret;
    char *p=NULL;
    char *p1=NULL;
    char *p2=NULL;
    u8 write_buffer[4]={'a','b','c','D'};
    u8 read_buffer[6]={0x00,0x00,0x00,0x00,0x00,0x00};
    u8 registerAdrr[2]={0x01,0x45};
    switch (msg)
    {
        case EVENT_UART_READY_TO_READ:
        {
            Ql_memset(m_Read_Buffer, 0x0, sizeof(m_Read_Buffer));
            rdLen = Ql_UART_Read(port, m_Read_Buffer, sizeof(m_Read_Buffer));

            //command-->Init the IIC , type 0 Лђеп 1.
            p = Ql_strstr(m_Read_Buffer,"Ql_IIC_Init=");
            if(p)
            {
                char* p1 = NULL;
                char* p2 = NULL;
                u8 NumberBuf[10];
                s8 IIC_type=0;
                p1 = Ql_strstr(m_Read_Buffer, "=");
                p2 = Ql_strstr(m_Read_Buffer, "\r\n");
                Ql_memset(NumberBuf, 0x0, sizeof(NumberBuf));
                Ql_memcpy(NumberBuf, p1 + 1, p2 - p1 -1);
                IIC_type = Ql_atoi(NumberBuf);
                if(0 == IIC_type)// simultion iic test, and we choose PINNAME_GPIO4, PINNAME_GPIO5 for IIC SCL and SDA pins
                {                    
                    ret = Ql_IIC_Init(0,PINNAME_CTS,PINNAME_RTS,0);
                    if(ret < 0)
                    {
                        APP_DEBUG("\r\n<--Failed!! Ql_IIC_Init channel 0 fail ret=%d-->\r\n",ret);
                        break;
                    }
                    APP_DEBUG("\r\n<--pins(%d & %d) Ql_IIC_Init channel 0 ret=%d-->\r\n",PINNAME_CTS,PINNAME_RTS,ret);
                    break;
                }
                else if(1 == IIC_type)// IIC controller
                {
                    ret = Ql_IIC_Init(1,PINNAME_RI,PINNAME_DCD,1);
                    if(ret < 0)
                    {
                        APP_DEBUG("\r\n<--Failed!! IIC controller Ql_IIC_Init channel 1 fail ret=%d-->\r\n",ret);
                        break;
                    }
                    APP_DEBUG("\r\n<--pins(SCL=%d,SDA=%d) IIC controller Ql_IIC_Init channel 1 ret=%d-->\r\n",PINNAME_RI,PINNAME_DCD,ret);
                    break;

                }
                else
                {
                    APP_DEBUG("\r\n<--IIC type error!!!!-->\r\n");
                    break;
                }

            }
	     //command-->IIC config,  IIC controller interface
            Ql_memset(m_buffer, 0x0, sizeof(m_buffer));
            Ql_sprintf(m_buffer, "Ql_IIC_Config=0\r\n");//   simultion IIC  (channel 0)
            ret = Ql_strncmp(m_Read_Buffer, m_buffer, Ql_strlen(m_buffer));                
            if(0 == ret)
            {
                ret = Ql_IIC_Config(0,TRUE, 0x07, 0);// simultion IIC interface ,do not care the IicSpeed.
                if(ret < 0)
                {
                    APP_DEBUG("\r\n<--Failed !!Ql_IIC_Config channel 0 fail ret=%d-->\r\n",ret);
                    break;
                }
                APP_DEBUG("\r\n<--Ql_IIC_Config channel 0 ret=%d-->\r\n",ret);
                break;
            }
            
            //command-->IIC config,  IIC controller interface
            Ql_memset(m_buffer, 0x0, sizeof(m_buffer));
            Ql_sprintf(m_buffer, "Ql_IIC_Config=1\r\n");//   IIC controller  (channel 1)
            ret = Ql_strncmp(m_Read_Buffer, m_buffer, Ql_strlen(m_buffer));                
            if(0 == ret)
            {
                ret = Ql_IIC_Config(1,TRUE, 0x07, 300);// just for the IIC controller
                if(ret < 0)
                {
                    APP_DEBUG("\r\n<--Failed !! IIC controller Ql_IIC_Config channel 1 fail ret=%d-->\r\n",ret);
                    break;
                }
                APP_DEBUG("\r\n<--IIC controller Ql_IIC_Config channel 1 ret=%d-->\r\n",ret);
                break;
            }
            
            //command-->IIC write  
            Ql_memset(m_buffer, 0x0, sizeof(m_buffer));
            Ql_sprintf(m_buffer, "Ql_IIC_Write=0\r\n");//  channel 0 
            ret = Ql_strncmp(m_Read_Buffer, m_buffer, Ql_strlen(m_buffer));                
            if(0 == ret)
            {
                ret = Ql_IIC_Write(0, 0x07, write_buffer, 4);
                if(ret < 0)
                {
                    APP_DEBUG("\r\n<--Failed !! Ql_IIC_Write channel 0 fail ret=%d-->\r\n",ret);
                    break;               
                }
                APP_DEBUG("\r\n<--channel 0 Ql_IIC_Write ret=%d-->\r\n",ret);
                break;
            }

            Ql_memset(m_buffer, 0x0, sizeof(m_buffer));
            Ql_sprintf(m_buffer, "Ql_IIC_Write=1\r\n");//  channel 1 
            ret = Ql_strncmp(m_Read_Buffer, m_buffer, Ql_strlen(m_buffer));                
            if(0 == ret)
            {
                ret = Ql_IIC_Write(1, 0x07, write_buffer,4);
                if(ret < 0)
                {
                    APP_DEBUG("\r\n<--Failed !! IIC controller Ql_IIC_Write channel 1 fail ret=%d-->\r\n",ret);
                    break;
                }
                APP_DEBUG("\r\n<--IIC controller Ql_IIC_Write ret=%d-->\r\n",ret);
                break;
            }

            //command-->IIC read  
            Ql_memset(m_buffer, 0x0, sizeof(m_buffer));
            Ql_sprintf(m_buffer, "Ql_IIC_Read=0\r\n");//  channel 0 
            ret = Ql_strncmp(m_Read_Buffer, m_buffer, Ql_strlen(m_buffer));                
            if(0 == ret)
            {
                ret = Ql_IIC_Read(0, 0x07, read_buffer, 6);
                if(ret < 0)
                {
                    APP_DEBUG("\r\n<--Failed !! Ql_IIC_Read channel 0 fail ret=%d-->\r\n",ret);
                    break;               
                }
                APP_DEBUG("\r\n<--channel 0 Ql_IIC_Read ret=%d-->\r\n",ret);
                break;
            }

            Ql_memset(m_buffer, 0x0, sizeof(m_buffer));
            Ql_sprintf(m_buffer, "Ql_IIC_Read=1\r\n");//  channel 1 
            ret = Ql_strncmp(m_Read_Buffer, m_buffer, Ql_strlen(m_buffer));                
            if(0 == ret)
            {
                ret = Ql_IIC_Read(1, 0x07, write_buffer, 6);
                if(ret < 0)
                {
                    APP_DEBUG("\r\n<--Failed !! IIC controller Ql_IIC_Read channel 1 fail ret=%d-->\r\n",ret);
                    break;
                }
                APP_DEBUG("\r\n<--IIC controller Ql_IIC_Read ret=%d-->\r\n",ret);
                break;
            }

            //command-->IIC write then read  
            Ql_memset(m_buffer, 0x0, sizeof(m_buffer));
            Ql_sprintf(m_buffer, "Ql_IIC_Write_Read=0\r\n");//  channel 0 
            ret = Ql_strncmp(m_Read_Buffer, m_buffer, Ql_strlen(m_buffer));                
            if(0 == ret)
            {
                ret = Ql_IIC_Write_Read(0, 0x07, registerAdrr, 2,read_buffer, 6);
                if(ret < 0)
                {
                    APP_DEBUG("\r\n<--Failed !! Ql_IIC_Write_Read channel 0 fail ret=%d-->\r\n",ret);
                    break;               
                }
                APP_DEBUG("\r\n<--channel 0 Ql_IIC_Write_Read ret=%d-->\r\n",ret);
                break;
            }

            Ql_memset(m_buffer, 0x0, sizeof(m_buffer));
            Ql_sprintf(m_buffer, "Ql_IIC_Write_Read=1\r\n");//  channel 1 
            ret = Ql_strncmp(m_Read_Buffer, m_buffer, Ql_strlen(m_buffer));                
            if(0 == ret)
            {
                ret = Ql_IIC_Write_Read(1, 0x07, registerAdrr, 2,read_buffer, 6);
                if(ret < 0)
                {
                    APP_DEBUG("\r\n<--Failed !! IIC controller Ql_IIC_Write_Read channel 1 fail ret=%d-->\r\n",ret);
                    break;
                }
                APP_DEBUG("\r\n<--IIC controller Ql_IIC_Write_Read ret=%d-->\r\n",ret);
                break;
            }

                
            
            //command-->IIC write then read  
            Ql_memset(m_buffer, 0x0, sizeof(m_buffer));
            Ql_sprintf(m_buffer, "Ql_IIC_Uninit=0\r\n");//  channel 0 
            ret = Ql_strncmp(m_Read_Buffer, m_buffer, Ql_strlen(m_buffer));                
            if(0 == ret)
            {
                ret = Ql_IIC_Uninit(0);
                if(ret < 0)
                {
                    APP_DEBUG("\r\n<--Failed !! Ql_IIC_Uninit channel 0 fail ret=%d-->\r\n",ret);
                    break;               
                }
                APP_DEBUG("\r\n<--channel 0 Ql_IIC_Uninit ret=%d-->\r\n",ret);
                break;
            }

            Ql_memset(m_buffer, 0x0, sizeof(m_buffer));
            Ql_sprintf(m_buffer, "Ql_IIC_Uninit=1\r\n");//  channel 1 
            ret = Ql_strncmp(m_Read_Buffer, m_buffer, Ql_strlen(m_buffer));                
            if(0 == ret)
            {
                ret = Ql_IIC_Uninit(1);
                if(ret < 0)
                {
                    APP_DEBUG("\r\n<--Failed !! IIC controller Ql_IIC_Uninit channel 1 fail ret=%d-->\r\n",ret);
                    break;
                }
                 APP_DEBUG("\r\n<--IIC controller (chnnlNo 1) Ql_IIC_Uninit  ret=%d-->\r\n",ret);
                break;
            }
            
        APP_DEBUG("\r\n<--Not found this command, please check you command-->\r\n");
        }
    default:
        break;
    }
}



#endif // __EXAMPLE_I2C__

