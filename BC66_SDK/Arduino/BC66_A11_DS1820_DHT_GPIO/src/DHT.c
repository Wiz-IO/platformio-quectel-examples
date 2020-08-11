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

#include "DHT.h"

#ifdef DHT_DEBUG
#undef DHT_DEBUG
#define DHT_DEBUG(...) printf(__VA_ARGS__);
#else
#define DHT_DEBUG(...)
#endif

int CNT[64] = {0};

static inline float scale_humidity(DHTType sensor_type, int *data)
{
    if (sensor_type == DHT11)
    {
        return (float)data[0];
    }
    else
    {
        float humidity = data[0] * 256 + data[1];
        return humidity /= 10;
    }
}

static inline float scale_temperature(DHTType sensor_type, int *data)
{
    if (sensor_type == DHT11)
    {
        return (float)data[2];
    }
    else
    {
        float temperature = data[2] & 0x7f;
        temperature *= 256;
        temperature += data[3];
        temperature /= 10;
        if (data[2] & 0x80)
            temperature *= -1;
        return temperature;
    }
}

char *DHTFloat2String(char *buffer, float value)
{
    sprintf(buffer, "%d.%d", (int)(value), (int)((value - (int)value) * 100));
    return buffer;
}

bool DHTRead(DHT_Sensor *sensor, DHT_Sensor_Data *output)
{
    int counter = 0;
    int laststate = 1;
    int i = 0;
    int j = 0;
    int checksum = 0;
    int data[100];
    data[0] = data[1] = data[2] = data[3] = data[4] = 0;

    hal_gpio_set_direction(sensor->pin, 1); // OUTPUT

    // Wake up device, 250ms of high
    //GPIO_OUTPUT_SET(pin, 1);
    hal_gpio_set_output(sensor->pin, 1);

    DELAY_M(250); // Hold low for 20ms

    //GPIO_OUTPUT_SET(pin, 0);
    hal_gpio_set_output(sensor->pin, 0);

    DELAY_M(20); // High for 40ms

    //GPIO_OUTPUT_SET(pin, 1);
    hal_gpio_set_output(sensor->pin, 1);

    DELAY_U(40);

    // Set DHT_PIN pin as an input
    //GPIO_DIS_OUTPUT(pin);
    hal_gpio_set_direction(sensor->pin, 0); // INPUT

    uint32_t result = 1;
    // wait for pin to drop?
    while (i < DHT_MAXCOUNT)
    {
        hal_gpio_get_input(sensor->pin, &result);
        if (result == 0)
            break;
        ////DELAY_U(1);
        i++;
    }
    //printf("drop = %d\n", i);
    if (i == DHT_MAXCOUNT)
    {
        DHT_DEBUG("DHT: Failed to get reading from PIN, dying\r\n");
        return false;
    }

    // read data
    for (i = 0; i < DHT_MAXTIMINGS; i++)
    {
        // Count high time (in approx us)
        counter = 0;
        while (1)
        {
            hal_gpio_get_input(sensor->pin, &result);
            if (result != laststate)
                break;
            counter++;
            //DELAY_U(1);
            if (counter == 1000)
                break;
        }
        hal_gpio_get_input(sensor->pin, &result);
        laststate = result;
        if (counter == 1000)
            break;

        // store data after 3 reads
        int index = j / 8;
        //CNT[index] = counter;
        if ((i > 3) && (i % 2 == 0))
        {
            // shove each bit into the storage bytes
            data[index] <<= 1;
            if (counter > 3) // DHT_BREAKTIME !!!!!!!!!!!!!!!!!!!
                data[index] |= 1;
            j++;
        }
    }

    if (j >= 39)
    {
        checksum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
        //DHT_DEBUG("DHT%s: %02X %02X %02X %02X [%02X] CS: %02X\r\n", sensor->type == DHT11 ? "11" : "22", data[0], data[1], data[2], data[3], data[4], checksum);
        if (data[4] == checksum)
        {
            // checksum is valid
            output->temperature = scale_temperature(sensor->type, data);
            output->humidity = scale_humidity(sensor->type, data);
            //DHT_DEBUG("DHT: Temperature =  %d *C, Humidity = %d %%\r\n", (int)(reading.temperature * 100), (int)(reading.humidity * 100));
            //DHT_DEBUG("DHT: Temperature*100 = %d, Humidity*100 = %d\n", (int)(output->temperature * 100), (int)(output->humidity * 100));

            //for (int x = 0; x < 64; x++) printf("%d ", CNT[x]);
            //printf("\n");
        }
        else
        {
            //DHT_DEBUG("Checksum was incorrect after %d bits. Expected %d but got %d\r\n", j, data[4], checksum);
            DHT_DEBUG("DHT: Checksum was incorrect after %d bits. Expected %d but got %d \n", j, data[4], checksum);
            return false;
        }
    }
    else
    {
        //DHT_DEBUG("Got too few bits: %d should be at least 40\r\n", j);
        DHT_DEBUG("DHT: Got too few bits: %d should be at least 40\n", j);
        return false;
    }
    return true;
}

bool DHTInit(DHT_Sensor *sensor, uint32_t MTK_PIN)
{
    sensor->pin = MTK_PIN;
    hal_gpio_set_direction(MTK_PIN, 1); // OUTPUT
    hal_gpio_set_output(MTK_PIN, 0);    // LOW
    return true;
}