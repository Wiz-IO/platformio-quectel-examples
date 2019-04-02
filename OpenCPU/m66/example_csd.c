/*******************************************************
* Example: 
*   CSD data transmission.
*
* Description:
*   This example demonstrates how to establish a AT command pipe
*   between application and core. So, this example only
*   receives AT command from MAIN port and pass it to
*   CORE, or prints responses from CORE to MAIN port.
*
* Usage:
*   Input AT command terminated by '\n' or '\r\n' through
*   UART1(MAIN) port, and the response will be returned
*   through this port.
********************************************************/
#ifdef __EXAMPLE_CSD__
#include "custom_feature_def.h"
#include "ril.h"
#include "ril_util.h"
#include "ril_network.h"
#include "ril_telephony.h"
#include "ql_stdlib.h"
#include "ql_error.h"
#include "ql_trace.h"
#include "ql_uart.h"
#include "ql_system.h"
#include "ql_uart.h"

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


// ---- Definitions for customization parameters ----
#define IS_CSD_CALLER   (TRUE)     // TRUE=call out, FALSE=call in
#define CSD_DIAL_NUMBER_A "18256056827\0"
#define CSD_DIAL_NUMBER_B "18256056827\0"



#if (IS_CSD_CALLER == TRUE)
#define CSD_DIAL_NUMBER CSD_DIAL_NUMBER_B       // A --CSD call--> B
#else
#define CSD_DIAL_NUMBER CSD_DIAL_NUMBER_A       // B --CSD call--> A
#endif

static bool m_bCsdConnected = FALSE;

#define SERIAL_RX_BUFFER_LEN  2560

// Define the virtual port and the receive data buffer
static Enum_SerialPort m_myVirtualPort = VIRTUAL_PORT2;
static u8 m_RxBuf_VirtualPort[SERIAL_RX_BUFFER_LEN];

// Define the UART port and the receive data buffer
static Enum_SerialPort m_myUartPort  = UART_PORT1;
static u8 m_RxBuf_UartPort[SERIAL_RX_BUFFER_LEN];

static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);
static void InitializeSerialPort(void);
static void SIM_Card_State_Ind(u32 sim_stat);

static char m_strTemp[200];

void CSD_Establish(char* csdNumber)
{
    char strCsdDialAT[30];
    s32 len = Ql_sprintf(strCsdDialAT, "ATD%s\r\n", csdNumber);
    APP_DEBUG("<-- Dialing CSD number: %s -->\r\n", csdNumber);
    Ql_UART_Write(m_myVirtualPort, strCsdDialAT, len);
}
void CSD_Answer(void)
{
    Ql_UART_Write(m_myVirtualPort, "ATA\n", 4);
}
void CSD_Disconnect(void)
{
    // -------- Received END command, exit from CSD data mode -------
    APP_DEBUG("<-- Exit from CSD data mode -->\r\n");
    Ql_UART_SendEscap(m_myVirtualPort);

    // -------- Close CSD connection -------
    APP_DEBUG("<-- Close CSD connection -->\r\n");
    Ql_UART_Write(m_myVirtualPort, "ATH\n", 4);
}

void proc_main_task(s32 taskId)
{
    s32 ret;
    ST_MSG msg;

    

    // Init serial port
    InitializeSerialPort();

    APP_DEBUG("\r\n<== OpenCPU: CSD Demo ==>\r\n");

    // START MESSAGE LOOP OF THIS TASK
    while(TRUE)
    {
        Ql_OS_GetMessage(&msg);
        switch(msg.message)
        {
        case MSG_ID_RIL_READY:
            APP_DEBUG("<-- RIL is ready -->\r\n");
            Ql_RIL_Initialize();
            break;
        case MSG_ID_URC_INDICATION:
            //APP_DEBUG("<-- Received URC: type: %d, -->\r\n", msg.param1);
            switch (msg.param1)
            {
            case URC_CFUN_STATE_IND:
                APP_DEBUG("<-- CFUN Status:%d -->\r\n", msg.param2);
                break;
            case URC_SYS_INIT_STATE_IND:
                APP_DEBUG("<-- Sys Init Status %d -->\r\n", msg.param2);
                break;
            case URC_SIM_CARD_STATE_IND:
                SIM_Card_State_Ind(msg.param2);
                break;
            case URC_GSM_NW_STATE_IND:
                APP_DEBUG("<-- GSM Network Status:%d -->\r\n", msg.param2);
                
                // If this program is host, then start to CSD dial
                if (NW_STAT_REGISTERED == msg.param2 && IS_CSD_CALLER)
                {// Start to CSD dial
                    CSD_Establish(CSD_DIAL_NUMBER);
                }
                break;
            case URC_CALL_STATE_IND:
                {
                    APP_DEBUG("<--call  Status:%d -->\r\n", msg.param2);
                    if(CALL_STATE_NO_CARRIER == msg.param2)
                    {
                        CSD_Disconnect();
                    } 
                }
                break;
            default:
                APP_DEBUG("<-- Other URC: type=%d\r\n", msg.param1);
                break;
            }
            break;
        default:
            break;
        }
    }
}

static void InitializeSerialPort(void)
{
    s32 ret;

     // Register & open UART port
    // UART port
    ret = Ql_UART_Register(m_myUartPort, CallBack_UART_Hdlr, NULL);
    if (ret < QL_RET_OK)
    {
        Ql_Debug_Trace("Fail to register serial port[%d], ret=%d\r\n", m_myUartPort, ret);
    }
    ret = Ql_UART_Open(m_myUartPort, 115200, FC_NONE);
    if (ret < QL_RET_OK)
    {
        Ql_Debug_Trace("Fail to open serial port[%d], ret=%d\r\n", m_myUartPort, ret);
    }
     Ql_UART_Write(m_myUartPort, "\r\nRDY\r\n", Ql_strlen("\r\nRDY\r\n")); 

    // Register & open virtual serial port
    ret = Ql_UART_Register(m_myVirtualPort, CallBack_UART_Hdlr, NULL);
    if (ret < QL_RET_OK)
    {
        APP_DEBUG("Fail to register serial port[%d], ret=%d\r\n", m_myVirtualPort, ret);
    }
    ret = Ql_UART_Open(m_myVirtualPort, 115200, FC_NONE);
    if (ret < QL_RET_OK)
    {
        APP_DEBUG("Fail to open serial port[%d], ret=%d\r\n", m_myVirtualPort, ret);
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
    s32 ret;
    APP_DEBUG("CallBack_UART_Hdlr: port=%d, event=%d, level=%d, p=%x\r\n", port, msg, level, customizedPara);
    switch (msg)
    {
    case EVENT_UART_READY_TO_READ:
        {
            if (m_myUartPort == port)
            {
                // Write the data from UART port to virtual serial port
                s32 totalBytes = ReadSerialPort(port, m_RxBuf_UartPort, sizeof(m_RxBuf_UartPort));
                APP_DEBUG("m_RxBuf_UartPort:%s\r\n",m_RxBuf_UartPort );
                if (totalBytes > 0)
                {
                    if (m_bCsdConnected && Ql_StrPrefixMatch(m_RxBuf_UartPort, "+++"))
                    {
                        // -------- Disconnect CSD  -------
                        CSD_Disconnect();
                        m_bCsdConnected = FALSE;
                    }else{
                        // No CSD connection, then transparently transmit the data from UART port to virtual serial port
                        // And make sure all data is sent out
                        s32 realSentByte = 0;
                        do 
                        {
                            ret = Ql_UART_Write(m_myVirtualPort, m_RxBuf_UartPort + realSentByte, totalBytes - realSentByte);
                            if (ret > 0 && ret < (totalBytes - realSentByte))
                            {
                                Ql_Sleep(100);
                            }
                            realSentByte += ret;
                                APP_DEBUG("CallBack_UART_Hdlr ret:%d realSentByte:%d \r\n",ret,realSentByte );
                                
                        } while (realSentByte != totalBytes);
                    }
                }
            }
            else if (m_myVirtualPort == port)
            {
                // Write the data from virtual serial port to UART port
                s32 totalBytes = ReadSerialPort(port, m_RxBuf_VirtualPort, sizeof(m_RxBuf_VirtualPort));
                APP_DEBUG(" m_RxBuf_VirtualPort:%s\r\n",m_RxBuf_VirtualPort );
                if (totalBytes > 0)
                {
                    if (Ql_strstr(m_RxBuf_VirtualPort, "\r\nRING\r\n"))
                    {
                        // -------- Received RING, answer CSD call -------
                        CSD_Answer();
                    } 
                    else if (Ql_strstr(m_RxBuf_VirtualPort, "CONNECT"))
                    {
                        // -------- Received CONNECT 9600, CSD connected -------
                        m_bCsdConnected = TRUE;
                    }
                    Ql_UART_Write(m_myUartPort, m_RxBuf_VirtualPort, totalBytes);
                }
            }
            break;
        }
    default:
        break;
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
#endif // __EXAMPLE_CSD__
