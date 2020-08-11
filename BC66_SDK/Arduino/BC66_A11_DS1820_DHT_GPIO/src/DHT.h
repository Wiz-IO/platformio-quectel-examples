
/*
    Driver for the temperature and humidity sensor DHT11 and DHT22
    Official repository: https://github.com/CHERTS/esp8266-dht11_22
    Copyright (C) 2014 Mikhail Grigorev (CHERTS)
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

    https://github.com/CHERTS/esp8266-dht11_22/blob/master/driver/dht22.c

    EDIT for Quectel BC66: Georgi Angelov 03.08.2020
*/

#ifndef _DHT_h
#define _DHT_h
#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <hal.h> 
#include <os_wizio.h> // for delay

    typedef enum
    {
        DHT11,
        DHT22
    } DHTType;

    typedef struct
    {
        float temperature;
        float humidity;
    } DHT_Sensor_Data;

    typedef struct
    {
        uint8_t pin;
        DHTType type;
    } DHT_Sensor;

#define DHT_MAXTIMINGS 10000
#define DHT_BREAKTIME 20
#define DHT_MAXCOUNT 32000

#define DHT_DEBUG 1

    bool DHTInit(DHT_Sensor *sensor, uint32_t MTK_PIN);
    bool DHTRead(DHT_Sensor *sensor, DHT_Sensor_Data *output);
    char *DHTFloat2String(char *buffer, float value);

#ifdef __cplusplus
}
#endif
#endif