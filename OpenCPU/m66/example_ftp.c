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
 *   example_ftp.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example demonstrates how to program ftp in OpenCPU.
 *
 *   All debug information will be output through DEBUG port.
 *
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_FTP_" in gcc_makefile file. And compile the 
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
#ifdef __EXAMPLE_FTP__
#include "custom_feature_def.h"
#include "ql_type.h"
#include "ql_stdlib.h"
#include "ql_trace.h"
#include "ql_timer.h"
#include "ql_uart.h"
#include "ql_error.h"
#include "ril.h"
#include "ril_network.h"
#include "ql_gprs.h"
#include "ql_fs.h"

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


/****************************************************************************
* Define local param
****************************************************************************/
#define APN      "cmnet\0"
#define USERID   ""
#define PASSWD   ""
#define FTP_SVR_ADDR    "124.74.41.170\0"
#define FTP_SVR_PORT    (u32)(21)
#define FTP_SVR_PATH    "/stanley/\0"
#define FTP_USER_NAME   "max.tang\0"
#define FTP_PASSWORD    "quectel!~@\0"
#define FTP_FILENAME_UL    "file.u\0"
#define FTP_FILENAME_DL    "file.d\0"

#define FTP_CONNECT_ATTEMPTS        (5)     // max 5 times for attempt to connect, or restart module

static void SIM_Card_State_Ind(u32 sim_stat);
static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);
static s32  FTP_Program_Upload(void);
static s32  FTP_Program_Download(void);

void proc_main_task(s32 taskId)
{
    s32 ret; 
    ST_MSG  msg; 
    
    // Register & open UART port
    Ql_UART_Register(UART_PORT1, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(UART_PORT1, 115200, FC_NONE);

    Ql_UART_Register(UART_PORT2, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(UART_PORT2, 115200, FC_NONE);

    APP_DEBUG("\r\nOpenCPU: example for FPT programming\r\n");
    while (1)
    {
        Ql_OS_GetMessage(&msg);
        switch(msg.message)
        {
            case MSG_ID_RIL_READY:
                APP_DEBUG("<-- RIL is ready -->\r\n");
                Ql_RIL_Initialize();
            case MSG_ID_URC_INDICATION:
                switch (msg.param1)
                {
                case URC_SYS_INIT_STATE_IND:
                    APP_DEBUG("<-- Sys Init Status %d -->\r\n", msg.param2);
                    break;
                case URC_CFUN_STATE_IND:
                    APP_DEBUG("<-- CFUN Status:%d -->\r\n", msg.param2);
                    break;
                case URC_SIM_CARD_STATE_IND:
                    SIM_Card_State_Ind(msg.param2);
                    break;
                case URC_GSM_NW_STATE_IND:
                    APP_DEBUG("<-- GSM Network Status:%d -->\r\n", msg.param2);
                    break;
                case URC_GPRS_NW_STATE_IND:
                    APP_DEBUG("<-- GPRS Network Status:%d -->\r\n", msg.param2);
                    if (NW_STAT_REGISTERED == msg.param2)
                    {
                        // Now, you can upload/download via FTP
                    #if 1
                        FTP_Program_Upload();
                    #else
                        FTP_Program_Download();
                    #endif
                    }
                    break;
                }
                break;
            default:
                break;
        }
    }
}

static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{
    //APP_DEBUG("CallBack_UART_Hdlr: port=%d, event=%d, level=%d, p=%x\r\n", port, msg, level, customizedPara);
}

static void SIM_Card_State_Ind(u32 sim_stat)
{
    switch (sim_stat)
    {
    case SIM_STAT_NOT_INSERTED:
        APP_DEBUG("<-- SIM Card Status: NOT INSERTED -->\r\n");
    	break;
    case SIM_STAT_READY:
        APP_DEBUG("<-- SIM Card Status: READY -->\r\n");
        break;
    case SIM_STAT_PIN_REQ:
        APP_DEBUG("<-- SIM Card Status: SIM PIN -->\r\n");
        break;
    case SIM_STAT_PUK_REQ:
        APP_DEBUG("<-- SIM Card Status: SIM PUK -->\r\n");
        break;
    case SIM_STAT_PH_PIN_REQ:
        APP_DEBUG("<-- SIM Card Status: PH-SIM PIN -->\r\n");
        break;
    case SIM_STAT_PH_PUK_REQ:
        APP_DEBUG("<-- SIM Card Status: PH-SIM PUK -->\r\n");
        break;
    case SIM_STAT_PIN2_REQ:
        APP_DEBUG("<-- SIM Card Status: SIM PIN2 -->\r\n");
        break;
    case SIM_STAT_PUK2_REQ:
        APP_DEBUG("<-- SIM Card Status: SIM PUK2 -->\r\n");
        break;
    case SIM_STAT_BUSY:
        APP_DEBUG("<-- SIM Card Status: BUSY -->\r\n");
        break;
    case SIM_STAT_NOT_READY:
        APP_DEBUG("<-- SIM Card Status: NOT READY -->\r\n");
        break;
    default:
        APP_DEBUG("<-- SIM Card Status: ERROR -->\r\n");
        break;
    }
}


static void FTP_Callback_OnUpDown(s32 result, s32 size)
{
    s32 ret;
    if (result)
    {
        APP_DEBUG("<-- Succeed in uploading/downloading image bin via FTP, file size:%d -->\r\n", size);
    }else{
        APP_DEBUG("<-- Failed to upload/download file to FTP server, cause=%d -->\r\n", result);
    }
    ret = RIL_FTP_QFTPCLOSE();
    APP_DEBUG("<-- FTP close connection, ret=%d -->\r\n", ret);

    APP_DEBUG("<-- Deactivating PDP context -->\r\n");
    ret = RIL_FTP_QIDEACT();
    APP_DEBUG("<-- Released PDP context, ret=%d -->\r\n", ret);
}
static s32 FTP_Program_Upload(void)
{
    s32 ret = 0;
    u8  attempts = 0;
    s32 fileSize = 0;
    
    // Set pdp context
    ret = RIL_NW_SetGPRSContext(Ql_GPRS_GetPDPContextId());
    APP_DEBUG("<-- Set GPRS PDP context, ret=%d -->\r\n", ret);

    // Set APN
    ret = RIL_NW_SetAPN(1, APN, USERID, PASSWD);
    APP_DEBUG("<-- Set GPRS APN, ret=%d -->\r\n", ret);

    // Open FTP connection
    do
    {
        ret = RIL_FTP_QFTPOPEN(FTP_SVR_ADDR, FTP_SVR_PORT, FTP_USER_NAME, FTP_PASSWORD, 1);
        APP_DEBUG("<-- FTP open connection, ret=%d -->\r\n", ret);
        if (RIL_AT_SUCCESS == ret)
        {
            attempts = 0;
            APP_DEBUG("<-- Open ftp connection -->\r\n");
            break;
        }
        attempts++;
        APP_DEBUG("<-- Retry to open FTP 2s later -->\r\n");
        Ql_Sleep(2000);
    } while (attempts < FTP_CONNECT_ATTEMPTS);
    if (FTP_CONNECT_ATTEMPTS == attempts)
    {
        APP_DEBUG("<-- Fail to open ftp connection for 5 times -->\r\n");

        // Reset the module
        APP_DEBUG("\r\n<##### Restart the module... #####>\r\n");
        Ql_Sleep(50);
        Ql_Reset(0);
        return -99;
    }

    // Set local storage
    ret = RIL_FTP_QFTPCFG(4, (u8*)"UFS"); 
    APP_DEBUG("<-- Set local storage, ret=%d -->\r\n", ret);

    // Set patch on server
    ret = RIL_FTP_QFTPPATH(FTP_SVR_PATH);   
    APP_DEBUG("<-- Set remote path, ret=%d -->\r\n", ret);

    // Upload file to ftp-server
    fileSize = Ql_FS_GetSize((u8 *)FTP_FILENAME_UL);
    APP_DEBUG("<-- Local file size: %d -->\r\n", fileSize);
    if (fileSize <= QL_RET_OK)
    {
        APP_DEBUG("<-- Failed to get local file size -->\r\n");
        
        ret = RIL_FTP_QFTPCLOSE();
        APP_DEBUG("<-- FTP close connection, ret=%d -->\r\n", ret);

        ret = RIL_FTP_QIDEACT();
        APP_DEBUG("<-- Release PDP context, ret=%d -->\r\n", ret);
        return -2;
    }
    ret = RIL_FTP_QFTPPUT((u8 *)FTP_FILENAME_UL, (u32)fileSize, 600, FTP_Callback_OnUpDown);
    if (ret < 0)
    {
        APP_DEBUG("<-- Failed to upload, cause=%d -->\r\n", ret);
        
        ret = RIL_FTP_QFTPCLOSE();
        APP_DEBUG("<-- FTP close connection, ret=%d -->\r\n", ret);

        ret = RIL_FTP_QIDEACT();
        APP_DEBUG("<-- Release PDP context, ret=%d -->\r\n", ret);
        return -3;
    }
    APP_DEBUG("<-- Uploading file, please wait... -->\r\n");
    return QL_RET_OK;
}

static s32 FTP_Program_Download(void)
{
    s32 ret = 0;
    u8  attempts = 0;
    s32 fileSize = 0;
    
    // Set pdp context
    ret = RIL_NW_SetGPRSContext(Ql_GPRS_GetPDPContextId());
    APP_DEBUG("<-- Set GPRS PDP context, ret=%d -->\r\n", ret);

    // Set APN
    ret = RIL_NW_SetAPN(1, APN, USERID, PASSWD);
    APP_DEBUG("<-- Set GPRS APN, ret=%d -->\r\n", ret);

    // Open FTP connection
    do
    {
        ret = RIL_FTP_QFTPOPEN(FTP_SVR_ADDR, FTP_SVR_PORT, FTP_USER_NAME, FTP_PASSWORD, 1);
        APP_DEBUG("<-- FTP open connection, ret=%d -->\r\n", ret);
        if (RIL_AT_SUCCESS == ret)
        {
            attempts = 0;
            APP_DEBUG("<-- Open ftp connection -->\r\n");
            break;
        }
        attempts++;
        APP_DEBUG("<-- Retry to open FTP 2s later -->\r\n");
        Ql_Sleep(2000);
    } while (attempts < FTP_CONNECT_ATTEMPTS);
    if (FTP_CONNECT_ATTEMPTS == attempts)
    {
        APP_DEBUG("<-- Fail to open ftp connection for 5 times -->\r\n");

        // Reset the module
        APP_DEBUG("\r\n<##### Restart the module... #####>\r\n");
        Ql_Sleep(50);
        Ql_Reset(0);
        return -99;
    }

    // Set local storage
    ret = RIL_FTP_QFTPCFG(4, (u8*)"UFS"); 
    APP_DEBUG("<-- Set local storage, ret=%d -->\r\n", ret);

    // Set patch on server
    ret = RIL_FTP_QFTPPATH(FTP_SVR_PATH);   
    APP_DEBUG("<-- Set remote path, ret=%d -->\r\n", ret);

    // Download file from ftp-server
    ret = RIL_FTP_QFTPGET((u8 *)FTP_FILENAME_DL, 0, FTP_Callback_OnUpDown);
    if (ret < 0)
    {
        APP_DEBUG("<-- Failed to download, cause=%d -->\r\n", ret);
        
        ret = RIL_FTP_QFTPCLOSE();
        APP_DEBUG("<-- FTP close connection, ret=%d -->\r\n", ret);

        ret = RIL_FTP_QIDEACT();
        APP_DEBUG("<-- Release PDP context, ret=%d -->\r\n", ret);
        return -3;
    }
    APP_DEBUG("<-- Downloading file, please wait... -->\r\n");
    return QL_RET_OK;
}

#endif // __EXAMPLE_FTP__

