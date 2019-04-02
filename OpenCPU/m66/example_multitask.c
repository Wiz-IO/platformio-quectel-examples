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
 *   example_multitask.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example demonstrates how to use multitask function with APIs in OpenCPU.
 *   Input the specified command through any uart port and the result will be 
 *   output through the debug port.
 *
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_MULTITASK__" in gcc_makefile file. And compile the 
 *     app using "make clean/new".
 *     Download image bin to module to run.
 * 
 *   Operation:
 *            
 *     If input "Ql_OS_SendMessage=", that will send message to others tasks.
 *     If input "TestMutex", that will learn how to use Mutex.
 *     If input "TestSemaphore=", that will learn how to use Semaphore.
 *     If input "Ql_SetLastErrorCode=", that will learn how to set errorcode.
 *     If input "Ql_GetLastErrorCode", that will get the current errorcode.
 *     If input "Ql_OS_GetAllTaskPriority", that will get the current priority of all subtask.
 *     If input "Ql_OS_GetAllTaskLeftStackSize", that will get the left number of bytes 
 *     in the current task stack.
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
#ifdef __EXAMPLE_MULTITASK__
#include "ql_type.h"
#include "ql_trace.h"
#include "ql_uart.h"
#include "ql_system.h"
#include "ql_stdlib.h"
#include "ql_error.h"


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



#define MAX_TASK_NUM                    11
#define MSG_ID_USER_DATA                MSG_ID_USER_START+0x100
#define MSG_ID_MUTEX_TEST               MSG_ID_USER_START+0x101
#define MSG_ID_SEMAPHORE_TEST           MSG_ID_USER_START+0x102
#define MSG_ID_GET_ALL_TASK_PRIORITY    MSG_ID_USER_START+0x103
#define MSG_ID_GET_ALL_TASK_REMAINSTACK MSG_ID_USER_START+0x104



static char textBuf[128];  
static int  s_iMutexId = 0;
static int  s_iSemaphoreId = 0;
static int  s_iSemMutex = 0;
static int  s_iTestSemNum = 3;
static int  s_iPassTask = 1;
    


static u8 m_Read_Buffer[1024];

static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara);

void MutextTest(int iTaskId)  //Two task Run this function at the same time
{

    APP_DEBUG("<---I am Task %d--->\r\n", iTaskId);
    Ql_OS_TakeMutex(s_iMutexId);
    APP_DEBUG("<----I am Task %d---->\r\n", iTaskId);   //Another Caller prints this sentence after 5 seconds
    Ql_Sleep(3000);                                                             
    APP_DEBUG("<--(TaskId=%d)Do not reboot with calling Ql_sleep-->\r\n", iTaskId);
    Ql_OS_GiveMutex(s_iMutexId);
}

void SemaphoreTest(int iTaskId)
{
    int iRet = -1;
    
    APP_DEBUG("\r\n<--I am Task %d-->", iTaskId);
    
    iRet = Ql_OS_TakeSemaphore(s_iSemaphoreId, TRUE);//TRUE or FLASE indicate the task should wait infinitely or return immediately.
    APP_DEBUG("\r\n<--=========Ql_osTakeSemaphore(%d, TRUE) = %d-->", s_iSemaphoreId, iRet);   //Flag Sentence

    Ql_OS_TakeMutex(s_iSemMutex);
    s_iTestSemNum--;
    Ql_OS_GiveMutex(s_iSemMutex);
    
    Ql_Sleep(3000);     //The last task  print Flag Sentence delaying 5 seconds;  
    Ql_OS_GiveSemaphore(s_iSemaphoreId);
    s_iTestSemNum++;
    APP_DEBUG("\r\n<--=========Task[%d]: s_iTestSemNum = %d-->", iTaskId, s_iTestSemNum);
}

void SemaphoreTest1(int iTaskId)
{
    int iRet = -1;
    
    APP_DEBUG("\r\n<--I am Task %d-->", iTaskId);

    while(1)
    {
        iRet = Ql_OS_TakeSemaphore(s_iSemaphoreId, FALSE);
        if(OS_SUCCESS == iRet)
        {
            APP_DEBUG("\r\n<--=========Task[%d]: take semaphore OK!-->", iTaskId);
            break;
        }
        else
        {

          //  APP_DEBUG("\r\n<--*****Task[%d]: take semaphore FAILED(iRet = %d)!-->", iTaskId, iRet);
            Ql_Sleep(300);
        }
    }

    Ql_OS_TakeMutex(s_iSemMutex);
    s_iTestSemNum--;
    Ql_OS_GiveMutex(s_iSemMutex);
    
    Ql_Sleep(5000);     //The last task  print Flag Sentence delaying 5 seconds;  
    Ql_OS_GiveSemaphore(s_iSemaphoreId);
    s_iTestSemNum++;
    APP_DEBUG("\r\n<--Task[%d]: s_iTestSemNum = %d-->", iTaskId, s_iTestSemNum);
}

void GetCurrentTaskPriority(int iTaskId)
{
    int iRet = 0;
    iRet = Ql_OS_GetCurrentTaskPriority();
    APP_DEBUG("\r\n<--Task[%d]: priority=%d-->", iTaskId, iRet);
}

void GetCurrenTaskRemainStackSize(int iTaskId)
{
    int iRet = 0;
    iRet = Ql_OS_GetCurrenTaskLeftStackSize();
    APP_DEBUG("\r\n<--Task[%d]: Task Remain Stack Size =%d-->", iTaskId, iRet);
}

void SendEvent2AllSubTask(u32 msgId,u32 iData1, u32 iData2)
{
    int iTmp = 0;
    int iRet = 0;
    
    for(iTmp=subtask1_id; iTmp<MAX_TASK_NUM+1; iTmp++)
    {

        iRet = Ql_OS_SendMessage(iTmp,msgId,iData1, iData2);
#if 0      
        if(iRet <0)
        {
           APP_DEBUG(textBuf, "<--failed!!,Ql_OS_SendMessage(%d,%d,%d,%d) fail, ret=%d-->\r\n", iTmp,msgId,iData1, iData2, iRet); 
        }
        APP_DEBUG(textBuf, "<--Ql_OS_SendMessage(%d, %d, %d, %d)=%d-->\r\n", iTmp,msgId,iData1, iData2, iRet);
#endif
    }
}

/**************************************************************
* main task
***************************************************************/
void proc_main_task(s32 taskId)
{
    ST_MSG msg;

    // Register & open UART port
    Ql_UART_Register(UART_PORT1, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(UART_PORT1, 115200, FC_NONE);

    Ql_UART_Register(UART_PORT2, CallBack_UART_Hdlr, NULL);
    Ql_UART_Open(UART_PORT2, 115200, FC_NONE);

    APP_DEBUG("\r\n<--OpenCPU: multitask TEST!-->\r\n");  

    while (1)
    {
         Ql_OS_GetMessage(&msg);
        switch(msg.message)
        {
        case 0:
            break;
        default:
            break;
        }        
    }
}

static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{
    //APP_DEBUG("CallBack_UART_Hdlr: port=%d, event=%d, level=%d, p=%x\r\n", port, msg, level, customizedPara);
    u32 iTmp,iNum,rdLen=0;
    s32 iRet;
    char *pData=NULL;
    char *p=NULL;
    char *q=NULL;
    switch (msg)
    {
        case EVENT_UART_READY_TO_READ:
        {
            Ql_memset(m_Read_Buffer, 0x0, sizeof(m_Read_Buffer));
            rdLen = Ql_UART_Read(port, m_Read_Buffer, sizeof(m_Read_Buffer));
            pData = m_Read_Buffer;

            if(NULL != (p = Ql_strstr(pData, "Ql_OS_SendMessage")))
            {                                          //Ql_osSendEvent=1,2
                p = Ql_strstr(p, "=");
                q = Ql_strstr(p, ",");
                if((p == NULL) || (q == NULL))
                {
                    APP_DEBUG("\r\n<-- Ql_OS_SendMessage: Error parameter!-->\r\n");
                    break;
                }
                p++; (*q) = 0; q++;

                iNum = Ql_atoi(p);
                iTmp = Ql_atoi(q);

                s_iPassTask = subtask1_id;
                iRet = Ql_OS_SendMessage(s_iPassTask,MSG_ID_USER_DATA,iNum, iTmp);
                if(iRet <0)
                {
                    APP_DEBUG("\r\n<--failed!!, Ql_OS_SendMessage(1, %d, %d) fail,  ret=%d-->\r\n", iNum, iTmp, iRet);
                }
                APP_DEBUG("\r\n<--Ql_OS_SendMessage(%d, %d, %d) ret=%d-->\r\n",MSG_ID_USER_DATA,iNum, iTmp, iRet);
                break;
            }
            else if(NULL != (p = Ql_strstr(pData, "TestMutex")))
            {
                s_iMutexId = Ql_OS_CreateMutex("MyMutex");
                APP_DEBUG("<--Ql_osCreateMutex(\"MyMutex\")=%08x -->\r\n", s_iMutexId);
                SendEvent2AllSubTask(MSG_ID_MUTEX_TEST,0,0);
                break;
            }
            else if(NULL != (p = Ql_strstr(pData, "TestSemaphore")))
            {
                p = Ql_strstr(p, "=");
                if(p == NULL)
                {
                    APP_DEBUG("\r\n<--TestSemaphore: parameter ERROR!-->\r\n");
                    break;
                }
                p++;
                s_iTestSemNum = Ql_atoi(p);
                s_iSemaphoreId = Ql_OS_CreateSemaphore("MySemaphore", s_iTestSemNum);
                APP_DEBUG("\r\n<--Ql_osCreateSemaphore(\"MySemaphore\", %d)=%08x -->\r\n", s_iTestSemNum, s_iSemaphoreId);

                s_iSemMutex= Ql_OS_CreateMutex("SemMutex");
                APP_DEBUG("\r\n<--Ql_osCreateMutex(\"SemMutex\")=%08x -->\r\n", s_iSemMutex);
                SendEvent2AllSubTask(MSG_ID_SEMAPHORE_TEST,0,0);
                break;
            }
            else if(NULL != (p = Ql_strstr(pData, "Ql_SetLastErrorCode")))
            {                                    //Ql_SetLastErrorCode=ErrorCode
                p = Ql_strstr(p, "=");
                if(p == NULL)
                {
                    APP_DEBUG("\r\n<--Ql_SetLastErrorCode: parameter ERROR!-->\r\n");
                    break;
                }

                p++;
                iNum = Ql_atoi(p);
                iRet = Ql_SetLastErrorCode(iNum);
                APP_DEBUG("\r\n<--Ql_SetLastErrorCode(%d)=%d-->\r\n", iNum, iRet);
                break;
            }
            else if(NULL != (p = Ql_strstr(pData, "Ql_GetLastErrorCode")))
            {
                iNum = Ql_GetLastErrorCode();
                APP_DEBUG("\r\n<--Ql_GetLastErrorCode: ErrorCode=%d-->\r\n", iNum);
                break;
            }
            else if(NULL != (p = Ql_strstr(pData, "Ql_OS_GetAllTaskPriority")))
            {
                SendEvent2AllSubTask(MSG_ID_GET_ALL_TASK_PRIORITY,0,0);
                break;
            }
            else if(NULL != (p = Ql_strstr(pData, "Ql_OS_GetAllTaskLeftStackSize")))
            {
                SendEvent2AllSubTask(MSG_ID_GET_ALL_TASK_REMAINSTACK,0,0);
                break;
            }
            APP_DEBUG("\r\n<--Not found this command, please check you command-->\r\n");
            break;
        }
        default:
            break;
            
    }
}


/**************************************************************
* the 1st sub task
***************************************************************/
void proc_subtask1(s32 TaskId)
{
    bool keepGoing = TRUE;
    ST_MSG subtask1_msg;
    
    Ql_Debug_Trace("<--multitask: example_task1_entry-->\r\n");
    while(keepGoing)
    {    
        Ql_OS_GetMessage(&subtask1_msg);
        switch(subtask1_msg.message)
        {
            case MSG_ID_USER_DATA:
            {
               APP_DEBUG("\r\n<--Sub task 1 recv MSG: SrcId=%d,MsgID=%d Data1=%d, Data2=%d-->\r\n", \
                        subtask1_msg.srcTaskId, \
                        subtask1_msg.message,\
                        subtask1_msg.param1, \
                        subtask1_msg.param2);
                    if(s_iPassTask == MAX_TASK_NUM)
                    {
                        s_iPassTask = main_task_id;
                    }
                    else
                    {
                        s_iPassTask++;
                    }
                    Ql_OS_SendMessage(s_iPassTask, MSG_ID_USER_DATA,subtask1_msg.param1, subtask1_msg.param2);
                
                break;
            }
            case MSG_ID_MUTEX_TEST:
            {
                MutextTest(TaskId);
                break;
            }
            case MSG_ID_SEMAPHORE_TEST:
            {
                SemaphoreTest1(TaskId);  
                break;
            }
            case MSG_ID_GET_ALL_TASK_PRIORITY:
            {
                GetCurrentTaskPriority(TaskId);
                break;
            }
            case MSG_ID_GET_ALL_TASK_REMAINSTACK:
            {
                GetCurrenTaskRemainStackSize(TaskId);
                break;
            } 
            default:
                break;
        }
    }    
}


/**************************************************************
* the 2nd sub task
***************************************************************/
void proc_subtask2(s32 TaskId)
{
    bool keepGoing = TRUE;
    ST_MSG subtask2_msg;

    Ql_Debug_Trace("<--multitask: example_task2_entry-->\r\n");

    while(keepGoing)
    {    
        Ql_OS_GetMessage(&subtask2_msg);
        switch(subtask2_msg.message)
        {
            case MSG_ID_USER_DATA:
            {
               APP_DEBUG("\r\n<--Sub task 2 recv MSG: SrcId=%d,MsgID=%d Data1=%d, Data2=%d-->\r\n", \
                        subtask2_msg.srcTaskId, \
                        subtask2_msg.message,\
                        subtask2_msg.param1, \
                        subtask2_msg.param2);
                    if(s_iPassTask == MAX_TASK_NUM)
                    {
                        s_iPassTask = main_task_id;
                    }
                    else
                    {
                        s_iPassTask++;
                    }
                    Ql_OS_SendMessage(s_iPassTask, MSG_ID_USER_DATA,subtask2_msg.param1, subtask2_msg.param2);
                
                break;
            }
            case MSG_ID_MUTEX_TEST:
            {
                MutextTest(TaskId);
                break;
            }
            case MSG_ID_SEMAPHORE_TEST:
            {
                SemaphoreTest1(TaskId);  
                break;
            }
            case MSG_ID_GET_ALL_TASK_PRIORITY:
            {
                GetCurrentTaskPriority(TaskId);
                break;
            }
            case MSG_ID_GET_ALL_TASK_REMAINSTACK:
            {
                GetCurrenTaskRemainStackSize(TaskId);
                break;
            } 
            default:
                break;
        }
    }
}


/**************************************************************
* the 3nd sub task
***************************************************************/
void proc_subtask3(s32 TaskId)
{
    bool keepGoing = TRUE;
    ST_MSG subtask3_msg;

    Ql_Debug_Trace("<--multitask: example_task3_entry-->\r\n");

    while(keepGoing)
    {    
        Ql_OS_GetMessage(&subtask3_msg);
        switch(subtask3_msg.message)
        {
            case MSG_ID_USER_DATA:
            {
               APP_DEBUG("\r\n<--Sub task 3 recv MSG: SrcId=%d,MsgID=%d Data1=%d, Data2=%d-->\r\n", \
                        subtask3_msg.srcTaskId, \
                        subtask3_msg.message,\
                        subtask3_msg.param1, \
                        subtask3_msg.param2);
                    if(s_iPassTask == MAX_TASK_NUM)
                    {
                        s_iPassTask = main_task_id;
                    }
                    else
                    {
                        s_iPassTask++;
                    }
                    Ql_OS_SendMessage(s_iPassTask, MSG_ID_USER_DATA,subtask3_msg.param1, subtask3_msg.param2);
                
                break;
            }
            case MSG_ID_MUTEX_TEST:
            {
                MutextTest(TaskId);
                break;
            }
            case MSG_ID_SEMAPHORE_TEST:
            {
                SemaphoreTest1(TaskId);  
                break;
            }
            case MSG_ID_GET_ALL_TASK_PRIORITY:
            {
                GetCurrentTaskPriority(TaskId);
                break;
            }
            case MSG_ID_GET_ALL_TASK_REMAINSTACK:
            {
                GetCurrenTaskRemainStackSize(TaskId);
                break;
            } 
            default:
                break;
        }
    }
}

/**************************************************************
* the 4nd sub task
***************************************************************/
void proc_subtask4(s32 TaskId)
{
    bool keepGoing = TRUE;
    ST_MSG subtask4_msg;
    Ql_Debug_Trace("<--multitask: example_task4_entry-->\r\n");

    while(keepGoing)
    {    
        Ql_OS_GetMessage(&subtask4_msg);
        switch(subtask4_msg.message)
        {
            case MSG_ID_USER_DATA:
            {
               APP_DEBUG("\r\n<--Sub task 4 recv MSG: SrcId=%d,MsgID=%d Data1=%d, Data2=%d-->\r\n", \
                        subtask4_msg.srcTaskId, \
                        subtask4_msg.message,\
                        subtask4_msg.param1, \
                        subtask4_msg.param2);
                    if(s_iPassTask == MAX_TASK_NUM)
                    {
                        s_iPassTask = main_task_id;
                    }
                    else
                    {
                        s_iPassTask++;
                    }
                    Ql_OS_SendMessage(s_iPassTask, MSG_ID_USER_DATA,subtask4_msg.param1, subtask4_msg.param2);
                
                break;
            }
            case MSG_ID_MUTEX_TEST:
            {
                MutextTest(TaskId);
                break;
            }
            case MSG_ID_SEMAPHORE_TEST:
            {
                SemaphoreTest1(TaskId);  
                break;
            }
            case MSG_ID_GET_ALL_TASK_PRIORITY:
            {
                GetCurrentTaskPriority(TaskId);
                break;
            }
            case MSG_ID_GET_ALL_TASK_REMAINSTACK:
            {
                GetCurrenTaskRemainStackSize(TaskId);
                break;
            } 
            default:
                break;
        }
    }
}

/**************************************************************
* the 5nd sub task
***************************************************************/
void proc_subtask5(s32 TaskId)
{
    bool keepGoing = TRUE;
    ST_MSG subtask5_msg;
    
    Ql_Debug_Trace("<--multitask: example_task5_entry-->\r\n");

    while(keepGoing)
    {    
        Ql_OS_GetMessage(&subtask5_msg);
        switch(subtask5_msg.message)
        {
            case MSG_ID_USER_DATA:
            {
               APP_DEBUG("\r\n<--Sub task 5 recv MSG: SrcId=%d,MsgID=%d Data1=%d, Data2=%d-->\r\n", \
                        subtask5_msg.srcTaskId, \
                        subtask5_msg.message,\
                        subtask5_msg.param1, \
                        subtask5_msg.param2);
                    if(s_iPassTask == MAX_TASK_NUM)
                    {
                        s_iPassTask = main_task_id;
                    }
                    else
                    {
                        s_iPassTask++;
                    }
                    Ql_OS_SendMessage(s_iPassTask, MSG_ID_USER_DATA,subtask5_msg.param1, subtask5_msg.param2);
                
                break;
            }
            case MSG_ID_MUTEX_TEST:
            {
                MutextTest(TaskId);
                break;
            }
            case MSG_ID_SEMAPHORE_TEST:
            {
                SemaphoreTest1(TaskId);  
                break;
            }
            case MSG_ID_GET_ALL_TASK_PRIORITY:
            {
                GetCurrentTaskPriority(TaskId);
                break;
            }
            case MSG_ID_GET_ALL_TASK_REMAINSTACK:
            {
                GetCurrenTaskRemainStackSize(TaskId);
                break;
            } 
            default:
                break;
        }
    }
}

/**************************************************************
* the 6nd sub task
***************************************************************/
void proc_subtask6(s32 TaskId)
{
    bool keepGoing = TRUE;
    ST_MSG subtask6_msg;
    
    Ql_Debug_Trace("<--multitask: example_task6_entry-->\r\n");

    while(keepGoing)
    {    
        Ql_OS_GetMessage(&subtask6_msg);
        switch(subtask6_msg.message)
        {
            case MSG_ID_USER_DATA:
            {
               APP_DEBUG("\r\n<--Sub task 6 recv MSG: SrcId=%d,MsgID=%d Data1=%d, Data2=%d-->\r\n", \
                        subtask6_msg.srcTaskId, \
                        subtask6_msg.message,\
                        subtask6_msg.param1, \
                        subtask6_msg.param2);
                    if(s_iPassTask == MAX_TASK_NUM)
                    {
                        s_iPassTask = main_task_id;
                    }
                    else
                    {
                        s_iPassTask++;
                    }
                    Ql_OS_SendMessage(s_iPassTask, MSG_ID_USER_DATA,subtask6_msg.param1, subtask6_msg.param2);
                
                break;
            }
            case MSG_ID_MUTEX_TEST:
            {
                MutextTest(TaskId);
                break;
            }
            case MSG_ID_SEMAPHORE_TEST:
            {
                SemaphoreTest1(TaskId);  
                break;
            }
            case MSG_ID_GET_ALL_TASK_PRIORITY:
            {
                GetCurrentTaskPriority(TaskId);
                break;
            }
            case MSG_ID_GET_ALL_TASK_REMAINSTACK:
            {
                GetCurrenTaskRemainStackSize(TaskId);
                break;
            } 
            default:
                break;
        }
    }
}

/**************************************************************
* the 7nd sub task
***************************************************************/
void proc_subtask7(s32 TaskId)
{
    bool keepGoing = TRUE;
    ST_MSG subtask7_msg;
    Ql_Debug_Trace("<--multitask: example_task7_entry-->\r\n");

    while(keepGoing)
    {    
        Ql_OS_GetMessage(&subtask7_msg);
        switch(subtask7_msg.message)
        {
            case MSG_ID_USER_DATA:
            {
               APP_DEBUG("\r\n<--Sub task 7 recv MSG: SrcId=%d,MsgID=%d Data1=%d, Data2=%d-->\r\n", \
                        subtask7_msg.srcTaskId, \
                        subtask7_msg.message,\
                        subtask7_msg.param1, \
                        subtask7_msg.param2);
                    if(s_iPassTask == MAX_TASK_NUM)
                    {
                        s_iPassTask = main_task_id;
                    }
                    else
                    {
                        s_iPassTask++;
                    }
                    Ql_OS_SendMessage(s_iPassTask, MSG_ID_USER_DATA,subtask7_msg.param1, subtask7_msg.param2);
                
                break;
            }
            case MSG_ID_MUTEX_TEST:
            {
                MutextTest(TaskId);
                break;
            }
            case MSG_ID_SEMAPHORE_TEST:
            {
                SemaphoreTest1(TaskId);  
                break;
            }
            case MSG_ID_GET_ALL_TASK_PRIORITY:
            {
                GetCurrentTaskPriority(TaskId);
                break;
            }
            case MSG_ID_GET_ALL_TASK_REMAINSTACK:
            {
                GetCurrenTaskRemainStackSize(TaskId);
                break;
            } 
            default:
                break;
        }
    }
}

/**************************************************************
* the 8nd sub task
***************************************************************/
void proc_subtask8(s32 TaskId)
{
    bool keepGoing = TRUE;
    ST_MSG subtask8_msg;

    Ql_Debug_Trace("<--multitask: example_task8_entry-->\r\n");

    while(keepGoing)
    {    
        Ql_OS_GetMessage(&subtask8_msg);
        switch(subtask8_msg.message)
        {
            case MSG_ID_USER_DATA:
            {
               APP_DEBUG("\r\n<--Sub task 8 recv MSG: SrcId=%d,MsgID=%d Data1=%d, Data2=%d-->\r\n", \
                        subtask8_msg.srcTaskId, \
                        subtask8_msg.message,\
                        subtask8_msg.param1, \
                        subtask8_msg.param2);
                break;
            }
            case MSG_ID_MUTEX_TEST:
            {
                MutextTest(TaskId);
                break;
            }
            case MSG_ID_SEMAPHORE_TEST:
            {
                SemaphoreTest1(TaskId);  
                break;
            }
            case MSG_ID_GET_ALL_TASK_PRIORITY:
            {
                GetCurrentTaskPriority(TaskId);
                break;
            }
            case MSG_ID_GET_ALL_TASK_REMAINSTACK:
            {
                GetCurrenTaskRemainStackSize(TaskId);
                break;
            } 
            default:
                break;
        }
    }
}

#endif // __EXAMPLE_MULTITASK__

