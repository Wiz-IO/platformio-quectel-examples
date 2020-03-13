#include <Arduino.h>
#include <Wire.h>

#define SI7021 0x40

float readTemperature()
{
  uint8_t data[2] = {0};
  Wire.beginTransmission(SI7021);
  Wire.write(0xF3);
  Wire.endTransmission();
  delay(500);
  Wire.requestFrom(SI7021, 2);
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
  Wire.beginTransmission(SI7021);
  Wire.write(0xF5);
  Wire.endTransmission();
  delay(500);
  Wire.requestFrom(SI7021, 2);
  if (Wire.available() == 2)
  {
    data[0] = Wire.read();
    data[1] = Wire.read();
  }
  float humidity = ((data[0] * 256.0) + data[1]);
  humidity = ((125 * humidity) / 65536.0) - 6;
  return humidity;
}

void displayData(float temperature, float humidity)
{
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.print(" % RH");
  Serial.print("\t");
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" C");
}

void setup()
{
  Serial.begin(115200, true);
  Serial.println("Arduino Quectel BC66 2020 Georgi Angelov");
  Dev.noSleep();
  delay(2000);
  Wire.begin();
  Wire.beginTransmission(SI7021);
  Wire.endTransmission();
  delay(500);
}

void loop()
{
  float temp = readTemperature();
  float hum = readHumidity();
  displayData(temp, hum);
  delay(3000);
}