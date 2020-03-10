#include <Arduino.h>
#include <IPAddress.h>
#include <LMQTTSecure.h>
#include "certs.h"
LMQTTSecure mqtt;
#define HOST "mqtt.eclipse.org"
#define MQTT "https://" HOST ":8883/"

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

void onDisconnect(void) { Serial.println("Disconnect"); }

void onMessage(MessageData *m)
{
  Serial.printf("[%.*s] %.*s\n",
                m->topicName->lenstring.len, (char *)m->topicName->lenstring.data,
                m->message->payloadlen, (char *)m->message->payload);
}

void setup()
{
  Serial.begin(115200, true);
  Serial.printf("Arduino Firmware: %s 2020 Georgi Angelov\n", Dev.version());
  Dev.urc(on_urc_response); 
  Dev.noSleep(true);
  Dev.band(8);
  Dev.dns();
  pinMode(LED, OUTPUT);
  IPAddress ip;
  while (0 == (ip = getHostByName(HOST))) // work as wait cereg
    delay(500);
  Serial.print(HOST " IP: ");
  Serial.println(ip);
  mqtt.onDisconnect = onDisconnect;
}

void loop()
{
  blink_loop();
  if (mqtt.connected())
  {
    mqtt.loop();
    static uint32_t begin = 0;
    char data[64];
    if (seconds() - begin > 60) // pub interval
    {
      begin = seconds();
      snprintf(data, 64, "Seconds: %08u, Battery : %u", seconds(), Dev.battery());
      mqtt.publish("outWiz", data);
      printf("[PUB] %s\n", data);
    }
  }
  else
  {
    Serial.print("Connecting...");
    if (mqtt.connect(MQTT, server_ca, NULL, NULL))
    {
      Serial.println("DONE");
      mqtt.publish("outWiz", "Hello World");
      mqtt.subscribe("inWiz", onMessage);
    }
    else
    {
      Serial.println("ERROR");
      delay(10000);
    }
  }
}
