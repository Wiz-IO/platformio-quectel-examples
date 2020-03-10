#include <Arduino.h>

void on_urc_response(ST_MSG *m)
{
  switch (m->param1)
  {
  case URC_SIM_CARD_STATE_IND:                                    // 2
    Serial.printf("  <sim> %d ( %d s )\n", m->param2, seconds()); // seconds() as millis()
    break;
  case URC_EGPRS_NW_STATE_IND: // 3
    Serial.printf("  <net> %d ( %d s )\n", m->param2, seconds());
    break;
  default:
    Serial.printf("  <urc> %d, %d, %d ( %d s )\n", m->message, m->param1, m->param2, seconds());
  }
}

int on_at_response(char *line, u32 len, void *userData)
{
  printf("%s", line); // just print
  if (Ql_RIL_FindString(line, len, (char *)"OK"))
    return RIL_ATRSP_SUCCESS;
  if (Ql_RIL_FindString(line, len, (char *)"ERROR"))
    return RIL_ATRSP_FAILED;
  return RIL_ATRSP_CONTINUE;
}

String imei;
void setup()
{
  Serial.begin(115200, true);                                                 // and enable ::printf()
  Serial.printf("Arduino Firmware: %s 2020 Georgi Angelov\n", Dev.version()); // Hello World
  Dev.urc(on_urc_response);                                                   // not important
  Dev.getImei(imei);                                                          // see LDEV.h
  Serial.printf("IMEI: %s\n", imei.c_str());                                  // ...
  Serial.printf("Battery: %d mV\n", Dev.battery());                           // ...
  Dev.send("ATI", 100, on_at_response);                                       // basic AT commands
  Dev.noSleep(true);                                                          // PSM
  Dev.band(8);                                                                // search for 900 MHz
  Dev.dns();                                                                  // google dns
  Dev.cereg(true);                                                            // wait network
  Rtc.printTo(Serial);                                                        // after connection, RTC date/time is valid
  pinMode(LED, OUTPUT);
}

void loop()
{
  static unsigned led = 0;
  digitalWrite(LED, ++led);
  delay(500);
}

/*
    Arduino Firmware: BC66NBR01A10 2020 Georgi Angelov
    IMEI: 867997030127091
    Battery: 3292 mV
    Quectel_Ltd
    Quectel_BC66
    Revision: BC66NBR01A10
    OK
      <sim> 1 ( 1 s )
      <net> 2 ( 1 s )
      <net> 1 ( 4 s )
    Fri Feb 28 08:49:51 2020
*/