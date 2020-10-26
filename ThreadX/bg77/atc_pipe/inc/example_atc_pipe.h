/******************************************************************************
*@file    example_atc_pipe.h
*@brief   example of atc pipe operation
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#ifndef __EXAMPLE_ATC_PIPE_H__
#define __EXAMPLE_ATC_PIPE_H__

#if defined(__EXAMPLE_ATC_PIPE__)
typedef enum{
	SIG_EVG_ATPIPE_STANDBY_E = 0x0001,
	SIG_EVG_ATPIPE_SENDING_E = 0x0010,
	SIG_EVG_ATPIPE_RECING_E = 0x0100,
	SIG_EVG_ATPIPE_REC_OK_E = 0x1000,

	SIG_EVG_ATPIPE_END
}SIG_EVG_ATPIPE_E;

#endif /*__EXAMPLE_ATC_PIPE__*/

#endif /*__EXAMPLE_ATCPIPE_H__*/

