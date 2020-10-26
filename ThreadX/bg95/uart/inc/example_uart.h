/******************************************************************************
*@file    example_uart.h
*@brief   example of uart operation
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#ifndef __EXAMPLE_UART_H__
#define __EXAMPLE_UART_H__

#if defined(__EXAMPLE_UART__)
#include "qapi_uart.h"

#define APP_UART_1_PORT		QAPI_UART_PORT_001_E
#define APP_UART_2_PORT	 	QAPI_UART_PORT_002_E
#define APP_UART_3_PORT	 	QAPI_UART_PORT_003_E

#define QT_UART_ENABLE_1ST
#define QT_UART_ENABLE_2ND
#define QT_UART_ENABLE_3RD

#endif /*__EXAMPLE_UART__*/

#endif /*__EXAMPLE_UART_H__*/

