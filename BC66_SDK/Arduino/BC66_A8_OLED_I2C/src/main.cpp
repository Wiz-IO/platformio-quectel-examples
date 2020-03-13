#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SLAVE_ADDRESS 0x78 >> 1
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 64, &Wire);
// https://forum.arduino.cc/index.php?topic=375985.0

#include "animate.h"

void oled_init()
{
  display.begin(SSD1306_SWITCHCAPVCC, SLAVE_ADDRESS);
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("  Arduino");
  display.setTextColor(BLACK, WHITE);
  display.print(" Quectel  ");
  display.println("    BC66  ");
  display.setTextSize(1);
  display.setTextColor(WHITE, BLACK);
  display.println();
  display.println("      WizIO 2020");
  display.display();
  delay(4000);
}

void setup()
{
  Serial.begin(115200, true);
  Serial.println("Arduino Quectel BC66 2020 Georgi Angelov");
  oled_init();
}

void loop()
{
  for (int i = 0; i < 15; i++)
  {
    display.fillRect(0, 0, 128, 64, WHITE);
    display.drawBitmap(0, 0, &horse[i++][0], 128, 64, BLACK);
    display.display();
  }
}