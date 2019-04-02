#ifdef __EXAMPLE_FOTA_FTP__
/***************************************************************************************************
FOTA  ftp testing
****************************************************************************************************/
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
#include "fota_main.h"


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
#define APP_START_FLAG_STRING "\r\n<--x-x-x-x-x-x-FTP_FOTA_APP-x-x-x-x-x-x-->\r\n\r\n"
#define APN      "cmnet"
#define USERID   ""
#define PASSWD   ""

#define FTP_SVR_ADDR    "124.74.41.170"
#define FTP_SVR_PORT    "21"
#define FTP_SVR_PATH    "/stanley/"
#define FTP_USER_NAME   "max.tang"
#define FTP_PASSWORD    "quectel!~@"
#define FTP_FILENAME    "M66_Fotahttp_app.bin"


#define URL_LEN 512
#define READ_BUF_LEN 1024

static u8 m_URL_Buffer[URL_LEN];
static u8 m_Read_Buffer[READ_BUF_LEN];


#define FOTAUPDATE_START_PERIOD 60000
#define FOTAUPDATE_TIMEOUT_PERIOD (400*1000)

static u32 FotaUpdate_Start_TmrId =     (TIMER_ID_USER_START + 1);
static s32 FotaUpdate_TimeOut_TmrId =   (TIMER_ID_USER_START + 2);

static void SIM_Card_State_Ind(u32 sim_stat);
static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);
static void UpgradeTimeOut_Callback_Timer(u32 timerId, void* param);
static void ftp_downfile_timer(u32 timerId, void* param);// timer 启动开始下载文件


void proc_main_task(s32 taskId)
{
    s32 ret; 
    ST_MSG  msg; 
    
    // Register & open UART port
    Ql_UART_Register(UART_PORT1, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(UART_PORT1, 115200, FC_NONE);

    Ql_UART_Register(UART_PORT2, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(UART_PORT2, 115200, FC_NONE);

    APP_DEBUG(APP_START_FLAG_STRING);

    //ret = Ql_Timer_Register(FotaUpdate_Start_TmrId, ftp_downfile_timer, NULL);
    //ret = Ql_Timer_Register(FotaUpdate_TimeOut_TmrId, UpgradeTimeOut_Callback_Timer, NULL);
    //ret = Ql_Timer_Start(FotaUpdate_Start_TmrId, FOTAUPDATE_START_PERIOD, 0); //after system started up! ,1 min later start to download file via ftp	 
    //ret = Ql_Timer_Start(FotaUpdate_TimeOut_TmrId, FOTAUPDATE_TIMEOUT_PERIOD, 0); //timer out restart system.
    
    while(TRUE)
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
                        //Ql_Timer_Stop(FotaUpdate_Start_TmrId);
                        ftp_downfile_timer(FotaUpdate_Start_TmrId, NULL);
                    }
                    break;
                }
                break;
            case MSG_ID_FTP_RESULT_IND:
                APP_DEBUG("\r\n<##### Restart FTP 3s later #####>\r\n");
                Ql_Sleep(3000);
                ftp_downfile_timer(FotaUpdate_Start_TmrId, NULL);
                break;
            case MSG_ID_RESET_MODULE_REQ:
                APP_DEBUG("\r\n<##### Restart the module... #####>\r\n");
                Ql_Sleep(50);
                Ql_Reset(0);
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

static void UpgradeTimeOut_Callback_Timer(u32 timerId, void* param)
{
    APP_DEBUG("\r\n<--ERROR, too long time used upgrade !!!-->\r\n<--Reboot 3 seconds later ...-->\r\n");
    Ql_Sleep(3000);
    Ql_Reset(0);
}
static void ftp_downfile_timer(u32 timerId, void* param)// timer 启动开始下载文件
{
    s32 strLen;
    ST_GprsConfig apnCfg;
    Ql_memcpy(apnCfg.apnName,   APN, Ql_strlen(APN));
    Ql_memcpy(apnCfg.apnUserId, USERID, Ql_strlen(USERID));
    Ql_memcpy(apnCfg.apnPasswd, PASSWD, Ql_strlen(PASSWD));

    //ftp://hostname/filePath/fileName:port@username:password
    strLen = Ql_sprintf(m_URL_Buffer, "ftp://%s%s%s:%s@%s:%s",FTP_SVR_ADDR, FTP_SVR_PATH, FTP_FILENAME, FTP_SVR_PORT, FTP_USER_NAME, FTP_PASSWORD);
    //APP_DEBUG("\r\n<-- URL:%s -->\r\n",m_URL_Buffer);
    Ql_UART_Write(UART_PORT1, m_URL_Buffer, strLen);
    
    Ql_FOTA_StartUpgrade(m_URL_Buffer, &apnCfg, NULL);
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

#endif // __FILE_STRESS__


