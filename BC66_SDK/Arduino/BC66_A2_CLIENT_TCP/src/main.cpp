#include <Arduino.h>
#include <LClient.h>
LClient c;
#define HOST "wizio.eu"
#define URL "/iot.php"

void on_urc_response(ST_MSG *m)
{
  Serial.printf("  <urc> %d, %d, %d ( %d s )\n", m->message, m->param1, m->param2, seconds());
}

void blink_loop()
{
  static int begin = 0, led = 0;
  if (millis() - begin > 100)
  {
    digitalWrite(LED, ++led);
    begin = millis();
  }
}

void client_loop()
{
  static int timeout = 0;
  static unsigned int interval = 100000UL;
  if (seconds() - interval > 30)
  {
    interval = seconds();
    Serial.println("\nConnecting to " HOST);
    if (c.connect(HOST, 80))
    {
      timeout = seconds();
      char data[] = "GET " URL " HTTP/1.1\r\nHost:" HOST "\r\n\r\n";
      int res = c.write(data, strlen(data));
      delay(100);
      while (seconds() - timeout < 10)
      {
        while (c.available())
        {
          Serial.write(res = c.read());
          if (res == ')') // eof ?!
            goto end;
        }
        delay(1);
      }
      Serial.println("timeout");
    end:
      c.stop();
    }
    else
      Serial.println("problem");
  }
}

void setup()
{
  Serial.begin(115200, true);                                                 // and enable ::printf()
  Serial.printf("Arduino Firmware: %s 2020 Georgi Angelov\n", Dev.version()); // Hello World
  Dev.urc(on_urc_response);                                                   // not important
  Dev.noSleep(true);                                                          // PSM
  Dev.band(8);                                                                // search for 900 MHz
  Dev.dns();                                                                  // google dns
  Dev.cereg(true);                                                            // wait network
  Rtc.printTo(Serial);                                                        // after connection, RTC date/time is valid
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