#include <Arduino.h>
#include <nbUdp.h>

/*
    Install Library:
    CoAP simple library by Hirotaka Niisato
    https://github.com/hirotakaster/CoAP-simple-library
*/
#include <coap.h> 

nbUDP U;
Coap coap(U);
void callback_response(CoapPacket &packet, IPAddress ip, int port) {
    Serial.printf("[COAP] Response: %.*s\n", packet.payloadlen, packet.payload);
}

void setup()
{
  String imei;
  Dev.noSleep();
  Serial.begin(115200);
  Serial.printf("Arduino %s\n", Dev.getVersion());
  pinMode(LED, OUTPUT);
  Dev.getImei(imei);
  Serial.printf("IMEI %s\n", imei.c_str());
  Dev.waitSimReady();
  Serial.println("Sim Ready");
  Dev.waitCereg();
  delay(200); // must
  Serial.println("Net Ready");
    coap.response(callback_response);
    coap.start();
    coap.get(IPAddress(0, 0, 0, 0), 5683, (char *)"Arduino Quectel"); // your coap server IP, port
}

void loop() {
    static int state = 0;
    char buffer[32];
    if (state % 2000 == 0)
        coap.get(IPAddress(0, 0, 0, 0), 5683, (char *)"datetime"); // your coap server IP, port        
    state++;
    coap.loop();
}

/*********************************************** 
NODE.JS 
npm install coap --save
npm install date-time
***********************************************/

/*********************************************** 

const dateTime = require('date-time');

const coap = require('coap')
  , server = coap.createServer()

server.on('request', function (req, res) {
  key = req.url.split('/')[1]
  console.log(key)
  if (key == "datetime")
    res.end(dateTime({ showTimeZone: true }))
  else
    res.end("saved")
  console.log('--------------------------------')
})

server.listen(function () {
  console.log('================================')
  console.log('WizIO CoAP Test Server Started')
  console.log(dateTime())
  console.log('================================')
})

*********************************************** /


/* RESULT **************************************

  Arduino BC66NBR01A04
  IMEI 867997030026081
  Sim Ready
  Net Ready
  [COAP] Response: saved
  [COAP] Response: 2019-04-03 10:31:16 UTC+3

***********************************************/