#ifndef __EXAMPLE_I2C_H__
#define __EXAMPLE_I2C_H__

#include "stdio.h"
#include "qapi_i2c_master.h"

void i2c_uart_dbg_init(void);
void i2c_uart_debug_print(const char* fmt, ...);

#endif /* __EXAMPLE_I2C_H__ */
