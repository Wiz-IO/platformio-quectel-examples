#include "nema_pro.h"
#include "ql_type.h"
#include "ql_stdlib.h"
#include "ql_trace.h"
#include "ql_gpio.h"
#include "ql_uart.h"
#include "ql_iic.h"
#include "ql_error.h"
#include "ql_timer.h"

#define IOP_LF_DATA 	0x0A // <LF>
#define IOP_CR_DATA 	0x0D // <CR> 
#define IOP_START_NMEA 	0x24// NMEA start 
#define NMEA_ID_QUE_SIZE 0x0100
#define MAX_NMEA_STN_LEN 0x0100
#define NMEA_RX_QUE_SIZE 0x8000



st_queue id_que[NMEA_ID_QUE_SIZE];
u8 tx_buf[NMEA_TX_MAX]={0};
s32 tx_data_len = 0;
s32 tx_size = 0;
s8 rx_que[NMEA_RX_QUE_SIZE]; 
u16 id_que_head; 
u16 id_que_tail; 
u16 rx_que_head; 
RX_SYNC_STATE_T rx_state; 
u32 u4SyncPkt; 
u32 u4OverflowPkt; 
u32 u4PktInQueue; 
u8 rd_buf[MAX_I2C_BUF_SIZE+1];

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

void get_nmea(void)
{
    s32 ret;
	if(read_gps_I2C_buffer() == TRUE)
	{
		extract_nmea();
	}
	else //modify initial value and restart timer.
	{
        APP_DEBUG("\r\n Please comfirm your IIC device is OK ?\r\n");
        ret=Ql_Timer_Stop(TIMERID1);
        if (ret < QL_RET_OK)
        {
            APP_DEBUG("\r\n<--Fail to stop timer: timerId=%d,ret=%d -->\r\n", TIMERID1 ,ret);
        }
        ret=Ql_Timer_Start(TIMERID1, INTERVAL500MS, 1);
        if (ret < QL_RET_OK)
        {
            APP_DEBUG("\r\n<--Fail to start 500ms timer: timerId=%d,ret=%d -->\r\n", TIMERID1 ,ret);
        }
	}
}

/******************************************************************************
 * Function Name:
 *   IIC_ReadBytes
 *
 * Description:
 *   read data from specified slave through IIC interface
 *
 * Parameter:
 *   chnnlNo:  IIC channel No, the No is specified by Ql_IIC_Init function
 *   slaveAddr:  slave address       
 *   buf:  Read buffer of reading the specified register from slave  
 *   len:  Number of bytes to read
 * Return:If no error, return the length of the read data
 *
 * NOTE:
 *   
 ******************************************************************************/
s32 IIC_ReadBytes(u32 chnnlNo,u8 slaveAddr,u8 *buf,u32 len)
{
    s32 i;
    s32 ret;
    u8 pBuffer[9]={0};
    s32 length;
    s32 remainder = 0;
    u8 registerAdrr = 0x00;
    length = len/8;
    remainder = len%8;
    for (i = 0; i < length; i++)
    {
        ret=Ql_IIC_Write_Read(chnnlNo, slaveAddr, &registerAdrr, 1 , pBuffer, 8);
        if(ret != 8)
        {
            APP_DEBUG("\r\n<--Failed to  Ql_IIC_Write_Read channel %d fail ret=%d-->\r\n",chnnlNo, ret); 
            break;
        } 
        else
        {
		   Ql_memcpy(buf, pBuffer, 8);
           buf += 8;
           registerAdrr += 8;
        }
        
    }
    if (remainder != 0)
    {
        ret=Ql_IIC_Write_Read(chnnlNo, slaveAddr, &registerAdrr, 1 , pBuffer, remainder);
        if(ret != remainder)
        {
            APP_DEBUG("\r\n<--Failed to read remainder channel %d fail ret=%d-->\r\n",chnnlNo, ret); 
            remainder=0;
        } 
        else
        {
		   Ql_memcpy(buf, pBuffer, remainder);
        }
    }
    return (i * 8 + remainder);
    
}
/******************************************************************************
 * Function Name:
 *   IIC_WriteBytyes
 *
 * Description:
 *   read data from specified slave through IIC interface
 *
 * Parameter:
 *   chnnlNo:  IIC channel No, the No is specified by Ql_IIC_Init function
 *   slaveAddr:  slave address       
 *   pdata:  Setting value to slave  
 *   len:  Number of bytes to write
 * Return:If no error, return the length of the write data
 *
 * NOTE:
 *   
 ******************************************************************************/
s32 IIC_WriteBytyes(u32 chnnlNo,u8 slaveAddr,u8 *pdata,u32 len)
{
    s32 i;
    s32 ret;
    for (i = 0; i < len; i++)
	{
        ret = Ql_IIC_Write(chnnlNo, slaveAddr, &pdata[i], 1);
        if(ret <0)
        {
            APP_DEBUG("\r\n<--Failed !! Ql_IIC_Write channel %d fail ret=%d-->\r\n",chnnlNo, ret); 
            break;
        } 
    }
    return i;
}
bool read_gps_I2C_buffer(void)
{
    s32 ret;
    s32 realwrlen = 0;
	Ql_memset(rd_buf, 0, sizeof(rd_buf)); 
    ret = IIC_ReadBytes(1, IIC_DEV_ADDR, rd_buf, MAX_I2C_BUF_SIZE); 
	if(ret == MAX_I2C_BUF_SIZE)
	{
		rd_buf[MAX_I2C_BUF_SIZE] = '\0';
		return TRUE;
	}
	else
	{
        APP_DEBUG("\r\n<--Failed to readBytes  ret=%d-->\r\n", ret); 
		return FALSE;
	}
}

void extract_nmea(void)
{
	u32 i = 0;
    s32 ret;
    s32 realwrlen = 0;
	u8 nmea_buf[MAX_NMEA_LEN+1];
	st_queue l_queue;
	Ql_memset(&l_queue,0,sizeof(l_queue));
	for(i=0; i<MAX_I2C_BUF_SIZE; i++)
	{
		iop_pcrx_nmea(rd_buf[i]);
	}
	if(rx_state != RXS_ETX)
	{
        ret=Ql_Timer_Stop(TIMERID1);
        if (ret < QL_RET_OK)
        {
            APP_DEBUG("\r\n<--Fail to stop timer: timerId=%d,ret=%d -->\r\n", TIMERID1 ,ret);
        }
        ret=Ql_Timer_Start(TIMERID1, INTERVAL5MS, 1);
        if (ret < QL_RET_OK)
        {
            APP_DEBUG("\r\n<--Fail to start 5ms timer: timerId=%d,ret=%d -->\r\n", TIMERID1 ,ret);
        }
	}
    else
    {
        ret=Ql_Timer_Stop(TIMERID1);
        if (ret < QL_RET_OK)
        {
            APP_DEBUG("\r\n<--Fail to stop timer: timerId=%d,ret=%d -->\r\n", TIMERID1 ,ret);
        }
        ret=Ql_Timer_Start(TIMERID1, INTERVAL500MS, 1);
        if (ret < QL_RET_OK)
        {
            APP_DEBUG("\r\n<--Fail to start 500ms timer: timerId=%d,ret=%d -->\r\n", TIMERID1 ,ret);
        }
    }
}

//Queue Functions 
bool iop_init_pcrx( void ) 
{ 
    /*---------------------------------------------------------- 
    variables 
    ----------------------------------------------------------*/ 
    s16 i; 
    /*---------------------------------------------------------- 
    initialize queue indexes 
    ----------------------------------------------------------*/ 
    id_que_head = 0; 
    id_que_tail = 0; 
    rx_que_head = 0;
    /*---------------------------------------------------------- 
    initialize identification queue 
    ----------------------------------------------------------*/ 
    for( i=0; i< NMEA_ID_QUE_SIZE; i++) 
    { 
        id_que[i].inst_id = -1; 
        id_que[i].dat_idx = 0; 
    } 
    /*---------------------------------------------------------- 
    initialize receive state 
    ----------------------------------------------------------*/ 
    rx_state = RXS_ETX; 
    /*---------------------------------------------------------- 
    initialize statistic information 
    ----------------------------------------------------------*/ 
    u4SyncPkt = 0; 
    u4OverflowPkt = 0; 
    u4PktInQueue = 0; 
    return TRUE; 
} 
/********************************************************************* 
* PROCEDURE NAME: 
* iop_inst_avail - Get available NMEA sentence information 
* 
* DESCRIPTION: 
* inst_id - NMEA sentence type 
* dat_idx - start data index in queue 
* dat_siz - NMEA sentence size 
*********************************************************************/ 
bool iop_inst_avail(s16 *inst_id, s16 *dat_idx, s16 *dat_siz) 
{ 
	//s8 buf[256];
    /*---------------------------------------------------------- 
    variables 
    ----------------------------------------------------------*/ 
    bool inst_avail =FALSE; 
    /*---------------------------------------------------------- 
    if packet is available then return id and index 
    ----------------------------------------------------------*/ 
    if ( id_que_tail != id_que_head ) 
    {
        *inst_id = id_que[ id_que_tail ].inst_id; 
        *dat_idx = id_que[ id_que_tail ].dat_idx; 
        *dat_siz = id_que[ id_que_tail ].dat_siz; 
        id_que[ id_que_tail ].inst_id = -1; 
        id_que_tail = ++id_que_tail & (u16)(NMEA_ID_QUE_SIZE - 1);
        inst_avail = TRUE; 
        if (u4PktInQueue > 0) 
        { 
            u4PktInQueue--; 
        } 
    } 
    else 
    { 
        inst_avail = FALSE; 
    } 
    return ( inst_avail ); 
} /* iop_inst_avail() end */ 

/********************************************************************* 
* PROCEDURE NAME: 
* iop_get_inst - Get available NMEA sentence from queue 
* 
* DESCRIPTION: 
* idx - start data index in queue 
* size - NMEA sentence size 
* data - data buffer used to save NMEA sentence 
*********************************************************************/ 
void iop_get_inst(s16 idx, s16 size, void *data) 
{ 
    /*---------------------------------------------------------- 
    variables 
    ----------------------------------------------------------*/ 
    s16 i; 
    u8 *ptr; 
    /*---------------------------------------------------------- 
    copy data from the receive queue to the data buffer 
    ----------------------------------------------------------*/ 
    ptr = (u8 *)data; 
    for (i = 0; i < size; i++) 
    { 
        *ptr = rx_que[idx];
        ptr++; 
        idx = ++idx & (u16)(NMEA_RX_QUE_SIZE - 1);
    } 
} /* iop_get_inst() end */ 
/********************************************************************* 
* PROCEDURE NAME: 
* iop_pcrx_nmea - Receive NMEA code 
* 
* DESCRIPTION: 
* The procedure fetch the s8acters between/includes '$' and <CR>. 
* That is, s8acter <CR><LF> is skipped. 
* and the maximum size of the sentence fetched by this procedure is 256 
* $xxxxxx*AA 
* 
*********************************************************************/ 

void iop_pcrx_nmea( u8 data ) 
{ 
    /*---------------------------------------------------------- 
    determine the receive state 
    ----------------------------------------------------------*/ 
    if (data == IOP_LF_DATA )
	{ 
		return; 
    } 
    switch (rx_state) 
    { 
        case RXS_DAT: 
        switch (data) 
        { 
            case IOP_CR_DATA: 
            // count total number of sync packets 
            u4SyncPkt += 1; 
            id_que_head = ++id_que_head & (u16)(NMEA_ID_QUE_SIZE - 1); 
            if (id_que_tail == id_que_head) 
            { 
                // count total number of overflow packets 
                u4OverflowPkt += 1; 
                id_que_tail = ++id_que_tail & (u16)(NMEA_ID_QUE_SIZE - 1); 
            } 
            else 
            { 
                u4PktInQueue++; 
            }
            rx_state = RXS_ETX; 
            /*---------------------------------------------------------- 
            set RxEvent signaled 
            ----------------------------------------------------------*/ 
            Ql_OS_SendMessage(main_task_id, MSG_ID_OUTPUT_INDICATION, 0, 0);
			
            break; 
            case IOP_START_NMEA: 
            { 
                // Restart NMEA sentence collection 
                rx_state = RXS_DAT; 
                id_que[id_que_head].inst_id = 1; 
                id_que[id_que_head].dat_idx = rx_que_head; 
                id_que[id_que_head].dat_siz = 0; 
                rx_que[rx_que_head] = data; 
                rx_que_head = ++rx_que_head & (u16)(NMEA_RX_QUE_SIZE - 1); 
                id_que[id_que_head].dat_siz++; 
            break; 
            } 
            default: 
                rx_que[rx_que_head] = data; 
                rx_que_head = ++rx_que_head & (u16)(NMEA_RX_QUE_SIZE - 1); 
                id_que[id_que_head].dat_siz++; 
                // if NMEA sentence length > 256, stop NMEA sentence collection 
                if (id_que[id_que_head].dat_siz == MAX_NMEA_STN_LEN) 
                { 
					id_que[id_que_head].inst_id = -1; 
					rx_state = RXS_ETX; 
                } 
            break; 
        }

        break; 
        case RXS_ETX: 
            if (data == IOP_START_NMEA) 
            { 
            rx_state = RXS_DAT; 
            id_que[id_que_head].inst_id = 1; 
            id_que[id_que_head].dat_idx = rx_que_head; 
            id_que[id_que_head].dat_siz = 0; 
            rx_que[rx_que_head] = data; 
            rx_que_head = ++rx_que_head & (u16)(NMEA_RX_QUE_SIZE - 1);
            id_que[id_que_head].dat_siz++; 
            } 
        break; 
        default: 
            rx_state = RXS_ETX; 
        break; 
    } 
} /* iop_pcrx_nmea() end */

