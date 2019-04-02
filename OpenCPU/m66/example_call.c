/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Quectel Co., Ltd. 2013
*
*****************************************************************************/
/*****************************************************************************
 *
 * Filename:
 * ---------
 *   example_call.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example demonstrates how to program telephony in OpenCPU.
 *   You can change the value of the variable "callOutPhoneNum" to a specified phone number. Then 
 *   the app can automatically dial this number if GSM network is ready.
 *  
 *   All trace information will be output through DEBUG port.
 *
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_CALL__" in gcc_makefile file. And compile the 
 *     app using "make clean/new".
 *     Download image bin to module to run.
 *
 *   Operation:
 *--------------
 *    
 *     
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
#ifdef __EXAMPLE_CALL__
#include "ril.h"
#include "custom_feature_def.h"
#include "ql_stdlib.h"
#include "ql_error.h"
#include "ql_trace.h"
#include "ql_system.h"
#include "ql_memory.h"
#include "ril_telephony.h"
#include "ril_network.h"
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

static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{
     
}


static void Telephony_CallOut(void)
{
    s32 ret;
    s32 callState;
    char callOutPhoneNum[] = "15056083774\0";
    ret = RIL_Telephony_Dial(0, callOutPhoneNum, &callState);
    if (RIL_AT_SUCCESS == ret && CALL_STATE_OK == callState)
    {
        APP_DEBUG("<-- Dial \"%s\" successfully -->\r\n", callOutPhoneNum);
    } else {
        APP_DEBUG("<-- Fail to dial \"%s\", cause: %d -->\r\n", ret);
    }
}

void proc_main_task(s32 taskId)
{
    s32 ret;
    ST_MSG msg;

    // Register & open UART port
    ret = Ql_UART_Register(UART_PORT1, CallBack_UART_Hdlr, NULL);
    if (ret < QL_RET_OK)
    {
        Ql_Debug_Trace("Fail to register serial port[%d], ret=%d\r\n", UART_PORT1, ret);
    }
    ret = Ql_UART_Open(UART_PORT1, 115200, FC_NONE);
    if (ret < QL_RET_OK)
    {
        Ql_Debug_Trace("Fail to open serial port[%d], ret=%d\r\n", UART_PORT1, ret);
    }

    APP_DEBUG("OpenCPU: Call Example\r\n");

    while (TRUE) 
    {
        Ql_OS_GetMessage(&msg);
        switch (msg.message)
        {
            // Application will receive this message when OpenCPU RIL starts up.
            // Then application needs to call Ql_RIL_Initialize to launch the initialization of RIL.
        case MSG_ID_RIL_READY:
            APP_DEBUG("<-- RIL is ready -->\r\n");
            Ql_RIL_Initialize();
            //
            // After RIL initialization, developer may call RIL-related APIs in the .h files in the directory of SDK\ril\inc
            // RIL-related APIs may simplify programming, and quicken the development.
            //
            break;

            // Handle URC messages.
            // URC messages include "module init state", "CFUN state", "SIM card state(change)",
            // "GSM network state(change)", "GPRS network state(change)" and other user customized URC.
        case MSG_ID_URC_INDICATION:
            //APP_DEBUG("<-- Received URC: type: %d, -->\r\n", msg.param1);
            switch (msg.param1)
            {
                // URC for module initialization state
            case URC_SYS_INIT_STATE_IND:
                APP_DEBUG("<-- Sys Init Status %d -->\r\n", msg.param2);
                break;

                // URC for SIM card state(change)
            case URC_SIM_CARD_STATE_IND:
                if (SIM_STAT_READY == msg.param2)
                {
                    APP_DEBUG("<-- SIM card is ready -->\r\n");
                }else{
                    APP_DEBUG("<-- SIM card is not available, cause:%d -->\r\n", msg.param2);
                    /* cause: 0 = SIM card not inserted
                     *        2 = Need to input PIN code
                     *        3 = Need to input PUK code
                     *        9 = SIM card is not recognized
                     */
                }
                break;

                // URC for GSM network state(change).
                // Application receives this URC message when GSM network state changes, such as register to 
                // GSM network during booting, GSM drops down.
            case URC_GSM_NW_STATE_IND:
                if (NW_STAT_REGISTERED == msg.param2 || NW_STAT_REGISTERED_ROAMING == msg.param2)
                {
                    APP_DEBUG("<-- Module has registered to GSM network -->\r\n");

                    // Now, you can program telephony
                    Telephony_CallOut();
                }else{
                    APP_DEBUG("<-- GSM network status:%d -->\r\n", msg.param2);
                    /* status: 0 = Not registered, module not currently search a new operator
                     *         2 = Not registered, but module is currently searching a new operator
                     *         3 = Registration denied 
                     */
                }
                break;

                // URC for GPRS network state(change).
                // Application receives this URC message when GPRS network state changes, such as register to 
                // GPRS network during booting, GSM drops down.
            case URC_GPRS_NW_STATE_IND:
                if (NW_STAT_REGISTERED == msg.param2 || NW_STAT_REGISTERED_ROAMING == msg.param2)
                {
                    APP_DEBUG("<-- Module has registered to GPRS network -->\r\n");
                }else{
                    APP_DEBUG("<-- GPRS network status:%d -->\r\n", msg.param2);
                    /* status: 0 = Not registered, module not currently search a new operator
                     *         2 = Not registered, but module is currently searching a new operator
                     *         3 = Registration denied 
                     */
                }
                break;
                
            case URC_CFUN_STATE_IND:
                APP_DEBUG("<-- CFUN Status:%d -->\r\n", msg.param2);
                break;

                // URC for incoming call indication.
                // Application receives this URC message when a new call is incoming.
            case URC_COMING_CALL_IND:
                {
                    // Here, the program demonstrates how to hang up the incoming call and call back.
                    s32 callState;
                    ST_ComingCall* pComingCall = (ST_ComingCall*)msg.param2;
                    APP_DEBUG("<-- Coming call, number:%s, type:%d -->\r\n", pComingCall->phoneNumber, pComingCall->type);
                    ret = RIL_Telephony_Hangup();
                    if (RIL_AT_SUCCESS == ret)
                    {
                        APP_DEBUG("<-- Hang up the coming call -->\r\n");
                    } else {
                        APP_DEBUG("<-- Fail to hang up the coming call -->\r\n");
                    }
                    ret = RIL_Telephony_Dial(0, pComingCall->phoneNumber, &callState);
                    if (RIL_AT_SUCCESS == ret && CALL_STATE_OK == callState)
                    {
                        APP_DEBUG("<-- Calling back... -->\r\n");
                    }
                    else if (RIL_AT_SUCCESS == ret)
                    {
                        APP_DEBUG("<-- Fail to call back, callState=%d-->\r\n", callState);
                    } else {
                        APP_DEBUG("<--RIL_Telephony_Dial() failed , ret =%d  -->",ret);
                    }
                    break;
                }
            
                // URC for call status indication.
                // Application receives this URC message when a call is hung up, or exceptionaly broken,
                // or the line is busy when calling out.
            case URC_CALL_STATE_IND:
                switch (msg.param2)
                {
                case CALL_STATE_BUSY:
                    APP_DEBUG("<-- The number you dialed is busy now -->\r\n");
                    break;
                case CALL_STATE_NO_ANSWER:
                    APP_DEBUG("<-- The number you dialed has no answer -->\r\n");
                    break;
                case CALL_STATE_NO_CARRIER:
                    APP_DEBUG("<-- The number you dialed cannot reach -->\r\n");
                    break;
                case CALL_STATE_NO_DIALTONE:
                    APP_DEBUG("<-- No Dial tone -->\r\n");
                    break;
                default:
                    break;
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

#endif //__EXAMPLE_CALL__

