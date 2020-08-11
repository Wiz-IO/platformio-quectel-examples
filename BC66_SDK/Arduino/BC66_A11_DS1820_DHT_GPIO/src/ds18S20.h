/*
 * ds18s20.h
 *
 * Created: 13-Jul-15 23:48:08
 * Author: Goce Boshkovski
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 * 
 * EDIT for Quectel BC66: Georgi Angelov 03.08.2020
 */

#ifndef DS18S20_H_
#define DS18S20_H_
#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <hal.h>
#include <os_wizio.h>

/**
 * @file ds18S20.h
 * @brief 
 * Header file of DS18S20 library. It contains the prototypes of all
 * functions available in the library, definitions of all macros
 * and constans.
 * 
 * @author Goce Boshkovski
 * @date 13-Jun-15
 * @copyright GNU General Public License v2.
 * 
 */

/** \defgroup DS18S20ROMCmd DS18x20 ROM Commands */
/* @{ */
#define SEARCH_ROM 0xF0
#define READ_ROM 0x33
#define MATCH_ROM 0x55
#define SKIP_ROM 0xCC
#define ALARM_SEARCH 0xEC
/* @} */

/** \defgroup DS18S20FuncCmd DS18x20 Commands */
/* @{ */
#define CONVERT_T 0x44
#define WRITE_SCRATCHPAD 0x4E
#define READ_SCRATCHPAD 0xBE
#define COPY_SCRATCHPAD 0x48
#define RECALL_E2 0xB8
#define READ_POWER_SUPPLY 0xB4
/* @} */

/** \defgroup DS18B20Conf DS18B20 Config Register */
/* @{ */
#define CONF_RES_9b 0x1F
#define CONF_RES_10b 0x3F
#define CONF_RES_11b 0x5F
#define CONF_RES_12b 0x7F
	/* @} */

	/**
 * @brief Defines the sensor model.
 */
	typedef enum ESensorModel
	{
		DS18S20Sensor = 0, /**< Define the sensor as DS18S20 */
		DS18B20Sensor	   /**< Define the sensor as DS18B20 */
	} TSensorModel;

	/**
 * @brief Represents the DS18S20 sensor including the 1-Wire bus configuration (MCU PORT and PIN
 * selected for DS18S20 connection).
 */
	typedef struct SDS18x20
	{
		int mtk_pin_id;
		uint8_t serialNumber[8];  /**< buffer for the DS18S20 serial number (its address on 1-wire bus)*/
		uint8_t scratchpad[9];	  /**< DS18S20 scratchpad */
		TSensorModel SensorModel; /**< Sensor Model */
	} TSDS18x20;

	/** \defgroup OneWire Implementation of 1-Wire Interface */
	/* @{ */
	/**@brief Send one bit via 1-Wire interface to DS18S20 sensor.
 * 
 * @param[in] pDS18x20 pointer to the structure that represent DS18S20
 * @param[in] bit a bit value to be send
 * @return void
 *
*/
	void OWWriteBit(TSDS18x20 *pDS18x20, uint8_t bit);

	/**@brief Send one byte via 1-Wire interface to DS18S20 sensor.
 * 
 * @param[in] pDS18x20 pointer to the structure that represent DS18S20
 * @param[in] value a byte value to be send
 * @return void
 *
*/
	void OWWriteByte(TSDS18x20 *pDS18x20, uint8_t value);

	/**@brief Read a bit from the 1-Wire interface.
 * 
 * @param[in] pDS18x20 pointer to the structure that represent DS18S20
 * @return uint8_t value read from the 1-Wire interface
 *
*/
	uint8_t OWReadBit(TSDS18x20 *pDS18x20);

	/**@brief Read a byte from the 1-Wire interface.
 * 
 * @param[in] pDS18x20 pointer to the structure that represent DS18S20
 * @return uint8_t byte read from the 1-Wire interface
 *
*/
	uint8_t OWReadByte(TSDS18x20 *pDS18x20);

	/**@brief Used for initialization of DS18S20 sensor before data exchange.
 * 
 * @param[in] pDS18x20 pointer to the structure that represent DS18S20
 * @return uint8_t Returns 0 for success, 1 for failed initialization attempt.
 *
*/
	uint8_t OWReset(TSDS18x20 *pDS18x20);

	/**
 * @brief Calculates CRC value based on the example in Avr-libc reference manual.
 *
 * @param [in] data pointer to the input data
 * @param [in] length data length
 * @return uint8_t the calculated CRC value
 *
 */
	uint8_t OWCheckCRC(uint8_t *data, uint8_t length);
	/* @} */

	/** \defgroup DS18S20func DS18x20 functions */
	/* @{ */

	/**
 * @brief  Init function for DS18x20.
 * The function prepares the 1-Wire bus and runs the sensors initialization procedure.
 * In case of DS18B20, the init function doesn't set the value of the Configuration register.
 * After the initialization is done it still has the default value. To change the resolution
 * use DS18x20_WriteScratchpad() function or DS18x20_SetResolution() macro.
 * 
 * @param[in,out] pDS18x20 pointer to the structure that represent DS18S20
 * @param[in] DS18x20_PORT is a pointer to the MCU PORT used for the 1-Wire bus
 * @param[in] DS18x20_PIN MCU PIN from DS18x20_PORT connected to the DS18S20 sensor
 * @return uint8_t. 0 for success init, 1 for failure.
 */
	uint8_t DS18x20_Init(TSDS18x20 *pDS18x20, int MTK_PIN);

	/**
 * @brief Reads DS18S20 64-bit ROM code without using the Search ROM procedure.
 * 
 * @param[in,out] pDS18x20 pointer to the structure that represent DS18S20
 * @return uint8_t 1 for valid data transfer, 0 for CRC error detected.
 */
	uint8_t DS18x20_ReadROM(TSDS18x20 *pDS18x20);

	/**
 * @brief This functions initiates a temperature conversion via CONVERT_T function command 
 * and then issues READ_SCRATCHPAD command for fetching the temperature reading.
 * 
 * @param[in,out] pDS18x20 pointer to the structure that represent DS18S20
 * @return uint8_t 1 on successful temperature measurement, 0 in case of error.
 */
	uint8_t DS18x20_MeasureTemperature(TSDS18x20 *pDS18x20);

	/**
 * @brief Read the contents of DS18S20 scratchpad.
 * 
 * @param[in,out] pDS18x20 pointer to the structure that represent DS18S20
 * @return uint8_t 0 if CRC error is detected, 1 for successful scratchpad read.
 */
	uint8_t DS18x20_ReadScratchPad(TSDS18x20 *pDS18x20);

	/**
 * @brief Returns the power supply type based on the respond from the sensor on Read Power Supply 
 * function command.
 * 
 * @param[in] pDS18x20 pointer to the structure that represent DS18S20
 * @return uint8_t 0 for the parasite powered, 1 for externally powered sensor.
 */
	uint8_t DS18x20_PowerSupplyType(TSDS18x20 *pDS18x20);

	/**
 * @brief Defines the values of the TH and TL registers. 
 * The function doesn't update the sensor registers directly.
 * Call DS18x20_WriteScratchpad() function to transfer those value to the sensor.
 * 
 * @param[in,out] pDS18x20 pointer to the structure that represent DS18S20
 * @param[in] TH value of the TH register
 * @param[in] TL value of the TL register 
 * @return void
 */
	void DS18x20_SetAlarmValues(TSDS18x20 *pDS18x20, uint8_t TH, uint8_t TL);

	/**
 * @brief Defines the resolution of DS18B20 sensor. 
 * The function doesn't update the sensor Configuration register directly.
 * Call DS18x20_WriteScratchpad() function to transfer the new resolution value to the sensor.
 * The function doesn't have any effect when it is called for DS18S20 sensor.
 * 
 * @param[in,out] pDS18x20 pointer to the structure that represent DS18S20
 * @param[in] CONF_REG new value of the CONF_REG register
 * @return void
 */
	void DS18x20_SetResolution(TSDS18x20 *pDS18x20, uint8_t CONF_REG);

	/**
 * @brief Writes 2 bytes of data to the SA18S20 scratchpad (TH and TL registers).
 * 
 * @param[in] pDS18x20 pointer to the structure that represent DS18S20
 * @return void
 */
	void DS18x20_WriteScratchpad(TSDS18x20 *pDS18x20);

	/**
 * @brief Copies the contents of the scratchpad TH and TL registers (bytes 2 and 3) to EEPROM.
 *
 * @param [in] pDS18x20 pointer to the structure that represent DS18S20
 * @return void
 *
 */
	void DS18x20_CopyScratchpad(TSDS18x20 *pDS18x20);

	/**
 * @brief Recalls the alarm trigger values (TH and TL) from EEPROM.
 *
 * @param [in] pDS18x20 pointer to the structure that represent DS18S20
 * @return void
 *
 */
	void DS18x20_RECALL_E2(TSDS18x20 *pDS18x20);

	/**
 * @brief Convert temperature reading to a floating point number.
 *
 * @param [in] pDS18x20 pointer to the structure that represent DS18S20
 * @return double
 *
 */
	double DS18x20_TemperatureValue(TSDS18x20 *pDS18x20);
	/* @} */

#ifdef __cplusplus
}
#endif
#endif /* DS18S20_H_ */
