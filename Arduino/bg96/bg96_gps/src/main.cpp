#include <Arduino.h>
#include <Device.h>
Device dev;
#include <NMEA.h>
NMEA nmea;

struct minmea_sentence_rmc rmc;
void onNMEA(char *line)
{
  switch (minmea_sentence_id(line, false))
  {
  case MINMEA_SENTENCE_RMC:
    if (minmea_parse_rmc(&rmc, line))
    {
      if (rmc.valid)
        Serial1.printf("GPS: LAT = %f, LON = %f\n", minmea_tocoord(&rmc.latitude), minmea_tocoord(&rmc.longitude));
    }
  }
}

void setup()
{
  Serial1.begin(115200);
  Serial1.println("\nArduino Quectel BG96 - GPS");
  dev.waitSim();
  nmea.begin(onNMEA);
  Serial1.println("GPS started");
}

void loop()
{
  delay(2000);
}

/* RESULT 
Arduino Quectel BG96 - GPS
GPS started
GPS: LAT = 42.644016, LON = 23.404339
GPS: LAT = 42.643997, LON = 23.404299
*/