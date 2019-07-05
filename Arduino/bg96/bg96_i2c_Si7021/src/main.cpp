#include <Arduino.h>
#include <Device.h>
Device dev;
#include <Wire.h>

// Sensor: Si7021-A20-GM1
// Store: https://store.comet.bg/en/Catalogue/Product/49730/#e30%3D
// PDF: https://store.comet.bg/download-file.php?id=16187

#define SLAVE_ADDRESS 0x40 /* 7 bit address */
void sensorInit()
{
  Wire.begin();
  Wire.beginTransmission(SLAVE_ADDRESS);
  Wire.endTransmission();
  delay(300);
}

float readTemperature()
{
  uint8_t data[2] = {0};
  Wire.beginTransmission(SLAVE_ADDRESS);
  Wire.write(0xF3);
  Wire.endTransmission();
  delay(500);
  Wire.requestFrom(SLAVE_ADDRESS, 2);
  if (Wire.available() == 2)
  {
    data[0] = Wire.read();
    data[1] = Wire.read();
  }
  float temp = ((data[0] * 256.0) + data[1]);
  float ctemp = ((175.72 * temp) / 65536.0) - 46.85;
  float ftemp = ctemp * 1.8 + 32;
  return ctemp;
}

float readHumidity()
{
  uint8_t data[2] = {0};
  Wire.beginTransmission(SLAVE_ADDRESS);
  Wire.write(0xF5);
  Wire.endTransmission();
  delay(500);
  Wire.requestFrom(SLAVE_ADDRESS, 2);
  if (Wire.available() == 2)
  {
    data[0] = Wire.read();
    data[1] = Wire.read();
  }
  float humidity = ((data[0] * 256.0) + data[1]);
  humidity = ((125 * humidity) / 65536.0) - 6;
  return humidity;
}

void setup()
{
  Serial1.begin(115200);
  Serial1.println("\nArduino Quectel BG96 - Sensor: Si7021-A20-GM1");
  dev.waitSim();
  sensorInit();
  Serial1.println("Sensor ready");
}

void loop()
{
  float T, H;
  T = readTemperature();
  H = readHumidity();
  Serial1.printf("SENSOR: Temperature = %f, Humidity = %f\n", T, H);
  delay(2000);
}

/* RESULT 
Arduino Quectel BG96 - Sensor: Si7021-A20-GM1
Sensor ready
SENSOR: Temperature = 29.528786, Humidity = 48.409027
SENSOR: Temperature = 28.225685, Humidity = 48.424286
*/