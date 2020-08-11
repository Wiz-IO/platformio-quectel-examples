#include <Arduino.h>

#define DALLAS

#ifdef DALLAS
#include "ds18S20.h"
TSDS18x20 DS18x20;
TSDS18x20 *pDS18x20 = &DS18x20;
/* PINNAME_GPIO1 */
#define DALLAS_A_PIN 9  /* select_arduino_pin */
#define DALLAS_M_PIN 28 /* select_mediatek_pin, see pinsMap[] variant_pins.c */
#else
#include "DHT.h"
DHT_Sensor DHT;
#define DHT_A_PIN /* select_arduino_pin */
#define DHT_M_PIN /* select_mediatek_pin, see pinsMap[] variant_pins.c */
#endif

void setup()
{
  int res;
  Serial.begin(115200, true);
  printf("Arduino Firmware: %s 2020 Georgi Angelov\n", Dev.version());
  pinMode(LED, OUTPUT); // PINNAME_NETLIGHT, blink
#ifdef DALLAS
  pinMode(DALLAS_A_PIN, INPUT);
  while ((res = DS18x20_Init(pDS18x20, DALLAS_M_PIN)))
  {
    printf("Init Error( %d )\n", res);
    delay(1000);
  }
#else
  pinMode(DHT_A_PIN, INPUT);
  DHTInit(&DHT, DHT_M_PIN);
#endif
}

void loop()
{
  int res;
  static unsigned led = 0;
  delay(1000);
  digitalWrite(LED, led);
  led = ~led;
#ifdef DALLAS
  if ((res = DS18x20_MeasureTemperature(pDS18x20)))
  {
    double v = DS18x20_TemperatureValue(pDS18x20);
    printf("Value %f\n", v);
  }
#else
  DHT_Sensor_Data DHT_Data;
  DHTRead(&DHT, &DHT_Data);
  printf("Temperature = %f, Humidity = %f\n", DHT_Data.temperature, DHT_Data.humidity);
#endif
}
