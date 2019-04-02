#ifdef __EXAMPLE_HTTP__
#include "custom_feature_def.h"
#include "ql_type.h"
#include "ql_stdlib.h"
#include "ql_trace.h"
#include "ql_timer.h"
#include "ql_uart.h"
#include "ql_error.h"
#include "ql_gprs.h"
#include "ql_fs.h"
#include "ril.h"
#include "ril_network.h"
#include "ril_http.h"


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
* Define http parameters
****************************************************************************/
#define APN_NAME        "CMNET\0"
#define APN_USERID      ""
#define APN_PASSWD      ""

#define HTTP_REQUEST  0  // 0=http-get, 1=http-post, 2=http-file

//
// http url address
//
#if (HTTP_REQUEST == 0)
// url for http-get
#define HTTP_URL_ADDR   "http://www.quectel.com/\0"
#elif (HTTP_REQUEST == 1)
// url for http-post
#define HTTP_URL_ADDR   "http://www.quectel.com/\0"
#elif (HTTP_REQUEST == 2)
// url for http-file
#define HTTP_URL_ADDR   "http://124.74.41.170:5015/index.html\0"
#endif

// for http-post (HTTP_REQUEST=1)
#define HTTP_POST_MSG   "Message=HelloQuectel\0"

// for downloading file via http  (HTTP_REQUEST=2)
#define RAM_FILE_NAME   "RAM:http.file\0"
#define RAM_FILE_SIZE   4096



u8 arrHttpRcvBuf[10*1024]; // 10K buffer for http data
static void HTTP_Program(u8 get_post);
static void Callback_HTTP_DwnldFile(u32 dllSize, u32 cntntLen, s32 errCode);
static void Callback_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);
static void SIM_Card_State_Ind(u32 sim_stat);

void proc_main_task(s32 taskId)
{
    ST_MSG msg; 
    
    // Register & open UART port
    Ql_UART_Register(UART_PORT1, Callback_UART_Hdlr, NULL);
    Ql_UART_Open(UART_PORT1, 115200, FC_NONE);

    Ql_UART_Register(UART_PORT2, Callback_UART_Hdlr, NULL);
    Ql_UART_Open(UART_PORT2, 115200, FC_NONE);

    APP_DEBUG("\r\nOpenCPU: example for HTTP programming\r\n");
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
                    if (NW_STAT_REGISTERED == msg.param2 || NW_STAT_REGISTERED_ROAMING == msg.param2)
                    {
                        // GPRS is ready.
                        // Now, you can start to program http
                        if (0 == HTTP_REQUEST)
                        {// HTTP-get
                            HTTP_Program(0);
                        }
                        else if (1 == HTTP_REQUEST)
                        {// HTTP-post
                            HTTP_Program(1);
                        }
                        else if (2 == HTTP_REQUEST)
                        {// HTTP downlaod file
                            HTTP_Program(2);
                        }
                    }
                    break;
                }
                break;
            default:
                break;
        }
    }
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

static u32 m_rcvDataLen = 0;
static void HTTP_RcvData(u8* ptrData, u32 dataLen, void* reserved)
{
    APP_DEBUG("<-- Data coming on http, total len:%d -->\r\n", m_rcvDataLen + dataLen);
    if ((m_rcvDataLen + dataLen) <= sizeof(arrHttpRcvBuf))
    {
        Ql_memcpy((void*)(arrHttpRcvBuf + m_rcvDataLen), (const void*)ptrData, dataLen);
    } else {
        if (m_rcvDataLen < sizeof(arrHttpRcvBuf))
        {// buffer is not enough
            u32 realAcceptLen = sizeof(arrHttpRcvBuf) - m_rcvDataLen;
            Ql_memcpy((void*)(arrHttpRcvBuf + m_rcvDataLen), (const void*)ptrData, realAcceptLen);
            APP_DEBUG("<-- Rcv-buffer is not enough, discard part of data (len:%d/%d) -->\r\n", dataLen - realAcceptLen, dataLen);
        } else {// No more buffer
            APP_DEBUG("<-- No more buffer, discard data (len:%d) -->\r\n", dataLen);
        }
    }
    m_rcvDataLen += dataLen;
}

static void HTTP_Program(u8 http_action)
{
    s32 ret;
    //u32 readDataLen = 0;
    
    // Set PDP context
    ret = RIL_NW_SetGPRSContext(Ql_GPRS_GetPDPContextId());
    APP_DEBUG("<-- Set GPRS PDP context, ret=%d -->\r\n", ret);

    // Set APN
    ret = RIL_NW_SetAPN(1, APN_NAME, APN_USERID, APN_PASSWD);
    APP_DEBUG("<-- Set GPRS APN, ret=%d -->\r\n", ret);

    // Open/Activate PDP context
    ret = RIL_NW_OpenPDPContext();
    APP_DEBUG("<-- Open PDP context, ret=%d -->\r\n", ret);

    // Set HTTP server address (URL)
    ret = RIL_HTTP_SetServerURL(HTTP_URL_ADDR, Ql_strlen(HTTP_URL_ADDR));
    APP_DEBUG("<-- Set http server URL, ret=%d -->\r\n", ret);

    // Send get/post request
    m_rcvDataLen = 0;
    if (0 == http_action)
    {
        // get-request
        ret = RIL_HTTP_RequestToGet(100);   // 100s timetout
        APP_DEBUG("<-- Send get-request, ret=%d -->\r\n", ret);

        // Read response from server
        ret = RIL_HTTP_ReadResponse(120, HTTP_RcvData);
        APP_DEBUG("<-- Read http response data, ret=%d, dataLen=%d -->\r\n", ret, m_rcvDataLen);
    }
    else if (1 == http_action)
    {
        // post-request
        ret = RIL_HTTP_RequestToPost(HTTP_POST_MSG, Ql_strlen((char*)HTTP_POST_MSG));
        APP_DEBUG("<-- Send post-request, postMsg=%s, ret=%d -->\r\n", (char*)HTTP_POST_MSG, ret);

        // Read response from server
        ret = RIL_HTTP_ReadResponse(120, HTTP_RcvData);
        APP_DEBUG("<-- Read http response data, ret=%d, dataLen=%d -->\r\n", ret, m_rcvDataLen);
    }
    else if (2 == http_action){
        // get-request
        ret = RIL_HTTP_RequestToGet(100);   // 100s timetout
        APP_DEBUG("<-- Send get-request, ret=%d -->\r\n", ret);

        // Download file from http server
        ret = RIL_HTTP_DownloadFile(RAM_FILE_NAME, RAM_FILE_SIZE, Callback_HTTP_DwnldFile);
        APP_DEBUG("<-- Download file from http server, ret=%d -->\r\n", ret);
    }
    
    // Close PDP context
    ret = RIL_NW_ClosePDPContext();
    APP_DEBUG("<-- Close PDP context, ret=%d -->\r\n", ret);
 }

static void Callback_HTTP_DwnldFile(u32 dllSize, u32 cntntLen, s32 errCode)
{
    s32 iRet;
    APP_DEBUG("<-- Finished to download file from http server, dllSize=%d, cntntLen=%d, errCode=%d -->\r\n", dllSize, cntntLen, errCode);
    iRet = Ql_FS_GetSize((char*)RAM_FILE_NAME);
    APP_DEBUG("<-- Check RAM file:%s, size:%d -->\r\n", (char*)RAM_FILE_NAME, iRet);
}

static void Callback_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{
    APP_DEBUG("Callback_UART_Hdlr: port=%d, event=%d, level=%d, p=%x\r\n", port, msg, level, customizedPara);
}

#endif  //__EXAMPLE_FTP__

