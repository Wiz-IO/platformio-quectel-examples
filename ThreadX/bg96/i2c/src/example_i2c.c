/******************************************************************************
*@file    example_i2c.c
*@brief   example of i2c
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#if defined(__EXAMPLE_I2C__)
#include "txm_module.h"
#include "qapi_diag.h"
#include "qapi_timer.h"
#include "qapi_uart.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "qapi_fs_types.h"
#include "qapi_fs.h"
#include "example_i2c.h"
#include "qapi_i2c_master.h"

///#include "qapi_quectel.h"

/**************************************************************************
*                                 DEFINE
***************************************************************************/

TX_BYTE_POOL *byte_pool_gpio;
#define GPIO_BYTE_POOL_SIZE		16*8*1024
UCHAR free_memory_Uart[GPIO_BYTE_POOL_SIZE];

/* uart rx tx buffer */
static char *rx_buff = NULL; /*!!! should keep this buffer as 4K Bytes */
static char *tx_buff = NULL;

/* uart config para*/
static QT_UART_CONF_PARA uart_conf;



/**************************************************************************
*                                 GLOBAL
***************************************************************************/



void    *client_handle = NULL;

static uint8   *rbuf= NULL;
static uint8	*buffer= NULL;


/*===========================================================================

                           Static & global Variable Declarations

===========================================================================*/
/*
@func
  quectel_dbg_uart_init
@brief
  Entry function for task. 
*/
/*=========================================================================*/
void quectel_dbg_uart_init(qapi_UART_Port_Id_e port_id)
{
	uart_conf.hdlr 	  = NULL;
	uart_conf.port_id = port_id;
	uart_conf.tx_buff = tx_buff;
	uart_conf.tx_len  = sizeof(tx_buff);
	uart_conf.rx_buff = rx_buff;
	uart_conf.rx_len  = sizeof(rx_buff);
	uart_conf.baudrate= 115200;

	/* uart init */
	uart_init(&uart_conf);

	/* start uart receive */
	uart_recv(&uart_conf);
}


// Transfer completed
int32 CB_Parameter;
void client_callback (const uint32 status, void *cb_para)  //  uint32
{
 
}

/******************************************** 
** <S>  - START bit
** <P>  - STOP  bit
** <Sr> - Repeat Start bit
** <A>  - Acknowledge bit
** <N>  - Not-Acknowledge bit
** <R>  - Read Transfer
** <W> - Write Transfer
********************************************/
qapi_Status_t i2c_read (uint8 slave_Address, uint8 reg)
{
   qapi_Status_t res = QAPI_OK;
    qapi_I2CM_Config_t config;
   qapi_I2CM_Descriptor_t desc[2];
   unsigned int  transferred1 = 0, transferred2 = 0;

   qt_uart_dbg(uart_conf.hdlr,"i2c_read: slave_addr[0x%x], reg[0x%x]\r\n", slave_Address, reg);

   // Configure the bus speed and slave address
   config.bus_Frequency_KHz = 50; 
   config.slave_Address     = slave_Address;
   config.SMBUS_Mode        = 0;
   config.slave_Max_Clock_Stretch_Us = 100000;
   config.core_Configuration1 = 0;
   config.core_Configuration2 = 0;

   // Read N bytes from a register 0x01 on a sensor device
   // <S><slave_address><W><A><register><A><S><slave_address><R><A><data1><A><data2><A>...<dataN><N><P>
   desc[0].buffer      = &reg;
   desc[0].length      = 1;
   desc[0].transferred = (uint32)&transferred1;
   desc[0].flags       = QAPI_I2C_FLAG_START | QAPI_I2C_FLAG_WRITE | QAPI_I2C_FLAG_STOP;
   res = qapi_I2CM_Transfer (client_handle, &config, &desc[0], 1, client_callback, &CB_Parameter, 100); 

   desc[1].buffer      = rbuf;
   desc[1].length      = 2;
   desc[1].transferred = (uint32)&transferred2;
   desc[1].flags       = QAPI_I2C_FLAG_START | QAPI_I2C_FLAG_READ  | QAPI_I2C_FLAG_STOP;

   res = qapi_I2CM_Transfer (client_handle, &config, &desc[1], 1, client_callback, &CB_Parameter, 100);   

   return res;
}



qapi_Status_t i2c_write (uint8 slave_Address, uint8 reg, uint8 *wbuf, uint8 len)
{
   qapi_Status_t res = QAPI_OK;
  qapi_I2CM_Config_t config;
  qapi_I2CM_Descriptor_t desc[2];
 //  static  uint8    buffer[4];
   uint8 	i;
   unsigned int  transferred1 = 0;

   buffer[0] = reg; // first byte is register addr
   for (i = 0; i < len; ++i)
   {
       buffer[i + 1] = wbuf[i];
   }

   qt_uart_dbg(uart_conf.hdlr,"i2c_write: slave_addr[0x%x], reg[0x%x], byte1[0x%x], byte2[0x%x]\r\n", slave_Address, buffer[0], buffer[1], buffer[2]);

   // Configure the bus speed and slave address
   config.bus_Frequency_KHz = 50; 
   config.slave_Address     = slave_Address;
   config.SMBUS_Mode        = 0;
   config.slave_Max_Clock_Stretch_Us = 100000;
   config.core_Configuration1 = 0;
   config.core_Configuration2 = 0;

   desc[0].buffer      =buffer;
   desc[0].length      = 3;
   desc[0].transferred = (uint32)&transferred1;
   desc[0].flags       = QAPI_I2C_FLAG_START | QAPI_I2C_FLAG_WRITE | QAPI_I2C_FLAG_STOP;


   res = qapi_I2CM_Transfer (client_handle, &config, &desc[0], 1, client_callback, &CB_Parameter, 100);
 
   return res;
}

 

int quectel_task_entry(void)
{
	
    qapi_Status_t res = QAPI_OK;
    uint8   slave_Address = 0x1B;  // address of CODEC ALC5616
    uint8   reg =0x19;
    uint8   wbuf[2] = {0x03, 0x04};    
	int ret = -1;
  
	qapi_Timer_Sleep(1, QAPI_TIMER_UNIT_SEC, true);
	

	ret = txm_module_object_allocate(&byte_pool_gpio, sizeof(TX_BYTE_POOL));
	if(ret != TX_SUCCESS)
	{
		return ret;
	}

	ret = tx_byte_pool_create(byte_pool_gpio, "Sensor1 application pool",free_memory_Uart, GPIO_BYTE_POOL_SIZE);
	if(ret != TX_SUCCESS)
	{
		return ret;
	}
	
	ret = tx_byte_allocate(byte_pool_gpio, (VOID *)&rx_buff, 4*1024, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		return ret;
	}

	ret = tx_byte_allocate(byte_pool_gpio, (VOID *)&tx_buff, 4*1024, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		return ret;
	}

	quectel_dbg_uart_init(QAPI_UART_PORT_002_E);

	/* prompt task running */
    qt_uart_dbg(uart_conf.hdlr,"[task_create] start task ~");


   tx_byte_allocate(byte_pool_gpio, (VOID *)&rbuf, 2, TX_NO_WAIT);

   tx_byte_allocate(byte_pool_gpio, (VOID *)&buffer, 3, TX_NO_WAIT);


    // Obtain a client specific connection handle to the i2c bus instance 4, I2C
    res = qapi_I2CM_Open(QAPI_I2CM_INSTANCE_004_E, &client_handle);    
    qt_uart_dbg(uart_conf.hdlr,"qapi_i2cm_open: res=%d, hdl=%x\r\n", res, client_handle);

    res = qapi_I2CM_Power_On(client_handle);
  	qt_uart_dbg(uart_conf.hdlr,"qapi_I2CM_Power_On: res=%d\r\n", res);
    
   // Reads the value of the register
   res = i2c_read(slave_Address, reg);     
   qt_uart_dbg(uart_conf.hdlr,"i2c_read:byte1[0x%x], byte2[0x%x]\r\n", rbuf[0], rbuf[1]);
 
   // write the register
   res = i2c_write(slave_Address, reg, wbuf, 2); 
    
   qapi_Timer_Sleep(500, QAPI_TIMER_UNIT_MSEC, true);
   qt_uart_dbg(uart_conf.hdlr,"i2c_write:res=%d\r\n", res);
  
    // read the register again
   res = i2c_read(slave_Address, reg);   
    
   qapi_Timer_Sleep(500, QAPI_TIMER_UNIT_MSEC, true);
   qt_uart_dbg(uart_conf.hdlr,"i2c_read:byte1[0x%x], byte2[0x%x]\r\n", rbuf[0], rbuf[1]);


    // Close the connection handle to the i2c bus instance
    res = qapi_I2CM_Close (client_handle);    
 	qt_uart_dbg(uart_conf.hdlr,"qapi_i2cm_close: res=%d\r\n", res);

	qt_uart_dbg(uart_conf.hdlr,"\r\n===quectel i2c task entry Exit!!!===\r\n");
  
    return 0;
}

#endif /*__EXAMPLE_I2C__*/

