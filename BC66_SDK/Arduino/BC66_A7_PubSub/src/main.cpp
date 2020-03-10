#include <Arduino.h>
/*
    Install Library:
    PubSubClient by Nick O'Leary
    https://github.com/knolleary
*/
#include <PubSubClient.h>

#define HOST "mqtt.eclipse.org"

#define MQTT_SECURE

#ifdef MQTT_SECURE
#include <LClientSecure.h>
#include "certs.h"
#define PORT 8883
LClientSecure client;
#else
#include <LClient.h>
#define PORT 1883
LClient client;
#endif

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.printf("[RECEIVE] (%s) %.*s\n", topic, length, payload);
}

PubSubClient mqtt(HOST, PORT, callback, client);

void blink_loop()
{
  static int begin = 0, led = 0;
  if (millis() - begin > 100)
  {
    digitalWrite(LED, ++led);
    begin = millis();
  }
}

void setup()
{
  Serial.begin(115200, true);
  Serial.printf("Arduino Firmware: %s 2020 Georgi Angelov\n", Dev.version());

#ifdef MQTT_SECURE
  client.set_ca_certificate(server_ca);
#endif

  Dev.noSleep(true);
  Dev.band(8);
  Dev.dns();
  pinMode(LED, OUTPUT);

  IPAddress ip;
  while (0 == (ip = getHostByName(HOST)))
    delay(500);
  Serial.print(HOST " IP: ");
  Serial.println(ip);

  Serial.printf("Connecting to %s : %d\n", HOST, PORT);
  delay(10000);
  while (1)
  {
    if (mqtt.connect("Arduino-Quectel-Client", 0, 0))
    {
      Serial.println("Connected");
      mqtt.publish("outWiz", "Hello World");
      Serial.println("Publish...");
      mqtt.subscribe("inWiz");
      Serial.println("Subscribe...");
      break;
    }
    Serial.println("ERROR");
    delay(10 * 1000); // reconnect
  }
}

void loop()
{
  blink_loop();
  mqtt.loop();
}

/*
  Arduino Firmware: BC66NBR01A10 2020 Georgi Angelov
  mqtt.eclipse.org IP: 137.135.83.217
  Connecting to mqtt.eclipse.org : 8883
  Connected
  Publish...
  Subscribe...
  [RECEIVE] (inWiz) HELLO WORLD
*/