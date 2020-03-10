/*  
  Arduino Quectel BC66
    Created on: 28.02.2020
        Author: Georgi Angelov
 */

#include <Arduino.h>
#include <IPAddress.h>
#include <LHTTP.h>
LHTTP c;
#define HOST "google.com"

void blink_loop()
{
  static int begin = 0, led = 0;
  if (millis() - begin > 100)
  {
    digitalWrite(LED, ++led ^ 2);
    begin = millis();
  }
}

void client_loop()
{
  static int timeout = 0;
  static unsigned int interval = 100000UL;
  static bool who = true;
  if (seconds() - interval > 30)
  {
    Serial.printf("\nConnecting %s ... ", who ? "HTTP" : "HTTPS");
    interval = seconds();
    if (who)
      c.get("http://wizio.eu/iot.php");
    else
      c.get("https://raw.githubusercontent.com/Wiz-IO/LIB/master/test_https");
    Serial.printf("Elapsed: %d sec\n", seconds() - interval);

    Serial.printf("%s", c.get_response());
    who = !who;
  }
}

void setup()
{
  Serial.begin(115200, true);                                                 // and enable ::printf()
  Serial.printf("Arduino Firmware: %s 2020 Georgi Angelov\n", Dev.version()); // Hello World
  Dev.noSleep(true);                                                          // PSM
  Dev.band(8);                                                                // search for 900 MHz
  Dev.dns();                                                                  // google dns
  Dev.cereg(true);                                                            // wait network
  pinMode(LED, OUTPUT);
  IPAddress ip;
  while (0 == (ip = getHostByName(HOST)))
    delay(1000);
  Serial.print(HOST " IP: ");
  Serial.println(ip);
  Serial.println("--- TEST BEGIN ---\n");
}

void loop()
{
  client_loop();
  blink_loop();
}

/*
Arduino Firmware: BC66NBR01A07 2020 Georgi Angelov
google.com IP: 172.217.169.110
--- TEST BEGIN ---
Connecting HTTP ... Elapsed: 2 sec
[WIZIO.EU] Hello World ( 2020-03-10 08:52:58 )
Connecting HTTPS ... Elapsed: 16 sec
[GITHUB] https test #
*/