/*
 * ds18s20.c
 *
 * Created: 13-Jul-15 23:49:17
 * Author: Goce Boshkovski
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 * 
 * EDIT for Quectel BC66: Georgi Angelov 03.08.2020
 */

#include "ds18S20.h"
#include <stdlib.h>

//#define DS_IRQ_MOD /* not need */

static uint8_t _crc_ibutton_update(uint8_t crc, uint8_t data)
{
	unsigned int i;
	crc = crc ^ data;
	for (i = 0; i < 8; i++)
	{
		if (crc & 0x01)
		{
			crc = (crc >> 1) ^ 0x8C;
		}
		else
		{
			crc >>= 1;
		}
	}
	return crc;
}

/** @file ds18S20.c
 *  @brief Implements the functions defined in the header file.
 *
 *  @date 13-Jun-15
 *  @author Goce Boshkovski
 */

uint8_t OWReset(TSDS18x20 *pDS18x20)
{
	uint32_t result;
#ifdef DS_IRQ_MOD
	__disable_irq();
#endif
	hal_gpio_set_direction(pDS18x20->mtk_pin_id, 1); // OUTPUT
	DELAY_U(450);
	hal_gpio_set_direction(pDS18x20->mtk_pin_id, 0); // INPUT
	DELAY_U(80);
	int res = hal_gpio_get_input(pDS18x20->mtk_pin_id, &result);
	DELAY_U(480);
#ifdef DS_IRQ_MOD
	__enable_irq();
#endif
	return result;
}

/* Send a bit over 1-Wire interface. */
void OWWriteBit(TSDS18x20 *pDS18x20, uint8_t bit)
{
	hal_gpio_set_direction(pDS18x20->mtk_pin_id, 1); // OUTPUT
	if (bit)
	{
		//DELAY_U(1);
		hal_gpio_set_direction(pDS18x20->mtk_pin_id, 0); // INPUT
		DELAY_U(45);
	}
	else
	{
		DELAY_U(60);
		hal_gpio_set_direction(pDS18x20->mtk_pin_id, 0); // INPUT
	}
}

/* Read a bit from the 1-Wire interface. */
uint8_t OWReadBit(TSDS18x20 *pDS18x20)
{
	uint32_t result = 0;
	hal_gpio_set_direction(pDS18x20->mtk_pin_id, 1); // OUTPUT
	//DELAY_U(1);
	hal_gpio_set_direction(pDS18x20->mtk_pin_id, 0); // INPUT
	//DELAY_U(1);
	hal_gpio_get_input(pDS18x20->mtk_pin_id, &result);
	DELAY_U(45);
	return result;
}

/* Send a byte over 1-Wire interface starting with most significant bit. */
void OWWriteByte(TSDS18x20 *pDS18x20, uint8_t value)
{
	uint8_t mask = 0x01;
#ifdef DS_IRQ_MOD
	__disable_irq();
#endif
	while (mask)
	{
		OWWriteBit(pDS18x20, value & mask);
		mask <<= 1;
	}
#ifdef DS_IRQ_MOD
	__enable_irq();
#endif
}

/* Read a byte form the 1-Wire interface. */
uint8_t OWReadByte(TSDS18x20 *pDS18x20)
{
	uint8_t data = 0x00;
	uint8_t mask = 0x01;
#ifdef DS_IRQ_MOD
	__disable_irq();
#endif
	while (mask)
	{
		if (OWReadBit(pDS18x20))
			data |= mask;
		mask <<= 1;
	}
#ifdef DS_IRQ_MOD
	__enable_irq();
#endif
	return data;
}

/* Calculates CRC value based on the example in Avr-libc reference manual. */
uint8_t OWCheckCRC(uint8_t *data, uint8_t length)
{
	uint8_t i;
	uint8_t CRCvalue = 0;
	//printf("DATA ");
	for (i = 0; i < length; i++)
	{
		//printf("%02X ", (int)*(data + i) & 0xFF);
		CRCvalue = _crc_ibutton_update(CRCvalue, *(data + i));
	}
	//printf(" CRC %02X\n", (int)CRCvalue & 0xFF);
	return CRCvalue;
}

/* Init function for DS18S20. */
uint8_t DS18x20_Init(TSDS18x20 *pDS18x20, int MTK_PIN)
{
	//Init ports/pins to which DS18S20 is attached.
	pDS18x20->mtk_pin_id = MTK_PIN;

	//Set DS18S20 PIN as input one and PORT bit to 0
	hal_gpio_set_direction(MTK_PIN, 0); // INPUT
	hal_gpio_set_output(MTK_PIN, 0);	// LOW

	if (OWReset(pDS18x20))
		return 1;

	//Check DS18x20 sensor type by reading the family code from the ROM code
	if (!DS18x20_ReadROM(pDS18x20))
		return 2;

	switch (pDS18x20->serialNumber[0])
	{
	case 0x28:
	{
		pDS18x20->SensorModel = DS18B20Sensor;
		// Set the resolution to its default value
		pDS18x20->scratchpad[4] = CONF_RES_12b;
	}
	break;
	case 0x10:
		pDS18x20->SensorModel = DS18S20Sensor;
		break;
	default:
		return 3;
	}
	return 0;
}

/* Reads DS18S20 64-bit ROM code without using the Search ROM procedure. */
uint8_t DS18x20_ReadROM(TSDS18x20 *pDS18x20)
{
	uint8_t i;

	OWWriteByte(pDS18x20, READ_ROM);

	for (i = 0; i < 8; i++)
	{
		pDS18x20->serialNumber[i] = OWReadByte(pDS18x20);
	}

	printf("ID ");
	for (i = 0; i < 8; i++)
	{
		printf("%02X ", pDS18x20->serialNumber[i] & 0xFF);
	}
	printf("\n");

	if (OWCheckCRC(pDS18x20->serialNumber, 7) == pDS18x20->serialNumber[7])
		return 1;
	else
		return 0;
}

/* This functions initiates a single temperature conversion and reads sensors scratchpad. */
uint8_t DS18x20_MeasureTemperature(TSDS18x20 *pDS18x20)
{
	OWReset(pDS18x20);
	OWWriteByte(pDS18x20, SKIP_ROM);
	OWWriteByte(pDS18x20, CONVERT_T);

	switch (pDS18x20->scratchpad[4])
	{
	case CONF_RES_9b:
		DELAY_M(94);
		break;
	case CONF_RES_10b:
		DELAY_M(188);
		break;
	case CONF_RES_11b:
		DELAY_M(375);
		break;
	default:
		DELAY_M(750);
	}

	return (DS18x20_ReadScratchPad(pDS18x20));
}

/* Read the content of DS18S20 scratchpad and check the integrity with CRC. */
uint8_t DS18x20_ReadScratchPad(TSDS18x20 *pDS18x20)
{
	uint8_t i;

	OWReset(pDS18x20);
	OWWriteByte(pDS18x20, SKIP_ROM);
	OWWriteByte(pDS18x20, READ_SCRATCHPAD);

	for (i = 0; i < 9; i++)
		pDS18x20->scratchpad[i] = OWReadByte(pDS18x20);

	if (OWCheckCRC(pDS18x20->scratchpad, 8) == pDS18x20->scratchpad[8])
		return 1;
	else
		return 0;
}

/* Returns the power supply type based on the respond from the sensor on Read Power Supply function command. */
uint8_t DS18x20_PowerSupplyType(TSDS18x20 *pDS18x20)
{
	OWReset(pDS18x20);
	OWWriteByte(pDS18x20, SKIP_ROM);
	OWWriteByte(pDS18x20, READ_POWER_SUPPLY);

	return OWReadBit(pDS18x20);
}

/* Defines the new values of TH and TL registers. */
void DS18x20_SetAlarmValues(TSDS18x20 *pDS18x20, uint8_t TH, uint8_t TL)
{
	pDS18x20->scratchpad[2] = TH;
	pDS18x20->scratchpad[3] = TL;

	return;
}

/* Defines the resolution of the sensor. Doesn't have any effect on DS18S20. */
void DS18x20_SetResolution(TSDS18x20 *pDS18x20, uint8_t CONF_REG)
{
	if (pDS18x20->SensorModel == DS18B20Sensor)
		pDS18x20->scratchpad[4] = CONF_REG;

	return;
}

/* Writes 2 bytes of data to the SA18S20 scratchpad (TH and TL registers).*/
void DS18x20_WriteScratchpad(TSDS18x20 *pDS18x20)
{
	OWReset(pDS18x20);
	OWWriteByte(pDS18x20, SKIP_ROM);
	OWWriteByte(pDS18x20, WRITE_SCRATCHPAD);

	OWWriteByte(pDS18x20, pDS18x20->scratchpad[2]);
	OWWriteByte(pDS18x20, pDS18x20->scratchpad[3]);
	if (pDS18x20->SensorModel == DS18B20Sensor)
		OWWriteByte(pDS18x20, pDS18x20->scratchpad[4]);

	return;
}

/* Copies the contents of the scratchpad TH and TL registers (bytes 2 and 3) to EEPROM. */
void DS18x20_CopyScratchpad(TSDS18x20 *pDS18x20)
{
	OWReset(pDS18x20);
	OWWriteByte(pDS18x20, SKIP_ROM);
	OWWriteByte(pDS18x20, COPY_SCRATCHPAD);

	while (!OWReadBit(pDS18x20))
		;

	return;
}

/* Recalls the alarm trigger values (TH and TL) from EEPROM. */
void DS18x20_RECALL_E2(TSDS18x20 *pDS18x20)
{
	OWReset(pDS18x20);
	OWWriteByte(pDS18x20, SKIP_ROM);
	OWWriteByte(pDS18x20, RECALL_E2);

	while (!OWReadBit(pDS18x20))
		;

	return;
}

/* Convert temperature reading to a floating point number. */
double DS18x20_TemperatureValue(TSDS18x20 *pDS18x20)
{
	int16_t Temperature = 0;
	int8_t sign = 1;

	Temperature = pDS18x20->scratchpad[1];
	Temperature <<= 8;
	Temperature |= pDS18x20->scratchpad[0];

	if (Temperature < 0)
		sign = -1;

	if (pDS18x20->SensorModel == DS18S20Sensor)
		return Temperature * 0.5;

	switch (pDS18x20->scratchpad[4])
	{
	case CONF_RES_10b:
		return sign * (abs(Temperature) / 4) * 0.25;
		break;
	case CONF_RES_11b:
		return sign * (abs(Temperature) / 2) * 0.125;
	case CONF_RES_12b:
		return Temperature * 0.0625;
		break;
	case CONF_RES_9b:
		return sign * (abs(Temperature) / 8) * 0.5;
		break;
	default:
		return 0.0;
	}
}
