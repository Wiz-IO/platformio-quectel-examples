#include <Arduino.h>

/* APN
  is set/saved as default in NVDM (required once)
  AT*MCGDEFCONT="IP","apn_from_operator"

  Enable scrambling function (required once)
  AT+QSPCHSC=1  
*/

int counter __attribute__((section(".sram"))); // value in SRAM, MAXIMUM 256 bytes

void setup()
{
  if (Ql_GetPowerOnReason() != QL_DEEP_SLEEP)
  {
    counter = 1; // init SRAM values on RESET
  }

  int t = millis();
  String imei;
  Serial.begin(115200);
  Serial.printf("Arduino %s\n", Dev.getVersion());
  Serial.printf("Wakeup [%d] from %s\n", counter++, Ql_GetPowerOnReason() == 1 ? "Sleep" : "Reset");
  Dev.getImei(imei);
  Serial.printf("IMEI: %s\n", imei.c_str());
  Dev.waitSimReady();
  Serial.println("SIM Ready");
  Dev.waitCereg();
  delay(200); // must
  Serial.println("NET Ready");
  Serial.printf("Elapsed: %d mili seconds\n", millis() - t);

  int active, period;
  active = Psm.getCellPsmActiveTime();
  period = Psm.getCellPsmPeriodicTime();
  Serial.printf("PSM: A=%d sec, P = %d sec\n", active, period);
  Psm.Sleep(); // enable

  //Psm.wakeup(100); // enable wakeup form timer or Psm.pin(...)

  //To enter in PSM modem must be OFF, no transfers, socket closed...
}

void loop()
{
  static int led = 0;  
  Serial.print(">"); // must be stop after period time
  digitalWrite(LED, ++led & 1);
  delay(1000);
}

/** RESULT **************************************************************


***************************************************************/
