/*  
  Arduino Quectel BC66
    Created on: 28.02.2020
        Author: Georgi Angelov
 */

#include <Arduino.h>
#include <IPAddress.h>
#include <LMQTT.h>
LMQTT mqtt;
#define HOST "mqtt.eclipse.org"
#define PORT "1883"

void blink_loop()
{
  static int begin = 0, led = 0;
  if (millis() - begin > 100) // ms
  {
    digitalWrite(LED, ++led ^ 2);
    begin = millis();
  }
}

void setup()
{
  Serial.begin(115200, true);
  Serial.printf("Arduino Firmware: %s 2020 Georgi Angelov\n", Dev.version());
  Dev.noSleep(true);
  Dev.band(8);
  Dev.dns();
  pinMode(LED, OUTPUT);
  IPAddress ip;
  while (0 == (ip = getHostByName(HOST))) // work as wait cereg
    delay(500);
  Serial.println();
  Serial.print(HOST " IP: ");
  Serial.println(ip);
  mqtt.begin("BC66-QUECTEL"); // client, user, pass
}

void onMessage(MessageData *m)
{
  Serial.printf("[%.*s] %.*s\n",
                m->topicName->lenstring.len,
                (char *)m->topicName->lenstring.data,
                m->message->payloadlen,
                (char *)m->message->payload);
}

void mqtt_loop()
{
  static uint32_t begin = 0;
  char data[64];
  if (mqtt.connected())
  {
    if (seconds() - begin > 60) // seconds interval
    {
      begin = seconds();
      snprintf(data, 64, "Seconds: %08u, Battery : %u", seconds(), Dev.battery());
      mqtt.publish("wiz/output", data);
      printf("[PUB] %s\n", data);
    }
    else
      mqtt.loop(100);
  }
}

void mqtt_reconnect()
{
  if (!mqtt.connected())
  {
    Serial.printf("\nConnecting to %s\n", HOST);
    if (mqtt.connect(HOST, PORT))
    {
      Serial.printf("Connected\n");
      mqtt.subscribe("wiz/input", onMessage);
      mqtt.publish("wiz/output", "Hello World");
    }
    else
    {
      delay(10000);
    }
  }
}

void loop()
{
  mqtt_reconnect();
  mqtt_loop();
  blink_loop();
}

/*
Arduino Firmware: BC66NBR01A07 2020 Georgi Angelov
mqtt.eclipse.org IP: 137.135.83.217
Connecting to mqtt.eclipse.org
Connected
[PUB] Seconds: 00000061, Battery : 32452
*/