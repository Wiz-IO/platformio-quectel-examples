/*  
  Arduino Quectel BC66
    Created on: 28.02.2020
        Author: Georgi Angelov
 */

#include <Arduino.h>

void onTimer(u32 timerId, void *param)
{
  static bool led = 0;
  digitalWrite(LED, led);
  led = !led;
}
#include <LTIMER.h>
LTIMER tmr(TIMER_1, onTimer);

void on_psm_eint(void *user) {}

int var __attribute__((section(".sram"))); // put variable in SRAM, MAXIMUM 256 bytes

void setup()
{
  Serial.begin(115200);
  Serial.printf("Arduino Firmware: %s 2020 Georgi Angelov\n", Dev.version());
  Dev.band(8);       // search band 900 MHz
  Dev.noSleep(true); // enable deep sleep or Psm.Sleep();

  int res = Psm.powerReason();
  Serial.printf("Wakeup form %s\n", res == QL_DEEP_SLEEP ? "Deep sleep" : "Reset");
  if (QL_DEEP_SLEEP == res)
    var++;
  else
    var = 0; // clear value in SRAM
  Serial.printf("SRAM value: %d\n", var);

  int period = 0, active = 0;
  Psm.get(&period, &active);
  Serial.printf("Psm: %d sec, %d sec\n", period, active);

  period = 0;
  Psm.getedrx(&period);
  Serial.printf("eDrx: %d sec\n", period);

  Psm.Sleep(); // enable deep sleep
  Psm.rtc(10); // wakeup after 10 seconds
  Psm.pin();   // wakeup from pin

  Dev.cereg();
  Serial.printf("Net Ready\n");
  pinMode(LED, OUTPUT);
  tmr.start(200);
}

void loop()
{
  delay(1000);
  Serial.printf("%u ", seconds());
}

/*
Arduino Firmware: BC66NBR01A07 2020 Georgi Angelov
Wakeup form Reset
SRAM value: 0
Psm: 10800 sec, 120 sec
eDrx: 81 sec
Net Ready
7 8 9 10 11 12 13 ...
*/