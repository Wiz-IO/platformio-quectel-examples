#ifndef __EXAMPLE_SMS_H__
#define __EXAMPLE_SMS_H__

#if 0
#include "wireless_messaging_service_v01.h"

typedef wms_message_tag_type_enum_v01 qapi_QT_SMS_Status_e;

typedef int32_t qapi_QT_Status_t;


typedef struct {
//uint8 fo; //no need to set this parameter
uint8 vp;
uint8 pid;
uint8 dcs;
} qapi_QT_SMS_Para_t;


typedef struct{
  uint32_t data_len;
  uint8_t data[WMS_MESSAGE_LENGTH_MAX_V01];
}qapi_QT_SMS_Message_Info_t;


typedef struct{
//time_t time;
qapi_QT_SMS_Status_e status;
qapi_QT_SMS_Message_Info_t sms_info;
}qapi_QT_SMS_Message_Rcvd_t;

#endif

typedef enum {
  WMS_MESSAGE_TAG_TYPE_ENUM_MIN_ENUM_VAL_V01 = -2147483647, /**< To force a 32 bit signed enum.  Do not change or use*/
  WMS_TAG_TYPE_MT_READ_V01 = 0x00, 
  WMS_TAG_TYPE_MT_NOT_READ_V01 = 0x01, 
  WMS_TAG_TYPE_MO_SENT_V01 = 0x02, 
  WMS_TAG_TYPE_MO_NOT_SENT_V01 = 0x03, 
  WMS_MESSAGE_TAG_TYPE_ENUM_MAX_ENUM_VAL_V01 = 2147483647 /**< To force a 32 bit signed enum.  Do not change or use*/
}wms_message_tag_type_enum_v01;

typedef wms_message_tag_type_enum_v01 qapi_QT_SMS_Status_e;

typedef struct{
  uint32_t data_len;
  uint8_t data[255];
}qapi_QT_SMS_Message_Info_t;


typedef struct{
//time_t time;
qapi_QT_SMS_Status_e status;
qapi_QT_SMS_Message_Info_t sms_info;
}qapi_QT_SMS_Message_Rcvd_t;

typedef struct {
char        address[20];
char        message[256];
} qapi_QT_SMS_Message_Content_t;

#endif

