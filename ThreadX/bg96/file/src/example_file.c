/******************************************************************************
*@file    example_file.c
*@brief   example of file operation
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#if defined(__EXAMPLE_FILE__)
/**************************************************************************
*                                 INCLUDE
***************************************************************************/
// STANDARED LIBRARY
#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

// Qualcomm include
#include "qapi_uart.h"
#include "qapi_timer.h"
#include "qapi_diag.h"
#include "qapi_fs.h"

// Quectel include
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "example_file.h"

/**************************************************************************
*                                 GLOBAL
***************************************************************************/
/* uart rx tx buffer */
TX_BYTE_POOL *byte_pool_uart;
#define UART_BYTE_POOL_SIZE		10*8*1024
UCHAR free_memory_uart[UART_BYTE_POOL_SIZE];

/* uart rx tx buffer */
static char *rx_buff = NULL;
static char *tx_buff = NULL;

/* uart config para*/
static QT_UART_CONF_PARA uart_conf =
{
	NULL,
	QT_UART_PORT_02,
	NULL,
	0,
	NULL,
	0,
	115200
};

/* conter used to count the total run times*/
static unsigned long task_run_couter = 0;


#define S_IFDIR        	0040000 	/**< Directory */
#define S_IFMT          0170000		/**< Mask of all values */
#define S_ISDIR(m)     	(((m) & S_IFMT) == S_IFDIR)

//DIR
#define QL_DIR_PATH_ROOT		"/datatx/dir_test"
#define QL_DIR_PATH_TEST00		"/datatx/dir_test/test00"
#define QL_DIR_PATH_TEST01		"/datatx/dir_test/test01"
#define QL_DIR_PATH_TEST02		"/datatx/dir_test/test01/test02"
//FILE
#define QL_FILE_PATH_TEST_FILE	"/datatx/dir_test/test01/test.txt"

/**************************************************************************
*                                 FUNCTION
***************************************************************************/
void quectel_file_list(char* path);

/*
@func
  quectel_efs_is_dir_exists
@brief
  find the path is a directory or directory is exist 
*/
int quectel_efs_is_dir_exists(const char *path)
{
     int result, ret_val = 0;
     struct qapi_FS_Stat_Type_s sbuf;

     memset (&sbuf, 0, sizeof (sbuf));

     result = qapi_FS_Stat(path, &sbuf);

     if (result != 0)
          goto End;

     if (S_ISDIR (sbuf.st_Mode) == 1)
          ret_val = 1;

     End:
          return ret_val;
}

/*
@func
  quectel_task_entry
@brief
  Entry function for task. 
*/
int quectel_task_entry(void)
{
	qapi_FS_Status_t status = QAPI_OK;
  	int    fd = -1;
	char   file_buff[1024] ;
	int   wr_bytes = 0;
	qapi_FS_Offset_t seek_status = 0;
	int ret = -1;

	qapi_Timer_Sleep(3, QAPI_TIMER_UNIT_SEC, true);

	/// MUST SETTING
	setlocale(LC_ALL, "C");	/// <locale.h>
//
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

	ret = tx_byte_allocate(byte_pool_uart, (VOID *)&rx_buff, 4*1024, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_allocate [rx_buff] failed, %d", ret);
	return ret;
	}

	ret = tx_byte_allocate(byte_pool_uart, (VOID *)&tx_buff, 4*1024, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_allocate [tx_buff] failed, %d", ret);
	return ret;
	}

	uart_conf.tx_buff = tx_buff;
	uart_conf.rx_buff = rx_buff;
	uart_conf.tx_len = 4096;
	uart_conf.rx_len = 4096;
//
	/* debug port init */
	uart_init(&uart_conf);
	/* start uart receive */
	uart_recv(&uart_conf);
	/* prompt task running */
	qt_uart_dbg(uart_conf.hdlr,"\r\nFILE Example\r\n");

	///create root dir for test;
	if(quectel_efs_is_dir_exists(QL_DIR_PATH_ROOT) != 1)
	{
		status = qapi_FS_Mk_Dir(QL_DIR_PATH_ROOT, 0677);
		qt_uart_dbg(uart_conf.hdlr,"qapi_FS_Mk_Dir %d", status);
	}

	if(quectel_efs_is_dir_exists(QL_DIR_PATH_TEST00) != 1)
	{
		status = qapi_FS_Mk_Dir(QL_DIR_PATH_TEST00, 0677);
		qt_uart_dbg(uart_conf.hdlr,"qapi_FS_Mk_Dir %d", status);
	}

	if(quectel_efs_is_dir_exists(QL_DIR_PATH_TEST01) != 1)
	{
		status = qapi_FS_Mk_Dir(QL_DIR_PATH_TEST01, 0677);
		qt_uart_dbg(uart_conf.hdlr,"qapi_FS_Mk_Dir %d", status);
	}

	if(quectel_efs_is_dir_exists(QL_DIR_PATH_TEST02) != 1)
	{
		status = qapi_FS_Mk_Dir(QL_DIR_PATH_TEST02, 0677);
		qt_uart_dbg(uart_conf.hdlr,"qapi_FS_Mk_Dir %d", status);
	}

	//create file for test;
	status = qapi_FS_Open(QL_FILE_PATH_TEST_FILE, QAPI_FS_O_RDWR_E | QAPI_FS_O_CREAT_E, &fd);
	if((status == QAPI_OK) && (-1 != fd))
	{
		memset(file_buff, 'T', sizeof(file_buff));
		status = qapi_FS_Seek(fd, 0, QAPI_FS_SEEK_END_E, &seek_status);
		qt_uart_dbg(uart_conf.hdlr,"qapi_FS_Seek %d %lld", status, seek_status);	

		status = qapi_FS_Write(fd, (uint8*)&file_buff, sizeof(file_buff), &wr_bytes);
		qt_uart_dbg(uart_conf.hdlr,"qapi_FS_Write %d %d", status, wr_bytes);		

		status = qapi_FS_Close(fd);
		qt_uart_dbg(uart_conf.hdlr,"qapi_FS_Close %d", status);	
	}

	//list all files
	quectel_file_list(QL_DIR_PATH_ROOT);

	while (1)
	{
		task_run_couter ++;

		/* print log to uart */
		qt_uart_dbg(uart_conf.hdlr,"task run times : %d", task_run_couter);

		/* sleep 15 seconds */
		qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_SEC, true);

	}
}

/*
@func
  quectel_file_list
@brief
  list all the files/directories the specified path.
@note
  the max number of the iterator is 5. if overflow, error number -150008 retured.
*/
void quectel_file_list(char* path)
{
	qapi_FS_Status_t status = QAPI_OK;
	qapi_FS_Iter_Handle_t iter_handle;
	struct qapi_FS_Iter_Entry_s  iter_entry;
	char path_buffer[128];

	memset(path_buffer, 0, sizeof(path_buffer));

	//iterate
	status = qapi_FS_Iter_Open(path, &iter_handle);
	qt_uart_dbg(uart_conf.hdlr,"qapi_FS_Iter_Open [%s] %d", path, status);

	do
	{
		status = qapi_FS_Iter_Next(iter_handle, &iter_entry);
	//	qt_uart_dbg(uart_conf.hdlr,"qapi_FS_Iter_Next %d", status);
		if(strlen(iter_entry.file_Path) == 0)
		{
			break;
		}
		if((strcmp(iter_entry.file_Path, ".") != 0) && (strcmp(iter_entry.file_Path, "..") != 0))
		{
			sprintf(path_buffer, "%s/%s", path, iter_entry.file_Path);
			if(quectel_efs_is_dir_exists(path_buffer) == 1)
			{
				qt_uart_dbg(uart_conf.hdlr,"# dir %s", path_buffer);
				qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_MSEC, true);
				quectel_file_list(path_buffer);
			}
			else
			{
				qt_uart_dbg(uart_conf.hdlr,"* file %s", path_buffer);
			}
		}
	} while(1);
	status = qapi_FS_Iter_Close(iter_handle);
	qt_uart_dbg(uart_conf.hdlr,"qapi_FS_Iter_Close %d", status);
}

#endif /*__EXAMPLE_FILE__*/

