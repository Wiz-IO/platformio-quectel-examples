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
 *   example_file.c
 *
 * Project:
 * --------
 *   OpenCPU
 *
 * Description:
 * ------------
 *   This example demonstrates how to use file function with APIs in OpenCPU.
 *   All debug information will be output  through DEBUG port.
 * Usage:
 * ------
 *   Compile & Run:
 *
 *     Set "C_PREDEF=-D __EXAMPLE_FILESYSTEM__" in gcc_makefile file. And compile the 
 *     app using "make clean/new".
 *     Download image bin to module to run.
 *
 *   Operation:
 *   Insert a SD card ,you can see the debug information from the com tools.
 *   If you want test in UFS,you can modify the storage and undefine __TEST_FOR_MEMORY_CARD__
 * 
 **********************************************************************************
 *example:
 *Send format                       (Command Format: format)
 *query space                       (Command Format: getspace)
 *create a dir                        (Command Format: dir=test)
 * delete a dir                      (Command Format: deldir=test)
 *list dir files                       (Command Format:  listfile=test )
 *
 *if Enum_FSStorage storage = Ql_FS_RAM , filename is RAM:test.txt
 *create a file                     (Command Format: cfile=test.txt)
 *write file                         (Command Format: wfile=test.txt)
 *write file content             (Command Format: content=abcdefg)
 *read file                         (Command Format: rfile=test.txt)
 *delete file                      (Command Format: dfile=test.txt)
 *query file size 					(Command Format: filesize=test.txt)
 ***********************************************************************************
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
#ifdef __EXAMPLE_FILESYSTEM__
#include "string.h"
#include "ql_type.h"
#include "ql_trace.h"
#include "ql_uart.h"
#include "ql_fs.h"
#include "ql_error.h"
#include "ql_system.h"
#include "ql_stdlib.h"


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


//#define __TEST_FOR_RAM_FILE__
//#define __TEST_FOR_UFS__
#define __TEST_FOR_MEMORY_CARD__

#if defined (__TEST_FOR_RAM_FILE__)
#define  PATH_ROOT    		((u8 *)"RAM:")
#elif defined (__TEST_FOR_UFS__)
#define  PATH_ROOT    		((u8 *)"myroot")
#else   //!__TEST_FOR_MEMORY_CARD__
#define  PATH_ROOT    		((u8 *)"SD:")
#endif


#define LENGTH 100


static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void* customizedPara)
{
    u8  phy_uart_buff[128] = {0};
    u8  vir_uart_buff[128] = {0};   
    u8  cmd_buff[128] = {0};
    u32 rd_len = 0;
    s32 ret = 0;
    u8  *p_buff = phy_uart_buff;
    u32 writenLen = 0;
    static s32 handle=-1;
    switch (msg)
    {
        case EVENT_UART_READY_TO_READ:
        {
            if(UART_PORT1 == port || UART_PORT2 == port)
            {
                Ql_memset(phy_uart_buff, 0x0, sizeof(phy_uart_buff));
                rd_len = Ql_UART_Read(port, phy_uart_buff, sizeof(phy_uart_buff));
                if (!rd_len)
                {
                    APP_DEBUG("\r\nread error.\r\n");
                    break;
                }

                //format 
                Ql_memset(cmd_buff, 0x0, sizeof(cmd_buff));           
                Ql_sprintf(cmd_buff, "format\r\n");                 
                ret = Ql_strncmp(phy_uart_buff, cmd_buff, Ql_strlen(cmd_buff));
                if(!ret)
                {
                    #if defined (__TEST_FOR_RAM_FILE__)
                        Enum_FSStorage storage = Ql_FS_RAM;
                    #elif defined (__TEST_FOR_UFS__)
                        Enum_FSStorage storage = Ql_FS_UFS;
                    #elif defined (__TEST_FOR_MEMORY_CARD__)
                        Enum_FSStorage storage = Ql_FS_SD;
                    #else
                        APP_DEBUG("\r\n<--Format does not define.-->\r\n");
                    #endif
                     // format 
                    ret = Ql_FS_Format(storage);
                    if(ret==QL_RET_OK)
                    {
                        APP_DEBUG("\r\n<-- Ql_FS_Format(storage=%d).Format OK! -->\r\n",storage);
                    }
                    else
                    {
                        APP_DEBUG("\r\n<--Format error.-->\r\n");
                    }
                    break;
                }

				 //query file size
                Ql_memset(cmd_buff, 0x0, sizeof(cmd_buff));
                Ql_sprintf(cmd_buff, "filesize=");
                ret = Ql_strncmp(phy_uart_buff, cmd_buff, Ql_strlen(cmd_buff));
                if(!ret)
                {
					s32 filesize;
					u8 filePath[LENGTH] = {0};
                    u8 *p1=NULL,*p2=NULL;
                    handle=-1;
                    p1 = phy_uart_buff + Ql_strlen(cmd_buff);
                    if(p1)
                    {
                        p2 = Ql_strstr(p1,"\r\n");
                        if(!p2)
                        {
                            APP_DEBUG("\r\ninvalid parameter.\r\n");
                            break;
                        }
                    }
                    Ql_strncpy(filePath,p1,p2-p1);
                    APP_DEBUG("\r\nfilePath=%s\r\n",filePath);
					
                    filesize = Ql_FS_GetSize(filePath);
					APP_DEBUG("\r\n<-- Ql_FS_GetSize(%s) = %d \r\n",filePath,filesize);
                    break;
                }
							
                //query space
                Ql_memset(cmd_buff, 0x0, sizeof(cmd_buff));
                Ql_sprintf(cmd_buff, "getspace\r\n");
                ret = Ql_strncmp(phy_uart_buff, cmd_buff, Ql_strlen(cmd_buff));
                if(!ret)
                {
                    s64  space = 0;
                    #if defined (__TEST_FOR_RAM_FILE__)
                        Enum_FSStorage storage = Ql_FS_RAM;
                    #elif defined (__TEST_FOR_UFS__)
                        Enum_FSStorage storage = Ql_FS_UFS;
                    #elif defined (__TEST_FOR_MEMORY_CARD__)
                        Enum_FSStorage storage = Ql_FS_SD;
                    #else
                        APP_DEBUG("\r\n<--Format does not define.-->\r\n");
                    #endif
                     //check freespace 
                     space  = Ql_FS_GetFreeSpace(storage);
                     APP_DEBUG("\r\n<--Ql_FS_GetFreeSpace(storage=%d) =%lld-->\r\n",storage,space);
    
                    //check total space
                    space = Ql_FS_GetTotalSpace(storage);                
                    APP_DEBUG("\r\n<--Ql_FS_GetTotalSpace(storage=%d)=%lld-->\r\n",storage,space);
                    break;
                }

                /* create a dir         */
                /* dir="SD:dir_a"\r\n   */
                Ql_memset(cmd_buff, 0x0, sizeof(cmd_buff));
                Ql_sprintf(cmd_buff, "dir=");
                ret = Ql_strncmp(phy_uart_buff, cmd_buff, Ql_strlen(cmd_buff)); 
                if (!ret)
                {
                    u8 dir_buff[128]={0};
                    u8 *p1=NULL,*p2=NULL;
                    p1 = phy_uart_buff + Ql_strlen(cmd_buff);
                    if(p1)
                    {
                        p2 = Ql_strstr(p1,"\r\n");
                        if(!p2)
                        {
                            APP_DEBUG("\r\ninvalid parameter.\r\n");
                            break;
                        }
                    }
                    Ql_strncpy(dir_buff,p1,p2-p1);
                    APP_DEBUG("\r\ndir_buff=%s\r\n",dir_buff);

                    ret = Ql_FS_CheckDir(dir_buff);
                    if(ret != QL_RET_OK)
                    {
                		APP_DEBUG("\r\n<-- Dir(%s) is not exist, creating.... -->", dir_buff);
                		ret  = Ql_FS_CreateDir(dir_buff);
                		if(ret != QL_RET_OK)
                		{
                			APP_DEBUG("\r\n<-- failed!! Create Dir(%s) fail-->", dir_buff);
                		}
                		else
                		{
                			APP_DEBUG("\r\n<-- CreateDir(%s) OK! -->", dir_buff);
                		}        
                    }
                    else
                    {
                        APP_DEBUG("\r\n<-- Dir(%s) is exist already -->", dir_buff);
                    }
                    break;
                }

                /* delete a dir */
                Ql_memset(cmd_buff, 0x0, sizeof(cmd_buff));
                Ql_sprintf(cmd_buff, "deldir=");               
                ret = Ql_strncmp(phy_uart_buff, cmd_buff, Ql_strlen(cmd_buff)); 
                if (!ret)
                {
                    u8 dir_buff[128]={0};
                    u8 *p1=NULL,*p2=NULL;
                    p1 = phy_uart_buff + Ql_strlen(cmd_buff);
                    if(p1)
                    {
                        p2 = Ql_strstr(p1,"\r\n");
                        if(!p2)
                        {
                            APP_DEBUG("\r\ninvalid parameter.\r\n");
                            break;
                        }
                    }
                    Ql_strncpy(dir_buff,p1,p2-p1);
                    ret = Ql_FS_CheckDir(dir_buff);
                    if(ret == QL_RET_OK)
                    {
                        ret = Ql_FS_DeleteDir(dir_buff);
                        APP_DEBUG("\r\n<-- Ql_FS_DeleteDir() ret=%d: -->\r\n",ret);  
                        if(!ret)
                        {
                            APP_DEBUG("\r\n<-- dir=%s delete ok! -->", dir_buff);
                        }
                        else
                        {
                            APP_DEBUG("\r\n<-- dir=%s delete fail! -->", dir_buff);
                        }
                    }
                    else
                    {
                        APP_DEBUG("\r\n<-- dir=%s is not exist -->", dir_buff);
                    }
                    break;
                }

                 //list dir files
                Ql_memset(cmd_buff, 0x0, sizeof(cmd_buff));
                Ql_sprintf(cmd_buff, "listfile=");               
                ret = Ql_strncmp(phy_uart_buff, cmd_buff, Ql_strlen(cmd_buff));
                if (!ret)
                {
                    u8 dir_buff[128]={0};
                    u8 *p1=NULL,*p2=NULL;
                    p1 = phy_uart_buff + Ql_strlen(cmd_buff);
                    if(p1)
                    {
                        p2 = Ql_strstr(p1,"\r\n");
                        if(!p2)
                        {
                            APP_DEBUG("\r\ninvalid parameter.\r\n");
                            break;
                        }
                    }
                    Ql_strncpy(dir_buff,p1,p2-p1);
                    ret = Ql_FS_CheckDir(dir_buff);
                    if(ret != QL_RET_OK)
                    {
                        APP_DEBUG("\r\n<-- dir=%s is not exist -->", dir_buff);
                        break;
                    }
                    else
                    {
                        bool isdir = FALSE;
                        u8 filePath[LENGTH] = {0};
                        u8 filename[LENGTH] = {0};
                        s32 filesize=0;
                        handle=-1;
                        Ql_memset(filePath, 0, LENGTH);        
                        Ql_memset(filename, 0, LENGTH);    
                        Ql_sprintf(filePath,"%s\\%s\0",dir_buff,"*");        
                        handle  = Ql_FS_FindFirst(filePath,filename,LENGTH,&filesize,&isdir);
                        if(handle > 0)
                        {
                            do
                            {
                                if(isdir)
                                {
                                    APP_DEBUG("\r\n<-- ql_fs_find dirname= %s -->\r\n",filename);
                                }
                                else
                                {
                                    APP_DEBUG("\r\n<-- ql_fs_find filename= %s filesize=%d: -->\r\n",filename,filesize);
                                }
                                Ql_memset(filename, 0, LENGTH);
                            }while((Ql_FS_FindNext(handle, filename,LENGTH,&filesize,&isdir)) == QL_RET_OK);
                            Ql_FS_FindClose(handle);
                            handle=-1;
                        }
                        else
                        {
                            APP_DEBUG("\r\n<-- No file in the dir -->\r\n");
                        }
                    }
                    break;
                }
                
                /* create a file */
                Ql_memset(cmd_buff, 0x0, sizeof(cmd_buff));
                Ql_sprintf(cmd_buff, "cfile=");               
                ret = Ql_strncmp(phy_uart_buff, cmd_buff, Ql_strlen(cmd_buff)); 
                if (!ret)
                {
                    u8 filePath[LENGTH] = {0};
                    u8 *p1=NULL,*p2=NULL;
                    handle=-1;
                    p1 = phy_uart_buff + Ql_strlen(cmd_buff);
                    if(p1)
                    {
                        p2 = Ql_strstr(p1,"\r\n");
                        if(!p2)
                        {
                            APP_DEBUG("\r\ninvalid parameter.\r\n");
                            break;
                        }
                    }
                    Ql_strncpy(filePath,p1,p2-p1);
                     //open file
                    #ifdef __TEST_FOR_RAM_FILE__
                        handle = Ql_FS_OpenRAMFile(filePath,QL_FS_CREATE|QL_FS_READ_WRITE, LENGTH);
                    #else
                        handle = Ql_FS_Open(filePath,QL_FS_READ_WRITE );
                    #endif
                    if(handle<0)
                    {
                        #ifdef __TEST_FOR_RAM_FILE__
                            handle = Ql_FS_OpenRAMFile(filePath,QL_FS_CREATE_ALWAYS, LENGTH);
                        #else
                            handle = Ql_FS_Open(filePath,QL_FS_CREATE_ALWAYS); //if file does not exist ,creat it
                        #endif
                        if(handle>0)
                        {
                            APP_DEBUG("\r\n<-- Create file (%s) OK! handle =%d -->\r\n",filePath,handle);
                        }
                        else
                        {
                            APP_DEBUG("\r\n<-- failed!! Create file (%s) fail-->",filePath);
                        }
                    }
                    else
                    {
                         APP_DEBUG("\r\n<--The file has exist-->\r\n");
                    }
                    Ql_FS_Close(handle);
                    handle=-1;
                    break;
                }

                //write file
                Ql_memset(cmd_buff, 0x0, sizeof(cmd_buff));
                Ql_sprintf(cmd_buff, "wfile=");               
                ret = Ql_strncmp(phy_uart_buff, cmd_buff, Ql_strlen(cmd_buff)); 
                if (!ret)
                {
                    u8 filePath[LENGTH] = {0};
                    u8 file_buff[128]={0};
                    u8 *p1=NULL,*p2=NULL;
                    handle=-1;               
                    p1 = phy_uart_buff + Ql_strlen(cmd_buff);
                    if(p1)
                    {
                        p2 = Ql_strstr(p1,"\r\n");
                        if(!p2)
                        {
                            APP_DEBUG("\r\ninvalid parameter.\r\n");
                            break;
                        }
                    }
                    Ql_strncpy(file_buff,p1,p2-p1);
                    #ifdef __TEST_FOR_RAM_FILE__
                        handle = Ql_FS_OpenRAMFile(file_buff,QL_FS_CREATE|QL_FS_READ_WRITE, LENGTH);
                    #else
                        handle = Ql_FS_Open(file_buff,QL_FS_READ_WRITE);
                    #endif
                    if(handle<0)
                    {
                       APP_DEBUG("\r\n<--The file does not exist,handle:%d-->\r\n",handle);
                       break;
                    }
                    else
                    {
                       APP_DEBUG("\r\n<--Please write:-->\r\n");
                      
                    }
                    
                    break;
                }

                //write file content
                Ql_memset(cmd_buff, 0x0, sizeof(cmd_buff));
                Ql_sprintf(cmd_buff, "content=");               
                ret = Ql_strncmp(phy_uart_buff, cmd_buff, Ql_strlen(cmd_buff)); 
                if (!ret)
                {
                    u8 *p1=NULL,*p2=NULL;
                    u8 file_buff[128]={0};
                    u32 writenLen = 0;
                    p1 = phy_uart_buff + Ql_strlen(cmd_buff);
                    if(p1)
                    {
                        p2 = Ql_strstr(p1,"\r\n");
                        if(!p2)
                        {
                            APP_DEBUG("\r\ninvalid parameter.\r\n");
                            break;
                        }
                    }
                    if(handle<0)
                    {
                        APP_DEBUG("\r\n<--The file does not exist!-->\r\n");
                        break;
                    }
                    Ql_strncpy(file_buff,p1,p2-p1);
                    ret = Ql_FS_Seek(handle,0,QL_FS_FILE_END);  //seek end 
                    ret = Ql_FS_Write(handle, file_buff, Ql_strlen(file_buff), &writenLen);
                    APP_DEBUG("\r\n<--!! Ql_FS_Write  ret =%d  writenLen =%d-->\r\n",ret,writenLen);
                    
                    Ql_FS_Flush(handle); //fflush
                    Ql_FS_Close(handle);//close the file
                    break;
                }

                //read file
                Ql_memset(cmd_buff, 0x0, sizeof(cmd_buff));
                Ql_sprintf(cmd_buff, "rfile=");               
                ret = Ql_strncmp(phy_uart_buff, cmd_buff, Ql_strlen(cmd_buff)); 
                if (!ret)
                {
                    u8 filePath[LENGTH] = {0};
                    u8 file_buff[128]={0};
                    u8 file_content[LENGTH]={0};
                    u8 *p1=NULL,*p2=NULL;
                    handle=-1;
                    s32 readenLen=0;
                    u8 position=0;
                    
                    p1 = phy_uart_buff + Ql_strlen(cmd_buff);
                    if(p1)
                    {
                        p2 = Ql_strstr(p1,"\r\n");
                        if(!p2)
                        {
                            APP_DEBUG("\r\ninvalid parameter.\r\n");
                            break;
                        }
                    }
                    Ql_strncpy(file_buff,p1,p2-p1);
                    #ifdef __TEST_FOR_RAM_FILE__
                        handle = Ql_FS_OpenRAMFile(file_buff,QL_FS_READ_ONLY, LENGTH);
                    #else
                        handle = Ql_FS_Open(file_buff,QL_FS_READ_WRITE);
                    #endif
                    
                    if(handle<0)
                    {
                        APP_DEBUG("\r\n<--The file does not exist!-->\r\n");
                        break;
                    }

                    APP_DEBUG("\r\n<--The content is -->\r\n");
                    ret = Ql_FS_Seek(handle,0,QL_FS_FILE_BEGIN); 
                    ret = Ql_FS_Read(handle, file_content, LENGTH-1, &readenLen);
                    APP_DEBUG("%s",file_content);
                    while(readenLen>=(LENGTH-1))
                    {
                        Ql_memset(file_content, 0x0, sizeof(file_content));
                        position = Ql_FS_GetFilePosition(handle);//get postion
                        ret = Ql_FS_Seek(handle,LENGTH-1,position);//seek     
                        ret = Ql_FS_Read(handle, file_content, LENGTH-1, &readenLen);
                        APP_DEBUG("%s",file_content);
                    }
                    APP_DEBUG("\r\n<--------------------------->\r\n");
                    Ql_FS_Close(handle);//close the file
                    handle=-1;
                    break;
                }

                //delete file
                Ql_memset(cmd_buff, 0x0, sizeof(cmd_buff));
                Ql_sprintf(cmd_buff, "dfile=");               
                ret = Ql_strncmp(phy_uart_buff, cmd_buff, Ql_strlen(cmd_buff)); 
                if (!ret)
                {
                    u8 filePath[LENGTH] = {0};
                    u8 file_buff[128]={0};
                    u8 file_content[LENGTH]={0};
                    u8 *p1=NULL,*p2=NULL;
                    handle=-1;
                    s32 readenLen=0;
                    p1 = phy_uart_buff + Ql_strlen(cmd_buff);
                    if(p1)
                    {
                        p2 = Ql_strstr(p1,"\r\n");
                        if(!p2)
                        {
                            APP_DEBUG("\r\ninvalid parameter.\r\n");
                            break;
                        }
                    }
                    Ql_strncpy(filePath,p1,p2-p1);
                    ret=Ql_FS_Check(filePath);
                    if(ret!=QL_RET_OK)
                    {
                         APP_DEBUG("\r\n<-- file=%s don't exist! -->",filePath);
                         break;
                    }
                    ret = Ql_FS_Delete(filePath);
                    if(ret==QL_RET_OK)
                    {
                         APP_DEBUG("\r\n<-- file=%s delete ok! -->",filePath);
                    }
                    else
                    {
                        APP_DEBUG("\r\n<-- Delete error.\r\n! -->");
                    }
                    break;
                }

                
                /* send data to kernel */
                Ql_UART_Write(VIRTUAL_PORT1,phy_uart_buff,rd_len);
              //APP_DEBUG("\r\n<--Not found this command, please check you command-->\r\n");
            }
            else if(VIRTUAL_PORT1 == port)
            {
                Ql_memset(vir_uart_buff, 0x0, sizeof(vir_uart_buff));
                rd_len = Ql_UART_Read(port, vir_uart_buff, sizeof(vir_uart_buff));
                if (!rd_len)
                {
                    APP_DEBUG("\r\nread error.\r\n");
                    break;
                }
                APP_DEBUG("%s\r\n",vir_uart_buff);
            }
            break;
        }
        default:
        break;
    }
}


/******************************************************************************/
/*                              Main Code                                     */
/******************************************************************************/
void proc_main_task(void)
{   
    ST_MSG msg;
    s32 ret = -1;
    s32 handle = -1;
    s64  space = 0;
    u32 writenLen = 0;
    u32 readenLen = 0;
    s32 filesize = 0;
    s32 position = 0;    
    bool isdir = FALSE;
    
    u8 *filename = "test.txt";
    u8 *fileNewName = "testNew.txt";

    
    u8 strBuf[LENGTH] = {0};
    u8 filePath[LENGTH] = {0};
    u8 fileNewPath[LENGTH] = {0};
    u8  writeBuffer[LENGTH] = {"This is test data!"};

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
	
	 // Register & open virtual serial port
    ret = Ql_UART_Register(VIRTUAL_PORT1, CallBack_UART_Hdlr, NULL);
    if (ret < QL_RET_OK)
    {
        APP_DEBUG("Fail to register serial port[%d], ret=%d\r\n", VIRTUAL_PORT1, ret);
    }

    ret = Ql_UART_Open(VIRTUAL_PORT1, 0, FC_NONE);
    if (ret < QL_RET_OK)
    {
        APP_DEBUG("Fail to open serial port[%d], ret=%d\r\n", VIRTUAL_PORT1, ret);
    }

#if defined (__TEST_FOR_RAM_FILE__)
	Enum_FSStorage storage = Ql_FS_RAM;
    APP_DEBUG("\r\n<--OpenCPU: FILE(RAM) TEST!-->\r\n");
#elif defined (__TEST_FOR_UFS__)
	Enum_FSStorage storage = Ql_FS_UFS;
    APP_DEBUG("\r\n<--OpenCPU: FILE(UFS) TEST!-->\r\n");
#else   //!__TEST_FOR_MEMORY_CARD__
	Enum_FSStorage storage = Ql_FS_SD;
    APP_DEBUG("\r\n<--OpenCPU: FILE(SD) TEST!-->\r\n");
#endif


#if !defined (__TEST_FOR_MEMORY_CARD__)
  // format 
    ret = Ql_FS_Format(storage);
    APP_DEBUG("\r\n<-- Ql_FS_Format(storage=%d)ret =%d -->\r\n",storage,ret);        
#endif
    
    //check freespace 
    space  = Ql_FS_GetFreeSpace(storage);
    APP_DEBUG("\r\n<--Ql_FS_GetFreeSpace(storage=%d) =%lld-->\r\n",storage,space);
    
    //check total space
    space = Ql_FS_GetTotalSpace(storage);                
    APP_DEBUG("\r\n<--Ql_FS_GetTotalSpace(storage=%d)=%lld-->\r\n",storage,space);
    Ql_memset(filePath, 0, LENGTH);        
    Ql_memset(strBuf, 0, LENGTH);    
    Ql_sprintf(filePath,"%s\\%s\0",PATH_ROOT,"*");        
    handle  = Ql_FS_FindFirst(filePath,strBuf,LENGTH,&filesize,&isdir);
    if(handle > 0)
    {
        do
        {
            if(isdir)
            {
                APP_DEBUG("\r\n<-- ql_fs_find dirname= %s -->\r\n",strBuf);
            }
            else
            {
                APP_DEBUG("\r\n<-- ql_fs_find filename= %s filesize=%d: -->\r\n",strBuf,filesize);
            }
            Ql_memset(strBuf, 0, LENGTH);
        }
        while((Ql_FS_FindNext(handle, strBuf,LENGTH,&filesize,&isdir)) == QL_RET_OK);
        Ql_FS_FindClose(handle);
    }
    while (TRUE)
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
#endif // __EXAMPLE_FILESYSTEM__


