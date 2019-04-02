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
 *   example_spi.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example demonstrates how to use SPI function with APIs in OpenCPU.
 *   Input the specified command through any uart port and the result will be 
 *   output through the debug port.
 *
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_SPI__" in gcc_makefile file. And compile the 
 *     app using "make clean/new".
 *     Download image bin to module to run.
 * 
 *   Operation:
 *            
 *     If input "Ql_SPI_Init", that will init the SPI channel.
 *     If input "Ql_SPI_Config", that will configure the SPI parameter.
 *     If input "Ql_SPI_Write", that will write bytes to slave equipment through SPI interface.
 *     If input "Ql_SPI_Read", that will read bytes from slave equipment through SPI interface.
 *     If input "Ql_SPI_WriteRead", that will read and write bytes through SPI interface.
 *     If input "Ql_SPI_Uninit", that will release the SPI pins.
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
#ifdef __EXAMPLE_SPI__

#include "ql_type.h"
#include "ql_trace.h"
#include "ql_stdlib.h"
#include "ql_gpio.h"
#include "ql_spi.h"
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



#define USR_SPI_CHANNAL		     (1)


/***************************************************************************
* This example is used for test SPI funtion by hardware.
* And the peripheral SPI flash type is MX25L1606E
****************************************************************************/

u8 Read_Buffer[1024];
u8 buffer[1024];

/****************************************************************************
*spi_usr_type = 0 is analog spi, spi_usr_type = 1 is hardware spi.
****************************************************************************/
u8 spi_usr_type = 1;

/***************************************************************************
* Function prototypes
*
****************************************************************************/
static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);
void time_delay(u32 cnt);



/***************************************************************************
* Function: proc_main_task
* Description: A temp delay function.
****************************************************************************/
void time_delay(u32 cnt)
{
    u32 i = 0;
    u32 j = 0; 
    for(i=0;i<cnt;i++)
        for(j=0;j<1000000;j++);  
}

//spi device select
void spi_flash_cs(bool CS)
{
	if (!spi_usr_type)
	{
		if (CS)
			Ql_GPIO_SetLevel(PINNAME_PCM_CLK,PINLEVEL_HIGH);
		else
			Ql_GPIO_SetLevel(PINNAME_PCM_CLK,PINLEVEL_LOW);
	}
}



/***************************************************************************
* Function: proc_main_task
* Description: entry
****************************************************************************/ 
void proc_main_task(void)
{
    s32 ret;
    ST_MSG msg;
    bool keepGoing = TRUE;



    // Register & open UART port
    Ql_UART_Register(UART_PORT1, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(UART_PORT1, 115200, FC_NONE);

    Ql_UART_Register(UART_PORT2, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(UART_PORT2, 115200, FC_NONE);

    APP_DEBUG("\r\n<-- OpenCPU: SPI TEST! -->\r\n") 
                //chnnlNo, //pinClk,   //pinMiso,       //pinMosi,      //pinCs,       //spiType
    ret = Ql_SPI_Init(USR_SPI_CHANNAL,PINNAME_PCM_IN,PINNAME_PCM_SYNC,PINNAME_PCM_OUT,PINNAME_PCM_CLK,spi_usr_type);
    
    if(ret <0)
    {
        APP_DEBUG("\r\n<-- Failed!! Ql_SPI_Init fail , ret =%d-->\r\n",ret)
    }
    else
    {
        APP_DEBUG("\r\n<-- Ql_SPI_Init ret =%d -->\r\n",ret)  
    }
    ret = Ql_SPI_Config(USR_SPI_CHANNAL,1,0,0,10000); //config sclk about 10MHz;
    if(ret <0)
    {
        APP_DEBUG("\r\n<--Failed!! Ql_SPI_Config fail  ret=%d -->\r\n",ret)
    }
    else
    {
        APP_DEBUG("\r\n<-- Ql_SPI_Config  =%d -->\r\n",ret)
    }   

	//init cs pin
	if (!spi_usr_type)
	{
		Ql_GPIO_Init(PINNAME_PCM_CLK,PINDIRECTION_OUT,PINLEVEL_HIGH,PINPULLSEL_PULLUP);   //CS high
	}

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

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
s8 status_reg = 0;
/***************************************************************************
* Function: cmd_is_over
* Description: query the operation has been completed or incompleted.
****************************************************************************/ 
s8 cmd_is_over(void)
{
    u32 i=0;
    s8 ret = TRUE;
    u32 cnt1 = 100000;
    u32 cnt2 = 0;
    flash_rd_status_reg();

    while(status_reg & 0x01)
    {
        time_delay(1000000);
        flash_rd_status_reg();
    }
    if (status_reg & 0x01)
    {
        ret = FALSE;
    }
   // APP_DEBUG("status_reg = %d .\r\n",status_reg) 
    return ret;
}

//@flash write enable
void flash_wr_enable()
{
    s32 ret = 0;               
    u8  cmd = 0x06; 
	spi_flash_cs(0);
    ret = Ql_SPI_Write(USR_SPI_CHANNAL, &cmd, 1);
	spi_flash_cs(1);
    if(ret==1)
    {
        APP_DEBUG("write enable.\r\n")
    }  
    else 
    {
        APP_DEBUG("func(%s),line(%d),here has a failed operation.\r\n",__func__,__LINE__)  
    }       
}

//@read the flash id
//manufacturer ID | memory type | memory density
//      C2        |      20     |     15
void flash_rd_id(void)
{
    s32 ret = 0;
    u8 wr_buff[32]={0x9f};
    u8 rd_buff[32]={0xff};

	spi_flash_cs(0);
    ret = Ql_SPI_WriteRead(USR_SPI_CHANNAL,wr_buff,1, rd_buff,3);
	spi_flash_cs(1);
    if(ret==3)
    {
        APP_DEBUG("%d\r\n",rd_buff[0])  
        APP_DEBUG("%d\r\n",rd_buff[1])  
        APP_DEBUG("%d\r\n",rd_buff[2])  
    }    
    else 
    {
        APP_DEBUG("func(%s),line(%d),here has a failed operation.\r\n",__func__,__LINE__)   
    }
}

//@REMS  manufacturer ID device ID
void flash_rd_id2(void)
{
    s32 ret = 0;
    u8 wr_buff[32]={0x90,0xFF,0xFF,0x00};
    u8 rd_buff[32]={0xff};
    spi_flash_cs(0);                    
    ret = Ql_SPI_WriteRead(USR_SPI_CHANNAL,wr_buff,4, rd_buff,2);
	spi_flash_cs(1);
    if(ret==2)
    {
        APP_DEBUG("%d\r\n",rd_buff[0])
        APP_DEBUG("%d\r\n",rd_buff[1])                        
    }
    else 
    {
        APP_DEBUG("func(%s),line(%d),here has a failed operation.\r\n",__func__,__LINE__)  
    }
}

//@read status register value;
void flash_rd_status_reg(void)
{
    s32 ret = 0;               
    u8 wr_buff[32]={0x05};
    spi_flash_cs(0);
    ret = Ql_SPI_WriteRead(USR_SPI_CHANNAL,wr_buff,1, &status_reg,1);
	spi_flash_cs(1);
    if(ret==1)
    {
        APP_DEBUG("%d\r\n",status_reg)
    }  
    else 
    {
        APP_DEBUG("func(%s),line(%d),here has a failed operation.\r\n",__func__,__LINE__) 
    }    
}

//@flash chip erase()
void flash_erase_chip()
{   
    s32 ret = 0;
    u8 cmd = 0x60;
    u32 j=1000000,i=0;
    flash_wr_enable();
	spi_flash_cs(0);
    ret = Ql_SPI_Write(USR_SPI_CHANNAL,&cmd,1);
	spi_flash_cs(1);
    //Because erase the whole chip must need more time to wait,
    time_delay(10000000);
    if(ret==1)
    {
        //Because erase the whole chip must need more time to wait ,so we must check the bit 1 of status register .
        APP_DEBUG("chip erase done.\r\n")    
    }
    else 
    {
        APP_DEBUG("func(%s),line(%d),here has a failed operation.\r\n",__func__,__LINE__) 
    }    
}

//@flash block erase(0-31)
void flash_erase_block(s8 nblock)
{
    u8 cmd = 0x52;
    s32 ret = 0;   
    u32 addr = nblock * 0x10000;
    u8 wr_buff[4]={cmd,(addr>>16)&0xff,(addr>>8)&0xff,addr & 0xff};
    if (nblock > 31)
    {
        APP_DEBUG("nblock is error para.\r\n")  
        return ;
    }    
    flash_wr_enable();
	spi_flash_cs(0);
    ret = Ql_SPI_Write(USR_SPI_CHANNAL,wr_buff,4);
	spi_flash_cs(1);

    if(ret==4)
    {   
        APP_DEBUG("chip erase ok.\r\n")   
    }
    else 
    {
        APP_DEBUG("func(%s),line(%d),here has a failed operation.\r\n",__func__,__LINE__) 
    }  
}

//@flash sector erase(0-511)
void flash_erase_sector(s16 nsector)
{
    u8 cmd = 0x20;
    s32 ret = 0;   
    u32 addr = nsector * 0x1000;
    u8 wr_buff[4]={cmd,(addr>>16)&0xff,(addr>>8)&0xff,addr & 0xff};

    if (nsector > 511)
    {
        APP_DEBUG("nsector is error para.\r\n")
        return ;
    }
    flash_wr_enable();
	spi_flash_cs(0);
    ret = Ql_SPI_Write(USR_SPI_CHANNAL,wr_buff,4);
	spi_flash_cs(1);
    if(ret==4)
    {
        APP_DEBUG("sector erase ok.\r\n")  
    }
    else 
    {
        APP_DEBUG("func(%s),line(%d),here has a failed operation.\r\n",__func__,__LINE__) 
    }
}

//@flash write data (page program)
void flash_wr_data(u32 addr, u8 *pbuff,u32 len)
{
    u8 *p_buff = pbuff;
    s32 ret = 0;  
    s16 i=0;
    u8 cmd = 0x02;
    u8 wr_buff[2048]={cmd,(addr>>16)&0xff, (addr>>8)&0xff, addr&0xff};
    
    if (len > 1024)
    {
        APP_DEBUG("length is too long.\r\n")  
        return ;
    }
    for(i=0; i<len; i++)
    {
       wr_buff[i+4] =  *p_buff;
       p_buff++;
    }
    
    flash_wr_enable();
	spi_flash_cs(0);
    ret = Ql_SPI_Write(USR_SPI_CHANNAL,wr_buff,len+4);
	spi_flash_cs(1);
    if(ret==(len+4))
    {
        APP_DEBUG("write data ok.\r\n")    
    }
    else 
    {
        APP_DEBUG("func(%s),line(%d),here has a failed operation.\r\n",__func__,__LINE__) 
    }       
}

//@flash read data
void flash_rd_data(u32 addr, u8 *pbuff, u32 len)
{
    s32 ret = 0;   
    u8 cmd = 0x03;
    u8 wr_buff[4]={cmd,(addr>>16)&0xff,(addr>>8)&0xff,addr & 0xff};

    if (len > 1024)
    {
        APP_DEBUG("length is too long.\r\n")
        return ;
    }
	spi_flash_cs(0);
    ret = Ql_SPI_WriteRead(USR_SPI_CHANNAL,wr_buff,4, pbuff,len);
	spi_flash_cs(1);
    if(ret==len)
    {
        u16 i=0;
        for(i=0;i<len;i++)
        {
            APP_DEBUG("%d ",pbuff[i])                     
        }
        APP_DEBUG("\r\n")
    }
    else 
    {
        APP_DEBUG("func(%s),line(%d),here has a failed operation.\r\n",__func__,__LINE__)  
    }
}
//MX25L1606E FLASH operation
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@


static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{
    //APP_DEBUG("CallBack_UART_Hdlr: port=%d, event=%d, level=%d, p=%x\r\n", port, msg, level, customizedPara)
    u32 rdLen=0;
    u32 wdLen = 0;
    s32 ret;
    u8  spi_write_buffer[]={0x01,0x02,0x03,0x04,0x11,0x22};
    u8  spi_read_buffer[100];
    char *p=NULL;
    switch (msg)
    {
        case EVENT_UART_READY_TO_READ:
        {
            if (UART_PORT1 == port || UART_PORT2 == port )
            {        
                Ql_memset(Read_Buffer, 0x0, sizeof(Read_Buffer));
                rdLen = Ql_UART_Read(port, Read_Buffer, sizeof(Read_Buffer));
                if (!rdLen)
                {
                    APP_DEBUG("\r\nread error.\r\n")
                    break;
                }
                APP_DEBUG("%s\r\n",Read_Buffer)


                //BEGIN: MX25L1606E FLASH OPERATION
                //read the flash's ID
                Ql_memset(buffer, 0x0, sizeof(buffer));
                Ql_sprintf(buffer, "ql_spi_rd_flash_id");               
                ret = Ql_strncmp(Read_Buffer, buffer, Ql_strlen(buffer)); 
                if (!ret)
                {
                  
                    if (cmd_is_over())
                        flash_rd_id();
                    else
                        APP_DEBUG("error.\r\n")
                   
                    
                    break;
                }
                //read the flash's status register 
                Ql_memset(buffer, 0x0, sizeof(buffer));
                Ql_sprintf(buffer, "ql_spi_rd_flash_status");               
                ret = Ql_strncmp(Read_Buffer, buffer, Ql_strlen(buffer)); 
                if (!ret)
                {   
                    
                    flash_rd_status_reg();

                    break;
                }    

                //read the manufacturer ID + device ID
                Ql_memset(buffer, 0x0, sizeof(buffer));
                Ql_sprintf(buffer, "ql_spi_rd_flash_md");               
                ret = Ql_strncmp(Read_Buffer, buffer, Ql_strlen(buffer)); 
                if (!ret)
                {
                    if (cmd_is_over())
                        flash_rd_id2();
                    else
                        APP_DEBUG("error.\r\n") 
 
                    break;
                }    
                // read data from spi flash.
                Ql_memset(buffer, 0x0, sizeof(buffer));
                Ql_sprintf(buffer, "ql_spi_rd_flash_data");               
                ret = Ql_strncmp(Read_Buffer, buffer, Ql_strlen(buffer)); 
                if (!ret)
                {
                    u8 buff[255]={0};
                    if (cmd_is_over())
                        flash_rd_data(0,buff,255);
                    else
                        APP_DEBUG("error.\r\n")

                    break;
                } 
                //erase the specificed sector
                Ql_memset(buffer, 0x0, sizeof(buffer));
                Ql_sprintf(buffer, "ql_spi_erase_flash_sector");               
                ret = Ql_strncmp(Read_Buffer, buffer, Ql_strlen(buffer)); 
                if (!ret)
                {
                    if (cmd_is_over())
                        flash_erase_sector(0);
                    else
                        APP_DEBUG("error.\r\n") 
                    break;
                }
                //erase the specificed block 
                Ql_memset(buffer, 0x0, sizeof(buffer));
                Ql_sprintf(buffer, "ql_spi_erase_flash_block");               
                ret = Ql_strncmp(Read_Buffer, buffer, Ql_strlen(buffer)); 
                if (!ret)
                {
                    if (cmd_is_over())
                        flash_erase_block(0);
                    else
                        APP_DEBUG("error.\r\n")
                    break;
                }
                //erase the whose chip 
                Ql_memset(buffer, 0x0, sizeof(buffer));
                Ql_sprintf(buffer, "ql_spi_erase_flash_chip");               
                ret = Ql_strncmp(Read_Buffer, buffer, Ql_strlen(buffer)); 
                if (!ret)
                {
                    if (cmd_is_over())
                        flash_erase_chip();
                    else
                        APP_DEBUG("error.\r\n")
                    break;
                }
               //write data to spi flash.
                Ql_memset(buffer, 0x0, sizeof(buffer));
                Ql_sprintf(buffer, "ql_spi_wr_flash_data");               
                ret = Ql_strncmp(Read_Buffer, buffer, Ql_strlen(buffer)); 
                if (!ret)
                {   
                    u8 buff[255];
                    u16 i;
                    for(i=0;i<256;i++)
                        buff[i] = i;
                   // APP_DEBUG("s_data = %d,buff[0] = %d\r\n",s_data,buff[0])
                    if (cmd_is_over())
                    {

                        flash_wr_data(0,buff,255);
                        
                    }
                    else
                        APP_DEBUG("error.\r\n")
                   
                    break;
                }                
                //END: MX25L1606E FLASH OPERATION
 
                Ql_memset(buffer, 0x0, sizeof(buffer));
                Ql_sprintf(buffer, "Ql_SPI_Uninit");
                ret = Ql_strncmp(Read_Buffer, buffer, Ql_strlen(buffer));   
                if(0 == ret)
                {
                    ret = Ql_SPI_Uninit(1);
                    if( ret < 0)
                    {
                        APP_DEBUG("<\r\n<--Failed!! Ql_SPI_Uninit fail, ret=%d\r\n",ret)
                        break;
                    }
                    APP_DEBUG("\r\n<-- Ql_SPI_Uninit ret =%d -->\r\n",ret)
                    break;
                }
                
                APP_DEBUG("\r\n<--Not found this command, please check you command-->\r\n")
            }
            break;
        }
        default:
        break;
    }
}

#endif // __SPI_TEST__

