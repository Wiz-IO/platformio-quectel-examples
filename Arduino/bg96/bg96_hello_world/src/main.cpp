#include <Arduino.h>
#include <Device.h>
Device dv;

void setup()
{
    Serial1.begin(115200);
    Serial1.debug();
    Serial1.println("\n[APP] Setup");
    dv.waitSim();
    Serial1.println("[APP] Sim ready");
    String S;
    dv.getVersion(S);
    Serial1.printf("[APP] Version: %s\n", S.c_str());
    dv.getIMEI(S);
    Serial1.printf("[APP] IMEI: %s\n", S.c_str());
    dv.getIMSI(S);
    Serial1.printf("[APP] IMSI: %s\n", S.c_str());
}

void loop()
{
    Serial1.println("[APP] Loop");
    delay(10000);
}

/* RESULT

[APP] Setup
[APP] Sim ready
[APP] Version: BG96MAR02A09M1G
[APP] IMEI: 866425030081451
[APP] IMSI: 284013911648811
[APP] Loop

*/