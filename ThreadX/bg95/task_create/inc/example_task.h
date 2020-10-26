/******************************************************************************
*@file    example_task.h
*@brief   example of new task creation
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#ifndef __QUECTEL_TASK_H__
#define __QUECTEL_TASK_H__

#if defined(__EXAMPLE_TASK_CREATE__)
#include "qapi_fs_types.h"
#include "txm_module.h"

#define QT_Q_MAX_INFO_NUM		16

typedef struct TASK_COMM_S{
	
	int msg_id;
	int dat;
	CHAR name[16];
	CHAR buffer[32];

}TASK_COMM;

#endif /*__EXAMPLE_TASK_CREATE__*/
#endif /*__QUECTEL_TASK_H__*/

