/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Quectel Co., Ltd. 2016
*
*****************************************************************************/
/*****************************************************************************
 *
 * Filename:
 * ---------
 *   example_EPO.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example demonstrates how to download MTK EPO file from RAM: MTKxx.EPO to GNSS module in OpenCPU.
 *   All debug information will be output  through DEBUG port.
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_DOWNLOAD_EPO__" in gcc_makefile file. And compile the 
 *     app using "make clean/new".
 *     Download image bin to module to run.
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
#ifdef __EXAMPLE_DOWNLOAD_EPO__
#include "string.h"
#include "ql_type.h"
#include "ql_trace.h"
#include "ql_uart.h"
#include "ql_fs.h"
#include "ql_error.h"
#include "ql_system.h"
#include "ql_stdlib.h"
#include "ril.h"
#include "ril_util.h"
#include "ril_telephony.h"
#include "ril_network.h"


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



#define  MY_FILE_TAG  		"\r\nQuectel GS2 : download EPO file test\r\n"

char textBuf[100];




#define SERIAL_RX_BUFFER_LEN  4096

// Define the UART port and the receive data buffer of MCU
static Enum_SerialPort m_mcuUartPort  = UART_PORT1;
static u8 m_RxBuf_mcuUart[SERIAL_RX_BUFFER_LEN];

// Defeine the UART port and the receive data buffer of L70 GNSS module 
static Enum_SerialPort m_gnssUartPort = UART_PORT3;
static u8 m_RxBuf_gnssUART[SERIAL_RX_BUFFER_LEN];

// Define the virtual port and the receive data buffer
static Enum_SerialPort m_myVirtualPort = VIRTUAL_PORT1;
static u8 m_RxBuf_VirtualPort[SERIAL_RX_BUFFER_LEN];


s8 s_s8FilePath[1024];   /*fileNamePath*/  /*example "RAM:MTK30.EPO"  or "RAM:MTK_1D.EPO"  */

u8 strBuf[100];
s32 filehandle = -1;

static u32 flag=0;
static int iReadSize = 0;
static int iReadLen = 0;

#define READ_SIZE 512  //bytes
char buf[READ_SIZE];



/*download epo  file */
#define MTKBIN_3EPO_PKT_LNG 227
#define MTK_EPO_SET_LNG 2304
#define MTK_EPO_SAT_LNG 72
#define RAMFILESIZE     276480 
//#define RAMFILESIZE   (56*72*4*30)
#define  EPO_BIN_ACK_LEN    12

int                 g_i4NumSvEpoPkt = 0;
u8  g_szPktData[MTKBIN_3EPO_PKT_LNG]={0};
int g_u2EpoSeq = 0;
int g_u2LastEpoSeq = 0;
int g_pEpoFile = 0;


static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);

static bool qmisc_gps_find_a_line(unsigned char *p, int total_len,int *line_len)
{
    int tmp = 0;	

    if(p == NULL || total_len == 0)
    {
        return FALSE;
    }


  
nextfind:
    while(*(p++) != 0x0D)  //<CR>
    {
        if(++tmp > total_len)
        {
            return FALSE;
        }
    }
    if(++tmp > total_len)
    {
        return FALSE;
    }

    if(*p == 0x0D)
    {
        p++;
        if(++tmp > total_len)
        {
            return FALSE;
        }

    }

    if(*p == 0x0A)          //<LF>
    {
        *line_len = ++tmp;
        return TRUE;
    }
    else
        goto nextfind;

}

bool fgEPO_Verify_File(int pEpoFile)
{

	long cur_pos;
	long len;

	
	//cur_pos = ftell( pEpoFile );
	cur_pos = Ql_FS_GetFilePosition(pEpoFile);

	//fseek( pEpoFile, 0, SEEK_END );
	Ql_FS_Seek(pEpoFile,0,QL_FS_FILE_END);

	//len = ftell( pEpoFile );
	len = Ql_FS_GetFilePosition( pEpoFile );

	//fseek( pEpoFile, cur_pos, SEEK_SET );
	Ql_FS_Seek(pEpoFile,cur_pos,QL_FS_FILE_CURRENT);
	return (len != 0) && ((len%MTK_EPO_SET_LNG)==0);
}

int i2EPO_Get_Num_Pkt(int pEpoFile)
{
	long cur_pos;
	long len;
    
#if 1
	cur_pos = Ql_FS_GetFilePosition(pEpoFile);
	Ql_FS_Seek(pEpoFile,0,QL_FS_FILE_END);

	len = Ql_FS_GetFilePosition( pEpoFile );
	Ql_FS_Seek(pEpoFile,cur_pos,QL_FS_FILE_CURRENT);
#else

	cur_pos = ftell( pEpoFile );
	fseek( pEpoFile, 0, SEEK_END );

	len = ftell( pEpoFile );
	ftell( pEpoFile, cur_pos, SEEK_SET );
#endif
	return ((len/MTK_EPO_SET_LNG * 32)%3 == 0)? (len/MTK_EPO_SET_LNG * 32)/3 : (int)(len/MTK_EPO_SET_LNG * 32)/3  + 1 ;
}

bool fgEPO_Get_One_Pkt(int u2EpoSeq, int pEpoFile,unsigned char *szData)
{
        u32   loopi = 0;
        u32  act_len = 0;
        u32 result;
	char checksum = 0;

	unsigned char buf[6+2+72*3+1+2]={0};
	
	unsigned char framehead[6]={0x04,0x24,0xE3,0x00,0xD3,0x02};
	unsigned char frametail[2]={0x0D,0x0A};
	int pos=0;
      	u16 wSeq = u2EpoSeq;
    
	Ql_memcpy(buf,framehead,sizeof(framehead));
	pos=6;
	
	buf[pos++]=(u8)wSeq ;	
	buf[pos++] = (u8)((wSeq >> 8 ) & 0xff);

	Ql_FS_Seek(pEpoFile,u2EpoSeq * 3 *MTK_EPO_SAT_LNG,QL_FS_FILE_CURRENT);	
        result = Ql_FS_Read(pEpoFile,&buf[pos],MTK_EPO_SAT_LNG*3,&act_len);
           
	pos = MTKBIN_3EPO_PKT_LNG - 2;

	Ql_memcpy(&buf[pos],frametail,sizeof(frametail));

	for(loopi=2;loopi<MTKBIN_3EPO_PKT_LNG-3;loopi++)
	{
		checksum = checksum^buf[loopi];
	}
		
	buf[MTKBIN_3EPO_PKT_LNG - 3] = checksum;


	for(loopi=0;loopi<MTKBIN_3EPO_PKT_LNG;loopi++)
	{		
		*(szData+loopi) = *(buf+loopi);
	}

	return TRUE;
}


void vEPO_Get_Final_Pkt(unsigned char *szData)
{
       u32  loopi = 0;
	char checksum = 0;

	unsigned char buf[6+2+72*3+1+2] = {0};
	
	unsigned char framehead[6]={0x04,0x24,0xE3,0x00,0xD3,0x02};
	unsigned char frametail[2]={0x0D,0x0A};
	int pos=0;
	Ql_memcpy(buf,framehead,sizeof(framehead));
	pos=6;
	
	buf[pos++]=0xFF;
	buf[pos++]=0xFF;
	
	pos = MTKBIN_3EPO_PKT_LNG - 2;

	Ql_memcpy(&buf[pos],frametail,sizeof(frametail));

	for(loopi=2;loopi<MTKBIN_3EPO_PKT_LNG-3;loopi++)
	{
			checksum = checksum^buf[loopi];
	}
		
	buf[MTKBIN_3EPO_PKT_LNG - 3] = checksum;


	for(loopi=0;loopi<MTKBIN_3EPO_PKT_LNG;loopi++)
	{
		*(szData+loopi) = *(buf+loopi);
	}
}

bool fgWait_Epo_Ack(u8 * inBuffer,int len)
{
	int count = 0,line_length = 0;
	unsigned char * cur_ptr = NULL;
	bool bRet = FALSE;
       
    	if(inBuffer == NULL || len < 12)
    	return FALSE;

    	count = 0;
    	while(count < len)
    	{
        	//find frame head.
        	if((inBuffer[count] != 0x04) || (inBuffer[count+1] != '$'))
        	{
            		++count;
            		continue;
        	}

        	cur_ptr = inBuffer + count;
        	bRet = qmisc_gps_find_a_line(cur_ptr,len-count,&line_length); 
             if(bRet)
        	{
            		if( (*(cur_ptr) == 0x04) && (*(cur_ptr+1) == 0x24) && (*(cur_ptr+2) == 0x0C) && (*(cur_ptr+3) == 0x00) && (*(cur_ptr+4) == 0x02) &&(*(cur_ptr+5) == 0x00) && (*(cur_ptr+8) == 0x01) && (*(cur_ptr+10) == 0x0d) &&(*(cur_ptr+11) == 0x0a))
            		{           
                           APP_DEBUG("fgWait_Epo_Ack bRet find out it \r\n");
                		return TRUE;
            		}

                	count += line_length;            
        	}
        	else
        	{
            		count = len;            
        	}

    	}
    	return FALSE;

}


bool SendMtkBinCmd(u8 *strCmd)
{
        int len = 0,ret = 0;
        unsigned char outBuffer[256];
    
	// At initial, the protocol setting of the communication UART is supposed to be PMTK protocol.
	// Since EPO data are transferred using MTK Binary Packet, you have to change the protocol setting to MTK Binary Protocol
	// before starting EPO Transfer Protocol. You can use PMTK command 253 to change the UART protocol setting.
	// Please refer to 7.1 for the details of PMTK command 253.

        Ql_memset(outBuffer,0,sizeof(outBuffer));                
        Ql_memcpy((void *)outBuffer, strCmd, Ql_strlen(strCmd));
        len = Ql_strlen(outBuffer);
        outBuffer[len++] = 0x0D;
        outBuffer[len++] = 0x0A;
        outBuffer[len] = '\0';   
   
	/*send the data of outBuffer to the GPS module example L70 ect. */
        ret = Ql_UART_Write(m_gnssUartPort,outBuffer,len);
        if (ret < len)
        {
            APP_DEBUG("SendMtkBinCmd Only part of bytes are written, %d/%d \r\n", ret, len);
            return FALSE;
        }
        
        return TRUE;	

}


bool VerifyFileAndSendFirstEpoPacket(u8 *strFilePath)
{
    int ret = 0,filesize = 0;
    int sendLen = 0;
        
    if((strFilePath == NULL) || (Ql_strlen(strFilePath) == 0))
    {
         APP_DEBUG("VerifyFileAndSendFirstEpoPacket, strFilePath %s \r\n", strFilePath);             
        return FALSE;
    }
    ret = Ql_FS_Check((char*)strFilePath);
    if(ret != QL_RET_OK)
    {
        APP_DEBUG("Ql_FS_Check, strFilePath %s error \r\n", strFilePath);            
        return FALSE;
    }
    filesize = Ql_FS_GetSize((char*)strFilePath);       
    if(filesize < 0)
    {
        APP_DEBUG("Ql_FS_GetSize, filesize %d error \r\n ", filesize);            
        return FALSE;
    }       
    if (Ql_strncmp((char*)strFilePath,"RAM:", 4))
    {
        g_pEpoFile = Ql_FS_Open((char*)strFilePath, QL_FS_READ_ONLY);
    }
    else
    {
        g_pEpoFile = Ql_FS_OpenRAMFile((char*)strFilePath, QL_FS_READ_ONLY, filesize);
    }        
    // Read in the EPO file, and then verify the validity of EPO data. If the input EPO file is not in valid MTK EPO format, the
    // programmer shall terminate the process.
    if(g_pEpoFile < 0)
    {
        Ql_FS_Close(g_pEpoFile);
        APP_DEBUG("Ql_FS_OpenRAMFile, g_pEpoFile %d error \r\n ", g_pEpoFile);            
        return FALSE;
    }
    else
    {
        if (!fgEPO_Verify_File(g_pEpoFile))
        {
            Ql_FS_Close(g_pEpoFile);
            APP_DEBUG("fgEPO_Verify_File, g_pEpoFile %d error \r\n", g_pEpoFile);                    
            return FALSE;
        }
    }                    
    // Get total number of MTK_BIN_EPO packets that will be sent.
    g_i4NumSvEpoPkt =  i2EPO_Get_Num_Pkt(g_pEpoFile);       
    // Start EPO Data Transfer Protocol to send EPO data
    // fgEPO_Get_One_Pkt takes out three SAT data from the EPO file and encapsulated them in a MTK_BIN_EPO packet
    // with appropriate EPO SEQ number.
    // In order to save the total transferring time, we suggest to generate current EPO packet first, and then wait for
    // MTK_BIN_ACK_EPO acknowledge of the previous MTK_BIN_EPO packet from the GPS receiver.
    // The function fgEPO_Get_One_Pkt must be implemented by the programmer.
    g_u2LastEpoSeq = 0;
    g_u2EpoSeq = 0;
    Ql_memset(g_szPktData,0,sizeof(g_szPktData));
    if (fgEPO_Get_One_Pkt(g_u2EpoSeq, g_pEpoFile, g_szPktData))
    {
        sendLen =  MTKBIN_3EPO_PKT_LNG;
        ret = Ql_UART_Write(m_gnssUartPort,g_szPktData,sendLen);
        if (ret < sendLen)
        {
            APP_DEBUG("VerifyFileAndSendFirstEpoPacket Only part of bytes are written, %d/%d \r\n", ret, sendLen);            
            return FALSE;
        }
        // Update sequence number
        g_u2LastEpoSeq = g_u2EpoSeq;
        g_u2EpoSeq++;
        return TRUE;
    }
    else
    {
        APP_DEBUG("VerifyFileAndSendFirstEpoPacket fgEPO_Get_One_Pkt error \r\n");
        return FALSE;
    }

}


bool ReadAckAndSendEpoPacket(u8 * inBuffer,int dataLen)
{
     int ret = 0,sendLen = 0;
     unsigned char outBuf[256] = {0};
    
     if(dataLen >= EPO_BIN_ACK_LEN)
    {       
        
        if((g_u2LastEpoSeq < g_i4NumSvEpoPkt) && (g_i4NumSvEpoPkt >= 1))
        {
            if(g_u2LastEpoSeq != g_u2EpoSeq)
            {                
                if(!fgWait_Epo_Ack(inBuffer,dataLen))
                {
                        /* send again */
                        sendLen = MTKBIN_3EPO_PKT_LNG;
                        ret = Ql_UART_Write(m_gnssUartPort,g_szPktData,sendLen);                        
                        if (ret < sendLen)
                        {
                            APP_DEBUG("ReadAckAndSendEpoPacket send again Only part of bytes are written, %d/%d \r\n", ret, sendLen);                            
                            return FALSE;
                        }
                        return TRUE;
        	    }
                else
                {        
                    APP_DEBUG(textBuf,"Update EPO Progress£º%d/%d \r\n",g_u2EpoSeq,g_i4NumSvEpoPkt);
                    if(g_u2EpoSeq < g_i4NumSvEpoPkt)
                    {
                        if (fgEPO_Get_One_Pkt(g_u2EpoSeq, g_pEpoFile, g_szPktData))
                       {                             
                            sendLen = MTKBIN_3EPO_PKT_LNG,
                            ret = Ql_UART_Write(m_gnssUartPort,g_szPktData,sendLen);
                            if (ret < sendLen)
                            {
                                APP_DEBUG("ReadAckAndSendEpoPacket Only part of bytes are written, %d/%d \r\n", ret, sendLen);                                
                                return FALSE;
                            }
                            g_u2LastEpoSeq = g_u2EpoSeq;
                            g_u2EpoSeq++;
                            return TRUE;
                        }
                        else
                        {
                            APP_DEBUG("ReadAckAndSendEpoPacket fgEPO_Get_One_Pkt error !!! \r\n");                            
                            return FALSE;
                        }

                    }
                    else
                    {  
                        // Generate final MTK_BIN_EPO packet to indicate the GPS receiver that the process is finish.
                        // The function fgEPO_Get_Final_Pkt must be implemented by the programmer.
                        vEPO_Get_Final_Pkt(g_szPktData);
                        // Send final MTK_BIN_EPO packet to the GPS receiver. The packet size of MTK_BIN_EPO is MTKBIN_3EPO_PKT_LNG.
                        // Then the process is finished.
                        // The function SendData must be implemented by the programmer.                        
                        sendLen = MTKBIN_3EPO_PKT_LNG;
                        ret = Ql_UART_Write(m_gnssUartPort,g_szPktData,sendLen);
                        if (ret < sendLen)
                        {
                            APP_DEBUG("ReadAckAndSendEpoPacket Only part of bytes are written, %d/%d \r\n", ret, sendLen);                            
                            return FALSE;
                        }
                        // Switch UART protocol setting to PMTK packet format and baudrate 115200 for the communication UART.
                        // Please refer to section 7.2 for the details of ¡°Change UART format packet¡±.
                        // The function SendMtkBinCmd must be implemented by the programmer.                                              
                        outBuf[0] = 0x04;
                        outBuf[1] = 0x24;
                        outBuf[2] = 0x0e;
                        outBuf[3] = 0x00;
                        outBuf[4] = 0xfd;
                        outBuf[5] = 0x00;
                        outBuf[6] = 0x00;
                        outBuf[7] = 0x00;
                        outBuf[8] = 0x00;
                        outBuf[9] = 0x00;
                        outBuf[10] = 0x00;
                        outBuf[11] = 0xf3;
                        outBuf[12] = 0x0d;
                        outBuf[13] = 0x0a;                        
                        sendLen = 14;
                        ret = Ql_UART_Write(m_gnssUartPort,outBuf,sendLen);
                        if (ret < sendLen)
                        {
                            APP_DEBUG("ReadAckAndSendEpoPacket restore packet, %d/%d \r\n", ret, sendLen);                            
                            return FALSE;
                        }
                        Ql_FS_Close(g_pEpoFile);
                        g_u2LastEpoSeq = 0;
                        g_i4NumSvEpoPkt = 0;
                        g_u2EpoSeq = 0;
                        g_pEpoFile = 0;
                        APP_DEBUG(textBuf,"\r\nOK \r\n");
                        return TRUE;
                    }                    

                    return FALSE;
                }
            }
            else
            {                
                APP_DEBUG("ReadAckAndSendEpoPacket else  error !!!\r\n");               
                return FALSE;
            }
        }
        else
        {
            return FALSE;
        }

    }
    return FALSE;
}


static void InitializeSerialPort(void)
{
    s32 ret;

    // Register & open UART port
    // UART1 for MCU
    ret = Ql_UART_Register(m_mcuUartPort, CallBack_UART_Hdlr, NULL);
    if (ret < QL_RET_OK)
    {
        Ql_Debug_Trace("Fail to register serial port[%d], ret=%d\r\n", m_mcuUartPort, ret);
    }
    ret = Ql_UART_Open(m_mcuUartPort, 115200, FC_NONE);
    if (ret < QL_RET_OK)
    {
        Ql_Debug_Trace("Fail to open serial port[%d], ret=%d\r\n", m_mcuUartPort, ret);
    }    

    // Register & open UART port
    // UART3 for MCU
    ret = Ql_UART_Register(m_gnssUartPort, CallBack_UART_Hdlr, NULL);
    if (ret < QL_RET_OK)
    {
        APP_DEBUG("Fail to register serial port[%d], ret=%d\r\n", m_gnssUartPort, ret);
    }
    ret = Ql_UART_Open(m_gnssUartPort, 9600, FC_NONE);
    if (ret < QL_RET_OK)
    {
        APP_DEBUG("Fail to open serial port[%d], ret=%d\r\n", m_gnssUartPort, ret);
    }
    

    // Register & open Modem port
    ret = Ql_UART_Register(m_myVirtualPort, CallBack_UART_Hdlr, NULL);
    if (ret < QL_RET_OK)
    {
        APP_DEBUG("Fail to register serial port[%d], ret=%d\r\n", m_myVirtualPort, ret);
    }
    ret = Ql_UART_Open(m_myVirtualPort, 0, FC_NONE);
    if (ret < QL_RET_OK)
    {
        APP_DEBUG("Fail to open serial port[%d], ret=%d\r\n", m_myVirtualPort, ret);
    }

    return;
}


/******************************************************************************/
/*                              Main Code                                     */
/******************************************************************************/
void proc_main_task(void)
{
    s32 ret;
    ST_MSG msg;
    bool keepGoing = TRUE;

        // Init serial port
    InitializeSerialPort();

    APP_DEBUG(MY_FILE_TAG);
    


    // START MESSAGE LOOP OF THIS TASK
    while (keepGoing)
    {
         Ql_OS_GetMessage(&msg);
        switch(msg.message)
        {
           
            case 0:
            {                
                break;
            }            
            default:
                break;
          
        }        
    }

}

static s32 ReadSerialPort(Enum_SerialPort port, /*[out]*/u8* pBuffer, /*[in]*/u32 bufLen)
{
    s32 rdLen = 0;
    s32 rdTotalLen = 0;
    int loopi = 0;
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


static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{
    //APP_DEBUG("CallBack_UART_Hdlr: port=%d, event=%d, level=%d, p=%x\r\n", port, msg, level, customizedPara);
    u32 rdLen=0;
    s32 ret;
    char *pData= NULL;
    char *p=NULL;
    char *p1=NULL;
    char *p2=NULL;
    bool    bRet = FALSE;
    
    switch (msg)
    {
        case EVENT_UART_READY_TO_READ:
        {
            if(UART_PORT1 == port)
            {  
                do
                {                   
                    Ql_memset(m_RxBuf_mcuUart, 0x0, sizeof(m_RxBuf_mcuUart));
                    rdLen = Ql_UART_Read(port, m_RxBuf_mcuUart, sizeof(m_RxBuf_mcuUart));
                    pData = m_RxBuf_mcuUart;
                    /*set last tail data eq 0*/
                    pData[rdLen + 1] = '\0';                    
                   
                    /*cmd: DEPO*/
                    p = Ql_strstr(pData,"DEPO");                    
                    if (p)
                    {
                        APP_DEBUG(textBuf,"\r\nDEPO \r\n");
                        bRet = SendMtkBinCmd("$PMTK253,1,0*37");
                        if(!bRet)
                        {
                            APP_DEBUG(textBuf,"\r\n Send $PMTK Error \r\n");
                            break;
                        }
                        else
                        {
                            bRet = VerifyFileAndSendFirstEpoPacket(s_s8FilePath);
                             if(!bRet)
                            {
                                APP_DEBUG(textBuf,"\r\n VerifyFile Error \r\n");                                
                                break;
                            }
                             else
                             {
                                APP_DEBUG(textBuf,"Update EPO Progress Start £º\r\n");
                             }
                        }
                        break;
                    }

                    
                    /*cmd:  QueryAvailableStorage=UFS*/
                    p = Ql_strstr(pData,"QueryAvailableStorage");
                    if (p)
                    {
                        if(Ql_strstr(pData,"=UFS"))
                        {
                            s64 size;
                            size = Ql_FS_GetFreeSpace(1);        // UFS
                            APP_DEBUG(textBuf,"QueryAvailableStorage=%lld\r\n",size);   
                                    
                        }else if(Ql_strstr(pData,"=SD"))
                        {
                            s64 size;
                            size = Ql_FS_GetFreeSpace(2);  //SD
                            APP_DEBUG(textBuf,"QueryAvailableStorage=%lld\r\n",size); 
                        }else if(Ql_strstr(pData,"=RAM"))
                        {
                            s64 size;
                            size = Ql_FS_GetFreeSpace(3);  //RAM
                            APP_DEBUG(textBuf,"QueryAvailableStorage=%lld\r\n",size); 
                        }else
                        {
                            APP_DEBUG(textBuf,"QueryAvailableStorage param error!\r\n"); 
                        }
                        break;
                    }
                    
                    /*cmd: WriteFile=password.txt*/
                    p = Ql_strstr(pData,"WriteFile=");
                    if (p)
                    {
                        s32 s32Res = -1;
                        u32 uiFlag = 0;
                        u32 i=0;
                        char stmpdir[100],dir[100];
                        char *p1[100];
                        //u32 filesize = 10240;
                        u32 filesize = 276480;

                        p = Ql_strstr(pData, "=");
                        (*p) = '\0';
                        p++;

                        Ql_memset(s_s8FilePath, 0, sizeof(s_s8FilePath));  //File Name
                        Ql_strcpy((char *)s_s8FilePath, p);

                        uiFlag |= QL_FS_READ_WRITE;
                        uiFlag |= QL_FS_CREATE;

                        flag = 1;                     

                        if(Ql_strncmp((char*)s_s8FilePath,"RAM:",4))
                        {
                            ret = Ql_FS_Open((u8*)s_s8FilePath, uiFlag);
                            
                        }
                        else
                        {
                           ret = Ql_FS_OpenRAMFile((u8*)s_s8FilePath, uiFlag,filesize); 
                        }
                        if(ret >= QL_RET_OK)
                        {
                            filehandle = ret;
                        }

                        APP_DEBUG(textBuf,"filehandle=%d\r\n",filehandle);

                        break;
                    }


                    //Receiving data and StopWrite
                    if (flag)
                    {

                        u32 writeedlen;
                        p = Ql_strstr(pData,"StopWrite");
                        if (p)
                        {
                            Ql_FS_Close(filehandle);
                            filehandle = -1;
                            flag =0;
                            APP_DEBUG(textBuf,"Ql_WriteFinish()\r\n");

                            break;
                        }
                        ret = Ql_FS_Write(filehandle, (u8*)pData, rdLen, &writeedlen);
                        if(ret== QL_RET_OK)
                        {
                            //APP_DEBUG(textBuf,"Ql_File()=%d: writeedlen=%d,Download times=%d\r\n",ret, writeedlen,flag++);

                        }else if(ret == QL_RET_ERR_FILEDISKFULL)
                        {
                            Ql_FS_Close(filehandle);
                            filehandle = -1;
                            flag =0;
                            APP_DEBUG(textBuf,"Storage Full,close file! ret=%d\r\n",ret);
                            break;
                        }else
                        {
                            Ql_FS_Close(filehandle);
                            filehandle = -1;
                            flag =0;
                            APP_DEBUG(textBuf,"write error,close file!ret=%d \r\n",ret);
                            break;
                        }
                    }                                
                   

                    /*cmd: AT Command*/

                    if (Ql_strstr(pData,"AT")||Ql_strstr(pData,"at") || Ql_strstr(pData,"At") || Ql_strstr(pData,"aT"))
                    {
                        //APP_DEBUG(textBuf,"%s", pData);
                        Ql_UART_Write(VIRTUAL_PORT1,(u8*)pData,rdLen);
                        
                        break;
                    }
                    
                }while(rdLen > 0);

            }
            else if(UART_PORT3 == port)
            {
                s32 totalBytes = ReadSerialPort(port, m_RxBuf_gnssUART, sizeof(m_RxBuf_gnssUART));                
                if (totalBytes <= 0)
                {
                    APP_DEBUG("<-- No data in gnssUART buffer! -->\r\n");
                    return;
                }
                
                 ReadAckAndSendEpoPacket(m_RxBuf_gnssUART,totalBytes);

            }   
           else if((VIRTUAL_PORT1 == port) || (VIRTUAL_PORT2 == port))
            {
                s32 ret,totalBytes;

                totalBytes = ReadSerialPort(port, m_RxBuf_VirtualPort, sizeof(m_RxBuf_VirtualPort));
                if (totalBytes <= 0)
                {
                    APP_DEBUG("<-- No data in virtural UART buffer! -->\r\n");
                    return;
                }
                Ql_UART_Write(UART_PORT1,(u8*)m_RxBuf_VirtualPort,totalBytes);

            }
            
            break;
        }
        default:
            break;
    }
}


#endif // __EXAMPLE_DOWNLOAD_EPO__


