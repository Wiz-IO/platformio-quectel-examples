/*
  Ultrasonic sensor HC-SR04 via SPI
  How to: https://github.com/fizban99/microbit_hcsr04
*/

#include <Arduino.h>
#include <hal.h>
#include "SPI.h"

void setup()
{
  int res;
  Serial.begin(115200, true);
  printf("Arduino Firmware: %s 2020 Georgi Angelov\n", Dev.version());
  pinMode(LED, OUTPUT);
  SPI.setFrequency(1000000);  
  SPI.begin();
}

uint8_t buffer[1024];
void loop()
{
  int triger = 0xFFFF0000;
  memset(buffer, 0, sizeof(buffer));
  SPI.transfer((uint8_t*)&triger, 4, buffer, 1000); // SPI DMA buffer is 1024, but if we read 1024... not work, so 1000 is ok
  int cnt = 0;
  for (int i = 0; i < 1000; i++)
    if (buffer[i] > 0) // count bit-ones
      cnt++;
  printf("%d ", cnt);
  delay(1000);
}