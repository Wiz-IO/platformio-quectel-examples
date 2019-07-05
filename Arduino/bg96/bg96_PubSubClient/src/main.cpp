#include <Arduino.h>
#include <Device.h>
#include <Dss.h>
#include <txDns.h>
#include <txClient.h>
txClient cc;

Dns dns;
Device dev;
String IMEI;
void onDssConnect(void);
Dss dss(onDssConnect);
void onDssConnect(void)
{
  Serial1.println("Data call ready");
  IPAddress local, gateway, primary, secondary;
  String iface;
  dss.getIFace(iface);
  Serial1.printf("[DSS] IFACE     %s\n", iface.c_str());
  dss.get(local, gateway, primary, secondary);
  Serial1.printf("[DSS] LOCAL     %s\n", local.toString().c_str());
  Serial1.printf("[DSS] GATEWAY   %s\n", gateway.toString().c_str());
  Serial1.printf("[DSS] PRIMARY   %s\n", primary.toString().c_str());
  Serial1.printf("[DSS] SECONDARY %s\n", secondary.toString().c_str());
  dns.start();                          // start DNS client
  dns.add(DNS_V4_PRIMARY, "8.8.8.8");   // can be used primary
  dns.add(DNS_V4_SECONDARY, "8.8.4.4"); // can be used secondary
}

// Nick O'Leary Library http://knolleary.net
#include <PubSubClient.h>
PubSubClient mqtt("iot.eclipse.org", 1883, cc); // https://iot.eclipse.org/getting-started/

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial1.printf("[MSG] <%s> %.*s\n", topic, length, payload);
}

void reconnect()
{
  while (!mqtt.connected())
  {
    Serial1.println("[MQTT] Connecting to Eclipse...");
    Serial1.printf("[MQTT] ClientID: %s\n", IMEI.c_str());
    if (mqtt.connect(IMEI.c_str()))
    {
      Serial1.println("[MQTT] Connected");
      mqtt.publish("output", "Hello world");
      mqtt.subscribe("input");
    }
    else
    {
      Serial1.printf("[ERROR] MQTT Connect: %d", (int)mqtt.state());
      delay(20000); // Wait reconnect
    }
  }
}

void setup()
{
  Serial1.begin(115200);
  Serial1.println("\nArduino Quectel BG96 - PubSubClient");
  dev.waitSim();
  Serial1.printf("Sim ready: %u\n", seconds());
  dss.apn("gprs", "user", "pass");
  dss.open(RADIO_UNKNOWN);
  dss.begin();
  Serial1.printf("Data call started: %u\n", seconds());
  while (!dss.connected())
    delay(100);
  Serial1.printf("Internet ready: %u\n", seconds());
  dev.getIMEI(IMEI);
  mqtt.setCallback(callback);
}

void loop()
{
  if (!mqtt.connected())
    reconnect();
  mqtt.loop();
}

/* RESULT 
MQTTlens
https://chrome.google.com/webstore/detail/mqttlens/hemojaaeigabkbcookmlgmdigohjobjm/related?hl=en

Arduino Quectel BG96 - PubSubClient
Sim ready: 5
Data call started: 5
Data call ready
[DSS] IFACE     rmnet_data0
[DSS] LOCAL     10.36.7.60
[DSS] GATEWAY   10.36.7.61
[DSS] PRIMARY   10.118.50.120
[DSS] SECONDARY 10.118.50.100
Internet ready: 8
[MQTT] Connecting to Eclipse...
[MQTT] ClientID: 866425030081451
[MQTT] Connected
[MSG] <input> Hello World
[MSG] <input> 1
[MSG] <input> 2
[MSG] <input> 3
[MSG] <input> 4
*/