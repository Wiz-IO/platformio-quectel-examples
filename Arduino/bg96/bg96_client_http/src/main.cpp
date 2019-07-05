#include <Arduino.h>
#include <Device.h>
#include <Dss.h>
#include <txDns.h>
#include <txClient.h>
txClient cc;

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

void http_get(Client &client, char *host, int port, char *url)
{
  Serial1.printf("\n[GET] Connecting %s\n", host);
  client.connect(host, port);
  delay(100);
  Serial1.println("[GET] Send");
  client.print("GET ");
  client.print(url);
  client.print(" HTTP/1.1\r\nConnection: close\r\nHost:");
  client.print(host);
  client.print("\r\n\r\n");
  Serial1.println("[GET] Receive");
  while (1)
  {
    int R = client.read();
    if (R > 0)
      Serial1.print((char)R);
    else
      break;
  }
  client.stop();
  Serial1.println("\n[GET] DONE");
}

void setup()
{
  Serial1.begin(115200);
  Serial1.println("\nArduino Quectel BG96 - Client and Http");
  dev.waitSim();
  Serial1.printf("Sim ready: %u\n", seconds());
  dss.apn("gprs", "user", "pass");
  dss.open(RADIO_UNKNOWN);
  dss.begin();
  Serial1.printf("Data call started: %u\n", seconds());
  while (!dss.connected())
    delay(100);
  Serial1.printf("Internet ready: %u\n", seconds());
  http_get(cc, "wizio.eu", 80, "/iot.php");
}

void loop()
{
  delay(1000);
}

/* RESULT 
Arduino Quectel BG96 - Client and Http
Sim ready: 5
Data call started: 5
Data call ready
[DSS] IFACE     rmnet_data0
[DSS] LOCAL     10.64.244.149
[DSS] GATEWAY   10.64.244.150
[DSS] PRIMARY   10.118.50.100
[DSS] SECONDARY 10.118.50.120
Internet ready: 9
[GET] Connecting wizio.eu
[GET] Send
[GET] Receive
HTTP/1.1 200 OK
Date: Fri, 05 Jul 2019 06:29:59 GMT
Server: Apache
Upgrade: h2,h2c
Connection: Upgrade, close
Cache-Control: max-age=0
Expires: Fri, 05 Jul 2019 06:29:59 GMT
Content-Length: 47
Content-Type: text/html
[WIZIO.EU] Hello World ( 2019-07-05 09:29:59 )
[GET] DONE
*/