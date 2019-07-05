#include <Arduino.h>
#include <Device.h>
#include <Dss.h>
#include <txDns.h>

Dns dns;
Device dev;
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

void setup()
{
  Serial1.begin(115200);
  Serial1.println("\nArduino Quectel BG96 - DSS and DNS");
  dev.waitSim();
  Serial1.printf("Sim ready: %u\n", seconds());
  dss.apn("gprs", "user", "pass");
  dss.open(RADIO_UNKNOWN);
  dss.begin();
  Serial1.printf("Data call started: %u\n", seconds());
  while (!dss.connected())
    delay(100);
  Serial1.printf("Internet ready: %u\n", seconds());
}

void loop()
{
  delay(1000);
}

/* RESULT
Arduino Quectel BG96 - DSS and DNS
Sim ready: 5
Data call started: 5
Data call ready
[DSS] IFACE     rmnet_data0
[DSS] LOCAL     10.72.46.39
[DSS] GATEWAY   10.72.46.40
[DSS] PRIMARY   10.118.50.100
[DSS] SECONDARY 10.118.50.120
Internet ready: 10
*/