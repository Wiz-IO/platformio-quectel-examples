/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Quectel Co., Ltd. 2014
*
*****************************************************************************/
/*****************************************************************************
*
* Filename:
* ---------
*   example_bluetooth.c
*
* Project:
* --------
*   OpenCPU
*
* Description:
* ------------
*   This example demonstrates how to program BLUETOOTH interface in OpenCPU.
*
*   All debug information will be output through DEBUG port.
*
* Usage:
* ------
*   Compile & Run:
*
*     Set "C_PREDEF=-D __EXAMPLE_BLUETOOTH__" in gcc_makefile file. And compile the 
*     app using "make clean/new".
*     Download image bin to module to run.


    Description: There are two scenes in bluetooth application 

    1.active side

      (1)power on module BT func
      (2)set a customized name for module BT
      (3)scan the bt devices around ,and device info will be indicated through URCS
      (4)choose one bt device to connect,if module has paired it before ,directly goes to step 7
      (5)for the first time to connect,you need to make a pair between module and BT device
      (6)while pair is established ,you need make a confirm for this pair
      (7)once the pair process is passed ,next goes to connection procedure,be aware of you connection mode
      (8)after connection,you can check the BT status of module 
      (9)last step is to execute BT read/write operations

    2.passive side

      (1)power on module BT func
      (2)set a customized name for module BT
      (3)external BT devices make a attempt to connect module,specilized URCS will be reported to reflect 
         the connection procedure
      (4)according to the specilized URCS ,we do different handles

NOTE:Current SDK is only support SPP mode ,so you can test this demo between two modules or with your phone by installing a 
        BT-UART.apk in your phone side

  * If customer wants to run this example, the following steps should be followed:
      1. After the module powers on,demo will print module bt name ,visible mode and enter bt scan mode automatically(scan time is 40s in default) 
      2. During the scan procedure,you can wait until scan finished or excute "stopscan" command to finish the scan manually
      3. After the scan finished,a device info table is prompted ,formated as below:
          DEVICEADDR    SCANID    DEVHANDLE   PAIRID   CONNID    PROFILEID    DEVICENAME
         33F09DCC6261      1      0x21945f74   -1         -1        -1           M66
          DEVICEADDR : the bt addr of the device
          SCANID:the scan id of the device ,0 represents scan finished
          DEVHANDLE : the hash value of the bt addr,is also unique
          PAIRID : the pair id of paired device (1-10),support 10 paired device at most,default is -1
          CONNID : the connect id of the connected device(0-2),default is -1
          PROFILEID : the profile type when connected(SPP for example),default is -1
          DEVICENAME : the bt name of the device
      4. So far ,you can take a review of the info table and decide which device to operate
      5. Using "pair=DEVHANDLE" to pair a device ,if you are using a phone ,you need to confirm the pair in your phone side
            and the module side is confirmed automatically 
      6. You can use "query" command to query current bt state info at any point of the flow
      7. After paired ,you can use "unpair=DEVHANDLE"command to unpair the dedicated pair
      8. For paired items ,you can use "conn=DEVHANDLE,PROFILEID,MODE"to make a connect.PROFILEID sees emum Enum_BTProfileId
            and MODE sees Enum_BT_SPP_ConnMode
      9. Using "disc=DEVHANDLE" to disconnect a connection
      10.Using "send=DEVHANDLE,data"command to send the contents of data area to the dedicated bt device
      11.Using "read"command to recieve the data from connected bt device
      12.If another device make a connect request to module ,you should type "connaccept"command to accpet the connect request

 

*
* Author: Lebron Liu  
* -------
* -------
*
*============================================================================
*             HISTORY
*----------------------------------------------------------------------------
* 
****************************************************************************/


#ifdef __EXAMPLE_BLUETOOTH__

#include "ql_stdlib.h"
#include "ql_uart.h"
#include "ql_trace.h"
#include "ql_type.h"
#include "ql_system.h"
#include "ril_bluetooth.h"
#include "ril.h"

#define DEBUG_ENABLE 1
#if DEBUG_ENABLE > 0
#define DEBUG_PORT  UART_PORT2
#define DBG_BUF_LEN   2048
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




#define BL_POWERON        (1)
#define BL_POWERDOWN      (0)
#define MAX_BLDEV_CNT     (20)
#define DEFA_CON_ID       (0)
#define SCAN_TIME_OUT     (60)   //s
#define SERIAL_RX_BUFFER_LEN  (2048)
#define BL_RX_BUFFER_LEN       (1024+1)
#define BT_EVTGRP_NAME      "BT_EVETNGRP"
#define BT_EVENT_PAIR      (0x00000004)




static char bt_name[BT_NAME_LEN] = "chengxiaofei";
static ST_BT_DevInfo **g_dev_info = NULL;
static u8 m_RxBuf_Uart1[SERIAL_RX_BUFFER_LEN];
static u8 m_RxBuf_BL[BL_RX_BUFFER_LEN];
static u8 SppSendBuff[BL_RX_BUFFER_LEN];
static ST_BT_BasicInfo pSppRecHdl;

static u32  g_nEventGrpId = 0;

static bool g_pair_search = FALSE;
static bool g_paired_before = FALSE;
static char pinCode[BT_PIN_LEN] = {0};
static bool g_connect_ok = FALSE;
//static bool  g_passive_dir = FALSE;


static ST_BT_DevInfo cur_btdev = {0};
static ST_BT_DevInfo BTSppDev1 = {0};
static ST_BT_DevInfo BTSppDev2 = {0};


static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);
static void BT_COM_Demo(void);
static void BT_Callback(s32 event, s32 errCode, void* param1, void* param2); 
static void BT_Dev_Append(ST_BT_BasicInfo* pstBtDev);
static void BT_Dev_Clean(void);



static void BT_DevMngmt_UpdatePairId(const u32 hdl, const s8 pairId);
static s32 ATResponse_Handler(char* line, u32 len, void* userData);




void proc_subtask1(s32 taskId)
{
    ST_MSG msg;
    // Register & open UART port
    Ql_UART_Register(UART_PORT1, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(UART_PORT1, 115200, FC_NONE);

    Ql_UART_Register(UART_PORT2, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(UART_PORT2, 115200, FC_NONE);

    APP_DEBUG("\r\n<-- OpenCPU: subtask -->\r\n")

    while (TRUE)
    {
        Ql_OS_GetMessage(&msg);
        switch(msg.message)
        {
            default:
                break;
        }
    }
}

//main function
void proc_main_task(s32 taskId)
{
    s32 ret;
    ST_MSG msg;


    g_nEventGrpId = Ql_OS_CreateEvent(BT_EVTGRP_NAME);
    
    APP_DEBUG("\r\n<-- OpenCPU: Bluetooth Test Example -->\r\n")

    // Start message loop of this task
    while (TRUE)
    {
        Ql_OS_GetMessage(&msg);
        switch(msg.message)
        {
            case MSG_ID_RIL_READY:
                
                APP_DEBUG("<-- RIL is ready -->\r\n");
                
                Ql_RIL_Initialize();
                
                BT_COM_Demo();
            break;

            case MSG_ID_USER_START:
                break;

            default:
                break;
        }
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

static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{
    s32 ret = RIL_AT_SUCCESS ;
    
    switch (msg)
    {
    case EVENT_UART_READY_TO_READ:
        {
            if (UART_PORT2 == port)
            {
                s32 totalBytes = ReadSerialPort(port, m_RxBuf_Uart1, sizeof(m_RxBuf_Uart1));
                
                if (totalBytes <= 0)
                {
                    Ql_Debug_Trace("<-- No data in UART buffer! -->\r\n");
                    return;
                }
                                              
                if(0 == Ql_memcmp(m_RxBuf_Uart1,"disc=",5)) 
                {
                    BT_DEV_HDL BTHdl = 0;

                    u8 *p1,*p2;
                    p1 = m_RxBuf_Uart1+5;
                    p2 = Ql_strstr(m_RxBuf_Uart1,"\r\n");
                    if( p1 && p2)
                    {
                        u8 str[11] = {0};
                        Ql_strncpy((char*)str,(char*)p1,p2-p1);
                        BTHdl = strtoul(str,NULL,16);
                    }
                    else
                    {
                        APP_DEBUG("Invalide parameter.\r\n");
                        break;
                    }

                    ret = RIL_BT_Disconnect(BTHdl);
                    
                    if(RIL_AT_SUCCESS != ret) 
                    {
                        APP_DEBUG("disconnect failed!\r\n");
                        break;
                    }

                    APP_DEBUG("Send disconnect req successful.\r\n");
                    break; ;
                }

                //set BT visible mode & time
                if(0 == Ql_memcmp(m_RxBuf_Uart1,"visible",7)) 
                {
                    ret = RIL_BT_SetVisble(2,60);
                    
                    if(RIL_AT_SUCCESS != ret) 
                    {
                        APP_DEBUG("visible failed!\r\n");
                    }
                    else
                    {
                        APP_DEBUG("set visible successful.\r\n"); 
                    }
                    
                    break;
                }

                //get BT device name
                if(0 == Ql_memcmp(m_RxBuf_Uart1,"name?",5))
                {
                    Ql_memset(bt_name,0,sizeof(bt_name));
                    ret = RIL_BT_GetName(bt_name,sizeof(bt_name));

                    if(RIL_AT_SUCCESS == ret)
                    {
                        APP_DEBUG("BT device name is: %s.\r\n",bt_name);
                    }
                    else
                    {
                        APP_DEBUG("Get BT device name failed.\r\n");
                    }
                    break;
                }

                //set BT device name
                if(0 == Ql_memcmp(m_RxBuf_Uart1,"name=",5)) 
                {
                    u8 *p1,*p2;
                    p1 = Ql_strstr(m_RxBuf_Uart1,"\"");
                    p2 = Ql_strstr(p1+1,"\"");
                    if( p1 && p2)
                    {
                        Ql_memset(bt_name,0,sizeof(bt_name));
                        Ql_strncpy(bt_name,p1+1,sizeof(bt_name)<=(p2-p1-1)?sizeof(bt_name):(p2-p1-1));
                        ret = RIL_BT_SetName(bt_name,Ql_strlen(bt_name));
                        if(RIL_AT_SUCCESS == ret)
                        {
                            APP_DEBUG("BT device name set successful.\r\n");
                        }
                        else
                        {
                            APP_DEBUG("BT device name set failed,ret=%d.\r\n",ret);
                        }
                    }
                    else
                    {
                        APP_DEBUG("Invalid parameters.\r\n");
                    }
                    break;
                }

                //pair BT device
                if(0 == Ql_strncmp(m_RxBuf_Uart1,"pair=",5)) 
                {
                    u8 *p1,*p2;
                    p1 = m_RxBuf_Uart1+5;
                    p2 = Ql_strstr(m_RxBuf_Uart1,"\r\n");
                    if( p1 && p2)
                    {
                        u8 BTHdl[11] = {0};
                        BT_DEV_HDL tBTDevHdl = 0;;
                        
                        Ql_strncpy((char*)BTHdl,(char*)p1,p2-p1);
                        tBTDevHdl = strtoul(BTHdl,NULL,16);
                        //here need to ensure the device not paired
                        //code...
                        ret = RIL_BT_PairReq(tBTDevHdl);
                        if(RIL_AT_SUCCESS == ret)
                        {
                            APP_DEBUG("Start pair operation...\r\n");
                        }
                        else
                        {
                            APP_DEBUG("Pair operation failed.\r\n");
                        }
                    }
                    else
                    {
                        APP_DEBUG("Error parameters.\r\n");
                    }
                    break;
                }

                //paircnf operation
                if(0 == Ql_strncmp(m_RxBuf_Uart1,"paircnf",5)) 
                {
                    u8 *p1,*p2;
                    p1 = Ql_strstr(m_RxBuf_Uart1,",");
                    if(p1)
                    {
                        p2 = Ql_strstr(m_RxBuf_Uart1,"\r\n");
                        if(p2)
                        {
                            char pinCode[BT_PIN_LEN];
                            Ql_memset(pinCode,0,BT_PIN_LEN);
                            Ql_strncpy(pinCode,p1+1,p2-p1-1);
                            APP_DEBUG("pinCode %s\r\n",pinCode);
                            ret = RIL_BT_PairConfirm(TRUE,pinCode);
                        }
                    }
                    else
                    {
                        ret = RIL_BT_PairConfirm(TRUE,NULL);
                    }

                    if(RIL_AT_SUCCESS != ret)
                    {
                        APP_DEBUG("Paircnf failed. ret=%d\r\n",ret);
                    }
                    else
                    {
                        APP_DEBUG("Paircnf successful.\r\n");
                    }
                }                

                //unpair operation
                if(0 == Ql_strncmp(m_RxBuf_Uart1,"unpair=",7)) 
                {
                    u8 *p1,*p2;
                    p1 = m_RxBuf_Uart1+7;
                    p2 = Ql_strstr(m_RxBuf_Uart1,"\r\n");
                    if( p1 && p2)
                    {
                        u8 BTHdl[11] = {0};
                        BT_DEV_HDL tBTDevHdl = 0;;
                        
                        Ql_strncpy((char*)BTHdl,(char*)p1,p2-p1);
                        tBTDevHdl = strtoul(BTHdl,NULL,16);
                        ret = RIL_BT_Unpair(tBTDevHdl);
                        if(RIL_AT_SUCCESS == ret)
                        {
                            APP_DEBUG("Unpair successful.\r\n");
                        }
                        else
                        {
                            APP_DEBUG("Unpair failed.\r\n");
                        }
                    }
                    else
                    {
                        APP_DEBUG("Error parameters.\r\n");
                    }
                    break;
                }

                //stop scan mode
                if(0 == Ql_memcmp(m_RxBuf_Uart1,"stopscan",8)) 
                {
                    ret = RIL_BT_StopScan();
                    
                    if(RIL_AT_SUCCESS != ret) 
                    {
                        APP_DEBUG("stop bt scan failed!\r\n");
                    }
                    
                    return ;
                }

                //start scan mode
                if(0 == Ql_memcmp(m_RxBuf_Uart1,"startscan",9)) 
                {                   
                    ret = RIL_BT_StartScan(MAX_BLDEV_CNT, DEFA_CON_ID, SCAN_TIME_OUT);
                    
                    if(RIL_AT_SUCCESS != ret) 
                    {
                        APP_DEBUG("BT scan failed!\r\n");
                        break;
                    }
                    APP_DEBUG("BT scanning device...\r\n");
                    break;
                }

                //query BT device status
                if(0 == Ql_memcmp(m_RxBuf_Uart1,"query",5)) 
                {
                    s32 status = 0;
                    ret = RIL_BT_QueryState(&status);
                    if(RIL_AT_SUCCESS != ret) 
                    {
                        APP_DEBUG("query bt state failed!\r\n");
                        break;
                    }
                    APP_DEBUG("STATUS = %d\r\n",status);
                    RIL_BT_GetDevListInfo();
                    break;
                }

                //connect with BT device
                //conn=BTHdl,profileId,mode
                if(0 == Ql_strncmp(m_RxBuf_Uart1,"conn=",5)) 
                {
                    BT_DEV_HDL BTDevHdl=0;  /*BT_DEV_HDL            */
                    u8 profileId=0;         /*Enum_BTProfileId      */
                    u8 mode=0;              /*Enum_BT_SPP_ConnMode  */
                    u8 *p1,*p2;
                    
                    //1 get BTHdl;
                    p1 = m_RxBuf_Uart1+5;
                    p2 = Ql_strstr(m_RxBuf_Uart1,",");
                    if( p1 && p2)
                    {
                        u8 str[11] = {0}; 
                        Ql_strncpy((char*)str,(char*)p1,p2-p1);
                        BTDevHdl = strtoul(str,NULL,16);

                    }
                    else
                    {
                         APP_DEBUG("Invalide para.\r\n");
                    }
                    
                    //2 get profileId
                    p1 = p2+1;
                    p2 = Ql_strstr(p1,",");
                    if( p1 && p2)
                    {
                        u8 str[2] = {0}; 
                        Ql_strncpy((char*)str,(char*)p1,p2-p1);
                        profileId = Ql_atoi(str);

                    }
                    else
                    {
                         APP_DEBUG("Invalide para.\r\n");
                    }

                    //3 get mode
                    p1 = p2+1;
                    p2 = Ql_strstr(p1,"\r\n");
                    if( p1 && p2)
                    {
                        u8 str[2] = {0}; 
                        Ql_strncpy((char*)str,(char*)p1,p2-p1);
                        mode = Ql_atoi(str);

                    }
                    else
                    {
                         APP_DEBUG("Invalide para.\r\n");
                    }
                    
                    //APP_DEBUG("BTDevHdl %x,profileId %d,mode %d\r\n",BTDevHdl,profileId,mode);
                    //connect req
                    ret = RIL_BT_ConnReq(BTDevHdl,profileId,mode);
                    if(RIL_AT_SUCCESS == ret)
                    {
                        APP_DEBUG("Send connect req successful.\r\n");
                    }
                    else
                    {
                        APP_DEBUG("Send connect req failed.\r\n");
                    }
                    
                    break;
                }

                //conn accept
                if(0 == Ql_strncmp(m_RxBuf_Uart1,"connaccept",10))
                {
                    ret = RIL_BT_ConnAccept(1,BT_SPP_CONN_MODE_BUF);

                    if(RIL_AT_SUCCESS != ret) 
                    {
                        APP_DEBUG("Send connect accept failed.\r\n");
                    }
                    else
                    {
                        APP_DEBUG("Send connect accept successful.\r\n");
                    }
                }

                //SPP send data
                if(0 == Ql_strncmp(m_RxBuf_Uart1,"send=",5))
                {
                    u8 *p1,*p2;
                    u16 send_size;
                    BT_DEV_HDL BTDevHdl=0; 

                    p1 = m_RxBuf_Uart1+5;
                    p2 = Ql_strstr(m_RxBuf_Uart1,",");
                    if( p1 && p2)
                    {
                        u8 str[11] = {0}; 
                        Ql_strncpy((char*)str,(char*)p1,p2-p1);
                        BTDevHdl = strtoul(str,NULL,16);

                    }
                    else
                    {
                         APP_DEBUG("Invalide para.\r\n");
                    }
                    
                    p1 = p2+1;
                    p2 = Ql_strstr(p1,"\r\n");
                    send_size = Ql_strlen(p1);
                    if(p2)
                    {
                        send_size = Ql_strlen(p1)-2;
                    }
                    
                    if(0 == send_size)
                    {
                        APP_DEBUG("No data for send.\r\n");
                        break;
                    }
                    send_size = send_size <= BL_RX_BUFFER_LEN ? send_size : BL_RX_BUFFER_LEN;
                    Ql_strncpy(SppSendBuff,p1,send_size);
                    ret = RIL_BT_SPP_Send(BTDevHdl,SppSendBuff,send_size,NULL);

                    if(RIL_AT_SUCCESS == ret)
                    {
                        APP_DEBUG("Send successful.\r\n");
                    }
                    else
                    {
                        APP_DEBUG("Send failed.\r\n");
                    }

                    break;
                }

                //SPP read
                if(0 == Ql_strncmp(m_RxBuf_Uart1,"read",4))
                {
                    u32 actualReadLen = 0;
                    
                    Ql_memset(m_RxBuf_BL,0,sizeof(m_RxBuf_BL));
                    if(pSppRecHdl.devHdl == 0)
                    {
                        APP_DEBUG("No sender yet.\r\n");
                        break;
                    }
                    ret = RIL_BT_SPP_Read(pSppRecHdl.devHdl, m_RxBuf_BL, sizeof(m_RxBuf_BL),&actualReadLen);
                    if(RIL_AT_SUCCESS != ret) 
                    {
                        APP_DEBUG("Read failed.ret=%d\r\n",ret);
                        break;
                    }
                    if(actualReadLen == 0)
                    {
                        APP_DEBUG("No more data.\r\n");
                        break;
                    }
                    APP_DEBUG("BTHdl[%x][len=%d]:\r\n%s\r\n",pSppRecHdl.devHdl,actualReadLen,m_RxBuf_BL);
                    break;
                }

            }
            else
            {
            //disable at interactive
#if 0
                s32 totalBytes = ReadSerialPort(port, m_RxBuf_Uart1, sizeof(m_RxBuf_Uart1));
                if (totalBytes <= 0)
                {
                    Ql_Debug_Trace("<-- No data in UART buffer! -->\r\n");
                    return;
                }
                else
                {
                    // Read data from UART
                    s32 ret;
                    char* pCh = NULL;

                    pCh = Ql_strstr((char*)m_RxBuf_Uart1, "\r\n");
                    if (pCh)
                    {
                        *(pCh + 0) = '\0';
                        *(pCh + 1) = '\0';
                    }

                    // No permission for single <cr><lf>
                    if (Ql_strlen((char*)m_RxBuf_Uart1) == 0)
                    {
                        return;
                    }

                    // Echo
                    Ql_UART_Write(UART_PORT1, m_RxBuf_Uart1, totalBytes);
                    ret = Ql_RIL_SendATCmd((char*)m_RxBuf_Uart1, totalBytes, ATResponse_Handler, NULL, 0);
                }
#endif
            }
            break;
        }
    case EVENT_UART_READY_TO_WRITE:
        break;
    default:
        break;
    }
}


static s32 ATResponse_Handler(char* line, u32 len, void* userData)
{
    Ql_UART_Write(UART_PORT1, (u8*)line, len);
    
    if (Ql_RIL_FindLine(line, len, "OK"))
    {  
        return  RIL_ATRSP_SUCCESS;
    }
    else if (Ql_RIL_FindLine(line, len, "ERROR"))
    {  
        return  RIL_ATRSP_FAILED;
    }
    else if (Ql_RIL_FindString(line, len, "+CME ERROR"))
    {
        return  RIL_ATRSP_FAILED;
    }
    else if (Ql_RIL_FindString(line, len, "+CMS ERROR:"))
    {
        return  RIL_ATRSP_FAILED;
    }
    return RIL_ATRSP_CONTINUE; //continue wait
}


static void BT_Callback(s32 event, s32 errCode, void* param1, void* param2)
{
    ST_BT_BasicInfo *pstNewBtdev = NULL;
    ST_BT_BasicInfo *pstconBtdev = NULL;
    s32 ret = RIL_AT_SUCCESS;
    s32 connid = -1;
   
    switch(event)
    {
        case MSG_BT_SCAN_IND :

            if(URC_BT_SCAN_FINISHED== errCode)
            {
                APP_DEBUG("Scan is over.\r\n");
                //APP_DEBUG("Pair/Connect if need.\r\n");
                RIL_BT_GetDevListInfo();
                //here obtain ril layer table list for later use
                g_dev_info = RIL_BT_GetDevListPointer();
                g_pair_search = TRUE;
            }
            if(URC_BT_SCAN_FOUND == errCode)
            {
                pstNewBtdev = (ST_BT_BasicInfo *)param1;
                //you can manage the scan device here,or you don't need to manage it ,for ril layer already handle it
                APP_DEBUG("BTHdl[0x%08x] Addr[%s] Name[%s]\r\n",pstNewBtdev->devHdl,pstNewBtdev->addr,pstNewBtdev->name);
                
            }
            break;
         case MSG_BT_PAIR_IND :
              if(URC_BT_NEED_PASSKEY == errCode)
              {
                    //must ask for pincode;
                    pstconBtdev = (ST_BT_BasicInfo*)param1;
                    APP_DEBUG("Pair device BTHdl: 0x%08x\r\n",pstconBtdev->devHdl);
                    APP_DEBUG("Pair device addr: %s\r\n",pstconBtdev->addr);
                    APP_DEBUG("Waiting for pair confirm with pinCode...\r\n");
              }
              else if(URC_BT_NO_NEED_PASSKEY == errCode)
              {               
                    //no need pincode
                    pstconBtdev = (ST_BT_BasicInfo*)param1;
                    Ql_strncpy(pinCode,(char*)param2,BT_PIN_LEN);
					pinCode[BT_PIN_LEN-1] = 0;
                    APP_DEBUG("Pair device BTHdl: 0x%08x\r\n",pstconBtdev->devHdl);
                    APP_DEBUG("Pair device addr: %s\r\n",pstconBtdev->addr);
                    APP_DEBUG("Pair pin code: %s\r\n",pinCode);
                    APP_DEBUG("pair confirm automatically\r\n");
                    RIL_BT_PairConfirm(TRUE,NULL);
              }  

            break;
         case MSG_BT_PAIR_CNF_IND :
            if(URC_BT_PAIR_CNF_SUCCESS == errCode)
            {
                pstconBtdev = (ST_BT_BasicInfo*)param1;
                APP_DEBUG("Paired successful.\r\n");
            }
            else
            {
                APP_DEBUG("Paired failed.\r\n");
            }            
            break;

         case MSG_BT_SPP_CONN_IND :
            
              if(URC_BT_CONN_SUCCESS == errCode )
              {
                
                Ql_memcpy(&(BTSppDev1.btDevice),(ST_BT_BasicInfo *)param1,sizeof(ST_BT_BasicInfo));
             
                APP_DEBUG("Connect successful.\r\n");
              }
              else
              {
                APP_DEBUG("Connect failed.\r\n");
              }
            
              break;

         case MSG_BT_RECV_IND :

             connid = *(s32 *)param1;
             pstconBtdev = (ST_BT_BasicInfo *)param2;
             Ql_memcpy(&pSppRecHdl,pstconBtdev,sizeof(pSppRecHdl));
             APP_DEBUG("SPP receive data from BTHdl[0x%08x].\r\n",pSppRecHdl.devHdl);
             break; 

         case MSG_BT_PAIR_REQ:

              if(URC_BT_NEED_PASSKEY == errCode)
              {
                   //must ask for pincode;
                    pstconBtdev = (ST_BT_BasicInfo*)param1;
                    APP_DEBUG("Pair device BTHdl: 0x%08x\r\n",pstconBtdev->devHdl);
                    APP_DEBUG("Pair device addr: %s\r\n",pstconBtdev->addr);
                    APP_DEBUG("Waiting for pair confirm with pinCode...\r\n");
              }

              if(URC_BT_NO_NEED_PASSKEY == errCode)
              {
                    //no need pincode
                    pstconBtdev = (ST_BT_BasicInfo*)param1;
                    Ql_strncpy(pinCode,(char*)param2,BT_PIN_LEN);
					pinCode[BT_PIN_LEN-1] = 0;
                    APP_DEBUG("Pair device BTHdl: 0x%08x\r\n",pstconBtdev->devHdl);
                    APP_DEBUG("Pair device addr: %s\r\n",pstconBtdev->addr);
                    APP_DEBUG("Pair pin code: %s\r\n",pinCode);
                    APP_DEBUG("pair confirm automatically\r\n");
                    RIL_BT_PairConfirm(TRUE,NULL);
              }  

            break;

        case  MSG_BT_CONN_REQ :
            
            pstconBtdev = (ST_BT_BasicInfo*)param1;
            APP_DEBUG("Get a connect req\r\n");
            APP_DEBUG("BTHdl: 0x%08x\r\n",pstconBtdev->devHdl);
            APP_DEBUG("Addr: %s\r\n",pstconBtdev->addr);
            APP_DEBUG("Name: %s\r\n",pstconBtdev->name);

            APP_DEBUG("Waiting connect accept.\r\n");          

       case  MSG_BT_DISCONN_IND :

             if(URC_BT_DISCONNECT_PASSIVE == errCode || URC_BT_DISCONNECT_POSITIVE == errCode)
             {
                APP_DEBUG("Disconnect ok!\r\n");
             }


             
          break;
 
             
        default :
            break;
    }
}



static void BT_COM_Demo(void)
{
    s32 cur_pwrstate = 0 ;
    s32 ret = RIL_AT_SUCCESS ;
	s32 visb_mode = -1;
    
    //1 power on BT device
    ret = RIL_BT_GetPwrState(&cur_pwrstate);
    
    if(RIL_AT_SUCCESS != ret) 
    {
        APP_DEBUG("Get BT device power status failed.\r\n");
        //if run to here, some erros maybe occur, need reset device;
        return;
    }

    if(1 == cur_pwrstate)
    {
        APP_DEBUG("BT device already power on.\r\n");
    }
    else if(0 == cur_pwrstate)
    {
       ret = RIL_BT_Switch(BL_POWERON);
       if(RIL_AT_SUCCESS != ret)
       {
            APP_DEBUG("BT power on failed,ret=%d.\r\n",ret);
            return;
       }
       APP_DEBUG("BT device power on.\r\n");
    }

    Ql_memset(bt_name,0,sizeof(bt_name));
    ret = RIL_BT_GetName(bt_name,BT_NAME_LEN);

    if(RIL_AT_SUCCESS != ret) 
    {
        APP_DEBUG("Get BT device name failed.\r\n");
        return;
    }

    APP_DEBUG("BT device name is: %s.\r\n",bt_name);



    //2. Register a callback function for BT operation.
    ret = RIL_BT_Initialize(BT_Callback);

    if(RIL_AT_SUCCESS != ret) 
    {
        APP_DEBUG("BT initialization failed.\r\n");
        return;
    }
    APP_DEBUG("BT callback function register successful.\r\n");

     ret = RIL_BT_GetVisble(&visb_mode);
	 if(RIL_AT_SUCCESS != ret) 
    {
        APP_DEBUG("Get BT visble failed.\r\n");
        return;
    }
	APP_DEBUG("BT visble mode is: %d.\r\n",visb_mode);

    //3 Now the BT device is power on, start scan operation
    //parameters:           maxDevCount, class of device, timeout
    ret = RIL_BT_StartScan(MAX_BLDEV_CNT, DEFA_CON_ID, 40);
    if(RIL_AT_SUCCESS != ret)
    {
        APP_DEBUG("BT scan failed.\r\n");
        return;
    }
    APP_DEBUG("BT scanning device...\r\n");
}




#endif

