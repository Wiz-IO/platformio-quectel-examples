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
 *   example_gps_iic.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example demonstrates how to use iic function with APIs in OpenCPU.
 *   Input the specified command through uart port1 and the result will be 
 *   output through the debug port.
 *   
 *
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_GPS_IIC__" in gcc_makefile file. And compile the 
 *     app using "make clean/new".
 *     Download image bin to module to run.
 * 
 *   Operation:
 *            
 *     If input "START IIC=1\r\n", that will start IIC controller to read.
 *     If input "START IIC=0\r\n", that will start simulated IIC to read.
 *     If input "STOP IIC\r\n", that will stop to read IIC.
 *     If input others, that will write to slave equipment through IIC interface. eg:"$PQVERNO,R*3F\r\n"
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
#ifdef __EXAMPLE_GPS_IIC__
#include "ql_type.h"
#include "ql_stdlib.h"
#include "ql_trace.h"
#include "ql_gpio.h"
#include "ql_uart.h"
#include "ql_iic.h"
#include "ql_error.h"
#include "ql_timer.h"
#include "nema_pro.h"

#define  SERIAL_RX_BUFFER_LEN  255
static u8 m_RxBuf_UartPort[SERIAL_RX_BUFFER_LEN];
#define CMD_PORT  UART_PORT1
#define OUT_PORT  UART_PORT2
#define HARDWARE_IIC 1      //hardware iic
#define SIMULATED_IIC 0    //simulated iic

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

void IIC_Config(u8 IIC_type);
static void Callback_Timer(u32 timerId, void* param);
static void Timer_Init(void);
static s32 ReadSerialPort(Enum_SerialPort port, /*[out]*/u8* pBuffer, /*[in]*/u32 bufLen);
static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);

/******************************************************************************
 * Function Name:
 *   IIC_Config
 *
 * Description:
 *   initialize IIC and config IIC
 *
 * Parameter:
 *   IIC_type:  HARDWARE_IIC  (hardware iic)
 *                  SIMULATED_IIC  (simulated iic)
 * Return:
 *
 * NOTE:
 *   
 ******************************************************************************/
void IIC_Config(u8 IIC_type)
{
    s32 ret;
    if (IIC_type == HARDWARE_IIC)  //hardware iic
    {
        ret = Ql_IIC_Init(1,PINNAME_RI,PINNAME_DCD,1); //hardware iic test, we choose PINNAME_RI, PINNAME_DCD for IIC SCL and SDA pins
        if(ret < 0)
        {
            APP_DEBUG("\r\n<--Failed!! Ql_IIC_Init channel 1 fail ret=%d-->\r\n",ret);
        }
        ret = Ql_IIC_Config(1,TRUE, IIC_DEV_ADDR, 300);// just for the IIC controller
        if(ret < 0)
        {
            APP_DEBUG("\r\n<--Failed !! hardware iic Ql_IIC_Config channel 1 fail ret=%d-->\r\n",ret);
        }
    }
    else  // simulated iic
    {
        ret = Ql_IIC_Init(1,PINNAME_CTS,PINNAME_RTS,0); // simulated iic test, and we choose PINNAME_CTS, PINNAME_RTS for IIC SCL and SDA pins
        if(ret < 0)
        {
            APP_DEBUG("\r\n<--Failed!! Ql_IIC_Init simulation channel 1 fail ret=%d-->\r\n",ret);
        }
        ret = Ql_IIC_Config(1,TRUE, IIC_DEV_ADDR, 0); // simulated IIC interface ,do not care the IIC Speed.
        if(ret < 0)
        {
            APP_DEBUG("\r\n<--Failed !! simulated iic Ql_IIC_Config  channel 1 fail ret=%d-->\r\n",ret);
        }
    }
}

//Callback function
static void Callback_Timer(u32 timerId, void* param)
{
    if (TIMERID1 ==timerId)
    {
        Ql_OS_SendMessage(main_task_id, MSG_ID_IIC_READ_INDICATION, 0, 0);
    }
}

static void Timer_Init(void)
{
    s32 ret;
    u32 param1=0;
    ret=Ql_Timer_Register(TIMERID1, Callback_Timer, &param1);
    if (ret < QL_RET_OK)
    {
        APP_DEBUG("<--Fail to register timer: ret=%d -->\r\n", ret);
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
        APP_DEBUG("Fail to read from port[%d]\r\n", port);
        return -99;
    }
    return rdTotalLen;
}

void proc_main_task(s32 taskId)
{
    ST_MSG msg;
    st_queue l_queue;
    u8 nmea_buf[MAX_NMEA_LEN+3];
    s32 realwrlen = 0;
    // Register & open UART port
    Ql_UART_Register(CMD_PORT, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(CMD_PORT, 115200, FC_NONE);
    Ql_UART_Register(OUT_PORT, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(OUT_PORT, 115200, FC_NONE);
    iop_init_pcrx();
    Timer_Init();

    while (1)
    {
        Ql_OS_GetMessage(&msg);
        switch(msg.message)
        {
            case MSG_ID_OUTPUT_INDICATION:
                Ql_memset(&l_queue,0,sizeof(l_queue));
    			if(iop_inst_avail(&l_queue.inst_id, &l_queue.dat_idx, &l_queue.dat_siz))
    			{
    				Ql_memset(nmea_buf, 0, sizeof(nmea_buf));
    				iop_get_inst(l_queue.dat_idx, l_queue.dat_siz, nmea_buf);
    				Ql_memcpy(nmea_buf + l_queue.dat_siz, "\r\n",sizeof("\r\n"));
                    Ql_memcpy(&tx_buf[tx_data_len], nmea_buf, Ql_strlen(nmea_buf));
                    if ( tx_data_len + Ql_strlen(nmea_buf) > NMEA_TX_MAX )
                    {
                        APP_DEBUG("\r\n<--Array is full  tx_data_len=%d-->\r\n", tx_data_len);
                    }
                    tx_data_len = (tx_data_len + Ql_strlen(nmea_buf)) % NMEA_TX_MAX;
                    realwrlen =Ql_UART_Write(OUT_PORT, tx_buf, tx_data_len);
                    if(realwrlen > 0)
                    {
                        tx_data_len -= realwrlen;
                        Ql_memmove(tx_buf, &tx_buf[realwrlen], tx_data_len);
                    }
    			}
                break;
            case MSG_ID_IIC_READ_INDICATION:
                get_nmea();
                break;
            default:
                break;
        }
    }
}


static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{ 
    s32 ret;    
    s32 totalBytes;
    s32 realwrlen;
    switch (msg)    
    {     
        case EVENT_UART_READY_TO_READ: 
            if (CMD_PORT == port)
            {
                totalBytes = ReadSerialPort(port, m_RxBuf_UartPort, sizeof(m_RxBuf_UartPort));
                if (totalBytes > 0)
                {
                    if (Ql_strcmp(m_RxBuf_UartPort, "START IIC=1\r\n") == 0)
                    {
                        IIC_Config(HARDWARE_IIC);
                        ret=Ql_Timer_Start(TIMERID1, INTERVAL500MS, 1);
                        if (ret < QL_RET_OK)
                        {
                            APP_DEBUG("\r\n<--Fail to start 500ms timer: timerId=%d,ret=%d -->\r\n", TIMERID1 ,ret);
                        }
                    }
                    else if (Ql_strcmp(m_RxBuf_UartPort, "START IIC=0\r\n") == 0)
                    {
                        IIC_Config(SIMULATED_IIC);
                        ret=Ql_Timer_Start(TIMERID1, INTERVAL500MS, 1);
                        if (ret < QL_RET_OK)
                        {
                            APP_DEBUG("\r\n<--Fail to start 500ms timer: timerId=%d,ret=%d -->\r\n", TIMERID1 ,ret);
                        }
                    }    

                    else if (Ql_strcmp(m_RxBuf_UartPort, "STOP IIC\r\n") == 0)
                    {
                        ret=Ql_IIC_Uninit(1);
                        if (ret < QL_RET_OK)
                        {
                            APP_DEBUG("\r\n<--Fail!! Ql_IIC_Uninit ret=%d -->\r\n",ret);
                        }
                        Ql_Timer_Stop(TIMERID1);
                    }
                    else
                    {
                        ret=IIC_WriteBytyes(1,IIC_DEV_ADDR,m_RxBuf_UartPort,sizeof(m_RxBuf_UartPort));
                        if (ret != sizeof(m_RxBuf_UartPort))
                        {
                            APP_DEBUG("Fail to writeBytyes  ret=%d\r\n ",ret);
                        }
                    }
                }
            }
            break; 
        case EVENT_UART_READY_TO_WRITE:
            if ( OUT_PORT == port)
            {
                realwrlen =Ql_UART_Write(OUT_PORT, tx_buf, tx_data_len);
        
                if(realwrlen > 0)
                {
                    tx_data_len -= realwrlen;
                    Ql_memmove(tx_buf, &tx_buf[realwrlen], tx_data_len);
                }
                else 
                {
                    APP_DEBUG("Fail to write to UART1  realwrlen=%d\r\n ",realwrlen);
                }
            }

            break;
        default:
            break;
    }
}

#endif // __EXAMPLE_I2C__


