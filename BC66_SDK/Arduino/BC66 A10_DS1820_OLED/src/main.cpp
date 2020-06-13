#include <Arduino.h>
#include <ds_uart.h>
#include <ds_onewire.h>
#include <ds_18x20.h>
#include <ds_common.h>

#include <dallas.h>
OWU ds;

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SLAVE_ADDRESS 0x78 >> 1
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 64, &Wire);

void ds_loop()
{
  char buf[10];
  int16_t temp_dc;
  ow_reset();
  if (DS18X20_start_meas(DS18X20_POWER_EXTERN, NULL) == DS18X20_OK)
  {
    while (DS18X20_conversion_in_progress() == DS18X20_CONVERTING)
    {
      delay(50); /* It will take a while */
    }
    display.clearDisplay();
    display.setCursor(10, 20);
    if (DS18X20_read_decicelsius(NULL, &temp_dc) == DS18X20_OK)
    {
      sprintf(buf, "%2d.%01d C", temp_dc / 10, temp_dc > 0 ? temp_dc % 10 : -temp_dc % 10);
      display.println(buf);
      display.display();
      return;
    }
  }
  printf("\nMEASURE FAILED!\n");
  display.println("ERROR");
  display.display();
}

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
  delay(1000);
  display.setTextSize(3);
  display.setTextColor(WHITE);
}

void setup()
{
  Serial.begin(115200, true);
  printf("Arduino Firmware: %s 2020 Georgi Angelov\n", Dev.version());
  printf("DS18S20 over UART\n");
  pinMode(LED, OUTPUT);
  Serial1.begin(9600);
  ow_init(0);
  oled_init();
}

void loop()
{
  static unsigned led = 0;
  digitalWrite(LED, ++led);
  //delay(100);
  ds_loop();
}