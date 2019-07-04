#ifndef __QUECTEL_LWM2M_EXT_H__
#define __QUECTEL_LWM2M_EXT_H__

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#if defined(__EXAMPLE_LWM2M_EXT__)
#include "qapi_fs_types.h"
#include "qapi_dss.h"

//#include "stringl/stringl.h"

#include "qapi_timer.h"
#include "qapi_diag.h"
#include "qapi_lwm2m.h"

/*===========================================================================
                             DEFINITION
===========================================================================*/
#if 0
typedef unsigned long long uint64_t;  /* Unsigned 64 bit value */
typedef unsigned long int  uint32_t;  /* Unsigned 32 bit value */
typedef unsigned short	   uint16_t;  /* Unsigned 16 bit value */
typedef unsigned char	   uint8_t;   /* Unsigned 8  bit value */

typedef signed long long   int64_t;   /* Signed 64 bit value */
typedef signed long int    int32_t;   /* Signed 32 bit value */
typedef signed short	   int16_t;   /* Signed 16 bit value */
typedef signed char 	   int8_t;	   /* Signed 8  bit value */
#endif

typedef long int		   time_t;	  /* time */
typedef unsigned int       size_t;

/* lwm2m_ext object, instance and resource related */
#define RES_M_SENSOR_VALUE             0 /* Last or current measured sensor value. */
#define RES_O_SENSOR_UNITS             1 /* Measurement unit definition e.g. "Celcius" for tempwerature. */
#define RES_O_MIN_MEASURED_VAL         2 /* Minimum value measured since power on or reset */
#define RES_O_MAX_MEASURED_VAL         3 /* Maximum value measured since power on or reset */
#define RES_M_MIN_RANGE_VAL            4 /* Minimum threshold value. */
#define RES_M_MAX_RANGE_VAL            5 /* Maximum threshold value. */
#define RES_O_RESET_MEASURED_VAL       6 /* Reset minimum and maximum measured value. */
#define RES_M_SENSOR_STATE             7 /* Sensor state on/off. */
#define RES_O_MEAN_VAL                 8 /* Mean measured value */

#define RES_M_DIG_INPUT_STATE                  0
#define RES_O_DIG_INPUT_COUNTER                1
#define RES_O_RESET_DIG_INPUT_COUNTER          2
#define RES_O_SENSOR_TYPE                      3
#define RES_O_BUSY_TO_CLEAR_DELAY              4
#define RES_O_CLEAR_TO_BUSY_DELAY              5

#define LWM2M_GENERIC_SENSOR_OBJECT_ID 			1001
#define LWM2M_PRESENCE_SENSOR_OBJECT_ID 		1002
#define PRV_MAX_STRSIZE                			255
#define MAX_EXT_INSTANCE                        30

#define LWM2M_MAX_ID   			((uint16_t)0xFFFF)
#define PROCESSING_DELAY 		2
#define PRV_LINK_BUFFER_SIZE  	1024

/**
 * @brief Describes the Generic sensor object resources
 *
 */
typedef struct _ext_obj_t
{
  struct _ext_obj_t *next;        /**< corresponding to lwm2m_list_t::next */
  uint16_t           id;          /**< corresponding to lwm2m_list_t::id  */
  float sen_val;                  /**< Sensor value (Mandatory) */
  char sen_unit[PRV_MAX_STRSIZE]; /**< Sensor unit (Optional) */
  float min_measured_val;         /**< Min measured value (Optional) */
  float max_measured_val;         /**< Max measured value (Optional) */
  float min_range_val;            /**< Min threshold value (Mandatory) */
  float max_range_val;            /**< Max threshold value (Mandatory) */
  bool sen_state;                 /**< Sensor state (Mandatory) */
  int16_t mean_val;               /**< Mean calculated value (Optional) */
}ext_obj_t;

//#define ERANGE  34 /* Math result not representable */
#define LWM2M_ATTR_FLAG_NUMERIC (QAPI_NET_LWM2M_LESS_THAN_E| QAPI_NET_LWM2M_GREATER_THAN_E| QAPI_NET_LWM2M_STEP_E)

#define LWM2M_ATTR_SERVER_ID_STR       "ep="
#define LWM2M_ATTR_SERVER_ID_LEN       3
#define LWM2M_ATTR_MIN_PERIOD_STR      "pmin="
#define LWM2M_ATTR_MIN_PERIOD_LEN      5
#define LWM2M_ATTR_MAX_PERIOD_STR      "pmax="
#define LWM2M_ATTR_MAX_PERIOD_LEN      5
#define LWM2M_ATTR_GREATER_THAN_STR    "gt="
#define LWM2M_ATTR_GREATER_THAN_LEN    3
#define LWM2M_ATTR_LESS_THAN_STR       "lt="
#define LWM2M_ATTR_LESS_THAN_LEN       3
#define LWM2M_ATTR_STEP_STR            "st="
#define LWM2M_ATTR_STEP_LEN            3
#define LWM2M_ATTR_DIMENSION_STR       "dim="
#define LWM2M_ATTR_DIMENSION_LEN       4

#define LWM2M_URI_MAX_STRING_LEN    18      // /65535/65535/65535
#define _PRV_64BIT_BUFFER_SIZE 8

#define LWM2M_LINK_ITEM_START             "<"
#define LWM2M_LINK_ITEM_START_SIZE        1
#define LWM2M_LINK_ITEM_END               ">,"
#define LWM2M_LINK_ITEM_END_SIZE          2
#define LWM2M_LINK_ITEM_DIM_START         ">;dim="
#define LWM2M_LINK_ITEM_DIM_START_SIZE    6
#define LWM2M_LINK_ITEM_ATTR_END          ","
#define LWM2M_LINK_ITEM_ATTR_END_SIZE     1
#define LWM2M_LINK_URI_SEPARATOR          "/"
#define LWM2M_LINK_URI_SEPARATOR_SIZE     1
#define LWM2M_LINK_ATTR_SEPARATOR         ";"
#define LWM2M_LINK_ATTR_SEPARATOR_SIZE    1

typedef enum
{
  URI_DEPTH_OBJECT,
  URI_DEPTH_OBJECT_INSTANCE,
  URI_DEPTH_RESOURCE,
  URI_DEPTH_RESOURCE_INSTANCE
}qapi_Net_LWM2M_Uri_Depth_t;

typedef struct qapi_Net_LWM2M_Uri_s
{
  uint32_t    flag;           // indicates which segments are set
  uint16_t    objectId;
  uint16_t    instanceId;
  uint16_t    resourceId;
  uint16_t    resourceInstId;
}qapi_Net_LWM2M_Uri_t;

typedef struct qapi_Net_LWM2M_Observe_info_s
{
  struct qapi_Net_LWM2M_Observe_info_s *next;
  bool active;
  bool update;
  qapi_Net_LWM2M_Attributes_t *attributes;
  time_t last_time;
  union
  {
     int64_t asInteger;
     double  asFloat;
  }lastvalue;
  uint8_t msg_id_len;
  uint8_t msg_id[QAPI_MAX_LWM2M_MSG_ID_LENGTH];
  uint16_t not_id;
}qapi_Net_LWM2M_Observe_info_t;

typedef struct qapi_Net_LWM2M_Observed_s
{
  struct qapi_Net_LWM2M_Observed_s *next;
  qapi_Net_LWM2M_Uri_t uri;
  qapi_Net_LWM2M_Observe_info_t *observe_info;
}qapi_Net_LWM2M_Observed_t;

typedef struct qapi_Net_LWM2M_Pending_Observed_s
{
  struct qapi_Net_LWM2M_Pending_Observed_s *next;
  qapi_Net_LWM2M_Observe_info_t *observe_info;
  qapi_Net_LWM2M_Uri_t uri;
  void *message;
}qapi_Net_LWM2M_Pending_Observed_t;

typedef enum qapi_Net_LWM2M_Ext_App_Msg_e
{
  QAPI_NET_LWM2M_EXT_REGISTER_APP_E,
  QAPI_NET_LWM2M_EXT_DEREGISTER_APP_E,
  QAPI_NET_LWM2M_EXT_CONFIG_CLIENT_E,  
  QAPI_NET_LWM2M_EXT_CREATE_OBJ_E,
  QAPI_NET_LWM2M_EXT_CREATE_OBJ_INSTANCE_E,
  QAPI_NET_LWM2M_EXT_DELETE_OBJ_INSTANCE_E,
  QAPI_NET_LWM2M_EXT_NOTIFY_E,
}qapi_Net_LWM2M_Ext_App_Msg_t;

typedef struct qapi_Net_LWM2M_Ext_Obj_s
{
  qapi_Net_LWM2M_App_Handler_t handler;
  qapi_Net_LWM2M_Server_Data_t *lwm2m_srv_data;
  void                         *data;
}qapi_Net_LWM2M_Ext_Obj_t;

typedef struct qapi_Net_LWM2M_Ext_App_s
{
  //uint8_t id;			//single application
  qapi_Net_LWM2M_Ext_App_Msg_t msg_type;
  qapi_Net_LWM2M_Obj_Info_t obj_info;
}qapi_Net_LWM2M_Ext_App_t;

typedef struct qapi_Net_LWM2M_Ext_s
{
  union
  {
    qapi_Net_LWM2M_Ext_Obj_t dl_op;
    qapi_Net_LWM2M_Ext_App_t ul_op;
  }app_data;
}qapi_Net_LWM2M_Ext_t;

/* Signal and message queue related */
typedef TX_QUEUE ext_obj_task_cmd_q;

typedef enum
{
  EXT_OBJ_MIN_CMD_EVT,
  EXT_OBJ_APP_ORIGINATED_EVT,
  EXT_OBJ_CALL_BACK_EVT,
  EXT_OBJ_MAX_CMD_EVT
}ext_obj_cmd_id_e;

typedef struct
{
  TX_EVENT_FLAGS_GROUP*         ext_obj_signal;
  TX_MUTEX*                     ext_obj_mutex;
  TX_QUEUE*                     cmd_q_ptr;
}ext_obj_info_t;

typedef struct{
  //q_link_type                   link;
  ext_obj_cmd_id_e                cmd_id;
}ext_obj_cmd_hdr_t;


typedef struct{
  void *data;
}ext_obj_cmd_data_t;

typedef struct{
  ext_obj_cmd_hdr_t               cmd_hdr;
  ext_obj_cmd_data_t              cmd_data;
} ext_obj_cmd_t;

/*------------------------------------------------------------------------------
  Signal Related Macros
------------------------------------------------------------------------------*/
#define EXT_OBJ_SIGNAL_MASK       0x00000001

#define EXT_OBJ_TASK_IS_SIG_SET(signal, signal_mask)  ((signal & signal_mask) != 0)

#define EXT_OBJ_TASK_CLEAR_SIG(signal, signal_mask) \
                                    tx_event_flags_set(signal,signal_mask, TX_AND)

#define EXT_OBJ_TASK_SET_SIG(signal, signal_mask) \
                                    tx_event_flags_set(signal, signal_mask, TX_OR)

#define EXT_OBJ_CMD_ID_RANGE_CHECK(cmd_id) \
                                       ( (cmd_id > EXT_OBJ_MIN_CMD_EVT) && \
                                         (cmd_id < EXT_OBJ_MAX_CMD_EVT) )


#define PRV_CONCAT_STR(buf, len, index, str, str_len)    \
	{													 \
	  if ((len)-(index) < (str_len)) return -1; 	   \
	  memcpy((buf)+(index), (str), (str_len));		   \
	  (index) += (str_len); 						   \
	}

/* Definition for Data Call */
#define QUEC_APN_LEN	  		(QAPI_DSS_CALL_INFO_APN_MAX_LEN + 1)
#define QUEC_APN_USER_LEN 		(QAPI_DSS_CALL_INFO_USERNAME_MAX_LEN + 1)
#define QUEC_APN_PASS_LEN 		(QAPI_DSS_CALL_INFO_PASSWORD_MAX_LEN + 1)
#define QUEC_DEV_NAME_LEN 		(QAPI_DSS_CALL_INFO_DEVICE_NAME_MAX_LEN + 1)

#define lower(c) (('A' <= c && c <= 'Z') ? c + ('a' - 'A') : c)
#define LEFT_SHIFT_OP(N)		(1 << (N))
#define _htons(x) \
        ((unsigned short)((((unsigned short)(x) & 0x00ff) << 8) | \
                  (((unsigned short)(x) & 0xff00) >> 8)))

typedef enum LWM2M_EXT_TASK_TYPE
{
	M2M_EXT_TASK_EXIT		= LEFT_SHIFT_OP(0),
} m2m_ext_task_e;

typedef enum DSS_Net_Evt_TYPE
{
	DSS_EVT_INVALID_E = 0x00,   /**< Invalid event. */
	DSS_EVT_NET_IS_CONN_E,      /**< Call connected. */
	DSS_EVT_NET_NO_NET_E,       /**< Call disconnected. */
	DSS_EVT_NET_RECONFIGURED_E, /**< Call reconfigured. */
	DSS_EVT_NET_NEWADDR_E,      /**< New address generated. */
	DSS_EVT_NET_DELADDR_E,      /**< Delete generated. */
	DSS_EVT_NIPD_DL_DATA_E,
	DSS_EVT_MAX_E
} DSS_Net_Evt_Type_e;

typedef enum DSS_Lib_Status
{
	DSS_LIB_STAT_INVALID_E = -1,
	DSS_LIB_STAT_FAIL_E,
	DSS_LIB_STAT_SUCCESS_E,
	DSS_LIB_STAT_MAX_E
} DSS_Lib_Status_e;

typedef enum DSS_SIG_EVENTS
{
	DSS_SIG_EVT_INV_E		= LEFT_SHIFT_OP(0),
	DSS_SIG_EVT_NO_CONN_E	= LEFT_SHIFT_OP(1),
	DSS_SIG_EVT_CONN_E		= LEFT_SHIFT_OP(2),
	DSS_SIG_EVT_DIS_E		= LEFT_SHIFT_OP(3),
	DSS_SIG_EVT_EXIT_E		= LEFT_SHIFT_OP(4),
	DSS_SIG_EVT_MAX_E		= LEFT_SHIFT_OP(5)
} DSS_Signal_Evt_e;

/*===========================================================================
                             DECLARATION
===========================================================================*/
void m2m_uart_dbg_init(void);
void m2m_uart_debug_print(const char* fmt, ...);


void ext_obj_task_init(void);
void ext_obj_cmdq_init(ext_obj_info_t *ext_obj_state);
ext_obj_cmd_t *ext_obj_get_cmd_buffer(void);
void ext_obj_enqueue_cmd(ext_obj_cmd_t* cmd_ptr);
qapi_Status_t ext_obj_delete_all(qapi_Net_LWM2M_App_Handler_t handler, qapi_Net_LWM2M_Server_Data_t op_data);
uint8_t ext_object_set_observe_param(qapi_Net_LWM2M_App_Handler_t handler, qapi_Net_LWM2M_Server_Data_t op_data);

qapi_Status_t ext_obj_handle_event(qapi_Net_LWM2M_App_Handler_t handler, qapi_Net_LWM2M_Server_Data_t op_data);
qapi_Status_t observe_cancel_ext_obj(qapi_Net_LWM2M_App_Handler_t handler, qapi_Net_LWM2M_Server_Data_t op_data);
qapi_Status_t m2m_create_object_instance(qapi_Net_LWM2M_App_Handler_t handler, qapi_Net_LWM2M_Server_Data_t op_data);
qapi_Status_t m2m_ext_read_obj
(
	qapi_Net_LWM2M_App_Handler_t handler, 
	qapi_Net_LWM2M_Server_Data_t op_data
);

qapi_Status_t m2m_ext_write_obj
(
	qapi_Net_LWM2M_App_Handler_t handler, 
	qapi_Net_LWM2M_Server_Data_t op_data
);

qapi_Status_t ext_object_write_attr
(
	qapi_Net_LWM2M_App_Handler_t handler, 
	qapi_Net_LWM2M_Server_Data_t op_data
);

qapi_Status_t disc_ext_obj
(
	qapi_Net_LWM2M_App_Handler_t handler, 
	qapi_Net_LWM2M_Server_Data_t op_data
);

qapi_Status_t exec_ext_obj
(
	qapi_Net_LWM2M_App_Handler_t handler, 
	qapi_Net_LWM2M_Server_Data_t op_data
);

qapi_Status_t delete_object_instance
(
	qapi_Net_LWM2M_App_Handler_t handler, 
	qapi_Net_LWM2M_Server_Data_t op_data
);

qapi_Status_t observe_ext_obj
(
	qapi_Net_LWM2M_App_Handler_t handler, 
	qapi_Net_LWM2M_Server_Data_t op_data
);

int ext_obj_discover_serialize
(
	qapi_Net_LWM2M_App_Handler_t handler,
	qapi_Net_LWM2M_Server_Data_t op_data,
	qapi_Net_LWM2M_Uri_t *uri,
	int size,
	qapi_Net_LWM2M_Flat_Data_t *data,
	uint8_t **buffer
);

qapi_Status_t ext_observer_add_notify_to_pending_list
(
	qapi_Net_LWM2M_App_Handler_t handler,
	qapi_Net_LWM2M_Observe_info_t *observe_info,
	qapi_Net_LWM2M_Uri_t uri,
	qapi_Net_LWM2M_Content_Type_t format,
	uint8_t *buffer, size_t length
);

qapi_Status_t read_data_ext
(	
	uint16_t instanceid, 
	int *size, 
	qapi_Net_LWM2M_Flat_Data_t **data,
    qapi_Net_LWM2M_Data_t *object
);

void m2m_ext_object_notify(int id, time_t current_time, time_t *timeout);

void m2m_ext_obj_task_entry(void *thread_ctxt);
void m2m_ext_obj_process_commands(void);

qapi_Status_t m2m_ext_object_configclient(void);
qapi_Status_t m2m_ext_object_deregister(void);
qapi_Status_t m2m_delete_object_instance_app(qapi_Net_LWM2M_Ext_t *obj_buffer);
qapi_Status_t m2m_create_object(void);
qapi_Status_t m2m_ext_object_register(void);
qapi_Status_t m2m_create_object_instance_app(qapi_Net_LWM2M_Ext_t *obj_buffer);

bool lwm2m_check_readable(uint16_t obj_id, uint8_t id);
qapi_Net_LWM2M_Observed_t * lwm2m_find_observed
(
	qapi_Net_LWM2M_App_Handler_t handler,
	qapi_Net_LWM2M_Uri_t *uri
);

qapi_Net_LWM2M_Observe_info_t *lwm2m_find_observe_info
(
	qapi_Net_LWM2M_Observed_t *observed,
	uint8_t *msg_id,
	uint8_t msg_id_len
);

qapi_Net_LWM2M_Observe_info_t *lwm2m_find_observe_info
(
	qapi_Net_LWM2M_Observed_t *observed,
	uint8_t *msg_id,
	uint8_t msg_id_len
);

qapi_Net_LWM2M_Instance_Info_t *obj_create_generic_sensor(qapi_Net_LWM2M_Data_t *lwm2m_data);

int32 http_inet_ntoa(const qapi_DSS_Addr_t inaddr, uint8 *buf, int32 buflen);

#endif /* __EXAMPLE_LWM2M_EXT__ */
#endif /* __QUECTEL_LWM2M_EXT_H__ */

