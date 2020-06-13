#ifndef __DS_COMMON_H__
#define __DS_COMMON_H__

#include <stdint.h>
#include <ds_onewire.h>

uint8_t ds_crc8(uint8_t *data_in, uint16_t number_of_bytes_to_read);

#endif /* __DS_COMMON_H__ */
