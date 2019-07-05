#include <Arduino.h>
#include <Device.h>
#include <Dss.h>
#include <txDns.h>
#include <txMqtt.h>

MqttClient mqtt; // is Internal API Client

Dns dns;
Device dev;
String IMEI;
void onDssConnect(void);
Dss dss(onDssConnect);

void onDssConnect(void)
{
  Serial1.println("Data call ready");
  Serial1.debug();
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

/*
  Amazon IoT Core - Manage
  Create Thing and Certificates, Download it and Activate
  Policies Allow
  Interact - get HTTPS URL for host name
*/

#define MQTT_PORT 8883
#define MQTT_HOST_NAME "<YOUR-HOST>.amazonaws.com"
#define SSL_CERT_FILE_NAME "/datatx/<YOUR>-certificate.pem.crt"
#define SSL_KEY_FILE_NAME "/datatx/<YOUR>-private.pem.key"

/*
    UPLOAD CERTIFICATE and PRIVATE to module /datatx/
    API will move files to secured EFS
*/

void onConnect(qapi_Net_MQTT_Hndl_t mqtt, int32_t reason)
{
  if (0 == reason)
    Serial1.println(" DONE");
}

void onPublish(qapi_Net_MQTT_Hndl_t mqtt,
               enum QAPI_NET_MQTT_MSG_TYPES msgtype,
               int qos,
               uint16_t msg_id)
{
  Serial1.printf("[MQTT] onPublish %d\n", msgtype);
}

void onMessage(qapi_Net_MQTT_Hndl_t mqtt, int32_t reason,
               const uint8_t *topic, int32_t topic_length,
               const uint8_t *msg, int32_t msg_length,
               int32_t qos, const void *sid)
{

  Serial1.printf("[MQTT] onMessage\n");
  Serial1.printf("   Topic: %.*s\n", topic_length, topic);
  Serial1.printf("   Message: %.*s\n", msg_length, msg);
}

void publish()
{
  char buff[256];
  snprintf(buff, sizeof(buff), "Hello World : Quectel BG96 : %d", seconds()); // publish some data
  if (mqtt.pub("wizio/pub", buff, strlen(buff)))
  {
    Serial1.printf("MQTT Publish: %s\n", buff);
  }
}

void setup()
{
  Serial1.begin(115200);
  Serial1.println("\nArduino Quectel BG96 - MQTT Amazon IoT Core");
  dev.waitSim();
  Serial1.println("Sim ready");
  dss.apn("gprs", "user", "pass");
  dss.open(RADIO_UNKNOWN);
  dss.begin();
  Serial1.println("Data call started");
  while (!dss.connected())
    delay(100);
  Serial1.println("Internet ready");
  dev.getIMEI(IMEI);

  IPAddress MQTT_IP;
  if (dns.query(MQTT_HOST_NAME, MQTT_IP))
  {
    mqtt.server((uint32_t)MQTT_IP, MQTT_PORT);
    mqtt.client(IMEI);
    mqtt.onMessage = onMessage;
    mqtt.secured();
    mqtt.certificate(SSL_CERT_FILE_NAME, SSL_KEY_FILE_NAME);
    mqtt.config.ca_list = "empty";
    Serial1.print("Amazon ");
    Serial1.println(MQTT_IP);
    Serial1.print("MQTT Connecting...");
    while (!mqtt.connect())
    {
      Serial1.print(".");
      delay(60000);
    }
    Serial1.println(" DONE");
    mqtt.sub("wizio/sub");
    publish();
  }
}

void loop()
{
  static int t = 0;
  if (seconds() - t > 60) // publish interval in seconds
  {
    t = seconds();
    publish();
  }
  delay(1000);
}

/* RESULT 
Arduino Quectel BG96 - MQTT Amazon
Sim ready
Data call started
Data call ready
[DSS] IFACE     rmnet_data0
[DSS] LOCAL     10.42.129.20
[DSS] GATEWAY   10.42.129.21
[DSS] PRIMARY   10.118.50.120
[DSS] SECONDARY 10.118.50.100
Internet ready
Amazon 3.18.39.170
MQTT Connecting... DONE
MQTT Publish: Hello World : Quectel BG96 : 10
*/