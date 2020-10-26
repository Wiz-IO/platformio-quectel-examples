/******************************************************************************
*@file   example_spi.c
*@brief  example of spi operation
*           This example is used for test SPI funtion by hardware.
*           And the peripheral SPI flash type is W25Q128FV
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#if defined(__EXAMPLE_SPI__)
#include "txm_module.h"
#include "qapi_diag.h"
#include "qapi_timer.h"
#include "qapi_uart.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "qapi_fs_types.h"
#include "qapi_fs.h"
#include "example_spi.h"
#include "qapi_spi_master.h"
#include <locale.h>
#include "qapi_quectel.h"

/**************************************************************************
*								  GLOBAL
***************************************************************************/
TX_BYTE_POOL *byte_pool_uart;
#define UART_BYTE_POOL_SIZE		10*8*1024
UCHAR free_memory_uart[UART_BYTE_POOL_SIZE];

/* uart2 rx tx buffer */
static char *rx2_buff=NULL;
static char *tx2_buff=NULL;

/* uart config para*/
static QT_UART_CONF_PARA uart2_conf =
{
	NULL,
	QT_QAPI_UART_PORT_01,
	NULL,
	0,
	NULL,
	0,
	115200
};

qapi_SPIM_Config_t spi_config; // for spi config
void *spi_hdl = NULL;

/*spi callback func*/
int cb_para = 0;


/**************************************************************************
*                                 FUNCTION
***************************************************************************/
void qapi_spi_cb_func(uint32 status, void *cb_para)
{
    if (QAPI_SPI_COMPLETE == status)
    {//The transaction is complete.
        qt_uart_dbg(uart2_conf.hdlr,"[==spim_cb_func==]: transfer success, status: %d, cb_p: %d", status, *((int*)cb_para));
    }
    else if (QAPI_SPI_QUEUED == status || QAPI_SPI_IN_PROGRESS == status)
    {//The transaction is processing.
        qt_uart_dbg(uart2_conf.hdlr,"[==spim_cb_func==]: transfer in progress, status: %d, cb_p: %d", status, *((int*)cb_para));
    }
    else
    {//An error occured in the transaction.
        qt_uart_dbg(uart2_conf.hdlr,"[==spim_cb_func==]: transfer failed, status: %d, cb_p: %d", status, *((int*)cb_para));
    }
}

int quectel_task_entry(void)
{
    qapi_Status_t res = QAPI_OK; 
    uint8 wr_buff[4] = {0x9f, 0xff, 0xff, 0xff}; // cmd for read flash id
    uint8 rd_buff[4] = {0xff, 0xff, 0xff, 0xff}; // buff for id
  
    qapi_SPIM_Descriptor_t spi_desc[1];

    int ret = -1;

    qapi_Timer_Sleep(8, QAPI_TIMER_UNIT_SEC, true);

	IOT_DEBUG("QT# quectel_task_entry Start");
	setlocale(LC_ALL, "C");

	ret = txm_module_object_allocate(&byte_pool_uart, sizeof(TX_BYTE_POOL));
  	if(ret != TX_SUCCESS)
  	{
  		IOT_DEBUG("txm_module_object_allocate [byte_pool_sensor] failed, %d", ret);
    	return ret;
  	}

	ret = tx_byte_pool_create(byte_pool_uart, "Sensor application pool", free_memory_uart, UART_BYTE_POOL_SIZE);
  	if(ret != TX_SUCCESS)
  	{
  		IOT_DEBUG("tx_byte_pool_create [byte_pool_sensor] failed, %d", ret);
    	return ret;
  	}

  /*QT_UART_ENABLE_2ND */ 
	ret = tx_byte_allocate(byte_pool_uart, (VOID *)&rx2_buff, 4*1024, TX_NO_WAIT);
  	if(ret != TX_SUCCESS)
  	{
  		IOT_DEBUG("tx_byte_allocate [rx2_buff] failed, %d", ret);
    	return ret;
  	}

  	ret = tx_byte_allocate(byte_pool_uart, (VOID *)&tx2_buff, 4*1024, TX_NO_WAIT);
  	if(ret != TX_SUCCESS)
  	{
  		IOT_DEBUG("tx_byte_allocate [tx2_buff] failed, %d", ret);
    	return ret;
  	}

	/* uart 2 init */
	uart_init(&uart2_conf);
	/* start uart 2 receive */
	uart_recv(&uart2_conf);
	/* prompt task running */
	qt_uart_dbg(uart2_conf.hdlr, "\r\n===quectel spi task entry Start!!!===\r\n");


    // Obtain a client specific connection handle to the spi bus instance 6
    res = qapi_SPIM_Open(QT_QAPI_SPIM_PORT_01, &spi_hdl);    
    qt_uart_dbg(uart2_conf.hdlr,"qapi_SPIM_Open: res=%d, hdl=%x", res, spi_hdl);
	IOT_DEBUG("qapi_SPIM_Open: res=%d, hdl=%x", res, spi_hdl);
	
    res = qapi_SPIM_Power_On(spi_hdl);
    qt_uart_dbg(uart2_conf.hdlr,"qapi_SPIM_Power_On: res=%d", res);
	

    //spi interface config
    spi_config.SPIM_Mode = QAPI_SPIM_MODE_0_E; // set the spi mode, determined by slave device
    spi_config.SPIM_CS_Polarity = QAPI_SPIM_CS_ACTIVE_LOW_E; // set CS low as active, determined by slave device
    spi_config.SPIM_endianness  = SPI_LITTLE_ENDIAN;
    spi_config.SPIM_Bits_Per_Word = 8;
    spi_config.SPIM_Slave_Index = 0;
    spi_config.Clk_Freq_Hz = 1000000; //config spi clk about 1Mhz
    spi_config.SPIM_CS_Mode = QAPI_SPIM_CS_KEEP_ASSERTED_E;
    spi_config.CS_Clk_Delay_Cycles = 0; // don't care, set 0 is ok.
    spi_config.Inter_Word_Delay_Cycles = 0; // don't care, set 0 is ok.
    spi_config.loopback_Mode =0;
    
    
    qt_uart_dbg(uart2_conf.hdlr,"[config] MDOE=%d, Clk_Freq_Hz=%d, Bits_Per_Word=%d\r\n", spi_config.SPIM_Mode, spi_config.Clk_Freq_Hz, spi_config.SPIM_Bits_Per_Word);   
    
    spi_desc[0].rx_buf = rd_buff;
    spi_desc[0].tx_buf = wr_buff;
    spi_desc[0].len = 4;
    cb_para = 1;
    
    res = qapi_SPIM_Full_Duplex(spi_hdl, &spi_config, spi_desc, 1, qapi_spi_cb_func, &cb_para, false); // at now only support one descriptor
    qapi_Timer_Sleep(500, QAPI_TIMER_UNIT_MSEC, true);
    qt_uart_dbg(uart2_conf.hdlr,"[Read flash ID] ret=%x, rd[1]=%x, rd[2]=%x, rd[3]=%x\r\n", res, rd_buff[1], rd_buff[2], rd_buff[3]);
	IOT_DEBUG("[Read flash ID] ret=%x, rd[1]=%x, rd[2]=%x, rd[3]=%x\r\n", res, rd_buff[1], rd_buff[2], rd_buff[3]);

    res = qapi_SPIM_Power_Off(spi_hdl);
    qt_uart_dbg(uart2_conf.hdlr,"qapi_SPIM_Power_Off: res=%d", res);
	

    // Close the connection handle to the spi bus instance
    res = qapi_SPIM_Close(spi_hdl);    
    qapi_Timer_Sleep(500, QAPI_TIMER_UNIT_MSEC, true);
    qt_uart_dbg(uart2_conf.hdlr,"qapi_SPIM_Close: res=%d", res);
 
	qt_uart_dbg(uart2_conf.hdlr,"\r\n===quectel spi task entry Exit!!!===\r\n");
    return 0;
}

#endif /*__EXAMPLE_SPI__*/

