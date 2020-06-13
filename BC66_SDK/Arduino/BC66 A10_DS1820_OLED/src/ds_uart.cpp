#include "Arduino.h"
#include <ds_uart.h>

static UART_REGISTER_T *uart_reg = (UART_REGISTER_T *)UART1_BASE; // <--- Serial1

int uart_init(char *dev_path)
{
  return UART_SUCCESS;
}

void uart_finit(void)
{
}

void uart_setb(uint32_t baud)
{
  if (115200 == baud)
  {
    uart_reg->DLM_DLL = 0x1;
    uart_reg->SAMPLE_REG_UNION.SAMPLE_REG = 0x6FE0;
    uart_reg->FRACDIV = 0x16C;
  }
  else
  {
    uart_reg->DLM_DLL = 0xB;
    uart_reg->SAMPLE_REG_UNION.SAMPLE_REG = 0x7AF5;
    uart_reg->FRACDIV = 0x20;
  }
}

void uart_putc(unsigned char c)
{
  Serial1.write(c);
}

unsigned char uart_getc(void)
{
  int n, c, t = -1;
  do
  {
    n = Serial1.available();
    delay(1);
  } while (0 == n);
  c = Serial1.read();
  return c & 0xFF;
}
