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
#if defined(__EXAMPLE_ANALOG_SPI__)
#include "txm_module.h"
#include "qapi_diag.h"
#include "qapi_timer.h"
#include "qapi_uart.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "qapi_fs_types.h"
#include "qapi_fs.h"
#include "example_analog_spi.h"
#include "qapi_spi_master.h"
#include <locale.h>
#include "qapi_quectel.h"


#include "quectel_utils.h"
#include "quectel_gpio.h"
#include "qapi_tlmm.h"


uint16 clk_mw_id=0;
uint16 cs_mw_id=0;
uint16 mosi_mw_id=0;
uint16 miso_mw_id=0;
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
static QT_UART_CONF_PARA uart1_conf =
{
	NULL,
	QT_QAPI_UART_PORT_01,
	NULL,
	0,
	NULL,
	0,
	115200
};



GPIO_MAP_TBL gpio_map_tbl[4] = {
 /* PIN NUM,	 PIN NAME,	  GPIO ID  GPIO FUNC */
	 {	4, 	 "MOSI",	 24,   0},
	 {	5, 	 "MISO",	 21,   0},
	 {	6, 	 "CS",		 22,   0},
	 {	7, 	 "CLK", 	 23,   0},	
 };



qapi_TLMM_Config_t tlmm_config[4];
qapi_GPIO_ID_t     gpio_id_tbl[4];
qapi_TLMM_Config_t tlmm_config[4];


#define MOSI 0
#define MOSO 1
#define CS   2
#define CLK  3

#define SPI_SET_CLK_HIGH()          qapi_TLMM_Drive_Gpio(clk_mw_id, gpio_map_tbl[CLK].gpio_id, QAPI_GPIO_HIGH_VALUE_E);
#define SPI_SET_CLK_LOW()           qapi_TLMM_Drive_Gpio(clk_mw_id, gpio_map_tbl[CLK].gpio_id, QAPI_GPIO_LOW_VALUE_E);
#define SPI_SET_MOSI_HIGH()         qapi_TLMM_Drive_Gpio(mosi_mw_id, gpio_map_tbl[MOSI].gpio_id, QAPI_GPIO_HIGH_VALUE_E);
#define SPI_SET_MOSI_LOW()          qapi_TLMM_Drive_Gpio(mosi_mw_id, gpio_map_tbl[MOSI].gpio_id, QAPI_GPIO_LOW_VALUE_E);
#define SPI_GET_MISO_BIT()          GPIO_ReadInputDataBit_MOSO()
#define SPI_SET_MOSI(x)             GPIO_OutDataBit_MOSI(x)

#define SPI_DELAY      500   //4us

/****************************************************************************
 * 
@func
  gpio_config
@brief
  [in]  m_pin
  		MODULE_PIN_ENUM type; the GPIO pin which customer want used for operation;
  [in]  gpio_dir
  		qapi_GPIO_Direction_t type; GPIO pin direction.
  [in]  gpio_pull
  		qapi_GPIO_Pull_t type; GPIO pin pull type.
  [in]  gpio_drive
  		qapi_GPIO_Drive_t type; GPIO pin drive strength. 
*
 ***************************************************************************/
void gpio_config(MODULE_PIN_ENUM m_pin,
				 qapi_GPIO_Direction_t gpio_dir,
				 qapi_GPIO_Pull_t gpio_pull,
				 qapi_GPIO_Drive_t gpio_drive
				 )
{
	qapi_Status_t status = QAPI_OK;

	tlmm_config[m_pin].pin   = gpio_map_tbl[m_pin].gpio_id;
	tlmm_config[m_pin].func  = gpio_map_tbl[m_pin].gpio_func;
	tlmm_config[m_pin].dir   = gpio_dir;
	tlmm_config[m_pin].pull  = gpio_pull;
	tlmm_config[m_pin].drive = gpio_drive;

	// the default here
	status = qapi_TLMM_Get_Gpio_ID(&tlmm_config[m_pin], &gpio_id_tbl[m_pin]);
    if(m_pin == MOSI)
    {
        mosi_mw_id = gpio_id_tbl[MOSI];
    }
    if(m_pin == MOSO)
    {
        miso_mw_id = gpio_id_tbl[MOSO];
    }
    if(m_pin == CLK)
    {
        clk_mw_id = gpio_id_tbl[CLK];
    }
    if(m_pin == CS)
    {
        cs_mw_id = gpio_id_tbl[CS];
    }

	IOT_DEBUG("QT# gpio_config status  %d",status);
	IOT_DEBUG("QT# gpio_map_tbl[m_pin].gpio_id %d",gpio_map_tbl[m_pin].gpio_id);
	if (status == QAPI_OK)
	{
		status = qapi_TLMM_Config_Gpio(gpio_id_tbl[m_pin], &tlmm_config[m_pin]);
		IOT_DEBUG("QT# gpio_config status  %d",status);
		if (status != QAPI_OK)
		{
			IOT_DEBUG("QT# gpio_config failed");
		}
	}

}

/*************************************************************************
* FUNCTION
*	GPIO_ReadInputDataBit_MOSO
*
* DESCRIPTION
*	This function is to GPIO_ReadInputDataBit_MOSO.
*
* PARAMETERS
*  None
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
qapi_GPIO_Value_t GPIO_ReadInputDataBit_MOSO(void)
{
	qapi_GPIO_Value_t level; 
	qapi_TLMM_Read_Gpio(gpio_id_tbl[MOSO], gpio_map_tbl[MOSO].gpio_id, &level);

	return level ;
}
/*************************************************************************
* FUNCTION
*	GPIO_OutDataBit_MOSI
*
* DESCRIPTION
*	This function is GPIO_OutDataBit_MOSI.
*
* PARAMETERS
*  None
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void GPIO_OutDataBit_MOSI(unsigned char val)
{
	if(val)
	{
	    qapi_TLMM_Drive_Gpio(mosi_mw_id, gpio_map_tbl[MOSI].gpio_id, QAPI_GPIO_HIGH_VALUE_E);
	}
	else
	{
		qapi_TLMM_Drive_Gpio(mosi_mw_id, gpio_map_tbl[MOSI].gpio_id, QAPI_GPIO_LOW_VALUE_E);
	}
}

/*************************************************************************
* FUNCTION
*	spi_flash_cs
*
* DESCRIPTION
*	This function is to spi_flash_cs.
*
* PARAMETERS
*  None
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void spi_flash_cs(unsigned char val)
{
	if (val)
	{
         qapi_TLMM_Drive_Gpio(cs_mw_id, gpio_map_tbl[CS].gpio_id, QAPI_GPIO_HIGH_VALUE_E);
	}
	else
	{
        qapi_TLMM_Drive_Gpio(cs_mw_id, gpio_map_tbl[CS].gpio_id, QAPI_GPIO_LOW_VALUE_E);
	}
}

/*************************************************************************
* FUNCTION
*	SetSpiDelay
*
* DESCRIPTION
*	This function is to Set Spi Delay.
*
* PARAMETERS
*  None
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void SetSpiDelay(void)
{
   uint16 delay;
   for(delay=0;delay<SPI_DELAY;delay++){}
}

/*************************************************************************
* FUNCTION
*	MOSI_GPIO_InitIO
*
* DESCRIPTION
*	This function initializes MOSI.
*
* PARAMETERS
*  None
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void MOSI_GPIO_InitIO(void)
{
	qapi_TLMM_Release_Gpio_ID(&tlmm_config[MOSI], gpio_id_tbl[MOSI]); 
	gpio_config(MOSI, QAPI_GPIO_OUTPUT_E, QAPI_GPIO_NO_PULL_E, QAPI_GPIO_2MA_E); 
}

/*************************************************************************
* FUNCTION
*	MOSO_GPIO_InitIO
*
* DESCRIPTION
*	This function initializes MOSO.
*
* PARAMETERS
*  None
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void MOSO_GPIO_InitIO(void)
{	
	qapi_TLMM_Release_Gpio_ID(&tlmm_config[MOSO], gpio_id_tbl[MOSO]); 
	gpio_config(MOSO, QAPI_GPIO_INPUT_E, QAPI_GPIO_PULL_DOWN_E, QAPI_GPIO_2MA_E); 
}

/*************************************************************************
* FUNCTION
*	CLK_GPIO_InitIO
*
* DESCRIPTION
*	This function initializes CLK.
*
* PARAMETERS
*  None
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void CLK_GPIO_InitIO(void)
{
	qapi_TLMM_Release_Gpio_ID(&tlmm_config[CLK], gpio_id_tbl[CLK]); 
	gpio_config(CLK, QAPI_GPIO_OUTPUT_E, QAPI_GPIO_NO_PULL_E, QAPI_GPIO_2MA_E); 
}

/*************************************************************************
* FUNCTION
*	CS_GPIO_InitIO
*
* DESCRIPTION
*	This function initializes CS.
*
* PARAMETERS
*  None
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void CS_GPIO_InitIO(void)
{
	qapi_TLMM_Release_Gpio_ID(&tlmm_config[CS], gpio_id_tbl[CS]); 
	gpio_config(CS, QAPI_GPIO_OUTPUT_E, QAPI_GPIO_NO_PULL_E, QAPI_GPIO_2MA_E); 
	qapi_TLMM_Drive_Gpio(cs_mw_id, gpio_map_tbl[CS].gpio_id, QAPI_GPIO_HIGH_VALUE_E);
}

/*************************************************************************
* FUNCTION
*	SpiInit
*
* DESCRIPTION
*	This function initializes Spi.
*
* PARAMETERS
*  None
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void SpiInit() 
{ 
    MOSI_GPIO_InitIO();
	SPI_SET_MOSI_LOW();
    MOSO_GPIO_InitIO();
    CLK_GPIO_InitIO();
    SPI_SET_CLK_LOW();
    qapi_Timer_Sleep(1, QAPI_TIMER_UNIT_MSEC, true);
}

/*************************************************************************
* FUNCTION
*	SpiModeCfg
*
* DESCRIPTION
*	This function cfg SPI mode.
*
* PARAMETERS
*  None
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void SpiModeCfg(uint8 SPImode) 
{
	uint16 delay;
	
	if(0 == SPImode || 1 == SPImode )   //cpol=0 的请情况下，clk在空闲状态时为低电平
    {
        SPI_SET_CLK_LOW(); 
    }
    else                                //cpol=1 的请情况下，clk在空闲状态时为高电平
    {
        SPI_SET_CLK_HIGH(); 
    }
	   
	for(delay=0;delay<1000;delay++){};	 
}


/*************************************************************************
* FUNCTION
*	SPI_write_bit_high
*
* DESCRIPTION
*	This function is to set a bit as high.
*
* PARAMETERS
*  None
* RETURNS
*	None    
*
* GLOBALS AFFECTED
*
*************************************************************************/
/*write 1*/
static void SpiWriteBitHigh(uint8 SPImode)
{
   switch(SPImode)
   {    
        //mode 0(cpol=0 cpha=0)
        case 0:
        {
            SPI_SET_MOSI_HIGH(); 
            
            SetSpiDelay();
            SPI_SET_CLK_HIGH(); 
            SetSpiDelay();
            SPI_SET_CLK_LOW(); 

            break;
        }
        // mode 1(cpol=0 cpha=1)
        case 1:
        {       
            SPI_SET_CLK_HIGH();
            SPI_SET_MOSI_HIGH(); 
            SetSpiDelay();
            SPI_SET_CLK_LOW();

            SetSpiDelay();    
            break;
        }
	  // mode 2(cpol=1 cpha=0)
		case 2:
		{ 
			SPI_SET_MOSI_HIGH(); 			  
			SetSpiDelay();
			SPI_SET_CLK_LOW();
			SetSpiDelay();
			SPI_SET_CLK_HIGH();

			break;
		}
	   // mode 3(cpol=1 cpha=1)
		case 3:
		{
			SPI_SET_CLK_LOW();
			SPI_SET_MOSI_HIGH(); 
			SetSpiDelay();
			SPI_SET_CLK_HIGH();

			SetSpiDelay();	
				break;
		}
	defaut:
		break;
   }
}

/*************************************************************************
* FUNCTION
*	SpiWriteBitLow
*
* DESCRIPTION
*	This function is to set a bit as low.
*
* PARAMETERS
*  None
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
/*write 0*/
static void SpiWriteBitLow(uint8 SPImode)
{
   switch(SPImode)
   {    
        //mode 0(cpol=0 cpha=0)
        case 0:
        {
            SPI_SET_MOSI_LOW();  
            SetSpiDelay();
            SPI_SET_CLK_HIGH();
            SetSpiDelay();
            SPI_SET_CLK_LOW();
            break;
        }
        // mode 1(cpol=0 cpha=1)
        case 1:
        {       

            SPI_SET_CLK_HIGH();
            SPI_SET_MOSI_LOW();
            SetSpiDelay();
            SPI_SET_CLK_LOW();
            SetSpiDelay();    
            break;
        }
	  // mode 2(cpol=1 cpha=0)
		case 2:
		{
			SPI_SET_MOSI_LOW();				 
			SetSpiDelay();
			SPI_SET_CLK_LOW();
			SetSpiDelay();
			SPI_SET_CLK_HIGH();
			break;
		}
	   // mode 3(cpol=1 cpha=1)
		case 3:
		{
			SPI_SET_CLK_LOW();
			SPI_SET_MOSI_LOW(); 
			SetSpiDelay();
			SPI_SET_CLK_HIGH();
			SetSpiDelay();	
			break;
		}
	    defaut:
			break;		
   }
}


/*************************************************************************
* FUNCTION
*	SpiReadData
*
* DESCRIPTION
*	This function is to read data via SPI.
*
* PARAMETERS
*  None
*
* RETURNS
*	received data
*
* GLOBALS AFFECTED
*
*************************************************************************/
uint8 SpiReadData(uint8 SPImode)
{ 
    uint8 data=0; 
    int    i;          
   switch(SPImode)
   {
        //mode 0(cpol=0 cpha=0)
        case 0:
        {
            for(i=7;i>=0;i--)
            {
                SPI_SET_CLK_HIGH();
                SetSpiDelay();
                SPI_SET_CLK_LOW();
                if(SPI_GET_MISO_BIT())
                	data |= (1<<i); 
                SetSpiDelay();
            }
            break;
        }
        //mode 1(cpol=0 cpha=1)
        case 1:
        {
            for(i=7;i>=0;i--)
            {
                SPI_SET_CLK_HIGH();
                if(SPI_GET_MISO_BIT())
                	data |= (1<<i); 
                SetSpiDelay();
                SPI_SET_CLK_LOW();
                SetSpiDelay();
            }
            break;
        }
        //mode 2(cpol=1 cpha=0)
        case 2:
        {
            for(i=7;i>=0;i--)
            {               
                SPI_SET_CLK_LOW();
                SetSpiDelay();
                SPI_SET_CLK_HIGH();
                if(SPI_GET_MISO_BIT())
                	data |= (1<<i); 
                SetSpiDelay();
            }
            break;
        }
        //mode 3(cpol=1 cpha=1)
	  case 3:
		{
			for(i=7;i>=0;i--)
			{
				SPI_SET_CLK_LOW();
				if(SPI_GET_MISO_BIT())
					data |= (1<<i); 
				SetSpiDelay();
				SPI_SET_CLK_HIGH();
				SetSpiDelay();
			}
			break;
		}	 
   }
	return data;
}

/*************************************************************************
* FUNCTION
*	SpiWriteData
*
* DESCRIPTION
*	This function is to write data via SPI.
*
* PARAMETERS
*  transmitted data
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void SpiWriteData(uint8 SPImode,uint8 Write_data)
{
    int i=7;
	for(i=7;i>=0;i--)
	{	
		if(Write_data & (1<<i))
		{
			SpiWriteBitHigh(SPImode);
		}
		else
		{
			SpiWriteBitLow(SPImode);
		}	
	}
}

/*************************************************************************
* FUNCTION
*	SpiWriteRead
*
* DESCRIPTION
*	This function is to Write and Read.
*
* PARAMETERS
*  None
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
uint8 SpiWriteRead(uint8 SPImode,uint8 Write_data)
{ 
	uint8 read_data=0; 
	int	 i; 	   
   switch(SPImode)
   {
		//mode 0(cpol=0 cpha=0)
		case 0:
		{
			for(i=7;i>=0;i--)
			{
				SPI_SET_MOSI(((Write_data & (1<<i)) != 0));
				SetSpiDelay();
				SPI_SET_CLK_HIGH();
				if(SPI_GET_MISO_BIT())
				read_data |= (1<<i); 
				SetSpiDelay();
				SPI_SET_CLK_LOW();
			}
			break;
		}
		//mode 1(cpol=0 cpha=1)
		case 1:
		{
			for(i=7;i>=0;i--)
			{
				SPI_SET_CLK_HIGH();
				SPI_SET_MOSI(((Write_data & (1<<i)) != 0));
				SetSpiDelay();
				SPI_SET_CLK_LOW();
				if(SPI_GET_MISO_BIT())
				read_data |= (1<<i); 
				SetSpiDelay();
			}
			break;
		}
		//mode 2(cpol=1 cpha=0)
		case 2:
		{
			for(i=7;i>=0;i--)
			{
				SPI_SET_MOSI(((Write_data & (1<<i)) != 0));
				SetSpiDelay();
				SPI_SET_CLK_LOW();
				if(SPI_GET_MISO_BIT())
				read_data |= (1<<i); 
				SetSpiDelay();
				SPI_SET_CLK_HIGH();				
			}
			break;
		}
		//mode 3(cpol=1 cpha=1)
		case 3:
		{
			for(i=7;i>=0;i--)
			{
				SPI_SET_CLK_LOW();
				SPI_SET_MOSI(((Write_data & (1<<i)) != 0));
				SetSpiDelay();
				SPI_SET_CLK_HIGH();
				if(SPI_GET_MISO_BIT())
				read_data |= (1<<i);
				SetSpiDelay();
			}
			break;
		}	
	}
	return read_data;
}

/*************************************************************************
* FUNCTION
*	quectel_SPI_WriteRead
*
* DESCRIPTION
*	This function is to Writes and Reads.
*
* PARAMETERS
*  None
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void quectel_SPI_WriteRead(uint8 *pData,uint32 wrtLen,uint8 * pBuffer,uint32 rdLen)
{
   
    uint32 i;
	uint8 *tpData = pData;
	uint8 *tpBuffer = pBuffer;
	uint8 write_data;
	uint8 read_data;

	for(i=0; i<wrtLen+rdLen; i++ )
	{					
		write_data = *tpData++;

		if(i >= wrtLen) 
		write_data = 0xFF;
		read_data = SpiWriteRead(0,write_data);

		if(i >= wrtLen)
		*tpBuffer++ = read_data; 				   
	}	
}

/*************************************************************************
* FUNCTION
*	quecte_SPI_Write
*
* DESCRIPTION
*	This function is to SPI Write.
*
* PARAMETERS
*  None
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void quecte_SPI_Write(uint8 * pData,uint32 len)
{
	uint8 data;
	uint32 i;
	for(i=0;i<len;i++ )
	{
		data = *(pData + i);
		SpiWriteData(0,data);
	}
}

/*************************************************************************
* FUNCTION
*	Flash_wr_enable
*
* DESCRIPTION
*	This function is flash_wr_enable.
*
* PARAMETERS
*  None
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void Flash_wr_enable()          //ok
{   
    qapi_Status_t ret = QAPI_OK; 
    uint8  cmd = 0x06; 
	spi_flash_cs(0);
	
    quecte_SPI_Write(&cmd,1);
	spi_flash_cs(1);
}

/*************************************************************************
* FUNCTION
*	spi_flash_read
*
* DESCRIPTION
*	This function is used to get data from flash.
*
* PARAMETERS
*  None
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void spi_flash_read(void)
{
    unsigned char status=0;
	uint8 wr_buff[32]={0x03,0x00,0x00,0x10};
	uint8 rd_buff[7]={0xff};
	spi_flash_cs(0);	
    quectel_SPI_WriteRead(wr_buff,4,rd_buff,5);
	spi_flash_cs(1);                         // 关闭片选	
	IOT_DEBUG("\r\n===111 spi_flash_read  %x  %x  %x %x %x!!!===\r\n",rd_buff[0],rd_buff[1],rd_buff[2],rd_buff[3],rd_buff[4]);	
}

/*************************************************************************
* FUNCTION
*	spi_flash_write
*
* DESCRIPTION
*	This function is used to write data to flash.
*
* PARAMETERS
*  None
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void spi_flash_write(void)
{
    unsigned char status=0;
	uint8 wr_buff[32]={0x02,0x00,0x00,0x10,0x11,0x22,0x33,0x44,0x55};
	uint8 rd_buff[32]={0xff};   
	Flash_wr_enable();
	spi_flash_cs(0);	
    quecte_SPI_Write(wr_buff,9);
	spi_flash_cs(1);                         // 关闭片选	
	IOT_DEBUG("\r\n===111  spi_flash_write  %x  %x  %x %x %x!!!===\r\n",wr_buff[4],wr_buff[5],wr_buff[6],wr_buff[7],wr_buff[8]);	
}

int quectel_task_entry(void)
{
    qapi_Status_t res = QAPI_OK; 

	uint8 wr_buff[32]={0x9f};
	uint8 rd_buff[32]={0xff};

    qapi_SPIM_Descriptor_t spi_desc[1];

    int ret = -1;

    qapi_Timer_Sleep(30, QAPI_TIMER_UNIT_SEC, true);

	IOT_DEBUG("QT# quectel_task_entry Start");
	
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
	uart_init(&uart1_conf);
	/* start uart 2 receive */
	uart_recv(&uart1_conf);
	/* prompt task running */
	qt_uart_dbg(uart1_conf.hdlr, "\r\n===quectel spi task entry Start!!!===\r\n");

///////////////////////////////////////////////////////////////////////////////////

   /******************* SPI  init**********************/
	SpiInit();

	SpiModeCfg(0);

	qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);
	CS_GPIO_InitIO();   
while(1)
{
	/*********** Write able ********************************/
	Flash_wr_enable();
    /*********** Read w25q64 id ********************************/  
    qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);
	spi_flash_cs(0);		
	quectel_SPI_WriteRead(wr_buff,1,rd_buff,3);
	spi_flash_cs(1);
	IOT_DEBUG("[spi]id:, %x  %x  %x", rd_buff[0] ,rd_buff[1],rd_buff[2]);  //读 id
	qt_uart_dbg(uart1_conf.hdlr, "[spi]id:, %x  %x  %x", rd_buff[0] ,rd_buff[1],rd_buff[2]);
	/*********** Write data to w25q64 ********************************/  
	spi_flash_write();
	/*********** Read data from w25q64 ********************************/  
	spi_flash_read();
	qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);
}
    return 0;
}
#endif /*__EXAMPLE_SPI__*/

