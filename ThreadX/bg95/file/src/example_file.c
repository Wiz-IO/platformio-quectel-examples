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
#include "qapi_quectel.h"
/**************************************************************************
*                                 GLOBAL
***************************************************************************/
/* uart1 rx tx buffer */
static char rx_buff[4096];	/*!!! should keep this buffer as 4K Bytes */
static char tx_buff[4096];

/* uart config para*/
static QT_UART_CONF_PARA uart_conf =
{
	NULL,
	QT_QAPI_UART_PORT_01,
	tx_buff,
	sizeof(tx_buff),
	rx_buff,
	sizeof(rx_buff),
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
	FS_SPACE_INFO storage_info = {0};
	qapi_Status_t fs_status = QAPI_QT_ERR_OK;

	struct qapi_FS_Stat_Type_s file01;

	qapi_Timer_Sleep(20, QAPI_TIMER_UNIT_SEC, true);
	
	IOT_INFO("QT# quectel_task_entry");

	/// MUST SETTING
	setlocale(LC_ALL, "C");	/// <locale.h>

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

	while (1)
	{
		task_run_couter ++;

		/* print log to uart */
		qt_uart_dbg(uart_conf.hdlr,"task run times : %d", task_run_couter);

		/* sleep 15 seconds */
		qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_SEC, true);

	}
}


#endif /*__EXAMPLE_FILE__*/

