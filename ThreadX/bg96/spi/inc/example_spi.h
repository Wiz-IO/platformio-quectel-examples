/******************************************************************************
*@file    example_spi.h
*@brief   example of spi operation
*  ---------------------------------------------------------------------------
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#ifndef __EXAMPLE_SPI_H__
#define __EXAMPLE_SPI_H__

#include "stdio.h"
#include "qapi_spi_master.h"

void spi_uart_dbg_init(void);
void spi_uart_debug_print(const char* fmt, ...);

#endif /* __EXAMPLE_SPI_H__ */
