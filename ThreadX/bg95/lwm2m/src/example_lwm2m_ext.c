/******************************************************************************
*@file    example_lwm2m_ext.c
*@brief   Create an extended object and connect to the LwM2M server.
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2017 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#if defined(__EXAMPLE_LWM2M_EXT__)
/* Standard C headers
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stdarg.h"
#include "float.h"
//#include <stringl/stringl.h>

/*===========================================================================
						   QAPI Header file
===========================================================================*/
#include "qapi_timer.h"
#include "qapi_uart.h"
#include "qapi_fs.h"
#include "qapi_status.h"
#include "qapi_lwm2m.h"

#include "quectel_uart_apis.h"
#include "quectel_utils.h"
#include "example_lwm2m_ext.h"

/*===========================================================================
                             DEFINITION
===========================================================================*/

#define QUEC_M2M_UART_DBG

#ifdef QUEC_M2M_UART_DBG
#define M2M_UART_PRINT(...)	\
{\
	m2m_uart_debug_print(__VA_ARGS__);	\
}
#else
#define M2M_UART_PRINT(...)
#endif

/*===========================================================================
                           Global variable
===========================================================================*/

TX_EVENT_FLAGS_GROUP *m2m_signal_handle;

qapi_DSS_Hndl_t m2m_dss_handle = NULL;	            /* Related to DSS netctrl */

typedef  unsigned char      boolean; 

#define BYTE_POOL_SIZE       (30720)

ULONG   m2m_free_mem[BYTE_POOL_SIZE];
TX_BYTE_POOL  *byte_pool_m2m;
TX_THREAD *m2m_thread_hdl;

/* lwm2m_ext global */
boolean m2m_obj_exist = 0;

qapi_Net_LWM2M_App_Handler_t ext_object_handler_app = NULL;

qapi_Net_LWM2M_Data_v2_t *object_generic = NULL;
qapi_Net_LWM2M_Data_t *object_presence = NULL;
qapi_Net_LWM2M_Observed_t *ext_observed_list = NULL;
qapi_Net_LWM2M_Pending_Observed_t *ext_pending_observed_list = NULL;
qapi_Net_LWM2M_Observed_t *ext_multi_observed_list = NULL;
qapi_Net_LWM2M_Pending_Observed_t *ext_multi_pending_observed_list = NULL;
qapi_Net_LWM2M_Config_Data_t* ext_object_clientconfig_data = NULL;

int obj_inst_index_generic;
int obj_inst_created_generic[MAX_EXT_INSTANCE] = {0};
int obj_inst_index_presence;
int obj_inst_created_presence[MAX_EXT_INSTANCE] = {0};

qapi_Net_LWM2M_Event_t ext_obj_event;
static bool lwm2m_client_in_sleep_state = false;

/* uart rx tx buffer */
static char *uart_rx_buff = NULL;	/*!!! should keep this buffer as 4K Bytes */
static char *uart_tx_buff = NULL;

/* uart config para*/
static QT_UART_CONF_PARA uart_conf =
{
	NULL,
	QT_QAPI_UART_PORT_01,
	NULL,
	0,
	NULL,
	0,
	115200
};

/*===========================================================================
                               FUNCTION
===========================================================================*/
static void m2m_uart_rx_cb(uint32_t num_bytes, void *cb_data)
{
	if(num_bytes == 0)
	{
		qapi_UART_Receive(uart_conf.hdlr, uart_conf.rx_buff, uart_conf.rx_len, NULL);
		return;
	}
	else if(num_bytes >= uart_conf.rx_len)
	{
		num_bytes = uart_conf.rx_len;
	}
	
	IOT_DEBUG("QT# uart_rx_cb data [%d][%s]", num_bytes, uart_conf.rx_buff);
	memset(uart_conf.rx_buff, 0, uart_conf.rx_len);

	qapi_UART_Receive(uart_conf.hdlr, uart_conf.rx_buff, uart_conf.rx_len, NULL);
}

static void m2m_uart_tx_cb(uint32_t num_bytes, void *cb_data)
{
	IOT_DEBUG("QT# uart_tx_cb, send [%d]", num_bytes);
}

void m2m_uart_dbg_init()
{
	qapi_Status_t status;
	qapi_UART_Open_Config_t uart_cfg;
	
	if (TX_SUCCESS != tx_byte_allocate(byte_pool_m2m, (VOID *)&uart_rx_buff, 4*1024, TX_NO_WAIT))
	{
		IOT_DEBUG("tx_byte_allocate [uart_rx_buff] failed!");
		return;
	}

	if (TX_SUCCESS != tx_byte_allocate(byte_pool_m2m, (VOID *)&uart_tx_buff, 4*1024, TX_NO_WAIT))
	{
		IOT_DEBUG("tx_byte_allocate [uart_tx_buff] failed!");
		return;
	}

	uart_conf.rx_buff = uart_rx_buff;
	uart_conf.tx_buff = uart_tx_buff;
	uart_conf.tx_len = 4096;
	uart_conf.rx_len = 4096;

	/* debug uart init			*/
	uart_cfg.baud_Rate			= 115200;
	uart_cfg.enable_Flow_Ctrl   = QAPI_FCTL_OFF_E;
	uart_cfg.bits_Per_Char		= QAPI_UART_8_BITS_PER_CHAR_E;
	uart_cfg.enable_Loopback	= 0;
	uart_cfg.num_Stop_Bits		= QAPI_UART_1_0_STOP_BITS_E;
	uart_cfg.parity_Mode		= QAPI_UART_NO_PARITY_E;
	uart_cfg.rx_CB_ISR			= (qapi_UART_Callback_Fn_t)m2m_uart_rx_cb;
	uart_cfg.tx_CB_ISR			= (qapi_UART_Callback_Fn_t)m2m_uart_tx_cb;

	status = qapi_UART_Open(&uart_conf.hdlr, uart_conf.port_id, &uart_cfg);
	IOT_DEBUG("QT# qapi_UART_Open [%d] status %d", (qapi_UART_Port_Id_e)uart_conf.port_id, status);
	if (QAPI_OK != status)
	{
		return;
	}
	status = qapi_UART_Power_On(uart_conf.hdlr);
	if (QAPI_OK != status)
	{
		return;
	}
	
	/* start uart receive */
	status = qapi_UART_Receive(uart_conf.hdlr, uart_conf.rx_buff, uart_conf.rx_len, NULL);
	IOT_DEBUG("QT# qapi_UART_Receive [%d] status %d", (qapi_UART_Port_Id_e)uart_conf.port_id, status);
}

void m2m_uart_debug_print(const char* fmt, ...) 
{
	va_list arg_list;
    char dbg_buffer[128] = {0};
    
	va_start(arg_list, fmt);
    vsnprintf((char *)(dbg_buffer), sizeof(dbg_buffer), (char *)fmt, arg_list);
    va_end(arg_list);
		
    qapi_UART_Transmit(uart_conf.hdlr, dbg_buffer, strlen(dbg_buffer), NULL);
    //qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_MSEC, true);   //Do not add delay unless necessary
}

#define     EXT_Q_NO_OF_MSGS         100
#define     EXT_Q_MESSAGE_SIZE       TX_1_ULONG
#define     EXT_QUEUE_SIZE           EXT_Q_NO_OF_MSGS*EXT_Q_MESSAGE_SIZE*sizeof(ext_obj_cmd_t)

static ext_obj_info_t          ext_obj_param;
static ext_obj_task_cmd_q*     ext_obj_cmdq;
boolean app_reg_status = false;

/*--------------------------------------------------------------------*
						lwm2m object signal init interface
 *--------------------------------------------------------------------*/
 
/*
 * @fn 		m2m_ext_obj_set_signal
 * @brief	Set the extensible object signal using lock protection
 */
void m2m_ext_obj_set_signal(void)
{
	IOT_DEBUG("m2m_ext_obj_set_signal \n");

	tx_mutex_get(ext_obj_param.ext_obj_mutex, TX_WAIT_FOREVER);

	EXT_OBJ_TASK_SET_SIG(ext_obj_param.ext_obj_signal,EXT_OBJ_SIGNAL_MASK);

	tx_mutex_put(ext_obj_param.ext_obj_mutex);

	return;
}

/*
 * @fn		ext_obj_mutex_init
 * @brief	extensible object mutex Initialization
 */
static void ext_obj_mutex_init(void)
{
	IOT_DEBUG("ext_obj_mutex_init \n");
    txm_module_object_allocate(&ext_obj_param.ext_obj_mutex, sizeof(TX_MUTEX));
	tx_mutex_create(ext_obj_param.ext_obj_mutex, "ext_obj_main_mutex", TX_NO_INHERIT);
	return;
}

/*
 * @fn		ext_obj_signal_init
 * @brief	Extensible Object Signal Initialization
 */
static void ext_obj_signal_init(void)
{
	IOT_DEBUG("ext_obj_signal_init \n");
    txm_module_object_allocate(&ext_obj_param.ext_obj_signal, sizeof(TX_EVENT_FLAGS_GROUP));
	tx_event_flags_create(ext_obj_param.ext_obj_signal, "ext_obj_main_sig");
	return;
}
extern void m2m_ext_obj_process_commands(void);

/*
 * @fn 		ext_obj_signal_wait
 * @brief 	This API will wait on the EXT_OBJ signal.
 *			The EXT_OBJ signal will be set during various operations.
 */
static void ext_obj_signal_wait(void)
{
	uint32_t received_sigs = 0;

	IOT_DEBUG("ext_obj_signal_wait \n");

	while (1)
	{
		/* Wait for the EXT_OBJ Signal to be set */
		tx_event_flags_get(ext_obj_param.ext_obj_signal, EXT_OBJ_SIGNAL_MASK, TX_OR_CLEAR, &received_sigs, TX_WAIT_FOREVER);

		if (EXT_OBJ_TASK_IS_SIG_SET (received_sigs, EXT_OBJ_SIGNAL_MASK))
		{
            IOT_DEBUG("receive EXT_OBJ_SIGNAL_MASK\n");
            
			// EXT_OBJ Signal is set Dequeue the EXT_OBJ Command queue
			m2m_ext_obj_process_commands();
        }
	}
}


/*
 * @fn	ext_obj_task_init
 * @brief
 *	 	Extensible object Task initialization
 */
void ext_obj_task_init(void)
{
	IOT_DEBUG("ext_obj_task_init \n");

	/* Reseting the Global EXT_OBJ Structure */
	memset(&ext_obj_param, 0, sizeof (ext_obj_info_t));

	/* Initialize the command queues and set the command queue pointers in the
	* ext_obj_info struct to the appropriate values. */
	ext_obj_cmdq_init(&ext_obj_param);
}

/*
 * @fn  m2m_ext_obj_task_entry
 * @brief
 *	 This is the entry point for Extensible object task
 *	 Reset the Global EXT_OBJ structure
 *	 Initalize the EXT_OBJ Qurt signal
 */
void m2m_ext_obj_task_entry(void *thread_ctxt)
{
	IOT_DEBUG("ext_obj_task_entry \n");
	ext_obj_task_init();

	/* Initialize Mutex Object */
	ext_obj_mutex_init();

	/* Initialize Signal for EXT_OBJ */
	ext_obj_signal_init ();

	/* Wait on the EXT_OBJ Signal */
	ext_obj_signal_wait();
}

/*--------------------------------------------------------------------*
						lwm2m timer interface
 *--------------------------------------------------------------------*/
static qapi_TIMER_handle_t obs_notify_Timer;

void notify_signal_cb(uint32 userData)
{
	ext_obj_cmd_t* cmd_ptr = NULL;
	qapi_Net_LWM2M_Ext_t *ext_param = NULL;

	tx_byte_allocate(byte_pool_m2m, (VOID **)&ext_param, sizeof(qapi_Net_LWM2M_Ext_t), TX_NO_WAIT);

	if (ext_param == NULL)
	{
		IOT_DEBUG("Cannot assign memory for extensible object parameter");
		return;
	}

	memset(ext_param, 0x00, sizeof(qapi_Net_LWM2M_Ext_Obj_t));
	ext_param->app_data.ul_op.msg_type = QAPI_NET_LWM2M_EXT_NOTIFY_E;

	IOT_DEBUG("Switching from Timer to application callback context");

	if ((cmd_ptr = ext_obj_get_cmd_buffer()) == NULL)
	{
		IOT_DEBUG("Cannot obtain command buffer");
		return;
	}
	else
	{
		cmd_ptr->cmd_hdr.cmd_id = EXT_OBJ_APP_ORIGINATED_EVT;
		cmd_ptr->cmd_data.data  = (void *)ext_param;

		if(app_reg_status == false)
		{
			tx_byte_release(ext_param);
			return;
		}

		IOT_DEBUG("Enqueue the command into the command queue. ");
		ext_obj_enqueue_cmd(cmd_ptr);
	}

	m2m_ext_obj_set_signal();

	return;
}

void start_ext_obs_notify_timer()
{
	if (ext_observed_list == NULL)
	{
		qapi_TIMER_define_attr_t time_attr;
		time_attr.deferrable = FALSE;
		time_attr.cb_type = QAPI_TIMER_FUNC1_CB_TYPE;
		time_attr.sigs_func_ptr = notify_signal_cb;
		time_attr.sigs_mask_data = 0;
		qapi_Timer_Def(&obs_notify_Timer, &time_attr);
	}
}

void stop_ext_obs_notify_timer(qapi_Net_LWM2M_App_Handler_t handler)
{
	if (handler == ext_object_handler_app)
	{
		qapi_Timer_Stop(obs_notify_Timer);
		qapi_Timer_Undef(obs_notify_Timer);
	}
}

/*-------------------------------------------------------------------*
					lwm2m common data decode and parse
 *-------------------------------------------------------------------*/
static qapi_Net_LWM2M_Flat_Data_t * lwm2m_data_new(int size)
{
  qapi_Net_LWM2M_Flat_Data_t * dataP = NULL;

  if (size <= 0) return NULL;

  tx_byte_allocate(byte_pool_m2m, (VOID **)&dataP, size * sizeof(qapi_Net_LWM2M_Flat_Data_t), TX_NO_WAIT);

  if (dataP != NULL)
  {
    memset(dataP, 0, size * sizeof(qapi_Net_LWM2M_Flat_Data_t));
  }

  return dataP;
}

static void lwm2m_data_free(int size, qapi_Net_LWM2M_Flat_Data_t* dataP)
{
	int i = 0;

	IOT_DEBUG("size: %d", size);
	if (size == 0 || dataP == NULL) return;

	for (i = 0; i < size; i++)
	{
		switch (dataP[i].type)
		{
			case QAPI_NET_LWM2M_TYPE_MULTIPLE_RESOURCE:
			case QAPI_NET_LWM2M_TYPE_OBJECT_INSTANCE:
			case QAPI_NET_LWM2M_TYPE_OBJECT:
			{
				lwm2m_data_free(dataP[i].value.asChildren.count, dataP[i].value.asChildren.array);
				break;
			}
			case QAPI_NET_LWM2M_TYPE_STRING_E:
			case QAPI_NET_LWM2M_TYPE_OPAQUE_E:
			{
				if (dataP[i].value.asBuffer.buffer != NULL)
				{
					tx_byte_release(dataP[i].value.asBuffer.buffer);
				}
			}
			default:
			{
				// do nothing
				break;
			}
		}
	}
	tx_byte_release(dataP);
}

static int setBuffer
(
	qapi_Net_LWM2M_Flat_Data_t *dataP,
	uint8_t * buffer,
	size_t bufferLen
)
{
  if( dataP == NULL || buffer == NULL || bufferLen == 0 ) 
  { 
    IOT_DEBUG(" Passed  Argument is NULL");
    return 0;
  }

  tx_byte_allocate(byte_pool_m2m, (VOID **)&(dataP->value.asBuffer.buffer), bufferLen, TX_NO_WAIT);
  if (dataP->value.asBuffer.buffer == NULL)
  {
    lwm2m_data_free(1, dataP);
    return 0;
  }
  dataP->value.asBuffer.length = bufferLen;
  memcpy(dataP->value.asBuffer.buffer, buffer, bufferLen);

  return 1;
}

static size_t utils_intToText
(
	int64_t data,
	uint8_t *string,
	size_t length
)
{
	int index = 0;
	bool minus = false;
	size_t result = 0;
	
	if (string == NULL)
	{
		IOT_DEBUG("Passed  Argument is NULL");
		return 0;
	}

	if (data < 0)
	{
		minus = true;
		data = 0 - data;
	}
	else
	{
		minus = false;
	}

	index = length - 1;
	do
	{
		string[index] = '0' + data%10;
		data /= 10;
		index --;
	} while(index >= 0 && data > 0);

	if(data > 0)
	{
		return 0;
	}
	
	if(minus == true)
	{
		if(index == 0)
			return 0;
		string[index] = '-';
	}
	else
	{
		index++;
	}

	result = length - index;

	if (result < length)
	{
		memcpy(string, string + index, result);
	}

	return result;
}

static size_t utils_floatToText
(
	double data,
	uint8_t * string,
	size_t length
)
{
  size_t intLength = 0;
  size_t decLength = 0;
  int64_t intPart = 0;
  double decPart = 0;

  if(string == NULL)
  {
    IOT_DEBUG(" Passed NULL Arguments\n");
    return 0;
  }

  if (data <= (double)INT64_MIN || data >= (double)INT64_MAX) 
  {
  	return 0;
  }

  intPart = (int64_t)data;
  decPart = data - intPart;
  if (decPart < 0)
  {
    decPart = 1 - decPart;
  }
  else
  {
    decPart = 1 + decPart;
  }

  if (decPart <= 1 + FLT_EPSILON)
  {
    decPart = 0;
  }

  if (intPart == 0 && data < 0)
  {
    // deal with numbers between -1 and 0
    if (length < 4) return 0;   // "-0.n"
    string[0] = '-';
    string[1] = '0';
    intLength = 2;
  }
  else
  {
    intLength = utils_intToText(intPart, string, length);
    if (intLength == 0) return 0;
  }
  decLength = 0;
  if (decPart >= FLT_EPSILON)
  {
    int i;
    double noiseFloor;

    if (intLength >= length - 1) return 0;

    i = 0;
    noiseFloor = FLT_EPSILON;
    do
    {
      decPart *= 10;
      noiseFloor *= 10;
      i++;
    } while (decPart - (int64_t)decPart > noiseFloor);

    decLength = utils_intToText(decPart, string + intLength, length - intLength);
    if (decLength <= 1) return 0;

    // replace the leading 1 with a dot
    string[intLength] = '.';
  }

  return intLength + decLength;
}

static int uri_toString
(
	qapi_Net_LWM2M_Uri_t *uri,
	uint8_t * buffer,
	size_t bufferLen,
	qapi_Net_LWM2M_Uri_Depth_t *depth
)
{
  size_t head = 0;
  int res = 0;

  if(buffer == NULL)
  {
    IOT_DEBUG("Passed NULL Arguments \n");
    return -1;
  }
  buffer[0] = '/';

  if(uri == NULL)
  {
    if(depth) 
    {
      *depth = URI_DEPTH_OBJECT;
    }
   return 1;
  }

  head = 1;

  res = utils_intToText(uri->objectId, buffer + head, bufferLen - head);
  if (res <= 0)
  {
    return -1;
  }
  head += res;

  if(head >= bufferLen - 1)
  {
    return -1;
  }
  if(depth) 
  {
    *depth = URI_DEPTH_OBJECT_INSTANCE;
  }

  if(uri->flag & QAPI_NET_LWM2M_INSTANCE_ID_E)
  {
    buffer[head] = '/';
    head++;
    res = utils_intToText(uri->instanceId, buffer + head, bufferLen - head);
    if(res <= 0)
    {
      return -1;
    }
    head += res;
    if(head >= bufferLen - 1) return -1;
    if(depth) *depth = URI_DEPTH_RESOURCE;
    if(uri->flag & QAPI_NET_LWM2M_RESOURCE_ID_E)
    {
      buffer[head] = '/';
      head++;
      res = utils_intToText(uri->resourceId, buffer + head, bufferLen - head);
      if(res <= 0) return -1;
      head += res;
      if(head >= bufferLen - 1) return -1;
      if(depth) *depth = URI_DEPTH_RESOURCE_INSTANCE;
    }
  }

  buffer[head] = '/';
  head++;

  IOT_DEBUG("length: %u, buffer: \"%.*s\"", head, head, buffer);

  return head;
}

static int utils_plainTextToInt64
(
	uint8_t * buffer,
	int length,
	int64_t * dataP)
{
  uint64_t result = 0;
  int sign = 1;
  int i = 0;

  if (0 == length || dataP == NULL || buffer == NULL ) return 0;

  if (buffer[0] == '-')
  {
    sign = -1;
    i = 1;
  }

  while (i < length)
  {
    if ('0' <= buffer[i] && buffer[i] <= '9')
    {
      if (result > (UINT64_MAX / 10)) return 0;
      result *= 10;
      result += buffer[i] - '0';
    }
    else
    {
      return 0;
    }
    i++;
  }

  if (result > INT64_MAX) return 0;

  if (sign == -1)
  {
    *dataP = 0 - result;
  }
  else
  {
    *dataP = result;
  }

  return 1;
}

static int utils_plainTextToFloat64
(
	uint8_t * buffer,
	int length,
	double * dataP
)
{
  double result;
  int sign;
  int i;

  if (0 == length || buffer == NULL || dataP == NULL) return 0;

  if (buffer[0] == '-')
  {
    sign = -1;
    i = 1;
  }
  else
  {
    sign = 1;
    i = 0;
  }

  result = 0;
  while (i < length && buffer[i] != '.')
  {
    if ('0' <= buffer[i] && buffer[i] <= '9')
    {
      if (result > (DBL_MAX / 10))
	  {
	  	return 0;
      }
      result *= 10;
      result += (buffer[i] - '0');
    }
    else
    {
      return 0;
    }
    i++;
  }
  if (buffer[i] == '.')
  {
    double dec;

    i++;
    if (i == length) return 0;

    dec = 0.1;
    while (i < length)
    {
      if ('0' <= buffer[i] && buffer[i] <= '9')
      {
        if (result > (DBL_MAX - 1)) 
		{	
			return 0;
        }
        result += (buffer[i] - '0') * dec;
        dec /= 10;
      }
      else
      {
        return 0;
      }
      i++;
    }
  }

  *dataP = result * sign;
  return 1;
}

static void lwm2m_data_encode_string(const char * string, qapi_Net_LWM2M_Flat_Data_t * dataP)
{
  size_t len = 0;
  int res = 0;

  if(dataP == NULL)
  {
    IOT_DEBUG("Passed NULL Arguments\n");
    return;
  }

  if (string == NULL)
  {
    len = 0;
  }
  else
  {
    for (len = 0; string[len] != 0; len++);
  }

  if (len == 0)
  {
    dataP->value.asBuffer.length = 0;
    dataP->value.asBuffer.buffer = NULL;
    res = 1;
  }
  else
  {
    res = setBuffer(dataP, (uint8_t *)string, len);	
  }

  if (res == 1)
  {
    dataP->type = QAPI_NET_LWM2M_TYPE_STRING_E;
  }
  else
  {
    dataP->type = QAPI_NET_LWM2M_TYPE_UNDEFINED;
  }
}

static void lwm2m_data_encode_int
(	
	int64_t value,
    qapi_Net_LWM2M_Flat_Data_t * dataP
)
{

  if (dataP == NULL) 
  { 
    IOT_DEBUG(" Passed  Argument is NULL");
    return ;
  }

  dataP->type = QAPI_NET_LWM2M_TYPE_INTEGER_E;
  dataP->value.asInteger = value;
}

static void lwm2m_data_encode_float(double value,
                             qapi_Net_LWM2M_Flat_Data_t * dataP)
{
  IOT_DEBUG("value: %d", value);
  if( dataP == NULL ) 
  { 
    IOT_DEBUG("Passed  Argument is NULL");
    return ;
  }

  dataP->type = QAPI_NET_LWM2M_TYPE_FLOAT_E;
  dataP->value.asFloat = value;
}

static void lwm2m_data_encode_bool(bool value,
                            qapi_Net_LWM2M_Flat_Data_t * dataP)
{

  if( dataP == NULL ) 
  { 
    IOT_DEBUG(" Passed  Argument is NULL");
    return ;
  }

  dataP->type = QAPI_NET_LWM2M_TYPE_BOOLEAN_E;
  dataP->value.asBoolean = value;
}

static int lwm2m_data_decode_int(qapi_Net_LWM2M_Flat_Data_t * dataP,
                                 int64_t * valueP)
{
  int result;
  if ( dataP == NULL || valueP == NULL ) 
  {
    IOT_DEBUG("Passed  Argument is NULL");
    return 0;
  }

  switch (dataP->type)
  {
    case QAPI_NET_LWM2M_TYPE_INTEGER_E:
      *valueP = dataP->value.asInteger;
      result = 1;
      break;

    case QAPI_NET_LWM2M_TYPE_STRING_E:
    case QAPI_NET_LWM2M_TYPE_OPAQUE_E:
      result = utils_plainTextToInt64(dataP->value.asBuffer.buffer, dataP->value.asBuffer.length, valueP);
      break;

    default:
      return 0;
  }

  return result;
}

static int lwm2m_data_decode_float(qapi_Net_LWM2M_Flat_Data_t * dataP,
                                   double * valueP)
{
  int result;
  if( dataP == NULL || valueP == NULL ) 
  {
    IOT_DEBUG(" Passed  Argument is NULL");
    return 0; 
  }

  switch (dataP->type)
  {
    case QAPI_NET_LWM2M_TYPE_FLOAT_E:
      *valueP = dataP->value.asFloat;
      result = 1;
      break;

    case QAPI_NET_LWM2M_TYPE_INTEGER_E:
      *valueP = (double)dataP->value.asInteger;
      result = 1;
      break;

    case QAPI_NET_LWM2M_TYPE_STRING_E:
    case QAPI_NET_LWM2M_TYPE_OPAQUE_E:
      result = utils_plainTextToFloat64(dataP->value.asBuffer.buffer, dataP->value.asBuffer.length, valueP);
      break;

    default:
      return 0;
  }

  return result;
}

static int lwm2m_data_decode_bool(qapi_Net_LWM2M_Flat_Data_t * dataP,
                                  bool * valueP)
{
  int result; 
  if ( dataP == NULL || valueP == NULL ) 
  {
    IOT_DEBUG(" Passed  Argument is NULL");
    return 0;
  }
	IOT_DEBUG(" dataP->type:%d", dataP->type);
  switch (dataP->type)
  {
    case QAPI_NET_LWM2M_TYPE_BOOLEAN_E:
      *valueP = dataP->value.asBoolean;
      result = 1;
      break;

    case QAPI_NET_LWM2M_TYPE_STRING_E:
    case QAPI_NET_LWM2M_TYPE_OPAQUE_E:
      if (dataP->value.asBuffer.length != 1) return 0;

      switch (dataP->value.asBuffer.buffer[0])
      {
        case '0':
          *valueP = false;
          result = 1;
          break;
        case '1':
          *valueP = true;
          result = 1;
          break;
        default:
          result = 0;
          break;
      }
      break;

    default:
      result = 0;
      break;
  }

  return result;
}

/*--------------------------------------------------------------------*
						lwm2m object command queue interface
 *--------------------------------------------------------------------*/

/*
 * @fn		ext_obj_cmdq_init()
 * @brief	
 *	  Initialize the command queues and set the command queue pointers in the
 *	  ext_obj_info struct to the appropriate values.
 */
void ext_obj_cmdq_init(ext_obj_info_t *ext_obj_state)
{
	void *cmdq_ptr = NULL;

	if (ext_obj_state == NULL)
	{
		IOT_DEBUG("EXT_OBJ State is NULL ");
		return;
	}

	tx_byte_allocate(byte_pool_m2m, (VOID **)&cmdq_ptr, EXT_QUEUE_SIZE, TX_NO_WAIT);

	IOT_DEBUG("ext_obj_cmdq_init");

	ext_obj_param = *ext_obj_state;

    txm_module_object_allocate(&ext_obj_cmdq, sizeof(ext_obj_task_cmd_q));    
	tx_queue_create(ext_obj_cmdq, "m2m_ext_obj_cmdq", EXT_Q_MESSAGE_SIZE,cmdq_ptr, EXT_QUEUE_SIZE);

	ext_obj_param.cmd_q_ptr = ext_obj_cmdq;
}


/*
 * @fn		 ext_obj_get_cmd_buffer
 * @brief
 *			 This function is used to allocate a command buffer that the client can
 * 			 then enqueue into the command queue.
 */
ext_obj_cmd_t *ext_obj_get_cmd_buffer(void)
{
	ext_obj_cmd_t *cmd_ptr = NULL;

	IOT_DEBUG("ext_obj_get_cmd_buffer \n ");

	tx_byte_allocate(byte_pool_m2m, (VOID **)&cmd_ptr, sizeof(ext_obj_cmd_t), TX_NO_WAIT);

	if (cmd_ptr == NULL) 
	{
		IOT_DEBUG("Command buffer allocation failed");
	}

	return cmd_ptr;
}

/*
 * @fn		 ext_obj_enqueue_cmd
 * @brief	 This function is used to enqueue the command into the EXT_OBJ queue
 * @parm	 Pointer to the command to enqueue
 * @ret		 void
 */
void ext_obj_enqueue_cmd(ext_obj_cmd_t* cmd_ptr)
{
	IOT_DEBUG("Cmd_ptr: 0x%x with cmd_id: %d, posted to int Q,",
	                         cmd_ptr, cmd_ptr->cmd_hdr.cmd_id);

	tx_queue_send(ext_obj_cmdq, &cmd_ptr, TX_WAIT_FOREVER);
	return;
}

/*
 * @fn		 ext_obj_dequeue_cmd
 * @brief	 This function de-queues and returns a command from the Extensible object task command queues
 * @ret 	 Pointer to the command if present in the queue. NULL otherwise
 */
static ext_obj_cmd_t* ext_obj_dequeue_cmd(void)
{
	ext_obj_cmd_t *cmd_ptr = NULL;

	IOT_DEBUG("ext_obj_dequeue_cmd called");

	tx_queue_receive(ext_obj_cmdq, &cmd_ptr, TX_WAIT_FOREVER);

	return cmd_ptr;
}

/*--------------------------------------------------------------------*
			      lwm2m message process interface 
 *--------------------------------------------------------------------*/

void send_response_message
(
	qapi_Net_LWM2M_App_Handler_t   handler,
	qapi_Net_LWM2M_Server_Data_t   op_data,
	qapi_Net_LWM2M_Response_Code_t status
)
{
	qapi_Net_LWM2M_App_Ex_Obj_Data_t app_data;

	memset(&app_data, 0x00, sizeof(qapi_Net_LWM2M_App_Ex_Obj_Data_t));

	app_data.msg_type = QAPI_NET_LWM2M_RESPONSE_MSG_E;
	app_data.obj_info = op_data.obj_info;
	app_data.status_code = status;
	app_data.conf_msg = 0;
	app_data.msg_id_len = op_data.msg_id_len;
	memcpy(app_data.msg_id, op_data.msg_id, op_data.msg_id_len);

	qapi_Net_LWM2M_Send_Message(handler, &app_data);
}

static qapi_Net_LWM2M_Observe_info_t *lwm2m_get_observe_info
(
	qapi_Net_LWM2M_App_Handler_t handler,
	qapi_Net_LWM2M_Uri_t *uri,
	uint8_t *msg_id,
	uint8_t msg_id_len,
	qapi_Net_LWM2M_Content_Type_t accept_type
)
{
	qapi_Net_LWM2M_Observed_t *observed = NULL;
	qapi_Net_LWM2M_Observe_info_t *observe_info = NULL;
	bool alloc = false;

	if (uri == NULL || msg_id == NULL || handler == NULL)
	{
		IOT_DEBUG("fun [%s] file [%s]  Passed  Argument is NULL\n",__func__,__FILE__);
		return NULL;
	}

	if (handler == ext_object_handler_app)
	{
#if 1
		if (ext_observed_list == NULL)
		{
			start_ext_obs_notify_timer();
		}
#endif
	}

	observed = lwm2m_find_observed(handler, uri);
	if (observed == NULL)
	{
		tx_byte_allocate(byte_pool_m2m, (VOID **)&observed, sizeof(qapi_Net_LWM2M_Observed_t), TX_NO_WAIT);
		if (observed == NULL)
		{
			return NULL;
		}
		
		alloc = true;
		memset(observed, 0x00, sizeof(qapi_Net_LWM2M_Observed_t));
		memcpy(&(observed->uri), uri, sizeof(qapi_Net_LWM2M_Uri_t));
		
		if (handler == ext_object_handler_app)
		{
			observed->next = ext_observed_list;
			ext_observed_list = observed;
		}
		
		observed->accept_type = accept_type;
	}

	observe_info = lwm2m_find_observe_info(observed, msg_id, msg_id_len);
	if(observe_info == NULL)
	{
		tx_byte_allocate(byte_pool_m2m, (VOID **)&observe_info, sizeof(qapi_Net_LWM2M_Observe_info_t), TX_NO_WAIT);
		if (observe_info == NULL)
		{
			if(alloc == true)
			{
				tx_byte_release(observed);
				observed = NULL;
			}
			
			return NULL;
		}
		
		memset(observe_info, 0x00, sizeof(qapi_Net_LWM2M_Observe_info_t));
		observe_info->active = false;
		observe_info->msg_id_len = msg_id_len;
		memcpy(observe_info->msg_id, msg_id, msg_id_len);
		observe_info->next = observed->observe_info;
		observed->observe_info = observe_info;
	}

	return observe_info;
}

qapi_Net_LWM2M_Observe_info_t* ext_findInheritedWatcher
(
	qapi_Net_LWM2M_App_Handler_t handler,
	qapi_Net_LWM2M_Uri_t *uri,
	uint8_t *msg_id,
	uint8_t msg_id_len,
	uint8_t flag
)
{
	qapi_Net_LWM2M_Observe_info_t *observe_info = NULL;
	qapi_Net_LWM2M_Observed_t *observed = NULL;

	if (uri == NULL || msg_id == NULL || handler == NULL)
	{
		IOT_DEBUG("fun [%s] file [%s]  Passed  Argument is NULL\n",__func__,__FILE__);
		return NULL;
	}

	if ((uri->flag & QAPI_NET_LWM2M_RESOURCE_ID_E))
	{
		uri->flag &= ~QAPI_NET_LWM2M_RESOURCE_ID_E;
		observed = lwm2m_find_observed(handler, uri);
		if(observed == NULL)
		{
			uri->flag &= ~QAPI_NET_LWM2M_INSTANCE_ID_E;
			observed = lwm2m_find_observed(handler, uri);
			
			if (observed == NULL)
			{
				//Do nothing
			}
			else
			{
				observe_info = lwm2m_find_observe_info(observed, msg_id, msg_id_len);
			}
			
			uri->flag |= QAPI_NET_LWM2M_INSTANCE_ID_E;
		}
		else
		{
			observe_info = lwm2m_find_observe_info(observed, msg_id, msg_id_len);
			if (observe_info != NULL && observe_info->attributes!= NULL)
			{
				if ((observe_info->attributes->set_attr_mask & flag) == 0)
				{
					uri->flag &= ~QAPI_NET_LWM2M_INSTANCE_ID_E;
					observed = lwm2m_find_observed(handler, uri);
					
					if (observed == NULL)
					{
						observe_info = NULL;
						IOT_DEBUG("Observed list not found for URI \n");
					}
					else
					{
						observe_info = lwm2m_find_observe_info(observed, msg_id, msg_id_len);
					}
					
					uri->flag |= QAPI_NET_LWM2M_INSTANCE_ID_E;
				}
			}
		}
		uri->flag |= QAPI_NET_LWM2M_RESOURCE_ID_E;
	}
	else if((uri->flag & QAPI_NET_LWM2M_INSTANCE_ID_E))
	{
		uri->flag &= ~QAPI_NET_LWM2M_INSTANCE_ID_E;
		observed = lwm2m_find_observed(handler, uri);
		
		if (observed == NULL)
		{
			//Do nothing
		}
		else
		{
			observe_info = lwm2m_find_observe_info(observed, msg_id, msg_id_len);
		}
		
		uri->flag |= QAPI_NET_LWM2M_INSTANCE_ID_E;
	}

	return observe_info;
}

/*--------------------------------------------------------------------*
			lwm2m object, instance and resource op
 *--------------------------------------------------------------------*/
 
qapi_Net_LWM2M_Observed_t * lwm2m_find_observed
(
	qapi_Net_LWM2M_App_Handler_t handler,
	qapi_Net_LWM2M_Uri_t *uri
)
{
	qapi_Net_LWM2M_Observed_t *target = NULL;

	if (uri == NULL || handler == NULL)
	{
		return NULL;
	}

	if (handler == ext_object_handler_app)
	{
		target = ext_observed_list;
	}

	while (target != NULL && (target->uri.objectId != uri->objectId
			|| target->uri.flag != uri->flag
			|| ((uri->flag & QAPI_NET_LWM2M_INSTANCE_ID_E) && target->uri.instanceId != uri->instanceId)
			|| ((uri->flag & QAPI_NET_LWM2M_RESOURCE_ID_E) && target->uri.resourceId != uri->resourceId)))
	{
		target = target->next;
	}

	return target;
}

static void lwm2m_unlink_observed
(
	qapi_Net_LWM2M_App_Handler_t handler,
	qapi_Net_LWM2M_Observed_t *observed
)
{
	if (handler == NULL || observed == NULL ) 
	{
		IOT_DEBUG("fun [%s] file [%s]  Passed  Argument is NULL\n",__func__,__FILE__);
		return;
	}

	if (handler == ext_object_handler_app && ext_observed_list == NULL)
	{
		IOT_DEBUG("fun [%s] file [%s]  Passed  Argument is NULL\n",__func__,__FILE__);
		return;
	}

	if (handler == ext_object_handler_app)
	{
		if (ext_observed_list == observed)
		{
			ext_observed_list = ext_observed_list->next;
		}
		else
		{
			qapi_Net_LWM2M_Observed_t *list = NULL;

			list = ext_observed_list;
			while (list->next != NULL && list->next != observed)
			{
				list = list->next;
			}

			if(list->next != NULL)
			{
				list->next = list->next->next;
			}
		}
	}

	return;
}

qapi_Net_LWM2M_Observe_info_t *lwm2m_find_observe_info
(
	qapi_Net_LWM2M_Observed_t *observed,
	uint8_t *msg_id,
	uint8_t msg_id_len
)
{
	qapi_Net_LWM2M_Observe_info_t *target = NULL;

	if (observed == NULL || msg_id == NULL ) 
	{
		IOT_DEBUG("fun [%s] file [%s]  Passed  Argument is NULL\n",__func__,__FILE__);
		return NULL;
	}

	target = observed->observe_info;
	while (target != NULL && (memcmp((target->msg_id + target->msg_id_len - 2), (msg_id +msg_id_len -2), 2) != 0))
	{
		target = target->next;
	}

	return target;
}

qapi_Net_LWM2M_Data_v2_t* lwm2m_object_add
(
	qapi_Net_LWM2M_Data_v2_t* head,
	qapi_Net_LWM2M_Data_v2_t* node
)
{
	qapi_Net_LWM2M_Data_v2_t* target = NULL;

	if (NULL == head) 
		return node;

	if (head->object_ID > node->object_ID)
	{
		node->next = head;
		return node;
	}

	target = head;
	while (NULL != target->next && target->next->object_ID < node->object_ID)
	{
		target = target->next;
	}

	node->next = target->next;
	target->next = node;

	return head;
}

qapi_Net_LWM2M_Instance_Info_v2_t* lwm2m_instance_add
(
	qapi_Net_LWM2M_Instance_Info_v2_t* head,
	qapi_Net_LWM2M_Instance_Info_v2_t* node
)
{
	qapi_Net_LWM2M_Instance_Info_v2_t* target = NULL;

	if (NULL == head) return node;

	if (head->instance_ID > node->instance_ID)
	{
		node->next = head;
		return node;
	}

	target = head;
	while (NULL != target->next && target->next->instance_ID < node->instance_ID)
	{
		target = target->next;
	}

	node->next = target->next;
	target->next = node;

	return head;
}

qapi_Net_LWM2M_Resource_Info_t* lwm2m_resource_add
(
	qapi_Net_LWM2M_Resource_Info_t* head,
	qapi_Net_LWM2M_Resource_Info_t* node
)
{
	qapi_Net_LWM2M_Resource_Info_t* target = NULL;

	if (NULL == head) 
		return node;

	if (head->resource_ID > node->resource_ID)
	{
		node->next = head;
		return node;
	}

	target = head;
	while (NULL != target->next && target->next->resource_ID < node->resource_ID)
	{
		target = target->next;
	}

	node->next = target->next;
	target->next = node;

	return head;
}

qapi_Net_LWM2M_Data_t* lwm2m_object_remove
(
	qapi_Net_LWM2M_Data_t *head,
	qapi_Net_LWM2M_Data_t ** nodeP,
	uint8_t id
)
{
	qapi_Net_LWM2M_Data_t * target;

	if (head == NULL)
	{
		if (nodeP) 
			*nodeP = NULL;
		return NULL;
	}

	if (head->object_ID == id)
	{
		if (nodeP) 
			*nodeP = head;
		return head->next;
	}

	target = head;
	while (NULL != target->next && target->next->object_ID < id)
	{
		target = target->next;
	}

	if (NULL != target->next && target->next->object_ID == id)
	{
		if (nodeP) 
			*nodeP = target->next;
		target->next = target->next->next;
	}
	else
	{
		if (nodeP) 
			*nodeP = NULL;
	}

	return head;
}

qapi_Net_LWM2M_Data_v2_t *lwm2m_object_find
(
	qapi_Net_LWM2M_Data_v2_t* head,
    uint16_t id
)
{
	while (NULL != head && head->object_ID < id)
	{
		head = head->next;
	}

	if (NULL != head && head->object_ID == id)
	{
		return head;
	}
	
	return NULL;
}

qapi_Net_LWM2M_Resource_Info_t* lwm2m_resource_find
(
	qapi_Net_LWM2M_Resource_Info_t* head,
    uint16_t id
)
{
	while (NULL != head && head->resource_ID < id)
	{
		head = head->next;
	}

	if (NULL != head && head->resource_ID == id)
	{
		return head;
	}
	
	return NULL;
}

qapi_Net_LWM2M_Instance_Info_v2_t* lwm2m_instance_remove
(
	qapi_Net_LWM2M_Instance_Info_v2_t   *head,
	qapi_Net_LWM2M_Instance_Info_v2_t  **nodeP,
	uint8_t                           id
)
{
  qapi_Net_LWM2M_Instance_Info_v2_t *target;

  if (head == NULL)
  {
    if (nodeP)
    {
		*nodeP = NULL;
    }
	return NULL;
  }

  if (head->instance_ID== id)
  {
    if (nodeP) 
    {
		*nodeP = head;
    }
	return head->next;
  }

  target = head;
  while (NULL != target->next && target->next->instance_ID< id)
  {
    target = target->next;
  }

  if (NULL != target->next && target->next->instance_ID== id)
  {
    if (nodeP) 
	{
		*nodeP = target->next;
    }
	target->next = target->next->next;
  }
  else
  {
    if (nodeP) 
	{
		*nodeP = NULL;
    }
  }

  return head;
}

qapi_Net_LWM2M_Instance_Info_v2_t* lwm2m_instance_find
(
	qapi_Net_LWM2M_Instance_Info_v2_t* head,
    uint16_t id
)
{
  while (NULL != head && head->instance_ID < id)
  {
    head = head->next;
  }

  if (NULL != head && head->instance_ID == id)
  return head;

  return NULL;
}

qapi_Status_t ext_object_read
(
	qapi_Net_LWM2M_App_Handler_t handler,
	qapi_Net_LWM2M_Uri_t uri, 
	qapi_Net_LWM2M_Content_Type_t format, 
	uint8_t **buffer, 
	uint32_t *length
)
{
  qapi_Status_t result = QAPI_ERROR;
  qapi_Net_LWM2M_Obj_Info_t  obj_info;
  qapi_Net_LWM2M_Data_v2_t *object = NULL;
  qapi_Net_LWM2M_Instance_Info_v2_t *instance = NULL;
  qapi_Net_LWM2M_Resource_Info_t *resource = NULL;
  qapi_Net_LWM2M_Flat_Data_t *data = NULL;
  int i = 0;
  int size = 0;
  qapi_Net_LWM2M_Content_Type_t cont_type;

  if(handler == NULL || buffer == NULL || length == NULL)
  {
    IOT_DEBUG("Application handle is not valid for current application");
    //send_response_message(op_data, QAPI_NET_LWM2M_400_BAD_REQUEST_E);
    return QAPI_ERR_INVALID_PARAM;
  }

  obj_info.obj_mask = (qapi_Net_LWM2M_ID_t)uri.flag;
  obj_info.obj_id= uri.objectId;
  obj_info.obj_inst_id= uri.instanceId;
  obj_info.res_id= uri.resourceId;
  obj_info.res_inst_id= uri.resourceInstId;

  if(obj_info.obj_mask & QAPI_NET_LWM2M_RESOURCE_ID_E)
  {
    if(handler == ext_object_handler_app)
    {
      if(lwm2m_object_find(object_generic, obj_info.obj_id))
      {
        if(lwm2m_check_readable(obj_info.obj_id, obj_info.res_id) == false)
        {
          IOT_DEBUG("Read on resource %d not allowed.", obj_info.res_id);
          return QAPI_ERROR;
        }
      }
    }    
  }

  if((obj_info.obj_mask & QAPI_NET_LWM2M_OBJECT_ID_E))
  {
    if(handler == ext_object_handler_app)
    {
      object = lwm2m_object_find(object_generic, obj_info.obj_id);
    }

    if(object != NULL)
    {
      if((obj_info.obj_mask & QAPI_NET_LWM2M_INSTANCE_ID_E))
      {
        instance = lwm2m_instance_find(object->instance_info, obj_info.obj_inst_id);
        if(instance == NULL)
        {
          IOT_DEBUG("Instance id not found.");
          result = QAPI_ERR_NO_ENTRY;
          goto end;
        }
        else
        {
          if((obj_info.obj_mask & QAPI_NET_LWM2M_RESOURCE_ID_E))
          {
            resource = lwm2m_resource_find(instance->resource_info, obj_info.res_id);
            if(resource == NULL)
            {
              IOT_DEBUG("Resource id not found.");
              result = QAPI_ERR_NO_ENTRY;
              goto end;
            }
            size = 1;
            data = lwm2m_data_new(size);
            if(data == NULL)
            {
              result = QAPI_ERR_NO_MEMORY;
              goto end;
            }
            (data)->id = resource->resource_ID;
          }
          result = read_data_ext(instance->instance_ID, &size, &data, object);
        }
      }
      else
      {
        for(instance = object->instance_info; instance != NULL; instance = instance->next)
        {
          size++;
        }
        data = lwm2m_data_new(size);
        if(data == NULL)
        {
          result = QAPI_ERR_NO_MEMORY;
          goto end;
        }
        instance = object->instance_info;
        i =0;
        result = QAPI_OK;
        while(instance != NULL && result == QAPI_OK)
        {
          result = read_data_ext(instance->instance_ID, (int *)&((data)[i].value.asChildren.count),
                                 &((data)[i].value.asChildren.array), object);
          (data)[i].id = instance->instance_ID;
          (data)[i].type = QAPI_NET_LWM2M_TYPE_OBJECT_INSTANCE;
          i++;
          instance = instance->next;
        }
      }
    }
    else
    {
      IOT_DEBUG("object id not found.");
      result = QAPI_ERR_NO_ENTRY;
      goto end;
    }
  }
  else
  {
    IOT_DEBUG("Not valid request.");
    result = QAPI_ERR_INVALID_PARAM;
    goto end;
  }

  if((obj_info.obj_mask & QAPI_NET_LWM2M_RESOURCE_ID_E))
  {
    cont_type = QAPI_NET_LWM2M_TEXT_PLAIN;
  }
  else
  {
    cont_type = QAPI_NET_LWM2M_M2M_TLV;
  }

  if(result == QAPI_OK)
  {
    /*qapi_Net_LWM2M_Encode_App_Payload is deprecated, use qapi_Net_LWM2M_Encode_Data instead.*/
    result = qapi_Net_LWM2M_Encode_Data(ext_object_handler_app, (qapi_Net_LWM2M_Obj_Info_t *)&uri,
                                               data,
                                               (size_t)size,
                                               NULL,
                                               cont_type,
                                               buffer,
                                               length);
     if (result != QAPI_OK || length == 0)
     {
       if (format != QAPI_NET_LWM2M_TEXT_PLAIN || size != 1
           || (data)->type != QAPI_NET_LWM2M_TYPE_STRING_E || (data)->value.asBuffer.length != 0)
       {
         result = QAPI_ERR_BAD_PAYLOAD;
         goto end;
       }
     }
  }

  if(data)
  {
    lwm2m_data_free(size, data);
  }

end:

  return result;

}

bool lwm2m_check_readable(uint16_t obj_id, uint8_t id)
{
  bool res = false;

  //if(lwm2m_object_find(object_generic, obj_id))
  {
    if(obj_id == LWM2M_GENERIC_SENSOR_OBJECT_ID)
    {
      switch(id)
      {
        case RES_M_SENSOR_VALUE:
        case RES_O_SENSOR_UNITS:
        case RES_O_MIN_MEASURED_VAL:
        case RES_O_MAX_MEASURED_VAL:
        case RES_M_MIN_RANGE_VAL:
        case RES_M_MAX_RANGE_VAL:
        case RES_M_SENSOR_STATE:
        case RES_O_MEAN_VAL:
             res = true;
        break;
        default:
             res = false;
        break;
      }
    }
    else if(obj_id == LWM2M_PRESENCE_SENSOR_OBJECT_ID)
    {
      switch(id)
      {
        case RES_M_DIG_INPUT_STATE:
        case RES_O_DIG_INPUT_COUNTER:
        case RES_O_SENSOR_TYPE:
        case RES_O_BUSY_TO_CLEAR_DELAY:
        case RES_O_CLEAR_TO_BUSY_DELAY:
             res = true;
        break;
        default:
             res = false;
        break;
      }
    }
  }
  return res;
}

bool check_writable(uint16_t obj_id, uint8_t id)
{
  bool res = false;

  //if(lwm2m_object_find(object_generic, obj_id))
  {
    if(obj_id == LWM2M_GENERIC_SENSOR_OBJECT_ID)
    {
      switch(id)
      {
        case RES_M_MIN_RANGE_VAL:
        case RES_M_MAX_RANGE_VAL:
        case RES_M_SENSOR_STATE:
             res = true;
        break;
        default:
             res = false;
        break;
      }
    }
    else if(obj_id == LWM2M_PRESENCE_SENSOR_OBJECT_ID)
    {
      switch(id)
      {
        case RES_O_BUSY_TO_CLEAR_DELAY:
        case RES_O_CLEAR_TO_BUSY_DELAY:
             res = true;
        break;
        default:
             res = false;
        break;
      }
    }
  }

  return res;
}

bool check_executable(uint16_t obj_id, uint8_t id)
{
  bool res = false;

  //if(lwm2m_object_find(object_generic, obj_id))
  {
    if(obj_id == LWM2M_GENERIC_SENSOR_OBJECT_ID)
    {
      switch(id)
      {
        case RES_O_RESET_MEASURED_VAL:
             res = true;
        break;
        default:
             res = false;
        break;
      }
    }
    else if(obj_id == LWM2M_PRESENCE_SENSOR_OBJECT_ID)
    {
      switch(id)
      {
        case RES_O_RESET_DIG_INPUT_COUNTER:
             res = true;
        break;
        default:
             res = false;
        break;
      }
    }
  }

  return res;
}

static qapi_Status_t qapi_Net_LWM2M_Free_Resource_Info(qapi_Net_LWM2M_Resource_Info_t *resourceP)
{
  qapi_Net_LWM2M_Resource_Info_t *resource_linkP = NULL;
  if (resourceP == NULL)
  {
    return QAPI_ERROR;
  }
  while (resourceP != NULL)
  {
    resource_linkP =   resourceP->next;
    if((resourceP->type == QAPI_NET_LWM2M_TYPE_STRING_E) && resourceP->value.asBuffer.buffer)
    {
      tx_byte_release(resourceP->value.asBuffer.buffer);
    }
    tx_byte_release(resourceP);
    resourceP = resource_linkP;
  }
   return QAPI_OK;
}

static qapi_Status_t qapi_Net_LWM2M_Free_Single_Instance_Info(qapi_Net_LWM2M_Instance_Info_v2_t *instanceP)
{
  qapi_Net_LWM2M_Instance_Info_v2_t *instance_linkP = NULL;
  if (instanceP == NULL)
  {
    return QAPI_ERROR;
  }
  if(instanceP != NULL)
  {
    instance_linkP = instanceP->next;
    qapi_Net_LWM2M_Free_Resource_Info(instanceP->resource_info);
    tx_byte_release(instanceP);
    instanceP = instance_linkP;
  }
  return QAPI_OK;
}

/*
 * @fn 		m2m_ext_obj_process_commands
 * @brief	This function dequeues all outstanding commands from and dispatches the
 *          processor functions.
 */
void m2m_ext_obj_process_commands(void)
{
	ext_obj_cmd_t                *ext_obj_cmd_ptr = NULL;
	qapi_Net_LWM2M_Ext_t         *obj_buffer      = NULL;
	qapi_Status_t                result           = QAPI_ERROR;
	bool flag = false;

	while ((ext_obj_cmd_ptr = ext_obj_dequeue_cmd()) != NULL ) 
	{
		IOT_DEBUG("Command ptr: 0x%x, cmd_id: %d", 
		ext_obj_cmd_ptr, 
		ext_obj_cmd_ptr->cmd_hdr.cmd_id);

		/* Bound check to ensure command id is bounded */
		if (!EXT_OBJ_CMD_ID_RANGE_CHECK(ext_obj_cmd_ptr->cmd_hdr.cmd_id))
		{
			IOT_DEBUG("Command ID Range check failed"); 
			continue;
		}

		switch(ext_obj_cmd_ptr->cmd_hdr.cmd_id)
		{
			case EXT_OBJ_CALL_BACK_EVT:
			{
				obj_buffer = (qapi_Net_LWM2M_Ext_t *)ext_obj_cmd_ptr->cmd_data.data;
				if (obj_buffer != NULL && obj_buffer->app_data.dl_op.lwm2m_srv_data != NULL)
				{
					switch(obj_buffer->app_data.dl_op.lwm2m_srv_data->msg_type)
					{
						case QAPI_NET_LWM2M_READ_REQ_E:
						{
							result = m2m_ext_read_obj(obj_buffer->app_data.dl_op.handler, *obj_buffer->app_data.dl_op.lwm2m_srv_data);
						}
						break;
						
						case QAPI_NET_LWM2M_WRITE_REPLACE_REQ_E:
						{
							result = m2m_ext_write_obj(obj_buffer->app_data.dl_op.handler, *obj_buffer->app_data.dl_op.lwm2m_srv_data);
						}
						break;
						
						case QAPI_NET_LWM2M_WRITE_PARTIAL_UPDATE_REQ_E:
						{
							result = m2m_ext_write_obj(obj_buffer->app_data.dl_op.handler, *obj_buffer->app_data.dl_op.lwm2m_srv_data);
						}
						break;
						
						case QAPI_NET_LWM2M_WRITE_ATTR_REQ_E:
						{
							result = ext_object_write_attr(obj_buffer->app_data.dl_op.handler, *obj_buffer->app_data.dl_op.lwm2m_srv_data);
						}
						break;
						
						case QAPI_NET_LWM2M_DISCOVER_REQ_E:
						{
							result = disc_ext_obj(obj_buffer->app_data.dl_op.handler, 
							*obj_buffer->app_data.dl_op.lwm2m_srv_data);
						}
						break;
						
						case QAPI_NET_LWM2M_EXECUTE_REQ_E:
						{
							result = exec_ext_obj(obj_buffer->app_data.dl_op.handler, 
							*obj_buffer->app_data.dl_op.lwm2m_srv_data);
						}
						break;
						
						case QAPI_NET_LWM2M_CREATE_REQ_E:
						{
							result = m2m_create_object_instance(obj_buffer->app_data.dl_op.handler, 
							*obj_buffer->app_data.dl_op.lwm2m_srv_data);
						}
						break;
						
						case QAPI_NET_LWM2M_DELETE_REQ_E:
						{
							result = delete_object_instance(obj_buffer->app_data.dl_op.handler, 
							*obj_buffer->app_data.dl_op.lwm2m_srv_data);
						}
						break;
						
						case QAPI_NET_LWM2M_OBSERVE_REQ_E:
						{
							result = observe_ext_obj(obj_buffer->app_data.dl_op.handler, 
							*obj_buffer->app_data.dl_op.lwm2m_srv_data);
						}
						break;
						
						case QAPI_NET_LWM2M_CANCEL_OBSERVE_REQ_E:
						{
							IOT_DEBUG("lwm2m cancel observe\n");
							result = observe_cancel_ext_obj(obj_buffer->app_data.dl_op.handler, 
							*obj_buffer->app_data.dl_op.lwm2m_srv_data);
						}
						break;
						
						case QAPI_NET_LWM2M_ACK_MSG_E:
							break;
							
						case QAPI_NET_LWM2M_INTERNAL_CLIENT_IND_E:
						{
							if (obj_buffer->app_data.dl_op.lwm2m_srv_data->event == QAPI_NET_LWM2M_DEVICE_REBOOT_E ||
								obj_buffer->app_data.dl_op.lwm2m_srv_data->event == QAPI_NET_LWM2M_DEVICE_FACTORY_RESET_E)
							{
								flag = true;
							}
							
							result = ext_obj_handle_event(obj_buffer->app_data.dl_op.handler, 
							*obj_buffer->app_data.dl_op.lwm2m_srv_data);
						}
						break;
						
						case QAPI_NET_LWM2M_DELETE_ALL_REQ_E:
						{
							result = ext_obj_delete_all(obj_buffer->app_data.dl_op.handler, 
							*obj_buffer->app_data.dl_op.lwm2m_srv_data);
						}
						break;
					}
					
					if (obj_buffer)
					{
						if(obj_buffer->app_data.dl_op.lwm2m_srv_data)
						{
							if(obj_buffer->app_data.dl_op.lwm2m_srv_data->lwm2m_attr)
							{
								tx_byte_release(obj_buffer->app_data.dl_op.lwm2m_srv_data->lwm2m_attr);
								obj_buffer->app_data.dl_op.lwm2m_srv_data->lwm2m_attr = NULL;
							}
							
							tx_byte_release(obj_buffer->app_data.dl_op.lwm2m_srv_data);
							obj_buffer->app_data.dl_op.lwm2m_srv_data = NULL;
						}
						
						tx_byte_release(obj_buffer);
						obj_buffer = NULL;
					}
					
					IOT_DEBUG("Operation result is %d ", result);
				}
			}
			break;

			case EXT_OBJ_APP_ORIGINATED_EVT:
			{
				obj_buffer = (qapi_Net_LWM2M_Ext_t *)ext_obj_cmd_ptr->cmd_data.data;
				if (obj_buffer != NULL)
				{
					switch(obj_buffer->app_data.ul_op.msg_type)
					{
						case QAPI_NET_LWM2M_EXT_REGISTER_APP_E:
						{
							result = m2m_ext_object_register();
						}
						break;

						case QAPI_NET_LWM2M_EXT_CREATE_OBJ_E:
						{
							result = m2m_create_object();
						}
						break;

						case QAPI_NET_LWM2M_EXT_CREATE_OBJ_INSTANCE_E:
						{
							result = m2m_create_object_instance_app(obj_buffer);
						}
						break;

						case QAPI_NET_LWM2M_EXT_DELETE_OBJ_INSTANCE_E:
						{
							result = m2m_delete_object_instance_app(obj_buffer);
						}
						break;

						case QAPI_NET_LWM2M_EXT_DEREGISTER_APP_E:
						{
							result = m2m_ext_object_deregister();
							app_reg_status = false;
							flag = true;
						}
						break;

						case QAPI_NET_LWM2M_EXT_NOTIFY_E:
						{
							time_t timeout = 5;
							qapi_time_get_t current_time;
							qapi_time_get(QAPI_TIME_SECS, &current_time);
							IOT_DEBUG("current_time.time_secs:%lld",current_time.time_secs);

							m2m_ext_object_notify(0,current_time.time_secs, &timeout);
						}
						break;

						case QAPI_NET_LWM2M_EXT_CONFIG_CLIENT_E:
						{
							m2m_ext_object_configclient();
						}			
						break;

						default:
						{
							IOT_DEBUG("Invalid request from application.");
						}
						break;
						
						}
					
						if(obj_buffer)
						{
							tx_byte_release(obj_buffer);
							obj_buffer = NULL;
						}
					}
			}
			break;
			default:
			break;
		}
		if (result == QAPI_OK && (ext_obj_cmd_ptr->cmd_hdr.cmd_id == EXT_OBJ_APP_ORIGINATED_EVT) && flag == true)
		{
			tx_byte_release(ext_obj_cmd_ptr);
			//ext_obj_task_exit();
		}
		else if(result == QAPI_OK && (ext_obj_cmd_ptr->cmd_hdr.cmd_id == EXT_OBJ_CALL_BACK_EVT) && flag == true)
		{
			tx_byte_release(ext_obj_cmd_ptr);
			//ext_obj_task_exit();
		}
		else
		{
			tx_byte_release(ext_obj_cmd_ptr);
		}
	} /* Loop to dequeue all outstanding commands*/
	return;
} /* ext_obj_process_commands */ 

/*--------------------------------------------------------------------*
			lwm2m downlink message process
 *--------------------------------------------------------------------*/
void payload_set_data_generic(qapi_Net_LWM2M_Flat_Data_t* data,
					  qapi_Net_LWM2M_Instance_Info_v2_t* instance)
{
  qapi_Net_LWM2M_Resource_Info_t *resource = NULL;
  if( data == NULL || instance == NULL) 
  {
	IOT_DEBUG("LWM2M_LOG: fun [%s] file [%s]  Passed  Argument is NULL\n",__func__,__FILE__);
	return;
  } 

  switch (data->id)
  {
	case RES_M_SENSOR_VALUE:
	  resource = lwm2m_resource_find(instance->resource_info, data->id);
	  if(resource != NULL)
	  lwm2m_data_encode_int(resource->value.asInteger, data);
	  break;

	case RES_O_SENSOR_UNITS:
	  resource = lwm2m_resource_find(instance->resource_info, data->id);
	  if(resource != NULL)
	  lwm2m_data_encode_string((const char *)resource->value.asBuffer.buffer, data);
	  break;

	case RES_O_MIN_MEASURED_VAL:
	  resource = lwm2m_resource_find(instance->resource_info, data->id);
	  if(resource != NULL)
	  lwm2m_data_encode_float(resource->value.asFloat, data);
	  break;

	case RES_O_MAX_MEASURED_VAL:
	  resource = lwm2m_resource_find(instance->resource_info, data->id);
	  if(resource != NULL)
	  lwm2m_data_encode_float(resource->value.asFloat, data);
	  break;

	case RES_M_MIN_RANGE_VAL:
	  resource = lwm2m_resource_find(instance->resource_info, data->id);
	  if(resource != NULL)
	  lwm2m_data_encode_float(resource->value.asFloat, data);
	  break;

	case RES_M_MAX_RANGE_VAL:
	  resource = lwm2m_resource_find(instance->resource_info, data->id);
	  if(resource != NULL)
	  lwm2m_data_encode_float(resource->value.asFloat, data);
	  break;

	case RES_M_SENSOR_STATE:
	  resource = lwm2m_resource_find(instance->resource_info, data->id);
	  if(resource != NULL)
	  lwm2m_data_encode_bool(resource->value.asBoolean, data);
	  break;

	case RES_O_MEAN_VAL:
	  resource = lwm2m_resource_find(instance->resource_info, data->id);
	  if(resource != NULL)
	  lwm2m_data_encode_int(resource->value.asInteger, data);
	  break;
  }
}

void payload_set_data_presence(qapi_Net_LWM2M_Flat_Data_t* data,
							   qapi_Net_LWM2M_Instance_Info_v2_t* instance)
{
  qapi_Net_LWM2M_Resource_Info_t *resource = NULL;
  if( data == NULL || instance == NULL) 
  {
	IOT_DEBUG("LWM2M_LOG: fun [%s] file [%s]  Passed  Argument is NULL\n",__func__,__FILE__);
	return;
  } 

  switch (data->id)
  {
	case RES_M_DIG_INPUT_STATE:
	  resource = lwm2m_resource_find(instance->resource_info, data->id);
	  if(resource != NULL)
	  lwm2m_data_encode_bool(resource->value.asBoolean, data);
	  break;

	case RES_O_DIG_INPUT_COUNTER:
	  resource = lwm2m_resource_find(instance->resource_info, data->id);
	  if(resource != NULL)
	  lwm2m_data_encode_int(resource->value.asInteger, data);
	  break;

	case RES_O_SENSOR_TYPE:
	  resource = lwm2m_resource_find(instance->resource_info, data->id);
	  if(resource != NULL)
	  lwm2m_data_encode_string((const char *)resource->value.asBuffer.buffer, data);
	  break;

	case RES_O_BUSY_TO_CLEAR_DELAY:
	  resource = lwm2m_resource_find(instance->resource_info, data->id);
	  if(resource != NULL)
	  lwm2m_data_encode_int(resource->value.asInteger, data);
	  break;

	case RES_O_CLEAR_TO_BUSY_DELAY:
	  resource = lwm2m_resource_find(instance->resource_info, data->id);
	  if(resource != NULL)
	  lwm2m_data_encode_int(resource->value.asInteger, data);
	  break;
  }

}
	
qapi_Status_t write_data_ext
(
	uint16_t instanceid, 
	int size, 
	qapi_Net_LWM2M_Flat_Data_t *data,
	qapi_Net_LWM2M_Data_v2_t *object
)
{
  qapi_Net_LWM2M_Instance_Info_v2_t *target = NULL;
  int i = 0;
  double float_value = 0;
  bool bool_val = false;
  int64_t int_val = 0;
  qapi_Net_LWM2M_Resource_Info_t *resource = NULL;
  qapi_Status_t result = QAPI_ERROR;

  target = lwm2m_instance_find(object->instance_info, instanceid);
  if(target == NULL)
  {
	return QAPI_ERR_NO_ENTRY;
  }

  if(object->object_ID == LWM2M_GENERIC_SENSOR_OBJECT_ID)
  {
	i = 0;
	do
	{
	  switch(data[i].id)
	  {
		case RES_M_SENSOR_VALUE:
		case RES_O_SENSOR_UNITS:
		case RES_O_MIN_MEASURED_VAL:
		case RES_O_MAX_MEASURED_VAL:
		case RES_O_RESET_MEASURED_VAL:
		case RES_O_MEAN_VAL:
			result = QAPI_ERR_NOT_SUPPORTED;
			break;
		case RES_M_MIN_RANGE_VAL:
		case RES_M_MAX_RANGE_VAL:
		case RES_M_SENSOR_STATE:
			result = QAPI_OK;
			break;
		default:
			result = QAPI_ERROR;
			break;
	  }
	  i++;
	}while(i < size && result == QAPI_OK);
  }

  else if(object->object_ID == LWM2M_PRESENCE_SENSOR_OBJECT_ID)
  {
	i = 0;
	do
	{
	  switch(data[i].id)
	  {
		case RES_M_DIG_INPUT_STATE:
		case RES_O_DIG_INPUT_COUNTER:
		case RES_O_RESET_DIG_INPUT_COUNTER:
		case RES_O_SENSOR_TYPE:
			result = QAPI_ERR_NOT_SUPPORTED;
			break;
		case RES_O_BUSY_TO_CLEAR_DELAY:
		case RES_O_CLEAR_TO_BUSY_DELAY:
			result = QAPI_OK;
			break;
		default:
			result = QAPI_ERROR;
			break;
	  }
	  i++;
	}while(i < size && result == QAPI_OK);
  }

  if(result == QAPI_OK)
  {
	if(object->object_ID == LWM2M_GENERIC_SENSOR_OBJECT_ID)
	{
	  i = 0;
	  do
	  {
		switch(data[i].id)
		{
		  case RES_M_MIN_RANGE_VAL:
		  {
			if(lwm2m_data_decode_float(data+i,&(float_value)) == 1)
			{
			  resource = lwm2m_resource_find(target->resource_info, data[i].id);
			  if(resource != NULL)
			  {
				resource->value.asFloat = float_value;
				result = QAPI_OK;
			  }
			  else
			  {
				result = QAPI_ERR_NO_ENTRY;
			  }
			}
			else
			{
			  result = QAPI_ERR_INVALID_PARAM;
			}
		  }
		  break;
		
		  case RES_M_MAX_RANGE_VAL:
		  {
			if(lwm2m_data_decode_float(data+i,&(float_value)) == 1)
			{
			  resource = lwm2m_resource_find(target->resource_info, data[i].id);
			  if(resource != NULL)
			  {
				resource->value.asFloat = float_value;
				result = QAPI_OK;
			  }
			  else
			  {
				result = QAPI_ERR_NO_ENTRY;
			  }
			}
			else
			{
			  result = QAPI_ERR_INVALID_PARAM;
			}
		  }
		  break;
	
		  case RES_M_SENSOR_STATE:
		  {
			if(lwm2m_data_decode_bool(data+i,&(bool_val)) == 1)
			{
			  resource = lwm2m_resource_find(target->resource_info, data[i].id);
			  if(resource != NULL)
			  {
				resource->value.asBoolean = bool_val;
				result = QAPI_OK;
			  }
			  else
			  {
				result = QAPI_ERR_NO_ENTRY;
			  }
			}
			else
			{
			  result = QAPI_ERR_INVALID_PARAM;
			}
		  }
		  break;
	
		  default:
		  if(size > 1)
		  result = QAPI_OK;
		  else
		  result = QAPI_ERR_NO_ENTRY;
		}
		i++;
	  }while(i < size && result == QAPI_OK);
	}
	
	else
	{
	  i = 0;
	  do
	  {
		switch(data[i].id)
		{
		  case RES_O_BUSY_TO_CLEAR_DELAY:
		  {
			if(lwm2m_data_decode_int(data+i,&(int_val)) == 1)
			{
			  resource = lwm2m_resource_find(target->resource_info, data[i].id);
			  if(resource != NULL)
			  {
				resource->value.asInteger = int_val;
				result = QAPI_OK;
			  }
			  else
			  {
				result = QAPI_ERR_NO_ENTRY;
			  }
			}
			else
			{
			  result = QAPI_ERR_INVALID_PARAM;
			}
		  }
		  break;
		
		  case RES_O_CLEAR_TO_BUSY_DELAY:
		  {
			if(lwm2m_data_decode_int(data+i,&(int_val)) == 1)
			{
			  resource = lwm2m_resource_find(target->resource_info, data[i].id);
			  if(resource != NULL)
			  {
				resource->value.asInteger = int_val;
				result = QAPI_OK;
			  }
			  else
			  {
				result = QAPI_ERR_NO_ENTRY;
			  }
			}
			else
			{
			  result = QAPI_ERR_INVALID_PARAM;
			}
		  }
		  break;
	
		  default:
		  if(size > 1)
		  result = QAPI_OK;
		  else
		  result = QAPI_ERR_NO_ENTRY;
		}
		i++;
	  }while(i < size && result == QAPI_OK);
	}
  }
  return result;
}

qapi_Status_t read_data_ext
(	
	uint16_t instanceid, 
	int *size, 
	qapi_Net_LWM2M_Flat_Data_t **data,
    qapi_Net_LWM2M_Data_v2_t *object
)
{
  qapi_Net_LWM2M_Instance_Info_v2_t *target = NULL;
  int i = 0;
  uint16_t reslist_ob1[] = {
                             RES_M_SENSOR_VALUE,
                             RES_O_SENSOR_UNITS,
                             RES_O_MIN_MEASURED_VAL,
                             RES_O_MAX_MEASURED_VAL,
                             RES_M_MIN_RANGE_VAL,
                             RES_M_MAX_RANGE_VAL,
                             RES_M_SENSOR_STATE,
                             RES_O_MEAN_VAL
                           };

  uint16_t reslist_ob2[] = {
                             RES_M_DIG_INPUT_STATE,
                             RES_O_DIG_INPUT_COUNTER,
                             RES_O_SENSOR_TYPE,
                             RES_O_BUSY_TO_CLEAR_DELAY,
                             RES_O_CLEAR_TO_BUSY_DELAY,
                           };

  int nbres;
  int temp;

  target = lwm2m_instance_find(object->instance_info, instanceid);
  if(target == NULL)
  {
    return QAPI_ERR_NO_ENTRY;
  }

  if(*size == 0)
  {
    if(object->object_ID == LWM2M_GENERIC_SENSOR_OBJECT_ID)
    {
      nbres = sizeof(reslist_ob1)/sizeof(uint16_t);
    }
    else
    {
      nbres = sizeof(reslist_ob2)/sizeof(uint16_t);
    }
    temp = nbres;
     
    *data = lwm2m_data_new(nbres);
    if(*data == NULL)
    {
      return QAPI_ERR_NO_MEMORY;
    }
    *size = nbres;

    if(object->object_ID == LWM2M_GENERIC_SENSOR_OBJECT_ID)
    {
      for(i = 0; i<temp; i++)
      {
        (*data)[i].id = reslist_ob1[i];
      }
    }
    else
    {
      for(i = 0; i<temp; i++)
      {
        (*data)[i].id = reslist_ob2[i];
      }
    }
  }

  i = 0;
  do
  {
    if(object->object_ID == LWM2M_GENERIC_SENSOR_OBJECT_ID)
    {
    	payload_set_data_generic((*data)+i, target);
    }
    else
    {
    	payload_set_data_presence((*data)+i, target);
    }
	
    i++;
  }while(i < *size);

  return QAPI_OK;
}

qapi_Status_t exec_data_ext(uint16_t instanceid, qapi_Net_LWM2M_Data_v2_t *object)
{
  qapi_Net_LWM2M_Instance_Info_v2_t *target = NULL;
  qapi_Net_LWM2M_Resource_Info_t *resource1 = NULL;
  qapi_Net_LWM2M_Resource_Info_t *resource2 = NULL;
  qapi_Net_LWM2M_Resource_Info_t *resource3 = NULL;
  qapi_Net_LWM2M_Resource_Info_t *resource4 = NULL;

  target = lwm2m_instance_find(object->instance_info, instanceid);
  if(target == NULL)
  {
    return QAPI_ERR_NO_ENTRY;
  }

  if(object->object_ID == LWM2M_GENERIC_SENSOR_OBJECT_ID)
  {
    resource1 = lwm2m_resource_find(target->resource_info, RES_O_MIN_MEASURED_VAL);
    resource2 = lwm2m_resource_find(target->resource_info, RES_M_MIN_RANGE_VAL);
    resource3 = lwm2m_resource_find(target->resource_info, RES_O_MAX_MEASURED_VAL);
    resource4 = lwm2m_resource_find(target->resource_info, RES_M_MAX_RANGE_VAL);
    if(resource1 != NULL && resource2 != NULL)
    {
      resource1->value.asFloat = resource2->value.asFloat;
    }
    else
    {
      return QAPI_ERR_INVALID_PARAM;
    }
    if(resource3 != NULL && resource4 != NULL)
    {
      resource3->value.asFloat = resource4->value.asFloat;
    }
    else
    {
      return QAPI_ERR_INVALID_PARAM;
    }
  }
  else
  {
    resource1 = lwm2m_resource_find(target->resource_info, RES_O_DIG_INPUT_COUNTER);
    if(resource1 != NULL)
    {
      resource1->value.asInteger = 0;
    }
    else
    {
      return QAPI_ERR_INVALID_PARAM;
    }
  }

  return QAPI_OK;
}
	
/**
 * @fn	   m2m_ext_object_configclient()
 *
 * @brief  This fucntion is used to config lwm2m client for Extensible object application.
 *
 * @return QAPI_OK	   Success case
 *		   QAPI_ERROR  Failure case
 */
qapi_Status_t m2m_ext_object_configclient(void)
{
	qapi_Status_t result = QAPI_ERROR;

	if (ext_object_handler_app == NULL)
	{
		IOT_DEBUG("LWM2M_LOG: Extensible object application not registered.\n");
		return QAPI_ERROR;
	}

	result = qapi_Net_LWM2M_ConfigClient(ext_object_handler_app, ext_object_clientconfig_data);

	if (result == QAPI_OK)
	{
		IOT_DEBUG("LWM2M_LOG: Extensible object application config client successfully.\n");
	}
	else
	{
		IOT_DEBUG("LWM2M_LOG: Error in Extensible object application config client.\n");
	}

	return result;
}
	
void m2m_ext_object_notify
(
	int id,
	time_t current_time,
	time_t *timeout
)
{
	qapi_Net_LWM2M_Observed_t *observed = NULL;
	bool notify_store = false;
	qapi_Net_LWM2M_Content_Type_t cont_type;
	qapi_Status_t result = QAPI_OK;
	qapi_Net_LWM2M_Observed_t *observed_list = NULL;
	qapi_Net_LWM2M_App_Handler_t handler = NULL;
	time_t last_time = 0; 
	time_t time_out = 0;

	if(id == 0)
	{
		handler = ext_object_handler_app;
	}

	if(handler == NULL || timeout == NULL)
	{
		return;
	}

	if(handler == ext_object_handler_app && ext_observed_list == NULL)
	{
		stop_ext_obs_notify_timer(handler);
		return;
	}

	if(handler == ext_object_handler_app)
	{
		observed_list = ext_observed_list;
	}
	else if(handler == ext_object_handler_app)
	{
		observed_list = ext_multi_observed_list;
	}

	IOT_DEBUG("Enter m2m_ext_object_notify.");
	//Send Pending Notifications
	if(!lwm2m_client_in_sleep_state && 
	((handler == ext_object_handler_app) && (ext_pending_observed_list != NULL)))
	{
		qapi_Net_LWM2M_Pending_Observed_t *node = NULL;
		qapi_Net_LWM2M_App_Ex_Obj_Data_t *notify_data = NULL;

		if(handler == ext_object_handler_app)
		{
			node = ext_pending_observed_list;
		}

		while(node && (node->observe_info))
		{  
			tx_byte_allocate(byte_pool_m2m, (VOID **)&notify_data, 
			sizeof(qapi_Net_LWM2M_App_Ex_Obj_Data_t), TX_NO_WAIT);
			if(notify_data == NULL)
			{
				IOT_DEBUG("Memory allocation failure.\n");
				//send_response_message(op_data, QAPI_NET_LWM2M_500_INTERNAL_SERVER_E);
				return;
			}
			memset(notify_data, 0x00, sizeof(qapi_Net_LWM2M_App_Ex_Obj_Data_t));

			//Sending the pedning notifies stored either when in sleep or after disbale timeout elapses  
			IOT_DEBUG("LWM2M_LOG: Sending pending notify \n");

			if(handler == ext_object_handler_app)
			{
				ext_pending_observed_list = node->next;
			}

			if(!node->observe_info || !node->message)
			{
				if(node->message) 
				{
					if(((qapi_Net_LWM2M_App_Ex_Obj_Data_t *)node->message)->payload)
					{
						tx_byte_release(((qapi_Net_LWM2M_App_Ex_Obj_Data_t *)node->message)->payload);
										((qapi_Net_LWM2M_App_Ex_Obj_Data_t *)node->message)->payload = NULL;
					}
					tx_byte_release(node->message);
					node->message = NULL;
				}
				tx_byte_release(node);
				node = NULL;
				if(handler == ext_object_handler_app)
				{
					node = ext_pending_observed_list;
				}

				continue;
			}

	  		node->observe_info->last_time = current_time;

			notify_data->payload_len = ((qapi_Net_LWM2M_App_Ex_Obj_Data_t *)node->message)->payload_len;
			tx_byte_allocate(byte_pool_m2m, (VOID **)&(notify_data->payload), 
						   notify_data->payload_len, TX_NO_WAIT);
			if(notify_data->payload == NULL)
			{
				IOT_DEBUG("Error in memory allocation");
				//send_response_message(op_data, QAPI_NET_LWM2M_500_INTERNAL_SERVER_E);
				result = QAPI_ERR_NO_MEMORY;
				goto end;
			}
			memcpy(notify_data->payload, ((qapi_Net_LWM2M_App_Ex_Obj_Data_t *)node->message)->payload, notify_data->payload_len);
			notify_data->msg_type    			= QAPI_NET_LWM2M_NOTIFY_MSG_E;
			notify_data->obj_info.obj_mask	= ((qapi_Net_LWM2M_App_Ex_Obj_Data_t *)node->message)->obj_info.obj_mask;
			notify_data->obj_info.obj_id		= ((qapi_Net_LWM2M_App_Ex_Obj_Data_t *)node->message)->obj_info.obj_id;
			notify_data->obj_info.obj_inst_id = ((qapi_Net_LWM2M_App_Ex_Obj_Data_t *)node->message)->obj_info.obj_inst_id;
			notify_data->obj_info.res_id		= ((qapi_Net_LWM2M_App_Ex_Obj_Data_t *)node->message)->obj_info.res_id;
			notify_data->obj_info.res_inst_id = ((qapi_Net_LWM2M_App_Ex_Obj_Data_t *)node->message)->obj_info.res_inst_id;
			notify_data->status_code 			= QAPI_NET_LWM2M_205_CONTENT_E;
			notify_data->conf_msg    			= 0;
			notify_data->msg_id_len  			= node->observe_info->msg_id_len;
			memcpy(notify_data->msg_id, node->observe_info->msg_id,  node->observe_info->msg_id_len);
			notify_data->content_type = ((qapi_Net_LWM2M_App_Ex_Obj_Data_t *)node->message)->content_type;

			result = qapi_Net_LWM2M_Send_Message(handler, notify_data);
			node->observe_info->not_id = notify_data->notification_id;
			node->observe_info->update = false;
			
end:
			if(notify_data)
			{
				if(notify_data->payload)
				{
					tx_byte_release(notify_data->payload);
					notify_data->payload = NULL;
				}
				tx_byte_release(notify_data);
				notify_data = NULL;
			}
			if(((qapi_Net_LWM2M_App_Ex_Obj_Data_t *)node->message)->payload)
			{
				tx_byte_release(((qapi_Net_LWM2M_App_Ex_Obj_Data_t *)node->message)->payload);
							  (((qapi_Net_LWM2M_App_Ex_Obj_Data_t *)node->message)->payload) = NULL;
			}
			if((qapi_Net_LWM2M_App_Ex_Obj_Data_t *)node->message)
			{
				tx_byte_release(node->message);
				node->message = NULL;
			}
			if(node)
			{
				tx_byte_release(node);
				node = NULL;
			}
			if(handler == ext_object_handler_app)
			{
				node = ext_pending_observed_list;
			}
		}
	}

	for(observed = observed_list; observed != NULL; observed = observed->next)
	{
		qapi_Net_LWM2M_Observe_info_t *observe_info = NULL;
		uint8_t * buffer = NULL;
		size_t length = 0;
		qapi_Net_LWM2M_Flat_Data_t * data = NULL;
		int size = 0;
		double float_value = 0;
		int64_t integer_value = 0;
		bool store_value = false;
		qapi_Net_LWM2M_App_Ex_Obj_Data_t *notify_data = NULL;
		int result = 0;
		time_t min_period = 0, max_period = 0;
		qapi_Net_LWM2M_Data_v2_t *object = NULL;
		qapi_Net_LWM2M_Observe_info_t *target = NULL;
		qapi_Net_LWM2M_Uri_t uri;

		memset(&uri, 0x00, sizeof(qapi_Net_LWM2M_Uri_t));

		if(observed->uri.flag & QAPI_NET_LWM2M_RESOURCE_ID_E)
		{
			if(handler == ext_object_handler_app)
			{
				object = lwm2m_object_find(object_generic, observed->uri.objectId);
			}

			if(object == NULL)
			{
				return;
			}
			// Read resource to value to check value change
			size = 1;
			data = lwm2m_data_new(size);
			if(data == NULL)
			{
				return;
			}
			(data)->id = observed->uri.resourceId;

	  		result = read_data_ext(observed->uri.instanceId, &size, &data, object);
			if(result != QAPI_OK)
			{
				if(data != NULL)
				{
					lwm2m_data_free(size, data);
				}
				continue;
			}
			if(data == NULL)
			continue;

			switch (data->type)
			{
				case QAPI_NET_LWM2M_TYPE_INTEGER_E:
					if (lwm2m_data_decode_int(data, &integer_value) != 1) continue;
					store_value = true;
					break;
				case QAPI_NET_LWM2M_TYPE_FLOAT_E:
					if (lwm2m_data_decode_float(data, &float_value) != 1) continue;
					store_value = true;
					break;
				default:
					break;
			}
		}

		for(observe_info = observed->observe_info; observe_info != NULL; observe_info = observe_info->next)
		{
			uint32_t ssid = 0;
			ssid = *((uint32_t *)(observe_info->msg_id + observe_info->msg_id_len -2));
			qapi_Net_LWM2M_Default_Attribute_Info(handler, ssid, &min_period, &max_period);

			if (observe_info->active == true)
			{
				bool notify = false; //Flag that decides if notify needs to be sent

				// check if the resource value has changed and set watcher->update flag to true to trigger notify
				if(store_value)
				{
					IOT_DEBUG("store value is true\n");
			  		switch (data->type)
		  			{
						case QAPI_NET_LWM2M_TYPE_INTEGER_E:
							if(integer_value != observe_info->lastvalue.asInteger)
							{
								IOT_DEBUG("Integer value changed\n");
								observe_info->update = true;
							}
							else
							{
								IOT_DEBUG("Integer value did not change\n");
							}
							break;

						case QAPI_NET_LWM2M_TYPE_FLOAT_E:
							if(float_value != observe_info->lastvalue.asFloat)
							{
								IOT_DEBUG("Float value changed\n");
								observe_info->update = true;
							}
							else
							{
								IOT_DEBUG("Float value did not change\n");
							}
							break;
						default:
							break;
					}
				} //End of value change check

				// Get Pmin and Pmax period and if pmin > pmax, notify on Pmax period
				memset(&uri, 0x00, sizeof(qapi_Net_LWM2M_Uri_t));
				memcpy(&uri, &observed->uri, sizeof(qapi_Net_LWM2M_Uri_t));

				// Get the Maximum period 
				if(observe_info->attributes == NULL || (observe_info->attributes != NULL 
				   && (observe_info->attributes->set_attr_mask & QAPI_NET_LWM2M_MAX_PERIOD_E) == 0))
				{
					target = ext_findInheritedWatcher(handler, &uri, observe_info->msg_id, observe_info->msg_id_len, QAPI_NET_LWM2M_MAX_PERIOD_E);
					if(target != NULL)
					{
						if(target->attributes != NULL && (target->attributes->set_attr_mask & QAPI_NET_LWM2M_MAX_PERIOD_E) != 0)
						{
							max_period = target->attributes->maxPeriod;
						}
					}
					else
					{
						if(observe_info->attributes != NULL && (observe_info->attributes->set_attr_mask & QAPI_NET_LWM2M_MAX_PERIOD_E) != 0)
						{
							max_period = observe_info->attributes->maxPeriod;
						}
					}
				}
				else
				{
					if(observe_info->attributes != NULL && (observe_info->attributes->set_attr_mask & QAPI_NET_LWM2M_MAX_PERIOD_E) != 0)
					{
						max_period = observe_info->attributes->maxPeriod;
					}
				}

				// Get the Minimum period 
				if(observe_info->attributes == NULL || (observe_info->attributes != NULL 
				   && (observe_info->attributes->set_attr_mask & QAPI_NET_LWM2M_MIN_PERIOD_E) == 0))
				{
					target = ext_findInheritedWatcher(handler, &uri, observe_info->msg_id, observe_info->msg_id_len, QAPI_NET_LWM2M_MIN_PERIOD_E);
					if(target != NULL)
					{
						if(target->attributes != NULL && (target->attributes->set_attr_mask & QAPI_NET_LWM2M_MIN_PERIOD_E) != 0)
						{
							min_period = target->attributes->minPeriod;
						}
					}
					else
					{
						if(observe_info->attributes != NULL && (observe_info->attributes->set_attr_mask & QAPI_NET_LWM2M_MIN_PERIOD_E) != 0)
						{
							min_period = observe_info->attributes->minPeriod;
						}
					}
				}
				else
				{
					if(observe_info->attributes != NULL && (observe_info->attributes->set_attr_mask & QAPI_NET_LWM2M_MIN_PERIOD_E) != 0)
					{
						min_period = observe_info->attributes->minPeriod;
					}
				}

				if(min_period <= max_period)
				{
					// If notify needs to be sent due to resource value change
					if (observe_info->update == true) 
					{
						// value changed, should we notify the server ?
						// If no parameters are set, wait for pMin time
						if (observe_info->attributes == NULL || observe_info->attributes->set_attr_mask == 0)
						{
							// no conditions
							notify = true;
							IOT_DEBUG("Notify with no conditions");
						}

						// If lt, gt and st parameters are set
						if (notify == false && observe_info->attributes != NULL
						&& (observe_info->attributes->set_attr_mask & LWM2M_ATTR_FLAG_NUMERIC) != 0
						&& data != NULL)
						{
							// check for less than parameter
							if ((observe_info->attributes->set_attr_mask & QAPI_NET_LWM2M_LESS_THAN_E) != 0)
							{
								IOT_DEBUG("Checking lower threshold");
								// Did we cross the lower treshold ?
								switch (data->type)
								{
									case QAPI_NET_LWM2M_TYPE_INTEGER_E:
										if ((integer_value < observe_info->attributes->lessThan
											&& observe_info->lastvalue.asInteger > observe_info->attributes->lessThan)
										  || (integer_value > observe_info->attributes->lessThan
											&& observe_info->lastvalue.asInteger < observe_info->attributes->lessThan))
										{
											IOT_DEBUG("Notify on lower threshold crossing");
											notify = true;
										}
										break;
									case QAPI_NET_LWM2M_TYPE_FLOAT_E:
										if ((float_value < observe_info->attributes->lessThan
											&& observe_info->lastvalue.asFloat > observe_info->attributes->lessThan)
										  || (float_value > observe_info->attributes->lessThan
											&& observe_info->lastvalue.asFloat < observe_info->attributes->lessThan))
										{
											IOT_DEBUG("Notify on lower threshold crossing");
											notify = true;
										}
										break;
									default:
										break;
								}
							}
							// check for greater than parameter
							if ((observe_info->attributes->set_attr_mask & QAPI_NET_LWM2M_GREATER_THAN_E) != 0)
							{
								IOT_DEBUG("Checking upper threshold");
								// Did we cross the upper treshold ?
								switch (data->type)
								{
									case QAPI_NET_LWM2M_TYPE_INTEGER_E:
										if ((integer_value < observe_info->attributes->greaterThan
											&& observe_info->lastvalue.asInteger > observe_info->attributes->greaterThan)
										  || (integer_value > observe_info->attributes->greaterThan
											&& observe_info->lastvalue.asInteger < observe_info->attributes->greaterThan))
										{
											IOT_DEBUG("Notify on upper threshold crossing");
											notify = true;
										}
										break;
									case QAPI_NET_LWM2M_TYPE_FLOAT_E:
										if ((float_value < observe_info->attributes->greaterThan
											&& observe_info->lastvalue.asFloat > observe_info->attributes->greaterThan)
										  || (float_value > observe_info->attributes->greaterThan
											&& observe_info->lastvalue.asFloat < observe_info->attributes->greaterThan))
										{
											IOT_DEBUG("Notify on upper threshold crossing");
											notify = true;
										}
										break;
									default:
										break;
								}
							}
							// check for step parameter
							if ((observe_info->attributes->set_attr_mask & QAPI_NET_LWM2M_STEP_E) != 0)
							{
								IOT_DEBUG("Checking step");

								switch (data->type)
								{
									case QAPI_NET_LWM2M_TYPE_INTEGER_E:
									{
										int64_t diff;

										diff = integer_value - observe_info->lastvalue.asInteger;
										if ((diff < 0 && (0 - diff) >= observe_info->attributes->step)
											|| (diff >= 0 && diff >= observe_info->attributes->step))
										{
											IOT_DEBUG("Notify on step condition");
											notify = true;
										}
									}
										break;
									case QAPI_NET_LWM2M_TYPE_FLOAT_E:
									{
										double diff;

										diff = float_value - observe_info->lastvalue.asFloat;
										if ((diff < 0 && (0 - diff) >= observe_info->attributes->step)
											|| (diff >= 0 && diff >= observe_info->attributes->step))
										{
											IOT_DEBUG("Notify on step condition");
											notify = true;
										}
									}
										break;
									default:
										break;
								}
							}
						}

						// If pmin or pmax parameters are set and vaue attribute is not set
						if (notify == false && observe_info->attributes != NULL
						  && ((observe_info->attributes->set_attr_mask & QAPI_NET_LWM2M_MIN_PERIOD_E)
						  || (observe_info->attributes->set_attr_mask & QAPI_NET_LWM2M_MAX_PERIOD_E)))
						{
							if((observe_info->attributes->set_attr_mask & LWM2M_ATTR_FLAG_NUMERIC)== 0)
							{
								notify = true;
							}
						}

						// Get the Minimum period 
						if(observe_info->attributes == NULL || (observe_info->attributes != NULL 
						&& (observe_info->attributes->set_attr_mask & QAPI_NET_LWM2M_MIN_PERIOD_E) == 0))
						{
							target = ext_findInheritedWatcher(handler, &uri, observe_info->msg_id, observe_info->msg_id_len,
															QAPI_NET_LWM2M_MIN_PERIOD_E);
							if(target != NULL)
							{
								if(target->attributes != NULL && (target->attributes->set_attr_mask & QAPI_NET_LWM2M_MIN_PERIOD_E) != 0)
								{
									min_period = target->attributes->minPeriod;
								}
							}
							else
							{
								if(observe_info->attributes != NULL && (observe_info->attributes->set_attr_mask & QAPI_NET_LWM2M_MIN_PERIOD_E) != 0)
								{
									min_period = observe_info->attributes->minPeriod;
								}
							}
						}
						else
						{
							if(observe_info->attributes != NULL && (observe_info->attributes->set_attr_mask & QAPI_NET_LWM2M_MIN_PERIOD_E) != 0)
							{
								min_period = observe_info->attributes->minPeriod;
							}
						}
						if(notify == true)
						{
							if (observe_info->last_time + min_period > current_time)
							{
								// Update the timeout to be aligned to next Pmin time 
								//TIMEOUT_VAL_WITH_PERIOD(observe_info, *timeout, current_time, min_period);
								notify = false;
							}
							else
							{
								IOT_DEBUG("Notify on minimal period");
								notify = true;
							}
						}
					} // watcher->update = true
				}
				// Is the Maximum Period reached ?
				if(notify == false)
				{
					if(observe_info->last_time + max_period <= current_time)
					{
						IOT_DEBUG("Notify on maximal period");
						notify = true;
					}
					else
					{
						//TIMEOUT_VAL_WITH_PERIOD(observe_info, *timeout, current_time, max_period);
					} 
				}
#if 0
				if((observed->uri.flag & QAPI_NET_LWM2M_RESOURCE_ID_E))
				{
					cont_type = QAPI_NET_LWM2M_TEXT_PLAIN;
				}
				else
				{
					cont_type = QAPI_NET_LWM2M_M2M_TLV;
				}
#endif
				cont_type = observed->accept_type;
		
				if (notify == true)
				{
					tx_byte_allocate(byte_pool_m2m, (VOID **)&notify_data, 
					sizeof(qapi_Net_LWM2M_App_Ex_Obj_Data_t), TX_NO_WAIT);
					if(notify_data == NULL)
					{
						IOT_DEBUG("Memory allocation failure.\n");
						//send_response_message(op_data, QAPI_NET_LWM2M_500_INTERNAL_SERVER_E);
						return;
					}
					memset(notify_data, 0x00, sizeof(qapi_Net_LWM2M_App_Ex_Obj_Data_t));
					
					if (buffer == NULL)
					{
						if (data != NULL)
						{
							/*qapi_Net_LWM2M_Encode_App_Payload is deprecated, use qapi_Net_LWM2M_Encode_Data instead.*/
							result = qapi_Net_LWM2M_Encode_Data(ext_object_handler_app,(qapi_Net_LWM2M_Obj_Info_t *)&observed->uri,
																	 data,
																	 size,
																	 NULL,
																	 cont_type,
																	 &buffer,
																	 &length);

							if(length == 0)
							{
								break;
							}
						}
						else
						{
							if (ext_object_read(handler, observed->uri, cont_type, &buffer, &length) != QAPI_OK)
							{
								buffer = NULL;
								break;
							}
						}

						notify_data->payload_len = length;
						tx_byte_allocate(byte_pool_m2m, (VOID **)&(notify_data->payload), 
									 length, TX_NO_WAIT);
						if(notify_data->payload == NULL)
						{
							IOT_DEBUG("Error in memory allocation");
							result = QAPI_ERR_NO_MEMORY;
							tx_byte_release(notify_data);
							notify_data = NULL;
							return;
						}
						memcpy(notify_data->payload, buffer, length);
						notify_data->msg_type	 = QAPI_NET_LWM2M_NOTIFY_MSG_E;
						notify_data->obj_info.obj_mask	  = (qapi_Net_LWM2M_ID_t)observed->uri.flag;
						notify_data->obj_info.obj_id	  = observed->uri.objectId;
						notify_data->obj_info.obj_inst_id = observed->uri.instanceId;
						notify_data->obj_info.res_id	  = observed->uri.resourceId;
						notify_data->obj_info.res_inst_id = observed->uri.resourceInstId;
						notify_data->status_code = QAPI_NET_LWM2M_205_CONTENT_E;
						notify_data->conf_msg	 = 0;
						notify_data->msg_id_len  = observed->observe_info->msg_id_len;
						memcpy(notify_data->msg_id, observed->observe_info->msg_id,  observed->observe_info->msg_id_len);
						notify_data->content_type = cont_type;

		  			}

					// check if the device is in sleep state
					if(lwm2m_client_in_sleep_state)
					{
						//Add into notify pending list
						IOT_DEBUG("Adding notifications into pending list for sleep state\n");
						ext_observer_add_notify_to_pending_list(handler, observe_info, observed->uri, cont_type, buffer, length);
						qapi_Net_LWM2M_Wakeup(handler ,observe_info->msg_id,observe_info->msg_id_len);

						observe_info->last_time = current_time; //To avoid observe_step from being called immediately

						if(notify_data)
						{
							if(notify_data->payload)
							{
								tx_byte_release(notify_data->payload);
								notify_data->payload = NULL;
							}
							tx_byte_release(notify_data);
							notify_data = NULL;
						}
						notify_store = true;
					} 
					else
					{
						// Send notifications

						observe_info->last_time = current_time;

						result = qapi_Net_LWM2M_Send_Message(handler, notify_data);
						observed->observe_info->not_id = notify_data->notification_id;

						observe_info->update = false;

						if ((notify == true) && (store_value == true))
						{
							switch (data->type)
							{
								case QAPI_NET_LWM2M_TYPE_INTEGER_E:
									observe_info->lastvalue.asInteger = integer_value;
									break;
								case QAPI_NET_LWM2M_TYPE_FLOAT_E:
									observe_info->lastvalue.asFloat = float_value;
									break;
								default:
									break;
							}
						}
					}
					if(notify_data)
					{
						if(notify_data->payload)
						{
							tx_byte_release(notify_data->payload);
							notify_data->payload = NULL;
						}
						tx_byte_release(notify_data);
						notify_data = NULL;
					}
					if(buffer != NULL && !lwm2m_client_in_sleep_state && !notify_store) 
					{
						//tx_byte_release(buffer);
						buffer = NULL;
					}
				} // notify == true

			}
		}
		if(data != NULL)
		{
			lwm2m_data_free(size, data);
		}
	}
  	IOT_DEBUG("result is %d",result);
}

qapi_Status_t m2m_ext_write_obj
(
	qapi_Net_LWM2M_App_Handler_t handler, 
	qapi_Net_LWM2M_Server_Data_t op_data
)
{
  qapi_Status_t result = QAPI_ERROR;
  qapi_Net_LWM2M_Obj_Info_t  obj_info;
  qapi_Net_LWM2M_Data_v2_t *object = NULL;
  qapi_Net_LWM2M_Instance_Info_v2_t *instance = NULL;
  qapi_Net_LWM2M_App_Ex_Obj_Data_t *write_data = NULL;
  qapi_Net_LWM2M_Flat_Data_t *data = NULL;
  size_t data_len;

#if 0
  if(object_generic == NULL)
  {
	IOT_DEBUG("Object is not valid.");
	send_response_message(op_data, QAPI_NET_LWM2M_500_INTERNAL_SERVER_E);
	return QAPI_ERROR;
  }
#endif

  if(handler == NULL)
  {
	IOT_DEBUG("Application handle is not valid for current application");
	//send_response_message(op_data, QAPI_NET_LWM2M_400_BAD_REQUEST_E);
	return QAPI_ERR_INVALID_PARAM;
  }
  
  obj_info = op_data.obj_info;

  if(obj_info.obj_mask & QAPI_NET_LWM2M_RESOURCE_ID_E)
  {
	if(handler == ext_object_handler_app)
	{
	  if(lwm2m_object_find(object_generic, obj_info.obj_id))
	  {
		if(check_writable(obj_info.obj_id, obj_info.res_id) == false)
		{
		  IOT_DEBUG("Write on resource %d not allowed.", obj_info.res_id);
		  send_response_message(handler, op_data, QAPI_NET_LWM2M_405_METHOD_NOT_ALLOWED_E);
		  return QAPI_ERROR;
		}
	  }
	}
   }

  tx_byte_allocate(byte_pool_m2m, (VOID **)&write_data, sizeof(qapi_Net_LWM2M_App_Ex_Obj_Data_t), TX_NO_WAIT);
  if(write_data == NULL)
  {
	IOT_DEBUG("Memory allocation failure.\n");
	send_response_message(handler, op_data, QAPI_NET_LWM2M_500_INTERNAL_SERVER_E);
	return QAPI_ERR_NO_MEMORY;
  }
  memset(write_data, 0x00, sizeof(qapi_Net_LWM2M_App_Ex_Obj_Data_t));

  if((obj_info.obj_mask & QAPI_NET_LWM2M_OBJECT_ID_E ))
  {
	if(handler == ext_object_handler_app)
	{
	  object = lwm2m_object_find(object_generic, obj_info.obj_id);
	}

	if(object != NULL)
	{
	  if((obj_info.obj_mask & QAPI_NET_LWM2M_INSTANCE_ID_E))
	  {
		instance = lwm2m_instance_find(object->instance_info, obj_info.obj_inst_id);
		if(instance != NULL)
		{
		  /*qapi_Net_LWM2M_Decode_App_Payload is deprecated, use qapi_Net_LWM2M_Decode_Data instead.*/
		  result = qapi_Net_LWM2M_Decode_Data(ext_object_handler_app, &op_data.obj_info,
													 op_data.payload,
													 op_data.payload_len,
													 op_data.content_type,
													 &data,
													 &data_len);
		  if(result != QAPI_OK || data_len == 0)
		  {
			IOT_DEBUG("Error in payload decoding.");
			result = QAPI_ERR_BAD_PAYLOAD;
			send_response_message(handler, op_data, QAPI_NET_LWM2M_406_NOT_ACCEPTABLE_E);
			goto end;
		  }
		}
		else
		{
		  IOT_DEBUG("Instance id not found.");
		  result = QAPI_ERR_NO_ENTRY;
		  send_response_message(handler, op_data, QAPI_NET_LWM2M_404_NOT_FOUND_E);
		  goto end;
		}
	  }
	  else
	  {
		IOT_DEBUG("Object level write not allowed.");
		result = QAPI_ERR_NOT_SUPPORTED;
		send_response_message(handler, op_data, QAPI_NET_LWM2M_405_METHOD_NOT_ALLOWED_E);
		goto end;
	  }
	}
	else
	{
	  IOT_DEBUG("Object id not found.");
	  result = QAPI_ERR_NO_ENTRY;
	  send_response_message(handler, op_data, QAPI_NET_LWM2M_404_NOT_FOUND_E);
	  goto end;
	}
  }
  else
  {
	IOT_DEBUG("Not valid request.");
	result = QAPI_ERR_INVALID_PARAM;
	send_response_message(handler, op_data, QAPI_NET_LWM2M_400_BAD_REQUEST_E);
	goto end;
  }

  result = write_data_ext(instance->instance_ID, data_len, data, object);

  if(data)
  {
	//lwm2m_data_free(data_len, data);
  }

  if(result == QAPI_OK)
  {
	write_data->msg_type = QAPI_NET_LWM2M_RESPONSE_MSG_E;
	write_data->obj_info = op_data.obj_info;
	write_data->status_code = QAPI_NET_LWM2M_204_CHANGED_E;
	write_data->conf_msg = 0;
	write_data->msg_id_len = op_data.msg_id_len;
	memcpy(write_data->msg_id, op_data.msg_id, op_data.msg_id_len);

	result = qapi_Net_LWM2M_Send_Message(handler, write_data);
  }
  else if(result == QAPI_ERR_NO_ENTRY)
  {
	IOT_DEBUG("Not valid request");
	result = QAPI_ERR_NO_ENTRY;
	send_response_message(handler, op_data, QAPI_NET_LWM2M_404_NOT_FOUND_E);
	goto end;
  }
  else if(result == QAPI_ERR_INVALID_PARAM)
  {
	IOT_DEBUG("Error in proccesing request");
	result = QAPI_ERR_INVALID_PARAM;
	send_response_message(handler, op_data, QAPI_NET_LWM2M_500_INTERNAL_SERVER_E);
	goto end;
  }
  else if(result == QAPI_ERR_NOT_SUPPORTED)
  {
	IOT_DEBUG("Write not allowed on readable resources");
	result = QAPI_ERR_NOT_SUPPORTED;
	send_response_message(handler, op_data, QAPI_NET_LWM2M_405_METHOD_NOT_ALLOWED_E);
	goto end;
  }
  else if(result == QAPI_ERROR)
  {
	IOT_DEBUG("Resource not present");
	result = QAPI_ERROR;
	send_response_message(handler, op_data, QAPI_NET_LWM2M_400_BAD_REQUEST_E);
	goto end;
  }

end:
  if(write_data)
  {
#if 0
	if(write_data->payload)
	{
	  tx_byte_release(write_data->payload);
	  //free(write_data->payload);
	  write_data->payload = NULL;
	}
#endif

	tx_byte_release(write_data);
	//free(write_data);
	write_data = NULL;
  }

  return result;
}

qapi_Status_t m2m_ext_read_obj
(
	qapi_Net_LWM2M_App_Handler_t handler, 
	qapi_Net_LWM2M_Server_Data_t op_data
)
{
  qapi_Status_t result = QAPI_ERROR;
  qapi_Net_LWM2M_Obj_Info_t  obj_info;
  qapi_Net_LWM2M_Data_v2_t *object = NULL;
  qapi_Net_LWM2M_Instance_Info_v2_t *instance = NULL;
  qapi_Net_LWM2M_Resource_Info_t *resource = NULL;
  qapi_Net_LWM2M_App_Ex_Obj_Data_t *read_data = NULL;
  qapi_Net_LWM2M_Flat_Data_t *data = NULL;
  uint8_t *payload = NULL;
  uint32_t payload_length = 0;
  int i = 0;
  int size = 0;
  qapi_Net_LWM2M_Content_Type_t cont_type;
  
#if 0
  if (object_generic == NULL)
  {
	IOT_DEBUG("Object is not valid.");
	send_response_message(op_data, QAPI_NET_LWM2M_500_INTERNAL_SERVER_E);
	return QAPI_ERROR;
  }
#endif

  if(handler == NULL)
  {
	IOT_DEBUG("Application handle is not valid for current application");
	//send_response_message(op_data, QAPI_NET_LWM2M_400_BAD_REQUEST_E);
	return QAPI_ERR_INVALID_PARAM;
  }

  obj_info = op_data.obj_info;

  if(obj_info.obj_mask & QAPI_NET_LWM2M_RESOURCE_ID_E)
  {
	if(handler == ext_object_handler_app)
	{
	  if(lwm2m_object_find(object_generic, obj_info.obj_id))
	  {
		if (lwm2m_check_readable(obj_info.obj_id, obj_info.res_id) == false)
		{
		  IOT_DEBUG("Read on resource %d not allowed.", obj_info.res_id);
		  send_response_message(handler, op_data, QAPI_NET_LWM2M_405_METHOD_NOT_ALLOWED_E);
		  return QAPI_ERROR;
		}
	  }
	}
  }

  tx_byte_allocate(byte_pool_m2m, (VOID **)&read_data, sizeof(qapi_Net_LWM2M_App_Ex_Obj_Data_t), TX_NO_WAIT);
  if(read_data == NULL)
  {
	IOT_DEBUG("Memory allocation failure.\n");
	send_response_message(handler, op_data, QAPI_NET_LWM2M_500_INTERNAL_SERVER_E);
	return QAPI_ERR_NO_MEMORY;
  }
  memset(read_data, 0x00, sizeof(qapi_Net_LWM2M_App_Ex_Obj_Data_t));

  if((obj_info.obj_mask & QAPI_NET_LWM2M_OBJECT_ID_E))
  {
	if(handler == ext_object_handler_app)
	{
	  object = lwm2m_object_find(object_generic, obj_info.obj_id);
	}

	if(object != NULL)
	{
	  if((obj_info.obj_mask & QAPI_NET_LWM2M_INSTANCE_ID_E))
	  {
		instance = lwm2m_instance_find(object->instance_info, obj_info.obj_inst_id);
		if(instance == NULL)
		{
		  IOT_DEBUG("Instance id not found.");
		  result = QAPI_ERR_NO_ENTRY;
		  send_response_message(handler, op_data, QAPI_NET_LWM2M_404_NOT_FOUND_E);
		  goto end;
		}
		else
		{
		  if((obj_info.obj_mask & QAPI_NET_LWM2M_RESOURCE_ID_E))
		  {
			resource = lwm2m_resource_find(instance->resource_info, obj_info.res_id);
			if(resource == NULL)
			{
			  IOT_DEBUG("Resource id not found.");
			  result = QAPI_ERR_NO_ENTRY;
			  send_response_message(handler, op_data, QAPI_NET_LWM2M_404_NOT_FOUND_E);
			  goto end;
			}
			size = 1;
			data = lwm2m_data_new(size);
			if(data == NULL)
			{
			  result = QAPI_ERR_NO_MEMORY;
			  send_response_message(handler, op_data, QAPI_NET_LWM2M_500_INTERNAL_SERVER_E);
			  goto end;
			}
			(data)->id = resource->resource_ID;
		  }
		  result = read_data_ext(instance->instance_ID, &size, &data, object);
		}
	  }
	  else
	  {
		for(instance = object->instance_info; instance != NULL; instance = instance->next)
		{
		  size++;
		}
		data = lwm2m_data_new(size);
		if(data == NULL)
		{
		  result = QAPI_ERR_NO_MEMORY;
		  send_response_message(handler, op_data, QAPI_NET_LWM2M_500_INTERNAL_SERVER_E);
		  goto end;
		}
		instance = object->instance_info;
		i =0;
		result = QAPI_OK;
		while(instance != NULL && result == QAPI_OK)
		{
		  result = read_data_ext(instance->instance_ID, (int *)&((data)[i].value.asChildren.count),
								 &((data)[i].value.asChildren.array), object);
		  (data)[i].id = instance->instance_ID;
		  (data)[i].type = QAPI_NET_LWM2M_TYPE_OBJECT_INSTANCE;
		  i++;
		  instance = instance->next;
		}
	  }
	}
	else
	{
	  IOT_DEBUG("object id not found.");
	  result = QAPI_ERR_NO_ENTRY;
	  send_response_message(handler, op_data, QAPI_NET_LWM2M_404_NOT_FOUND_E);
	  goto end;
	}
  }
  else
  {
	IOT_DEBUG("Not valid request.");
	result = QAPI_ERR_INVALID_PARAM;
	send_response_message(handler, op_data, QAPI_NET_LWM2M_400_BAD_REQUEST_E);
	goto end;
  }

  if((obj_info.obj_mask & QAPI_NET_LWM2M_RESOURCE_ID_E))
  {
	cont_type = QAPI_NET_LWM2M_TEXT_PLAIN;
  }
  else
  {
	cont_type = QAPI_NET_LWM2M_M2M_TLV;
  }

  if(result == QAPI_OK)
  {
  	/*qapi_Net_LWM2M_Encode_App_Payload is deprecated, use qapi_Net_LWM2M_Encode_Data instead.*/	
	result = qapi_Net_LWM2M_Encode_Data(ext_object_handler_app, &op_data.obj_info,
											   data,
											   (size_t)size,
											   NULL,
											   cont_type,
											   &payload,
											   &payload_length);
	 if (result != QAPI_OK || payload_length == 0)
	 {
	   if (op_data.content_type != QAPI_NET_LWM2M_TEXT_PLAIN || size != 1
		   || (data)->type != QAPI_NET_LWM2M_TYPE_STRING_E || (data)->value.asBuffer.length != 0)
	   {
		 result = QAPI_ERR_BAD_PAYLOAD;
		 send_response_message(handler, op_data, QAPI_NET_LWM2M_404_NOT_FOUND_E);
		 goto end;
	   }
	 }
  }

  if(data)
  {
	lwm2m_data_free(size, data);
  }

  if (payload_length == 0)
  {
	IOT_DEBUG("Payload Length is zero");
	result = QAPI_ERROR;
	goto end;
  }
  else
  {
	read_data->payload_len = payload_length;
	tx_byte_allocate(byte_pool_m2m, (VOID **)&(read_data->payload), 
					 payload_length, TX_NO_WAIT);

	if(read_data->payload == NULL)
	{
	  IOT_DEBUG("Error in memory allocation");
	  send_response_message(handler, op_data, QAPI_NET_LWM2M_500_INTERNAL_SERVER_E);
	  result = QAPI_ERR_NO_MEMORY;
	  goto end;
	}
	if (payload == NULL)
	{
	  IOT_DEBUG("Payload is NULL");
	  result = QAPI_ERROR;
	  goto end;
	}
	else
	{
	  memcpy(read_data->payload, payload, payload_length);
	  read_data->msg_type	 = QAPI_NET_LWM2M_RESPONSE_MSG_E;
	  read_data->obj_info	 = obj_info;
	  read_data->status_code = QAPI_NET_LWM2M_205_CONTENT_E;
	  read_data->conf_msg	 = 0;
	  read_data->msg_id_len  = op_data.msg_id_len;
	  memcpy(read_data->msg_id, op_data.msg_id, op_data.msg_id_len);
	  read_data->content_type = cont_type;
	
	  result = qapi_Net_LWM2M_Send_Message(handler, read_data);
	}
  }

end:
  if(read_data)
  {
	if(read_data->payload)
	{
	  tx_byte_release(read_data->payload);
	  read_data->payload = NULL;
	}
	tx_byte_release(read_data);
	read_data = NULL;
  }
  return result;
}

qapi_Status_t exec_ext_obj
(
	qapi_Net_LWM2M_App_Handler_t handler, 
	qapi_Net_LWM2M_Server_Data_t op_data
)
{
  qapi_Status_t result = QAPI_ERROR;
  qapi_Net_LWM2M_Data_v2_t *object = NULL;
  qapi_Net_LWM2M_Obj_Info_t  obj_info;
  qapi_Net_LWM2M_Instance_Info_v2_t *instance = NULL;
  qapi_Net_LWM2M_Resource_Info_t *resource = NULL;
  qapi_Net_LWM2M_App_Ex_Obj_Data_t *exec_data = NULL;

  /*if(object_generic == NULL)
  {
    IOT_DEBUG("Object is not valid.");
    send_response_message(op_data, QAPI_NET_LWM2M_500_INTERNAL_SERVER_E);
    return QAPI_ERROR;
  }*/

  if(handler == NULL )
  {
    IOT_DEBUG("Application handle is not valid for current application");
    //send_response_message(op_data, QAPI_NET_LWM2M_400_BAD_REQUEST_E);
    return QAPI_ERR_INVALID_PARAM;
  }

  
  obj_info = op_data.obj_info;

  if(obj_info.obj_mask & QAPI_NET_LWM2M_RESOURCE_ID_E)
  {
    if(handler == ext_object_handler_app)
    {
      if(lwm2m_object_find(object_generic, obj_info.obj_id))
      {
        if(check_executable(obj_info.obj_id, obj_info.res_id) == false)
        {
          IOT_DEBUG("Execute on resource %d not allowed.", obj_info.res_id);
          send_response_message(handler, op_data, QAPI_NET_LWM2M_405_METHOD_NOT_ALLOWED_E);
          return QAPI_ERROR;
        }
      }
    }    
  }

  if((obj_info.obj_mask & QAPI_NET_LWM2M_OBJECT_ID_E))
  {
    if(handler == ext_object_handler_app)
    {
      object = lwm2m_object_find(object_generic, obj_info.obj_id);
    }

    if(object == NULL)
    {
      IOT_DEBUG("Object id not found.");
      send_response_message(handler, op_data, QAPI_NET_LWM2M_404_NOT_FOUND_E);
      return QAPI_ERR_NO_ENTRY;
    }
    else
    {
      if((obj_info.obj_mask & QAPI_NET_LWM2M_INSTANCE_ID_E ))
      {
        instance = lwm2m_instance_find(object->instance_info, obj_info.obj_inst_id);
        if(instance == NULL)
        {
          IOT_DEBUG("Instance id not found.");
          send_response_message(handler, op_data, QAPI_NET_LWM2M_404_NOT_FOUND_E);
          return QAPI_ERR_NO_ENTRY;
        }
        else
        {
          if((obj_info.obj_mask & QAPI_NET_LWM2M_RESOURCE_ID_E))
          {
            resource = lwm2m_resource_find(instance->resource_info, obj_info.res_id);
            if(resource == NULL)
            {
              IOT_DEBUG("Resource id not found.");
              send_response_message(handler, op_data, QAPI_NET_LWM2M_404_NOT_FOUND_E);
              return QAPI_ERR_NO_ENTRY;
            }
            else
            {
              result = exec_data_ext(instance->instance_ID, object);

              if(result != QAPI_OK)
              {
                IOT_DEBUG("Execute Operation unsuccessful.");
                send_response_message(handler, op_data, QAPI_NET_LWM2M_500_INTERNAL_SERVER_E);
                return QAPI_ERR_NO_ENTRY;
              }

              tx_byte_allocate(byte_pool_m2m, (VOID **)&exec_data, sizeof(qapi_Net_LWM2M_App_Ex_Obj_Data_t), TX_NO_WAIT);
              if(exec_data == NULL)
              {
                IOT_DEBUG("Error in memory allocation");
                send_response_message(handler, op_data, QAPI_NET_LWM2M_500_INTERNAL_SERVER_E);
                return QAPI_ERR_NO_MEMORY;
              }
              memset(exec_data, 0x00, sizeof(qapi_Net_LWM2M_App_Ex_Obj_Data_t));

              exec_data->msg_type = QAPI_NET_LWM2M_RESPONSE_MSG_E;
              exec_data->obj_info = op_data.obj_info;
              exec_data->status_code = QAPI_NET_LWM2M_204_CHANGED_E;
              exec_data->conf_msg = 0;
              exec_data->msg_id_len = op_data.msg_id_len;
              memcpy(exec_data->msg_id, op_data.msg_id, op_data.msg_id_len);

              result = qapi_Net_LWM2M_Send_Message(handler, exec_data);
  
               if(exec_data)
               {
                 /*if(exec_data->payload)
                 {
                   tx_byte_release(exec_data->payload);
                   //free(exec_data->payload);
                   exec_data->payload = NULL;
                 }*/
                 tx_byte_release(exec_data);
                 //free(exec_data);
                 exec_data = NULL;
               }
            }
          }
          else
          {
            IOT_DEBUG("Execute on instance level not allowed.");
            send_response_message(handler, op_data, QAPI_NET_LWM2M_405_METHOD_NOT_ALLOWED_E);
            return QAPI_ERR_INVALID_PARAM;
          }
        }
      }
      else
      {
        IOT_DEBUG("Execute on object level not allowed.");
        send_response_message(handler, op_data, QAPI_NET_LWM2M_405_METHOD_NOT_ALLOWED_E);
        return QAPI_ERR_INVALID_PARAM;
      }
    }
  }

  return result;
}

qapi_Status_t discover_data_ext(uint16_t instanceid, int *size, qapi_Net_LWM2M_Flat_Data_t **data,
                                qapi_Net_LWM2M_Data_v2_t *object)
{
  qapi_Net_LWM2M_Instance_Info_v2_t *target = NULL;
  int i = 0;
  uint16_t reslist_ob1[] = {
                             RES_M_SENSOR_VALUE,
                             RES_O_SENSOR_UNITS,
                             RES_O_MIN_MEASURED_VAL,
                             RES_O_MAX_MEASURED_VAL,
                             RES_M_MIN_RANGE_VAL,
                             RES_M_MAX_RANGE_VAL,
                             RES_O_RESET_MEASURED_VAL,
                             RES_M_SENSOR_STATE,
                             RES_O_MEAN_VAL
                           };

  uint16_t reslist_ob2[] = {
                             RES_M_DIG_INPUT_STATE,
                             RES_O_DIG_INPUT_COUNTER,
                             RES_O_RESET_DIG_INPUT_COUNTER,
                             RES_O_SENSOR_TYPE,
                             RES_O_BUSY_TO_CLEAR_DELAY,
                             RES_O_CLEAR_TO_BUSY_DELAY,
                           };

  int nbres;
  int temp;

  target = lwm2m_instance_find(object->instance_info, instanceid);
  if(target == NULL)
  {
    return QAPI_ERR_NO_ENTRY;
  }

  if(*size == 0)
  {
    if(object->object_ID == LWM2M_GENERIC_SENSOR_OBJECT_ID)
    {
      nbres = sizeof(reslist_ob1)/sizeof(uint16_t);
    }
    else
    {
      nbres = sizeof(reslist_ob2)/sizeof(uint16_t);
    }
    temp = nbres;
     
    *data = lwm2m_data_new(nbres);
    if(*data == NULL)
    {
      return QAPI_ERR_NO_MEMORY;
    }
    *size = nbres;

    if(object->object_ID == LWM2M_GENERIC_SENSOR_OBJECT_ID)
    {
      for(i = 0; i<temp; i++)
      {
        (*data)[i].id = reslist_ob1[i];
      }
    }
    else
    {
      for(i = 0; i<temp; i++)
      {
        (*data)[i].id = reslist_ob2[i];
      }
    }
  }

  else
  {
    if(object->object_ID == LWM2M_GENERIC_SENSOR_OBJECT_ID)
    {
      for(i =0; i < *size ; i++)
      {
        switch((*data)[i].id)
        {
          case RES_M_SENSOR_VALUE:
          case RES_O_SENSOR_UNITS:
          case RES_O_MIN_MEASURED_VAL:
          case RES_O_MAX_MEASURED_VAL:
          case RES_M_MIN_RANGE_VAL:
          case RES_M_MAX_RANGE_VAL:
          case RES_O_RESET_MEASURED_VAL:
          case RES_M_SENSOR_STATE:
          case RES_O_MEAN_VAL:
               break;
          default:
              return QAPI_ERROR;
        }
      }
    }
    else
    {
      for(i =0; i < *size ; i++)
      {
        switch((*data)[i].id)
        {
          case RES_M_DIG_INPUT_STATE:
          case RES_O_DIG_INPUT_COUNTER:
          case RES_O_RESET_DIG_INPUT_COUNTER:
          case RES_O_SENSOR_TYPE:
          case RES_O_BUSY_TO_CLEAR_DELAY:
          case RES_O_CLEAR_TO_BUSY_DELAY:
               break;
          default:
              return QAPI_ERROR;
        }
      }
    }
  }

  return QAPI_OK;
}

qapi_Status_t disc_ext_obj
(
	qapi_Net_LWM2M_App_Handler_t handler, 
	qapi_Net_LWM2M_Server_Data_t op_data
)
{
  qapi_Status_t result = QAPI_ERROR;
  qapi_Net_LWM2M_Obj_Info_t  obj_info;
  qapi_Net_LWM2M_Data_v2_t *object = NULL;
  qapi_Net_LWM2M_Instance_Info_v2_t *instance = NULL;
  qapi_Net_LWM2M_Resource_Info_t *resource = NULL;
  qapi_Net_LWM2M_App_Ex_Obj_Data_t *disc_data = NULL;
  qapi_Net_LWM2M_Flat_Data_t *data = NULL;
  uint8_t *payload = NULL;
  uint32_t payload_length = 0;
  int i = 0;
  int size = 0;

  if(handler == NULL)
  {
    IOT_DEBUG("Application handle is not valid for current application");
    return QAPI_ERR_INVALID_PARAM;
  }

  obj_info = op_data.obj_info;

  if((obj_info.obj_mask & QAPI_NET_LWM2M_OBJECT_ID_E))
  {
    if(handler == ext_object_handler_app)
    {
      object = lwm2m_object_find(object_generic, obj_info.obj_id);
    }
	
    if(object != NULL)
    {
      if(object->instance_info == NULL)
      {
        IOT_DEBUG("Instance list is empty.");
        result = QAPI_ERR_NO_ENTRY;
        send_response_message(handler, op_data, QAPI_NET_LWM2M_404_NOT_FOUND_E);
        goto end;
      }

      if((obj_info.obj_mask & QAPI_NET_LWM2M_INSTANCE_ID_E))
      {
        instance = lwm2m_instance_find(object->instance_info, obj_info.obj_inst_id);
        if(instance == NULL)
        {
          IOT_DEBUG("Instance id not found.");
          result = QAPI_ERR_NO_ENTRY;
          send_response_message(handler, op_data, QAPI_NET_LWM2M_404_NOT_FOUND_E);
          goto end;
        }
        else
        {
          if((obj_info.obj_mask & QAPI_NET_LWM2M_RESOURCE_ID_E))
          {
            resource = lwm2m_resource_find(instance->resource_info, obj_info.res_id);
            if(resource == NULL)
            {
              IOT_DEBUG("Resource id not found.");
              result = QAPI_ERR_NO_ENTRY;
              send_response_message(handler, op_data, QAPI_NET_LWM2M_404_NOT_FOUND_E);
              goto end;
            }
            size = 1;
            data = lwm2m_data_new(size);
            if(data == NULL)
            {
              result = QAPI_ERR_NO_MEMORY;
              send_response_message(handler, op_data, QAPI_NET_LWM2M_500_INTERNAL_SERVER_E);
              goto end;
            }
            (data)->id = resource->resource_ID;
          }
          result = discover_data_ext(instance->instance_ID, &size, &data, object);
        }
      }
      else
      {
        for(instance = object->instance_info; instance != NULL; instance = instance->next)
        {
          size++;
        }
        data = lwm2m_data_new(size);
        if(data == NULL)
        {
          result = QAPI_ERR_NO_MEMORY;
          send_response_message(handler, op_data, QAPI_NET_LWM2M_500_INTERNAL_SERVER_E);
          goto end;
        }
        instance = object->instance_info;
        i =0;
        result = QAPI_OK;
        while(instance != NULL && result == QAPI_OK)
        {
          result = discover_data_ext(instance->instance_ID, (int *)&((data)[i].value.asChildren.count),
                                     &((data)[i].value.asChildren.array), object);
          (data)[i].id = instance->instance_ID;
          (data)[i].type = QAPI_NET_LWM2M_TYPE_OBJECT_INSTANCE;
          i++;
          instance = instance->next;
        }
      }
    }
    else
    {
      IOT_DEBUG("object id not found.");
      result = QAPI_ERR_NO_ENTRY;
      send_response_message(handler, op_data, QAPI_NET_LWM2M_404_NOT_FOUND_E);
      goto end;
    }
  }
  else
  {
    IOT_DEBUG("Not valid request.");
    result = QAPI_ERR_INVALID_PARAM;
    send_response_message(handler, op_data, QAPI_NET_LWM2M_400_BAD_REQUEST_E);
    goto end;
  }

  if(result == QAPI_OK)
  {
    int len = 0;
    qapi_Net_LWM2M_Uri_t uri;
    memset(&uri, 0x00, sizeof(qapi_Net_LWM2M_Uri_t));
    uri.flag = obj_info.obj_mask;
    uri.objectId = obj_info.obj_id;
    uri.instanceId = obj_info.obj_inst_id;
    uri.resourceId = obj_info.res_id;
    uri.resourceInstId = obj_info.res_inst_id;
    
    len = ext_obj_discover_serialize(handler, op_data, &uri, size, data, &payload);
  
    if(len <= 0)
    {
      send_response_message(handler, op_data, QAPI_NET_LWM2M_500_INTERNAL_SERVER_E);
      result = QAPI_ERR_NO_MEMORY;
      goto end;
    }
    else
    {
      payload_length = len;
    }
  }

  if(data)
  {
    lwm2m_data_free(size, data);
  }

  if(payload == NULL || payload_length == 0)
  {
    send_response_message(handler, op_data, QAPI_NET_LWM2M_500_INTERNAL_SERVER_E);
    result = QAPI_ERR_NO_MEMORY;
    goto end;
  }

  tx_byte_allocate(byte_pool_m2m, (VOID **)&disc_data, 
                   sizeof(qapi_Net_LWM2M_App_Ex_Obj_Data_t), TX_NO_WAIT);
  if(disc_data == NULL)
  {
    IOT_DEBUG("Memory allocation failure.\n");
    send_response_message(handler, op_data, QAPI_NET_LWM2M_500_INTERNAL_SERVER_E);
    return QAPI_ERR_NO_MEMORY;
  }
  memset(disc_data, 0x00, sizeof(qapi_Net_LWM2M_App_Ex_Obj_Data_t));

  disc_data->payload_len = payload_length;
  tx_byte_allocate(byte_pool_m2m, (VOID **)&(disc_data->payload), 
                   payload_length, TX_NO_WAIT);
  if(disc_data->payload == NULL)
  {
    IOT_DEBUG("Error in memory allocation");
    send_response_message(handler, op_data, QAPI_NET_LWM2M_500_INTERNAL_SERVER_E);
    result = QAPI_ERR_NO_MEMORY;
    goto end;
  }
  memcpy(disc_data->payload, payload, payload_length);
  disc_data->msg_type    = QAPI_NET_LWM2M_RESPONSE_MSG_E;
  disc_data->obj_info    = obj_info;
  disc_data->status_code = QAPI_NET_LWM2M_205_CONTENT_E;
  disc_data->conf_msg    = 0;
  disc_data->msg_id_len  = op_data.msg_id_len;
  memcpy(disc_data->msg_id, op_data.msg_id, op_data.msg_id_len);
  disc_data->content_type = QAPI_NET_LWM2M_APPLICATION_LINK_FORMAT;

  result = qapi_Net_LWM2M_Send_Message(handler, disc_data);

end:
  if(disc_data)
  {
    if(disc_data->payload)
    {
      tx_byte_release(disc_data->payload);
      disc_data->payload = NULL;
    }
    tx_byte_release(disc_data);
    disc_data = NULL;
  }
  return result;
}

qapi_Status_t observe_ext_handle_req
(
	qapi_Net_LWM2M_App_Handler_t handler,
	qapi_Net_LWM2M_Data_v2_t *object,
	qapi_Net_LWM2M_Server_Data_t op_data,
	int size, qapi_Net_LWM2M_Flat_Data_t *data
)
{
  qapi_Net_LWM2M_Observe_info_t *observe_info = NULL;
  qapi_Net_LWM2M_Uri_t uri;
  uint8_t msg_id[QAPI_MAX_LWM2M_MSG_ID_LENGTH];
  uint8_t msg_id_len;
  qapi_time_get_t curr_time;
  qapi_Net_LWM2M_Content_Type_t accpet_type;

  if(handler == NULL || object == NULL || data == NULL)
  {
    IOT_DEBUG("fun [%s] file [%s]  Passed  Argument is NULL\n",__func__,__FILE__);
    return QAPI_ERROR;
  }

  memset(&uri, 0x00, sizeof(qapi_Net_LWM2M_Uri_t));

  uri.flag = op_data.obj_info.obj_mask;
  uri.objectId = op_data.obj_info.obj_id;
  uri.instanceId = op_data.obj_info.obj_inst_id;
  uri.resourceId = op_data.obj_info.res_id;
  uri.resourceInstId = op_data.obj_info.res_inst_id;

  msg_id_len = op_data.msg_id_len;
  memcpy(msg_id, op_data.msg_id, op_data.msg_id_len);
  if(op_data.accept_is_valid == false)
  {
	accpet_type = QAPI_NET_LWM2M_APPLICATION_OCTET_STREAM;
  }
  else
  {
  	accpet_type = op_data.accept;
  }

  observe_info = lwm2m_get_observe_info(handler, &uri, msg_id, msg_id_len, accpet_type);

  if(observe_info == NULL)
  {
    IOT_DEBUG("Observe info is NULL\n");
    return QAPI_ERROR;
  }

  if(handler == ext_object_handler_app)
  {
#if 1
    qapi_TIMER_set_attr_t time_attr;
    time_attr.unit = QAPI_TIMER_UNIT_SEC;
    time_attr.reload = TRUE;
    time_attr.time = 1;
    qapi_Timer_Set(obs_notify_Timer, &time_attr);
#endif
  }

  observe_info->active = true;
  qapi_time_get(QAPI_TIME_SECS, &curr_time);

  observe_info->last_time = curr_time.time_secs;

  if(uri.flag & QAPI_NET_LWM2M_RESOURCE_ID_E)
  {
    switch(data->type)
    {
      case QAPI_NET_LWM2M_TYPE_INTEGER_E:
           if (1 != lwm2m_data_decode_int(data, &(observe_info->lastvalue.asInteger))) 
           return QAPI_ERROR;
           break;
      case QAPI_NET_LWM2M_TYPE_FLOAT_E:
            if (1 != lwm2m_data_decode_float(data, &(observe_info->lastvalue.asFloat))) 
            return QAPI_ERROR;
            break;
      default:
            break;
    }
  }

  return QAPI_OK;
}

qapi_Status_t observe_cancel_ext_handle_req
(
	qapi_Net_LWM2M_App_Handler_t handler,
	qapi_Net_LWM2M_Server_Data_t op_data
)
{
  qapi_Status_t result = QAPI_ERROR;
  qapi_Net_LWM2M_Observed_t *obs = NULL;
  qapi_Net_LWM2M_Observed_t *observed = NULL;

  if(handler == NULL)
  {
    IOT_DEBUG("fun [%s] file [%s]  Passed  Argument is NULL\n",__func__,__FILE__);
    return QAPI_ERROR;
  }

  if(handler == ext_object_handler_app)
  {
    obs = ext_observed_list;
  }

  for(observed = obs; observed != NULL; observed = observed->next)
  {
    qapi_Net_LWM2M_Observe_info_t *target = NULL;

    if((LWM2M_MAX_ID == op_data.notification_id && (strcmp((char *)observed->observe_info->msg_id, (char *)op_data.msg_id) == 0))
       || observed->observe_info->not_id == op_data.notification_id)
    {
      target = observed->observe_info;
      observed->observe_info = observed->observe_info->next;
    }
    else
    {
      qapi_Net_LWM2M_Observe_info_t *observer = NULL;

      observer = observed->observe_info;
      while(observer->next != NULL && (observer->next->not_id != op_data.notification_id )) 
            //(strcmp((char *)observer->next->msg_id, (char *)op_data.msg_id) == 0 ))
      {
        observer = observer->next;
      }
      if (observer->next != NULL)
      {
        target = observer->next;
        observer->next = observer->next->next;
      }
    }

    if(target != NULL)
    {
      tx_byte_release(target);
      target = NULL;
      if (observed->observe_info == NULL)
      {
        IOT_DEBUG("LWM2M_LOG: Removing the Observe request  from the watcher list ,\n");
        lwm2m_unlink_observed(handler, observed);
        IOT_DEBUG("LWM2M_LOG: object id %d\n",observed->uri.objectId)

        if(observed->uri.flag & QAPI_NET_LWM2M_INSTANCE_ID_E)
        {
          IOT_DEBUG("LWM2M_LOG: instance id %d\n",observed->uri.instanceId) 
          if(observed->uri.flag & QAPI_NET_LWM2M_RESOURCE_ID_E)
          {
            IOT_DEBUG("LWM2M_LOG: resource id %d\n",observed->uri.resourceId);    
            if(observed->uri.flag & QAPI_NET_LWM2M_RESOURCE_INSTANCE_ID_E)
            {
              IOT_DEBUG("LWM2M_LOG: resource instance id %d\n",observed->uri.resourceInstId);
            }
          }
        } 
        tx_byte_release(observed);
        observed = NULL;
      }  
      return QAPI_OK;
    }
  }
  return result;
}

qapi_Status_t observe_ext_obj
(
	qapi_Net_LWM2M_App_Handler_t handler, 
	qapi_Net_LWM2M_Server_Data_t op_data
)
{
  qapi_Status_t result = QAPI_ERROR;
  qapi_Net_LWM2M_Obj_Info_t  obj_info;
  qapi_Net_LWM2M_Data_v2_t *object = NULL;
  qapi_Net_LWM2M_Instance_Info_v2_t *instance = NULL;
  qapi_Net_LWM2M_Resource_Info_t *resource = NULL;
  qapi_Net_LWM2M_App_Ex_Obj_Data_t *obs_data = NULL;
  qapi_Net_LWM2M_Flat_Data_t *data = NULL;
  uint8_t *payload = NULL;
  uint32_t payload_length = 0;
  int i = 0;
  int size = 0;
  qapi_Net_LWM2M_Content_Type_t cont_type;

  if(handler == NULL)
  {
    IOT_DEBUG("Application handle is not valid for current application");
    //send_response_message(op_data, QAPI_NET_LWM2M_400_BAD_REQUEST_E);
    return QAPI_ERR_INVALID_PARAM;
  }

  obj_info = op_data.obj_info;

  if((obj_info.obj_mask & QAPI_NET_LWM2M_OBJECT_ID_E))
  {
    if(handler == ext_object_handler_app)
    {
      object = lwm2m_object_find(object_generic, obj_info.obj_id);
    }

    if(object != NULL)
    {
      if((obj_info.obj_mask & QAPI_NET_LWM2M_INSTANCE_ID_E))
      {
        instance = lwm2m_instance_find(object->instance_info, obj_info.obj_inst_id);
        if(instance == NULL)
        {
          IOT_DEBUG("Instance id not found.");
          result = QAPI_ERR_NO_ENTRY;
          send_response_message(handler, op_data, QAPI_NET_LWM2M_404_NOT_FOUND_E);
          goto end;
        }
        else
        {
          if((obj_info.obj_mask & QAPI_NET_LWM2M_RESOURCE_ID_E))
          {
            resource = lwm2m_resource_find(instance->resource_info, obj_info.res_id);
            if(resource == NULL)
            {
              IOT_DEBUG("Resource id not found.");
              result = QAPI_ERR_NO_ENTRY;
              send_response_message(handler, op_data, QAPI_NET_LWM2M_404_NOT_FOUND_E);
              goto end;
            }
            size = 1;
            data = lwm2m_data_new(size);
            if(data == NULL)
            {
              result = QAPI_ERR_NO_MEMORY;
              send_response_message(handler, op_data, QAPI_NET_LWM2M_500_INTERNAL_SERVER_E);
              goto end;
            }
            (data)->id = resource->resource_ID;
          }
          result = read_data_ext(instance->instance_ID, &size, &data, object);
        }
      }
      else
      {
        for(instance = object->instance_info; instance != NULL; instance = instance->next)
        {
          size++;
        }
        data = lwm2m_data_new(size);
        if(data == NULL)
        {
          result = QAPI_ERR_NO_MEMORY;
          send_response_message(handler, op_data, QAPI_NET_LWM2M_500_INTERNAL_SERVER_E);
          goto end;
        }
        instance = object->instance_info;
        i =0;
        result = QAPI_OK;
        while(instance != NULL && result == QAPI_OK)
        {
          result = read_data_ext(instance->instance_ID, (int *)&((data)[i].value.asChildren.count),
                                 &((data)[i].value.asChildren.array), object);
          (data)[i].id = instance->instance_ID;
          (data)[i].type = QAPI_NET_LWM2M_TYPE_OBJECT_INSTANCE;
          i++;
          instance = instance->next;
        }
      }
    }
    else
    {
      IOT_DEBUG("object id not found.");
      result = QAPI_ERR_NO_ENTRY;
      send_response_message(handler, op_data, QAPI_NET_LWM2M_404_NOT_FOUND_E);
      goto end;
    }
  }
  else
  {
    IOT_DEBUG("Not valid request.");
    result = QAPI_ERR_INVALID_PARAM;
    send_response_message(handler, op_data, QAPI_NET_LWM2M_400_BAD_REQUEST_E);
    goto end;
  }

  if(result == QAPI_OK)
  {
    result = observe_ext_handle_req(handler, object, op_data, size, data);
  }

#if 0
  if((obj_info.obj_mask & QAPI_NET_LWM2M_RESOURCE_ID_E))
  {
  	cont_type = QAPI_NET_LWM2M_TEXT_PLAIN;
  }
  else
  {
    cont_type = QAPI_NET_LWM2M_M2M_TLV;
  }
#endif
  if(op_data.accept_is_valid == false)
  {
	cont_type = QAPI_NET_LWM2M_APPLICATION_OCTET_STREAM;
  }
  else
  {
	cont_type = op_data.accept;
  }

  if(result == QAPI_OK)
  {
    /*qapi_Net_LWM2M_Encode_App_Payload is deprecated, use qapi_Net_LWM2M_Encode_Data instead.*/	  	
    result = qapi_Net_LWM2M_Encode_Data(ext_object_handler_app, &op_data.obj_info,
                                               data,
                                               (size_t)size,
                                               NULL,
                                               cont_type,
                                               &payload,
                                               &payload_length);
     if (result != QAPI_OK || payload_length == 0)
     {
       if (op_data.content_type != QAPI_NET_LWM2M_TEXT_PLAIN || size != 1
           || (data)->type != QAPI_NET_LWM2M_TYPE_STRING_E || (data)->value.asBuffer.length != 0)
       {
         result = QAPI_ERR_BAD_PAYLOAD;
         send_response_message(handler, op_data, QAPI_NET_LWM2M_404_NOT_FOUND_E);
         goto end;
       }
     }
  }

  if(data)
  {
    lwm2m_data_free(size, data);
  }
  if(payload == NULL || payload_length == 0)
  {
    send_response_message(handler, op_data, QAPI_NET_LWM2M_404_NOT_FOUND_E);
    return QAPI_ERR_BAD_PAYLOAD;
  }
  tx_byte_allocate(byte_pool_m2m, (VOID **)&obs_data, 
                   sizeof(qapi_Net_LWM2M_App_Ex_Obj_Data_t), TX_NO_WAIT);
  if(obs_data == NULL)
  {
    IOT_DEBUG("Memory allocation failure.\n");
    send_response_message(handler, op_data, QAPI_NET_LWM2M_500_INTERNAL_SERVER_E);
    return QAPI_ERR_NO_MEMORY;
  }
  memset(obs_data, 0x00, sizeof(qapi_Net_LWM2M_App_Ex_Obj_Data_t));

  obs_data->payload_len = payload_length;
  tx_byte_allocate(byte_pool_m2m, (VOID **)&(obs_data->payload), 
                   payload_length, TX_NO_WAIT);
  if(obs_data->payload == NULL)
  {
    IOT_DEBUG("Error in memory allocation");
    send_response_message(handler, op_data, QAPI_NET_LWM2M_500_INTERNAL_SERVER_E);
    result = QAPI_ERR_NO_MEMORY;
    goto end;
  }
  memcpy(obs_data->payload, payload, payload_length);
//  obs_data->msg_type    = QAPI_NET_LWM2M_RESPONSE_MSG_E;
  obs_data->msg_type    = QAPI_NET_LWM2M_NOTIFY_MSG_E;

  obs_data->obj_info    = obj_info;
  obs_data->status_code = QAPI_NET_LWM2M_205_CONTENT_E;
  obs_data->conf_msg    = 0;
  obs_data->msg_id_len  = op_data.msg_id_len;
  memcpy(obs_data->msg_id, op_data.msg_id, op_data.msg_id_len);
  obs_data->content_type = cont_type;
  
  result = qapi_Net_LWM2M_Send_Message(handler, obs_data);

  if(obs_data)
  {
    if(obs_data->payload)
    {
      tx_byte_release(obs_data->payload);
      obs_data->payload = NULL;
    }
    tx_byte_release(obs_data);
    obs_data = NULL;
  }

end:
  return result;

}

qapi_Status_t observe_cancel_ext_obj
(
	qapi_Net_LWM2M_App_Handler_t handler, 
	qapi_Net_LWM2M_Server_Data_t op_data
)
{
  qapi_Status_t result = QAPI_ERROR;

  if(handler == NULL)
  {
    IOT_DEBUG("Application handle is not valid for current application");
    //send_response_message(handler, op_data, QAPI_NET_LWM2M_400_BAD_REQUEST_E);
    return QAPI_ERR_INVALID_PARAM;
  }

  result = observe_cancel_ext_handle_req(handler, op_data);

  return result;
}

qapi_Status_t ext_object_write_attr
(
	qapi_Net_LWM2M_App_Handler_t handler, 
	qapi_Net_LWM2M_Server_Data_t op_data
)
{
  uint8_t result = QAPI_NET_LWM2M_IGNORE_E;

  if(handler == NULL )
  {
    IOT_DEBUG("Application handle is not valid for current application");
    //send_response_message(op_data, QAPI_NET_LWM2M_400_BAD_REQUEST_E);
    return QAPI_ERR_INVALID_PARAM;
  }

  result = ext_object_set_observe_param(handler, op_data);

  if(result == QAPI_NET_LWM2M_204_CHANGED_E)
  {
    return QAPI_OK;
  }
  else
  {
    return QAPI_ERROR;
  }

}

uint8_t object_check_readable
(
	qapi_Net_LWM2M_Data_v2_t *object,
	qapi_Net_LWM2M_Uri_t *uri
)
{
  uint8_t result = QAPI_NET_LWM2M_IGNORE_E;
  qapi_Net_LWM2M_Data_v2_t *target = NULL;
  qapi_Net_LWM2M_Flat_Data_t *data = NULL;
  int size;

  if(object == NULL || uri == NULL ) 
  {
    IOT_DEBUG(" fun [%s] file [%s]  Passed  Argument is NULL\n",__func__,__FILE__);
    return QAPI_NET_LWM2M_400_BAD_REQUEST_E;
  }

  target = lwm2m_object_find(object, uri->objectId);

  if(target == NULL)
  {
    return QAPI_NET_LWM2M_404_NOT_FOUND_E;
  }
  
  if(!(uri->flag & QAPI_NET_LWM2M_INSTANCE_ID_E)) 
  {
    return QAPI_NET_LWM2M_205_CONTENT_E;
  }
  
  if(lwm2m_instance_find(target->instance_info, uri->instanceId) == NULL)
  {
    return QAPI_NET_LWM2M_404_NOT_FOUND_E;
  }
  
  if(!(uri->flag & QAPI_NET_LWM2M_RESOURCE_ID_E)) 
  {
    return QAPI_NET_LWM2M_205_CONTENT_E;
  }

  size = 1;
  data = lwm2m_data_new(1);

  if(data == NULL)
  {
    return QAPI_NET_LWM2M_500_INTERNAL_SERVER_E;
  }
  
  data->id = uri->resourceId;

  result = read_data_ext(uri->instanceId, &size, &data, object);

  lwm2m_data_free(1, data);

  if(result == QAPI_OK)
  {
    result = QAPI_NET_LWM2M_205_CONTENT_E;
  }

  return result;
}

uint8_t object_check_numeric
(
	qapi_Net_LWM2M_Data_v2_t *object,
	qapi_Net_LWM2M_Uri_t *uri
)
{
  uint8_t result = QAPI_NET_LWM2M_IGNORE_E;
  qapi_Net_LWM2M_Data_v2_t *target = NULL;
  qapi_Net_LWM2M_Flat_Data_t *data = NULL;
  int size;

  if (object == NULL || uri == NULL ) 
  {
    IOT_DEBUG("fun [%s] file [%s]  Passed  Argument is NULL\n",__func__,__FILE__);
    return QAPI_NET_LWM2M_400_BAD_REQUEST_E;
  }

  if(!(uri->flag & QAPI_NET_LWM2M_RESOURCE_ID_E)) 
  {
    return QAPI_NET_LWM2M_405_METHOD_NOT_ALLOWED_E;
  }

  target = lwm2m_object_find(object, uri->objectId);

  if (target == NULL)
  {
    return QAPI_NET_LWM2M_404_NOT_FOUND_E;
  }

  size = 1;
  data = lwm2m_data_new(1);

  if(data == NULL)
  {
    return QAPI_NET_LWM2M_500_INTERNAL_SERVER_E;
  }
  
  data->id = uri->resourceId;

  result = read_data_ext(uri->instanceId, &size, &data, object);

  if (result == QAPI_OK)
  {
    switch (data->type)
    {
      case QAPI_NET_LWM2M_TYPE_INTEGER_E:
      case QAPI_NET_LWM2M_TYPE_FLOAT_E:
        result = (uint8_t)QAPI_NET_LWM2M_205_CONTENT_E;
        break;
      default:
        result = (uint8_t) QAPI_NET_LWM2M_405_METHOD_NOT_ALLOWED_E;
    }
  }

  lwm2m_data_free(1, data);

  return result;
}

qapi_Status_t ext_observer_add_notify_to_pending_list
(
	qapi_Net_LWM2M_App_Handler_t handler,
	qapi_Net_LWM2M_Observe_info_t *observe_info,
	qapi_Net_LWM2M_Uri_t uri,
	qapi_Net_LWM2M_Content_Type_t format,
	uint8_t *buffer, size_t length
)
{
	qapi_Net_LWM2M_Pending_Observed_t *pending_node = NULL;
	qapi_Net_LWM2M_Pending_Observed_t *notify = NULL;
	qapi_Net_LWM2M_App_Ex_Obj_Data_t *notify_data = NULL;
	qapi_Status_t result = QAPI_ERROR;

	IOT_DEBUG("Entering ext_observer_add_notify_to_pending_list");
	if (observe_info == NULL || buffer == NULL || handler == NULL)
	{
		IOT_DEBUG("NULL parameter check failed\n");
		return QAPI_ERROR;
	}

	tx_byte_allocate(byte_pool_m2m, (VOID **)&notify_data, 
	sizeof(qapi_Net_LWM2M_App_Ex_Obj_Data_t), TX_NO_WAIT);
	if(notify_data == NULL)
	{
		IOT_DEBUG("Memory allocation failure.\n");
		return QAPI_ERR_NO_MEMORY;
	}
	
	memset(notify_data, 0x00, sizeof(qapi_Net_LWM2M_App_Ex_Obj_Data_t));

	tx_byte_allocate(byte_pool_m2m, (VOID **)&notify, 
	sizeof(qapi_Net_LWM2M_Pending_Observed_t), TX_NO_WAIT);
	if(notify == NULL)
	{
		IOT_DEBUG("LWM2M_LOG: Malloc failed\n");
		result = QAPI_ERR_NO_MEMORY;
		goto end;
	}
	memset(notify, 0x00, sizeof(qapi_Net_LWM2M_Pending_Observed_t));

	notify->observe_info = observe_info;

	notify_data->payload_len = length;
	tx_byte_allocate(byte_pool_m2m, (VOID **)&(notify_data->payload), 
	length, TX_NO_WAIT);
	if(notify_data->payload == NULL)
	{
		IOT_DEBUG("Error in memory allocation");
		result = QAPI_ERR_NO_MEMORY;
		goto end;
	}
	
	memcpy(notify_data->payload, buffer, length);
	notify_data->msg_type    = QAPI_NET_LWM2M_NOTIFY_MSG_E;
	notify_data->obj_info.obj_mask    = (qapi_Net_LWM2M_ID_t)uri.flag;
	notify_data->obj_info.obj_id      = uri.objectId;
	notify_data->obj_info.obj_inst_id = uri.instanceId;
	notify_data->obj_info.res_id      = uri.resourceId;
	notify_data->obj_info.res_inst_id = uri.resourceInstId;
	notify_data->status_code = QAPI_NET_LWM2M_205_CONTENT_E;
	notify_data->conf_msg    = 0;
	notify_data->msg_id_len  = observe_info->msg_id_len;
	memcpy(notify_data->msg_id, observe_info->msg_id, observe_info->msg_id_len);
	notify_data->content_type = format;

	notify->message = (void *)notify_data;

	if(handler == ext_object_handler_app)
	{
		if(ext_pending_observed_list == NULL)
		{ 
			notify->next = ext_pending_observed_list;
			ext_pending_observed_list = notify;
		} 
		else 
		{
			for(pending_node = ext_pending_observed_list; (pending_node != NULL && pending_node->next != NULL) ; pending_node = pending_node->next);
			pending_node->next = notify;
		}
	}  

	end:

	return result;
}

uint8_t ext_object_set_observe_param
(
	qapi_Net_LWM2M_App_Handler_t handler,
	qapi_Net_LWM2M_Server_Data_t op_data
)
{
  uint8_t result = QAPI_NET_LWM2M_IGNORE_E;
  qapi_Net_LWM2M_Observe_info_t *observe_info = NULL;
  qapi_Net_LWM2M_Uri_t uri;
  uint8_t msg_id[QAPI_MAX_LWM2M_MSG_ID_LENGTH];
  uint8_t msg_id_len;
  qapi_Net_LWM2M_Attributes_t attributes;
  qapi_Net_LWM2M_Content_Type_t accept_type;

  memset(&uri, 0x00, sizeof(qapi_Net_LWM2M_Uri_t));
  memset(&attributes, 0x00, sizeof(qapi_Net_LWM2M_Attributes_t));

  if(op_data.lwm2m_attr == NULL) 
  {
    IOT_DEBUG("fun [%s] file [%s]  Passed  Argument is NULL\n",__func__,__FILE__);
    send_response_message(handler, op_data, QAPI_NET_LWM2M_400_BAD_REQUEST_E);
    return QAPI_NET_LWM2M_400_BAD_REQUEST_E;
  }

  memcpy(&attributes, op_data.lwm2m_attr, sizeof(qapi_Net_LWM2M_Attributes_t));
  uri.flag = op_data.obj_info.obj_mask;
  uri.objectId = op_data.obj_info.obj_id;
  uri.instanceId = op_data.obj_info.obj_inst_id;
  uri.resourceId = op_data.obj_info.res_id;
  uri.resourceInstId = op_data.obj_info.res_inst_id;

  msg_id_len = op_data.msg_id_len;
  memcpy(msg_id, op_data.msg_id, msg_id_len);

  if(op_data.accept_is_valid == false)
  {
  	accept_type = QAPI_NET_LWM2M_APPLICATION_OCTET_STREAM;
  }
  else
  {
	accept_type = op_data.accept;
  }
  

  if(!(uri.flag & QAPI_NET_LWM2M_INSTANCE_ID_E) && (uri.flag & QAPI_NET_LWM2M_RESOURCE_ID_E))
  {
    send_response_message(handler, op_data, QAPI_NET_LWM2M_400_BAD_REQUEST_E);
    return QAPI_NET_LWM2M_400_BAD_REQUEST_E;
  }

  if(handler == ext_object_handler_app)
  {
    result = object_check_readable(object_generic, &uri);
  }

  if (result != QAPI_NET_LWM2M_205_CONTENT_E)
  {
    send_response_message(handler, op_data, (qapi_Net_LWM2M_Response_Code_t)result);
    return (uint8_t)result;
  }

  if((attributes.set_attr_mask & LWM2M_ATTR_FLAG_NUMERIC) != 0)
  {
    if(handler == ext_object_handler_app)
    {
      result = object_check_numeric(object_generic, &uri);
    }

    if (result != QAPI_NET_LWM2M_205_CONTENT_E)
    {
      send_response_message(handler, op_data, (qapi_Net_LWM2M_Response_Code_t)result);
      return(uint8_t) result;
    }
  }

  observe_info = lwm2m_get_observe_info(handler, &uri, msg_id, msg_id_len, accept_type);
  
  if(observe_info == NULL)
  {
    send_response_message(handler, op_data, QAPI_NET_LWM2M_500_INTERNAL_SERVER_E);
    return (uint8_t) QAPI_NET_LWM2M_500_INTERNAL_SERVER_E; 
  }
  
  if ((((attributes.set_attr_mask | (observe_info->attributes?observe_info->attributes->set_attr_mask:0))
          & ~(attributes.clr_attr_mask)) & LWM2M_ATTR_FLAG_NUMERIC) == LWM2M_ATTR_FLAG_NUMERIC)
  {
    float gt = 0.0;
    float lt = 0.0;
    float stp = 0.0;

    if ((attributes.set_attr_mask & QAPI_NET_LWM2M_GREATER_THAN_E) != 0)
    {
      gt = attributes.greaterThan;
    }
    else if (observe_info->attributes != NULL)
    {
      gt = observe_info->attributes->greaterThan;
    }
    else
    {
      //Do nothing
    }

    if ((attributes.set_attr_mask & QAPI_NET_LWM2M_LESS_THAN_E) != 0)
    {
      lt = attributes.lessThan;
    }
    else if (observe_info->attributes != NULL)
    {
      lt = observe_info->attributes->lessThan;
    }
    else
    {
      //Do nothing
    }

     if ((attributes.set_attr_mask & QAPI_NET_LWM2M_STEP_E) != 0)
    {
      stp = attributes.step;
    }
    else if (observe_info->attributes != NULL)
    {
      stp = observe_info->attributes->step;
    }
    else
    {
      //Do nothing
    }

    if (lt + (2 * stp) >= gt) 
    {
      send_response_message(handler, op_data, QAPI_NET_LWM2M_400_BAD_REQUEST_E);
      return (uint8_t)QAPI_NET_LWM2M_400_BAD_REQUEST_E;
    }
  }

  if(observe_info->attributes == NULL)
  {
    if(attributes.set_attr_mask != 0)
    {
      tx_byte_allocate(byte_pool_m2m, (VOID **)&(observe_info->attributes), 
                   sizeof(qapi_Net_LWM2M_Attributes_t), TX_NO_WAIT);

      if(observe_info->attributes == NULL)
      {
        send_response_message(handler, op_data, QAPI_NET_LWM2M_500_INTERNAL_SERVER_E);
        return (uint8_t)QAPI_NET_LWM2M_500_INTERNAL_SERVER_E;
      }
      memset(observe_info->attributes, 0x00, sizeof(qapi_Net_LWM2M_Attributes_t));
      memcpy(observe_info->attributes, &attributes, sizeof(qapi_Net_LWM2M_Attributes_t));
    }
  }
  else
  {
    observe_info->attributes->set_attr_mask &= ~(attributes.clr_attr_mask);
    observe_info->attributes->set_attr_mask |= attributes.set_attr_mask;
    
    if(attributes.set_attr_mask & QAPI_NET_LWM2M_MIN_PERIOD_E)
    {
      observe_info->attributes->minPeriod = attributes.minPeriod;
    }
    if(attributes.set_attr_mask & QAPI_NET_LWM2M_MAX_PERIOD_E)
    {
      observe_info->attributes->maxPeriod = attributes.maxPeriod;
    }
    if(attributes.set_attr_mask & QAPI_NET_LWM2M_LESS_THAN_E)
    {
      observe_info->attributes->lessThan = attributes.lessThan;
    }
    if(attributes.set_attr_mask & QAPI_NET_LWM2M_GREATER_THAN_E)
    {
      observe_info->attributes->greaterThan = attributes.greaterThan;
    }
    if(attributes.set_attr_mask & QAPI_NET_LWM2M_STEP_E)
    {
      observe_info->attributes->step = attributes.step;
    }
    if(attributes.clr_attr_mask & QAPI_NET_LWM2M_MIN_PERIOD_E)
    {
      observe_info->attributes->minPeriod = 0;
    }
    if(attributes.clr_attr_mask & QAPI_NET_LWM2M_MAX_PERIOD_E)
    {
      observe_info->attributes->maxPeriod = 0;
    }
    if(attributes.clr_attr_mask & QAPI_NET_LWM2M_LESS_THAN_E)
    {
      observe_info->attributes->lessThan = 0;
    }
    if(attributes.clr_attr_mask & QAPI_NET_LWM2M_GREATER_THAN_E)
    {
      observe_info->attributes->greaterThan = 0;
    }
    if(attributes.clr_attr_mask & QAPI_NET_LWM2M_STEP_E)
    {
      observe_info->attributes->step = 0;
    }
  }

  IOT_DEBUG("Final toSet: %08X, minPeriod: %d, maxPeriod: %d, greaterThan: %d, lessThan: %d, step: %d",
      observe_info->attributes->set_attr_mask, observe_info->attributes->minPeriod, observe_info->attributes->maxPeriod,
      observe_info->attributes->greaterThan, observe_info->attributes->lessThan, observe_info->attributes->step);

  send_response_message(handler, op_data, QAPI_NET_LWM2M_204_CHANGED_E);
  return QAPI_NET_LWM2M_204_CHANGED_E;
}

qapi_Net_LWM2M_Observed_t* observe_find_by_uri
(
	qapi_Net_LWM2M_App_Handler_t handler,
	qapi_Net_LWM2M_Uri_t *uri
)
{
	qapi_Net_LWM2M_Observed_t *target = NULL;

	if (handler == NULL || uri == NULL) 
	{ 
		IOT_DEBUG("fun [%s] file [%s]  Passed  Argument is NULL\n", __func__, __FILE__); 
		return NULL; 
	}

	if (handler == ext_object_handler_app && ext_observed_list != NULL)
	{
		target = ext_observed_list;
	}

	while (target != NULL)
	{
		if (target->uri.objectId == uri->objectId)
		{
			if ((!(uri->flag & QAPI_NET_LWM2M_INSTANCE_ID_E) && !(target->uri.flag & QAPI_NET_LWM2M_INSTANCE_ID_E))
				||((uri->flag & QAPI_NET_LWM2M_INSTANCE_ID_E) && (target->uri.flag & QAPI_NET_LWM2M_INSTANCE_ID_E) 
				&& (uri->instanceId == target->uri.instanceId)))
			{
				if ((!(uri->flag & QAPI_NET_LWM2M_RESOURCE_ID_E) && !(target->uri.flag & QAPI_NET_LWM2M_RESOURCE_ID_E))
					||((uri->flag & QAPI_NET_LWM2M_RESOURCE_ID_E) && (target->uri.flag & QAPI_NET_LWM2M_RESOURCE_ID_E) 
					&& (uri->resourceId == target->uri.resourceId)))
				{
					IOT_DEBUG("Found one with %s observers.", target->observe_info ? "" : " no");
					return target;
				}
			}
		}
		
		target = target->next;
	}

	IOT_DEBUG("Found nothing");
	return NULL;
}

qapi_Net_LWM2M_Attributes_t *lwm2m_find_attributes
(
	qapi_Net_LWM2M_App_Handler_t handler,
	qapi_Net_LWM2M_Server_Data_t op_data,
	qapi_Net_LWM2M_Uri_t *uri
)
{
	qapi_Net_LWM2M_Observed_t *observed;
	qapi_Net_LWM2M_Observe_info_t *observe_info;
	qapi_Net_LWM2M_Attributes_t *param = NULL;

	if (uri == NULL || handler == NULL)
	{
		return NULL;
	}

	observed = observe_find_by_uri(handler, uri);

	if (observed == NULL || observed->observe_info == NULL)
	{
		return NULL;
	}

	for (observe_info = observed->observe_info; observe_info != NULL; observe_info = observe_info->next)
	{
		if(memcmp((observe_info->msg_id + observe_info->msg_id_len - 2), (op_data.msg_id + op_data.msg_id_len -2), 2) == 0)
		{
			param = observe_info->attributes;
		}
	}

	return param;
}

int ext_serialize_attributes
(
	qapi_Net_LWM2M_App_Handler_t handler,
	qapi_Net_LWM2M_Server_Data_t op_data,
	qapi_Net_LWM2M_Uri_t *uri,
	qapi_Net_LWM2M_Attributes_t *object_param,
	uint8_t *buffer,
	size_t uri_len,
	size_t buffer_len
)
{
  int head;
  int res;
  qapi_Net_LWM2M_Attributes_t *param = NULL;

  head = 0;
  if(handler == NULL || uri == NULL || buffer == NULL) 
  {
    IOT_DEBUG(" Passed  Argument is NULL");
    return -1;
  }

  param = lwm2m_find_attributes(handler, op_data, uri);
  if(param == NULL)
  {
    param = object_param;
  }

  if(param != NULL)
  {
    head = uri_len;

    if(param->set_attr_mask & QAPI_NET_LWM2M_MIN_PERIOD_E)
    {
      PRV_CONCAT_STR(buffer, buffer_len, head, LWM2M_LINK_ATTR_SEPARATOR, LWM2M_LINK_ATTR_SEPARATOR_SIZE);
      PRV_CONCAT_STR(buffer, buffer_len, head, LWM2M_ATTR_MIN_PERIOD_STR, LWM2M_ATTR_MIN_PERIOD_LEN);

      res = utils_intToText(param->minPeriod, buffer + head, buffer_len - head);
      if(res <= 0)
      {
        return -1;
      }
      head += res;
    }
    else if((object_param) && (object_param->set_attr_mask & QAPI_NET_LWM2M_MIN_PERIOD_E))
    {
      PRV_CONCAT_STR(buffer, buffer_len, head, LWM2M_LINK_ATTR_SEPARATOR, LWM2M_LINK_ATTR_SEPARATOR_SIZE);
      PRV_CONCAT_STR(buffer, buffer_len, head, LWM2M_ATTR_MIN_PERIOD_STR, LWM2M_ATTR_MIN_PERIOD_LEN);

      res = utils_intToText(object_param->minPeriod, buffer + head, buffer_len - head);
      if(res <= 0) return -1;
      head += res;
    }

    if(param->set_attr_mask & QAPI_NET_LWM2M_MAX_PERIOD_E)
    {
      PRV_CONCAT_STR(buffer, buffer_len, head, LWM2M_LINK_ATTR_SEPARATOR, LWM2M_LINK_ATTR_SEPARATOR_SIZE);
      PRV_CONCAT_STR(buffer, buffer_len, head, LWM2M_ATTR_MAX_PERIOD_STR, LWM2M_ATTR_MAX_PERIOD_LEN);

      res = utils_intToText(param->maxPeriod, buffer + head, buffer_len - head);
      if(res <= 0)
      {
        return -1;
      }
      head += res;
    }
    else if((object_param != NULL) && (object_param->set_attr_mask & QAPI_NET_LWM2M_MAX_PERIOD_E))
    {
      PRV_CONCAT_STR(buffer, buffer_len, head, LWM2M_LINK_ATTR_SEPARATOR, LWM2M_LINK_ATTR_SEPARATOR_SIZE);
      PRV_CONCAT_STR(buffer, buffer_len, head, LWM2M_ATTR_MAX_PERIOD_STR, LWM2M_ATTR_MAX_PERIOD_LEN);

      res = utils_intToText(object_param->maxPeriod, buffer + head, buffer_len - head);
      if(res <= 0)
      {
        return -1;
      }
      head += res;
    }

    if(param->set_attr_mask & QAPI_NET_LWM2M_GREATER_THAN_E)
    {
      PRV_CONCAT_STR(buffer, buffer_len, head, LWM2M_LINK_ATTR_SEPARATOR, LWM2M_LINK_ATTR_SEPARATOR_SIZE);
      PRV_CONCAT_STR(buffer, buffer_len, head, LWM2M_ATTR_GREATER_THAN_STR, LWM2M_ATTR_GREATER_THAN_LEN);

      res = utils_floatToText(param->greaterThan, buffer + head, buffer_len - head);
      if(res <= 0)
      {
        return -1;
      }
      head += res;
    }
    if(param->set_attr_mask & QAPI_NET_LWM2M_LESS_THAN_E)
    {
      PRV_CONCAT_STR(buffer, buffer_len, head, LWM2M_LINK_ATTR_SEPARATOR, LWM2M_LINK_ATTR_SEPARATOR_SIZE);
      PRV_CONCAT_STR(buffer, buffer_len, head, LWM2M_ATTR_LESS_THAN_STR, LWM2M_ATTR_LESS_THAN_LEN);

      res = utils_floatToText(param->lessThan, buffer + head, buffer_len - head);
      if(res <= 0)
      {
        return -1;
      }
      head += res;
    }
    if(param->set_attr_mask & QAPI_NET_LWM2M_STEP_E)
    {
      PRV_CONCAT_STR(buffer, buffer_len, head, LWM2M_LINK_ATTR_SEPARATOR, LWM2M_LINK_ATTR_SEPARATOR_SIZE);
      PRV_CONCAT_STR(buffer, buffer_len, head, LWM2M_ATTR_STEP_STR, LWM2M_ATTR_STEP_LEN);

      res = utils_floatToText(param->step, buffer + head, buffer_len - head);
      if(res <= 0)
      {
        return -1;
      }
      head += res;
    }
    PRV_CONCAT_STR(buffer, buffer_len, head, LWM2M_LINK_ITEM_ATTR_END, LWM2M_LINK_ITEM_ATTR_END_SIZE);
  }

  if(head > 0)
  {
    head -= uri_len+1;
  }
  return head;
}

int ext_serialize_link_data
(
	qapi_Net_LWM2M_App_Handler_t handler,
	qapi_Net_LWM2M_Server_Data_t op_data,
	qapi_Net_LWM2M_Flat_Data_t *tlv,
	qapi_Net_LWM2M_Attributes_t *object_param,
	qapi_Net_LWM2M_Uri_t *parent_uri,
	uint8_t *parent_uri_str,
	size_t parent_uri_len,
	uint8_t *buffer,
	size_t buffer_len
)
{
	int head = 0;
	int res;
	qapi_Net_LWM2M_Uri_t uri;

	if (handler == NULL || tlv == NULL || parent_uri == NULL 
		|| parent_uri_str == NULL || buffer == NULL ) 
	{
		IOT_DEBUG(" Passed  Argument is NULL");
		return -1;
	}

	switch (tlv->type)
	{
		case QAPI_NET_LWM2M_TYPE_UNDEFINED:
		case QAPI_NET_LWM2M_TYPE_STRING_E:
		case QAPI_NET_LWM2M_TYPE_OPAQUE_E:
		case QAPI_NET_LWM2M_TYPE_INTEGER_E:
		case QAPI_NET_LWM2M_TYPE_FLOAT_E:
		case QAPI_NET_LWM2M_TYPE_BOOLEAN_E:
		case QAPI_NET_LWM2M_TYPE_OBJECT_LINK:
		case QAPI_NET_LWM2M_TYPE_MULTIPLE_RESOURCE:
		{
			if (buffer_len < LWM2M_LINK_ITEM_START_SIZE)
			{
				return -1;
			}

			memcpy(buffer + head, LWM2M_LINK_ITEM_START, LWM2M_LINK_ITEM_START_SIZE);
			head = LWM2M_LINK_ITEM_START_SIZE;

			if (parent_uri_len > 0)
			{
				if (buffer_len - head < parent_uri_len)
				{
					return -1;
				}

				memcpy(buffer + head, parent_uri_str, parent_uri_len);
				head += parent_uri_len;
			}

			if (buffer_len - head < LWM2M_LINK_URI_SEPARATOR_SIZE)
			{
				return -1;
			}

			memcpy(buffer + head, LWM2M_LINK_URI_SEPARATOR, LWM2M_LINK_URI_SEPARATOR_SIZE);
			head += LWM2M_LINK_URI_SEPARATOR_SIZE;

			res = utils_intToText(tlv->id, buffer + head, buffer_len - head);
			if (res <= 0)
			{
				return -1;
			}
			head += res;

			if (tlv->type == QAPI_NET_LWM2M_TYPE_MULTIPLE_RESOURCE)
			{
				if (buffer_len - head < LWM2M_LINK_ITEM_DIM_START_SIZE)
				{
					return -1;
				}
				memcpy(buffer + head, LWM2M_LINK_ITEM_DIM_START, LWM2M_LINK_ITEM_DIM_START_SIZE);
				head += LWM2M_LINK_ITEM_DIM_START_SIZE;

				res = utils_intToText(tlv->value.asChildren.count, buffer + head, buffer_len - head);
				if (res <= 0)
				{
					return -1;
				}
				head += res;

				if (buffer_len - head < LWM2M_LINK_ITEM_ATTR_END_SIZE)
				{
					return -1;
				}
				
				memcpy(buffer + head, LWM2M_LINK_ITEM_ATTR_END, LWM2M_LINK_ITEM_ATTR_END_SIZE);
				head += LWM2M_LINK_ITEM_ATTR_END_SIZE;
			}
			else
			{
				if (buffer_len - head < LWM2M_LINK_ITEM_END_SIZE)
				{
					return -1;
				}
				memcpy(buffer + head, LWM2M_LINK_ITEM_END, LWM2M_LINK_ITEM_END_SIZE);
				head += LWM2M_LINK_ITEM_END_SIZE;
			}

			memcpy(&uri, parent_uri, sizeof(qapi_Net_LWM2M_Uri_t));
			uri.resourceId = tlv->id;
			uri.flag |= QAPI_NET_LWM2M_RESOURCE_ID_E;
			res = ext_serialize_attributes(handler, op_data, &uri, object_param, buffer, head - 1, buffer_len);
			if (res < 0)
			{
				return -1;
			}
			
			if (res > 0)
			{
				head += res;
			}
			break;
		}
		case QAPI_NET_LWM2M_TYPE_OBJECT_INSTANCE:
		{
			uint8_t uri_str[LWM2M_URI_MAX_STRING_LEN];
			size_t uri_len;
			size_t index;

			if (parent_uri_len > 0)
			{
				if (LWM2M_URI_MAX_STRING_LEN < parent_uri_len)
				{
					return -1;
				}

				memcpy(uri_str, parent_uri_str, parent_uri_len);
				uri_len = parent_uri_len;
			}
			else
			{
				uri_len = 0;
			}

			if (LWM2M_URI_MAX_STRING_LEN - uri_len < LWM2M_LINK_URI_SEPARATOR_SIZE)
			{
				return -1;
			}

			if (uri_len >= LWM2M_URI_MAX_STRING_LEN)
			{
				return -1;
			}

			memcpy(uri_str + uri_len, LWM2M_LINK_URI_SEPARATOR, LWM2M_LINK_URI_SEPARATOR_SIZE);
			uri_len += LWM2M_LINK_URI_SEPARATOR_SIZE;

			res = utils_intToText(tlv->id, uri_str + uri_len, LWM2M_URI_MAX_STRING_LEN - uri_len);
			if (res <= 0)
			{
				return -1;
			}
			uri_len += res;

			memcpy(&uri, parent_uri, sizeof(qapi_Net_LWM2M_Uri_t));
			uri.instanceId = tlv->id;
			uri.flag |= QAPI_NET_LWM2M_INSTANCE_ID_E;

			head = 0;
			PRV_CONCAT_STR(buffer, buffer_len, head, LWM2M_LINK_ITEM_START, LWM2M_LINK_ITEM_START_SIZE);
			PRV_CONCAT_STR(buffer, buffer_len, head, uri_str, uri_len);
			PRV_CONCAT_STR(buffer, buffer_len, head, LWM2M_LINK_ITEM_END, LWM2M_LINK_ITEM_END_SIZE);

			res = ext_serialize_attributes(handler, op_data, &uri, NULL, buffer, head - 1, buffer_len);
			if (res < 0)
			{
				return -1;
			}
			
			if (res == 0)
			{
				head = 0;
			}
			else
			{
				head += res;
			}
			
			for (index = 0; index < tlv->value.asChildren.count; index++)
			{
				res = ext_serialize_link_data(handler, op_data, tlv->value.asChildren.array + index, object_param, &uri, uri_str, uri_len, buffer + head, buffer_len - head);
				if (res < 0)
				{
					return -1;
				}
				head += res;
			}

			break;
		}
		case QAPI_NET_LWM2M_TYPE_OBJECT:
		default:
		{
			return -1;
		}
	}

	return head;
}

int ext_obj_discover_serialize
(
	qapi_Net_LWM2M_App_Handler_t handler,
	qapi_Net_LWM2M_Server_Data_t op_data,
	qapi_Net_LWM2M_Uri_t *uri,
	int size,
	qapi_Net_LWM2M_Flat_Data_t *data,
	uint8_t **buffer
)
{
  uint8_t buffer_link[PRV_LINK_BUFFER_SIZE];
  uint8_t base_uri_str[LWM2M_URI_MAX_STRING_LEN];
  int base_uri_len = 0;
  int index = 0;
  size_t head = 0;
  int res = 0;
  qapi_Net_LWM2M_Uri_t parent_uri;
  qapi_Net_LWM2M_Attributes_t *param;
  qapi_Net_LWM2M_Attributes_t merged_param;

  if(handler == NULL || uri == NULL || data == NULL || buffer == NULL ) 
  {
    IOT_DEBUG("Passed  Argument is NULL");
    return -1;
  }
  IOT_DEBUG("size: %d", size);

  memset(&parent_uri, 0, sizeof(qapi_Net_LWM2M_Uri_t));
  parent_uri.objectId = uri->objectId;
  parent_uri.flag = QAPI_NET_LWM2M_OBJECT_ID_E;

  if(uri->flag & QAPI_NET_LWM2M_RESOURCE_ID_E)
  {
    qapi_Net_LWM2M_Uri_t temp_uri;
    qapi_Net_LWM2M_Attributes_t *obj_param;
    qapi_Net_LWM2M_Attributes_t *inst_param;

    memset(&temp_uri, 0, sizeof(qapi_Net_LWM2M_Uri_t));
    temp_uri.objectId = uri->objectId;
    temp_uri.flag = QAPI_NET_LWM2M_OBJECT_ID_E;

    // get object level attributes
    obj_param = lwm2m_find_attributes(handler, op_data, &temp_uri);

    // get object instance level attributes
    temp_uri.instanceId = uri->instanceId;
    temp_uri.flag = QAPI_NET_LWM2M_INSTANCE_ID_E;
    inst_param = lwm2m_find_attributes(handler, op_data, &temp_uri);

    if(obj_param != NULL)
    {
      if(inst_param != NULL)
      {
        memset(&merged_param, 0, sizeof(qapi_Net_LWM2M_Attributes_t));
        merged_param.set_attr_mask = obj_param->set_attr_mask;
        merged_param.set_attr_mask |= inst_param->set_attr_mask;
        if(merged_param.set_attr_mask & QAPI_NET_LWM2M_MIN_PERIOD_E)
        {
          if(inst_param->set_attr_mask & QAPI_NET_LWM2M_MIN_PERIOD_E)
          {
            merged_param.minPeriod = inst_param->minPeriod;
          }
          else
          {
            merged_param.minPeriod = obj_param->minPeriod;
          }
        }
        if(merged_param.set_attr_mask & QAPI_NET_LWM2M_MAX_PERIOD_E)
        {
          if(inst_param->set_attr_mask & QAPI_NET_LWM2M_MAX_PERIOD_E)
          {
            merged_param.maxPeriod = inst_param->maxPeriod;
          }
          else
          {
            merged_param.maxPeriod = obj_param->maxPeriod;
          }
        }
        param = &merged_param;
      }
      else
      {
        param = obj_param;
      }
    }
    else
    {
      param = inst_param;
    }
    uri->flag &= ~QAPI_NET_LWM2M_RESOURCE_ID_E;
  }
  else
  {
    param = NULL;

    if(uri->flag & QAPI_NET_LWM2M_INSTANCE_ID_E)
    {
      PRV_CONCAT_STR(buffer_link, PRV_LINK_BUFFER_SIZE, head, LWM2M_LINK_ITEM_START, LWM2M_LINK_ITEM_START_SIZE);
      PRV_CONCAT_STR(buffer_link, PRV_LINK_BUFFER_SIZE, head, LWM2M_LINK_URI_SEPARATOR, LWM2M_LINK_URI_SEPARATOR_SIZE);

      res = utils_intToText(uri->objectId, buffer_link + head, PRV_LINK_BUFFER_SIZE - head);

      if(res <= 0)
      {
        return -1;
      }
      head += res;
      PRV_CONCAT_STR(buffer_link, PRV_LINK_BUFFER_SIZE, head, LWM2M_LINK_URI_SEPARATOR, LWM2M_LINK_URI_SEPARATOR_SIZE);

      res = utils_intToText(uri->instanceId, buffer_link + head, PRV_LINK_BUFFER_SIZE - head);
      if(res <= 0)
      {
        return -1;
      }
      head += res;
      PRV_CONCAT_STR(buffer_link, PRV_LINK_BUFFER_SIZE, head, LWM2M_LINK_ITEM_END, LWM2M_LINK_ITEM_END_SIZE);
      parent_uri.instanceId = uri->instanceId;
      parent_uri.flag = QAPI_NET_LWM2M_INSTANCE_ID_E;

      res = ext_serialize_attributes(handler, op_data, &parent_uri, NULL, buffer_link, head - 1, PRV_LINK_BUFFER_SIZE);
      if(res < 0)
      {
        return -1;
      }
      head += res;
    }
    else
    {
      PRV_CONCAT_STR(buffer_link, PRV_LINK_BUFFER_SIZE, head, LWM2M_LINK_ITEM_START, LWM2M_LINK_ITEM_START_SIZE);
      PRV_CONCAT_STR(buffer_link, PRV_LINK_BUFFER_SIZE, head, LWM2M_LINK_URI_SEPARATOR, LWM2M_LINK_URI_SEPARATOR_SIZE);

      res = utils_intToText(uri->objectId, buffer_link + head, PRV_LINK_BUFFER_SIZE - head);
      if(res <= 0)
      {
        return -1;
      }
      head += res;
      PRV_CONCAT_STR(buffer_link, PRV_LINK_BUFFER_SIZE, head, LWM2M_LINK_ITEM_END, LWM2M_LINK_ITEM_END_SIZE);

      res = ext_serialize_attributes(handler, op_data, &parent_uri, NULL, buffer_link, head - 1, PRV_LINK_BUFFER_SIZE);
      if(res < 0)
      {
        return -1;
      }
      head += res;
    }
  }

  base_uri_len = uri_toString(uri, base_uri_str, LWM2M_URI_MAX_STRING_LEN, NULL);
  if(base_uri_len < 0)
  {
    return -1;
  }
  base_uri_len -= 1;

  for(index = 0; index < size && head < PRV_LINK_BUFFER_SIZE; index++)
  {
    res = ext_serialize_link_data(handler, op_data, data + index, param, uri, base_uri_str, base_uri_len, buffer_link + head, PRV_LINK_BUFFER_SIZE - head);
    if(res < 0) return -1;
    head += res;
  }

  if (head > 0)
  {
    head -= 1;
    tx_byte_allocate(byte_pool_m2m, (VOID **)&(*buffer), 
                     head, TX_NO_WAIT);
    if(*buffer == NULL) return 0;
    memcpy(*buffer, buffer_link, head);
  }

  return (int)head;
}

/***********************************************************************************
                    lwm2m ext command handler or callback
************************************************************************************/
qapi_Status_t m2m_ext_object_callback_app
(
	qapi_Net_LWM2M_App_Handler_t  handler, 
	qapi_Net_LWM2M_Server_Data_t *lwm2m_srv_data,
	void                         *user_data
)
{
	ext_obj_cmd_t*        cmd_ptr = NULL;
	qapi_Net_LWM2M_Ext_t *ext_param = NULL;

	tx_byte_allocate(byte_pool_m2m, (VOID **)&ext_param, sizeof(qapi_Net_LWM2M_Ext_t), TX_NO_WAIT);

	if (ext_param == NULL)
	{
		IOT_DEBUG("Cannot assign memory for buffer");
		return QAPI_ERR_NO_MEMORY;
	}

	memset(ext_param, 0x00, sizeof(qapi_Net_LWM2M_Ext_t));

	tx_byte_allocate(byte_pool_m2m, (VOID **)&(ext_param->app_data.dl_op.lwm2m_srv_data), 
	sizeof(qapi_Net_LWM2M_Server_Data_t), TX_NO_WAIT);

	if (ext_param->app_data.dl_op.lwm2m_srv_data == NULL)
	{
		IOT_DEBUG("Cannot assign memory for buffer");
		return QAPI_ERR_NO_MEMORY;
	}

	memset(ext_param->app_data.dl_op.lwm2m_srv_data, 0x00, sizeof(qapi_Net_LWM2M_Server_Data_t));

	ext_param->app_data.dl_op.handler = handler;
	memcpy(ext_param->app_data.dl_op.lwm2m_srv_data, lwm2m_srv_data, sizeof(qapi_Net_LWM2M_Server_Data_t));

	if(lwm2m_srv_data->lwm2m_attr != NULL)
	{
		tx_byte_allocate(byte_pool_m2m, (VOID **)&(ext_param->app_data.dl_op.lwm2m_srv_data->lwm2m_attr), 
							sizeof(qapi_Net_LWM2M_Attributes_t), TX_NO_WAIT);

		if (ext_param->app_data.dl_op.lwm2m_srv_data->lwm2m_attr == NULL)
		{
			IOT_DEBUG("Cannot assign memory for buffer");
			return QAPI_ERR_NO_MEMORY;
		}

		memset(ext_param->app_data.dl_op.lwm2m_srv_data->lwm2m_attr, 0x00, sizeof(qapi_Net_LWM2M_Attributes_t));
		memcpy(ext_param->app_data.dl_op.lwm2m_srv_data->lwm2m_attr, lwm2m_srv_data->lwm2m_attr, sizeof(qapi_Net_LWM2M_Attributes_t));
	}

	ext_param->app_data.dl_op.data = user_data;

	IOT_DEBUG("Switching from QCLI to application callback context");

	if ((cmd_ptr = ext_obj_get_cmd_buffer()) == NULL)
	{
		IOT_DEBUG("Cannot obtain command buffer");
		return QAPI_ERR_NO_ENTRY;
	}
	else
	{
		cmd_ptr->cmd_hdr.cmd_id              = EXT_OBJ_CALL_BACK_EVT;
		cmd_ptr->cmd_data.data               = (void *)ext_param;

		if(app_reg_status == false)
		{
			IOT_DEBUG("Application not registerd ");
			if (ext_param->app_data.dl_op.lwm2m_srv_data->lwm2m_attr != NULL)
			{
				tx_byte_release(ext_param->app_data.dl_op.lwm2m_srv_data->lwm2m_attr);
			}

			tx_byte_release(ext_param->app_data.dl_op.lwm2m_srv_data);
			tx_byte_release(ext_param);
			return QAPI_ERROR;
		}

		IOT_DEBUG("Enqueue the command into the command queue. ");
		ext_obj_enqueue_cmd(cmd_ptr);
	}

	m2m_ext_obj_set_signal();

	return QAPI_OK;

}

/**
 * @fn     m2m_ext_object_register()
 *
 * @brief  This fucntion is used to register for Extensible object application.
 *
 * @param  void 
 * @return QAPI_OK     Success case
 *         QAPI_ERROR  Failure case
 */
qapi_Status_t m2m_ext_object_register(void)
{
	qapi_Status_t result = QAPI_ERROR;
	void *user_data = NULL;

    IOT_DEBUG("LWM2M_LOG: m2m_ext_object_register...\n");

	if (ext_object_handler_app != NULL)
	{
	  IOT_DEBUG("LWM2M_LOG: Extensible object application already registered.\n");
	  return result;
	}

	/* register lwm2m_ext application */
	result = qapi_Net_LWM2M_Register_App_Extended(&ext_object_handler_app, 
	                                              user_data,
	                                              m2m_ext_object_callback_app);
	if (result != QAPI_OK)
	{
		ext_object_handler_app = NULL;
		user_data = NULL;
		IOT_DEBUG("LWM2M_LOG: Error in Extensible object application registration.\n");
	}
	else
	{
		IOT_DEBUG("LWM2M_LOG: Extensible object application registered successfully.\n");
	}
    qapi_Net_LWM2M_Pass_Pool_Ptr(ext_object_handler_app, byte_pool_m2m);

	return result;
}

/*
 * @fn		m2m_ext_object_deregister()
 * @brief   This fucntion is used to deregister for Extensible object application
 */
qapi_Status_t m2m_ext_object_deregister(void)
{
  qapi_Status_t result = QAPI_ERROR;
  int i = 0;
  qapi_Net_LWM2M_Instance_Info_v2_t *instance = NULL;
  qapi_Net_LWM2M_Data_v2_t *object = NULL;

  if(ext_object_handler_app == NULL)
  {
	IOT_DEBUG("LWM2M_LOG: Extensible object application not registered.\n");
	return result;
  }
  
  result = qapi_Net_LWM2M_DeRegister_App(ext_object_handler_app);
  if (result == QAPI_OK)
  {
      ext_object_handler_app = NULL;

      object = lwm2m_object_find(object_generic, LWM2M_GENERIC_SENSOR_OBJECT_ID);
      if(object != NULL)
      {
        for(i =0; i< MAX_EXT_INSTANCE; i++)
        {
          if(obj_inst_created_generic[i] == 1)
          {
            instance = lwm2m_instance_find(object->instance_info, i);
            if(instance != NULL)
            {
              object->instance_info = lwm2m_instance_remove(object->instance_info, &instance, i);
              if(!qapi_Net_LWM2M_Free_Single_Instance_Info(instance))
              {
                IOT_DEBUG("Instance id %d cleared ", i);
                object->no_instances --;
                obj_inst_created_generic[i] = 0;
                instance = NULL;
              }
            }
          }
        }
      }
    
    IOT_DEBUG("LWM2M_LOG: Extensible object application de-registered successfully.\n");
  }
  else
  {
    IOT_DEBUG("LWM2M_LOG: Error in Extensible object application de-registration.\n");
  }

  return result;
}

/**
 * @fn     m2m_create_object()
 *
 * @brief  This fucntion is used to create object for Extensible object application.
 *
 * @return QAPI_OK     Success case
 *         QAPI_ERROR  Failure case
 */
qapi_Status_t m2m_create_object(void)
{
    qapi_Net_LWM2M_Data_v2_t *object = NULL;

	if (object_generic == NULL)
	{
		tx_byte_allocate(byte_pool_m2m, (VOID **)&object, sizeof(qapi_Net_LWM2M_Data_v2_t), TX_NO_WAIT);
		if (object == NULL)
		{
			IOT_DEBUG("Memory allocation failure. \n");
			return QAPI_ERR_NO_MEMORY;
		}

		memset(object, 0x00, sizeof(qapi_Net_LWM2M_Data_v2_t));

		object->object_ID = LWM2M_GENERIC_SENSOR_OBJECT_ID;		//1001 - sensor object
		object->next = NULL;
		object->no_instances = 0;
		object->instance_info = NULL;

		object_generic = lwm2m_object_add(object_generic, object);

		if(object_generic!= NULL)
		{
			IOT_DEBUG("Extensible object 1001 created. \n");
			return QAPI_OK;
		}
		else
		{
			IOT_DEBUG("Error in creation of Extensible object 1001. \n");
			return QAPI_ERROR;
		}
	}

    IOT_DEBUG("Extensible object 1001 already created.\n");
    return QAPI_ERROR;
}

qapi_Status_t m2m_create_object_instance_app(qapi_Net_LWM2M_Ext_t *obj_buffer)
{
	qapi_Status_t result = QAPI_ERROR;
	qapi_Net_LWM2M_Data_v2_t *object = NULL;
	qapi_Net_LWM2M_Instance_Info_v2_t *instance = NULL;
	qapi_Net_LWM2M_Data_v2_t lwm2m_data;
	qapi_Net_LWM2M_Instance_Info_v2_t lwm2m_inst;
	qapi_Net_LWM2M_Resource_Info_t lwm2m_res;
	uint32_t obj_id;

	memset(&lwm2m_inst, 0x00, sizeof(qapi_Net_LWM2M_Instance_Info_v2_t));
	memset(&lwm2m_res, 0x00, sizeof(qapi_Net_LWM2M_Resource_Info_t));

	if (ext_object_handler_app == NULL)
	{
		IOT_DEBUG("Application 1 not registerd Please register application ");
		return QAPI_ERROR;
	}
	
	if (object_generic == NULL)
	{
		IOT_DEBUG("Object is not valid.");
		return QAPI_ERROR;
	}

	if (obj_buffer->app_data.ul_op.obj_info.obj_mask & QAPI_NET_LWM2M_OBJECT_ID_E)
	{
		obj_id = obj_buffer->app_data.ul_op.obj_info.obj_id;
	}
	else
	{
		IOT_DEBUG("Object id is not valid.");
		return QAPI_ERR_INVALID_PARAM;
	}

	object = lwm2m_object_find(object_generic, obj_id);

	if (object == NULL)
	{
		IOT_DEBUG("Object not found.");
		return QAPI_ERR_NO_ENTRY;
	}

	memset(&lwm2m_data, 0x00, sizeof(qapi_Net_LWM2M_Data_v2_t));

	if (object->object_ID == LWM2M_GENERIC_SENSOR_OBJECT_ID)
	{
		instance = obj_create_generic_sensor(&lwm2m_data);
	}

	if (lwm2m_data.object_ID == LWM2M_GENERIC_SENSOR_OBJECT_ID && instance != NULL)
	{
		lwm2m_data.instance_info = &lwm2m_inst;
		lwm2m_data.instance_info->instance_ID = obj_inst_index_generic;
		lwm2m_data.instance_info->no_resources = instance->no_resources;
		lwm2m_data.instance_info->resource_info = &lwm2m_res;
	}

	if ((instance != NULL) && ext_object_handler_app != NULL)
	{
		result = qapi_Net_LWM2M_Create_Object_Instance_v2(ext_object_handler_app, &lwm2m_data);
	}

	else
	{
		IOT_DEBUG("Cannot create object instance");
		return QAPI_ERR_NO_MEMORY;
	}

	if (result == QAPI_OK)
	{
		IOT_DEBUG("Object instance created successfully.\n");
		object->instance_info = lwm2m_instance_add(object->instance_info, instance);
		if (object->instance_info && object->object_ID == LWM2M_GENERIC_SENSOR_OBJECT_ID)
		{
			obj_inst_created_generic[obj_inst_index_generic] = 1;
		}
	}

	else
	{
		qapi_Net_LWM2M_Free_Single_Instance_Info(instance);
		IOT_DEBUG("Object instance not created.Error is %d\n", result);
	}

	return result;
}  

qapi_Status_t m2m_delete_object_instance_app(qapi_Net_LWM2M_Ext_t *obj_buffer)
{
  qapi_Status_t result = QAPI_ERROR;
  qapi_Net_LWM2M_Obj_Info_t  obj_info;
  qapi_Net_LWM2M_Data_v2_t         *object = NULL;
  qapi_Net_LWM2M_Instance_Info_v2_t *instance = NULL;
  qapi_Net_LWM2M_Object_Info_v2_t instance_info;

  if(ext_object_handler_app == NULL)
  {
    IOT_DEBUG("Application not registerd Please register application ");
    return QAPI_ERROR;
  }
  
  if(object_generic == NULL)
  {
    IOT_DEBUG("Object is not valid.");
    return QAPI_ERROR;
  }
  
  obj_info = obj_buffer->app_data.ul_op.obj_info;
  memset(&instance_info, 0x00, sizeof(qapi_Net_LWM2M_Object_Info_v2_t));
  
  if((obj_info.obj_mask & QAPI_NET_LWM2M_OBJECT_ID_E))
  {
    if((obj_info.obj_mask & QAPI_NET_LWM2M_INSTANCE_ID_E))
    {
      if((obj_info.obj_mask & QAPI_NET_LWM2M_RESOURCE_ID_E))
      {
        IOT_DEBUG("Deletion on resource level not allowed \n ");
        return QAPI_ERROR;
      }
      else
      {
        object = lwm2m_object_find(object_generic, obj_info.obj_id);
        if(object != NULL)
        {
          instance = lwm2m_instance_find(object->instance_info, obj_info.obj_inst_id);
          if(instance != NULL)
          {
            instance_info.no_object_info = 1;
            tx_byte_allocate(byte_pool_m2m, (VOID **)&instance_info.id_info, sizeof(qapi_Net_LWM2M_Id_Info_v2_t), TX_NO_WAIT);
            if(instance_info.id_info == NULL)
            {
              return QAPI_ERR_NO_MEMORY;
            }
            memset(instance_info.id_info, 0x00, sizeof(qapi_Net_LWM2M_Id_Info_v2_t));
            instance_info.id_info->id_set |= QAPI_NET_LWM2M_OBJECT_ID_E;
            instance_info.id_info->id_set |= QAPI_NET_LWM2M_INSTANCE_ID_E;
            instance_info.id_info->object_ID = obj_info.obj_id;
            instance_info.id_info->instance_ID = obj_info.obj_inst_id;
  
            result = qapi_Net_LWM2M_Delete_Object_Instance_v2(ext_object_handler_app, &instance_info);
  
            if(instance_info.id_info)
            {
              tx_byte_release(instance_info.id_info);
              instance_info.id_info = NULL;
            }
            if(result == QAPI_OK)
            {
              object->instance_info = lwm2m_instance_remove(object->instance_info, &instance, obj_info.obj_inst_id);
              if(!qapi_Net_LWM2M_Free_Single_Instance_Info(instance))
              {
                IOT_DEBUG("Instance id %d cleared ", obj_info.obj_inst_id);
                object->no_instances --;
                if(object->object_ID == LWM2M_GENERIC_SENSOR_OBJECT_ID)
                {
                  obj_inst_created_generic[obj_info.obj_inst_id] = 0;
                }
                return QAPI_OK;
              }
            }
          }
          else
          {
            IOT_DEBUG("Instance id %d not valid", obj_info.obj_inst_id);
          }
        }
        else
        {
          IOT_DEBUG("Object id %d not found", obj_info.obj_id);
          return QAPI_ERR_NO_ENTRY;
        }
      }
    }
    else
    {
      IOT_DEBUG("Instance id is required.\n ");
      return QAPI_ERROR;
    }
  }
  
  IOT_DEBUG("Instance id not deleted");
  return result;
}

/*
 * @func
 * 		m2m_create_object_instance
 * @brief
 *		Create object and instance for lwm2m client
 */
qapi_Status_t m2m_create_object_instance
(
	qapi_Net_LWM2M_App_Handler_t handler, 
	qapi_Net_LWM2M_Server_Data_t op_data
)
{
	qapi_Status_t result = QAPI_ERROR;
	qapi_Net_LWM2M_Data_v2_t *object = NULL;
	qapi_Net_LWM2M_Instance_Info_v2_t *instance = NULL;
	qapi_Net_LWM2M_Data_v2_t lwm2m_data;
	qapi_Net_LWM2M_Instance_Info_v2_t lwm2m_inst;
	qapi_Net_LWM2M_Resource_Info_t lwm2m_res;
	uint32_t obj_id;
	qapi_Net_LWM2M_App_Ex_Obj_Data_t app_data;

	obj_id = op_data.obj_info.obj_id;

	memset(&lwm2m_inst, 0x00, sizeof(qapi_Net_LWM2M_Instance_Info_v2_t));
	memset(&lwm2m_res, 0x00, sizeof(qapi_Net_LWM2M_Resource_Info_t));
	memset(&lwm2m_data, 0x00, sizeof(qapi_Net_LWM2M_Data_v2_t));

	if(handler == NULL)
	{
		IOT_DEBUG("Application handle is not valid for current application");
		return QAPI_ERR_INVALID_PARAM;
	}

	if(handler == ext_object_handler_app)
	{
		object = lwm2m_object_find(object_generic, obj_id);
		if(object == NULL)
		{
		  IOT_DEBUG("Object not found.");
		  return QAPI_ERR_NO_ENTRY;
		}

		if(object->object_ID == LWM2M_GENERIC_SENSOR_OBJECT_ID)
		{
		  instance = obj_create_generic_sensor(&lwm2m_data);
		}

		if(lwm2m_data.object_ID == LWM2M_GENERIC_SENSOR_OBJECT_ID && instance != NULL)
		{
		  lwm2m_data.instance_info = &lwm2m_inst;
		  lwm2m_data.instance_info->instance_ID = obj_inst_index_generic;
		  lwm2m_data.instance_info->no_resources = instance->no_resources;
		  lwm2m_data.instance_info->resource_info = &lwm2m_res;
		}
	}	
	else
	{
		IOT_DEBUG("Object not found.");
		return QAPI_ERR_NO_ENTRY;
	}

	if(instance != NULL)
	{
		IOT_DEBUG("Object instance created successfully.\n");
	}
	else
	{
		IOT_DEBUG("Cannot create object instance");
		return QAPI_ERR_NO_MEMORY;
	}
	object->instance_info = lwm2m_instance_add(object->instance_info, instance);
	if(object->instance_info)
	{

	memset(&app_data, 0x00, sizeof(qapi_Net_LWM2M_App_Ex_Obj_Data_t));
	app_data.obj_info.obj_mask |= QAPI_NET_LWM2M_INSTANCE_ID_E;

	if(object->object_ID == LWM2M_GENERIC_SENSOR_OBJECT_ID)
	{
	  obj_inst_created_generic[obj_inst_index_generic] = 1;
	  app_data.obj_info.obj_inst_id = obj_inst_index_generic;
	}
	else if(object->object_ID == LWM2M_PRESENCE_SENSOR_OBJECT_ID)
	{
	  obj_inst_created_presence[obj_inst_index_presence] = 1;
	  app_data.obj_info.obj_inst_id = obj_inst_index_presence;
	}

	app_data.msg_type = QAPI_NET_LWM2M_CREATE_RESPONSE_MSG_E;
	app_data.obj_info.obj_id = obj_id;
	app_data.obj_info.obj_mask |= QAPI_NET_LWM2M_OBJECT_ID_E;
	app_data.status_code = QAPI_NET_LWM2M_201_CREATED_E;
	app_data.conf_msg = 0;
	app_data.msg_id_len = op_data.msg_id_len;
	memcpy(app_data.msg_id, op_data.msg_id, op_data.msg_id_len);

	qapi_Net_LWM2M_Send_Message(handler, &app_data);

	}

	if((instance != NULL))
	{
		result = qapi_Net_LWM2M_Create_Object_Instance_v2(handler, &lwm2m_data);
	}
	else
	{
		IOT_DEBUG("Cannot create object instance");
		return QAPI_ERR_NO_MEMORY;
	}

	return result;
}

qapi_Status_t delete_object_instance
(
	qapi_Net_LWM2M_App_Handler_t handler, 
	qapi_Net_LWM2M_Server_Data_t op_data
)
{
  qapi_Status_t result = QAPI_ERROR;
  qapi_Net_LWM2M_Obj_Info_t  obj_info;
  qapi_Net_LWM2M_Data_v2_t         *object = NULL;
  qapi_Net_LWM2M_Instance_Info_v2_t *instance = NULL;
  qapi_Net_LWM2M_Object_Info_v2_t instance_info;

  if(handler == NULL)
  {
    IOT_DEBUG("Application handle is not valid for current application");
    return QAPI_ERR_INVALID_PARAM;
  }

  obj_info = op_data.obj_info;
  memset(&instance_info, 0x00, sizeof(qapi_Net_LWM2M_Object_Info_v2_t));

  if((obj_info.obj_mask & QAPI_NET_LWM2M_OBJECT_ID_E))
  {
    if((obj_info.obj_mask & QAPI_NET_LWM2M_INSTANCE_ID_E))
    {
      if((obj_info.obj_mask & QAPI_NET_LWM2M_RESOURCE_ID_E))
      {
        IOT_DEBUG("Deletion on resource level not allowed \n ");
        send_response_message(handler, op_data, QAPI_NET_LWM2M_405_METHOD_NOT_ALLOWED_E);
        return QAPI_ERROR;
      }
      else
      {
        if(handler == ext_object_handler_app)
        {
          object = lwm2m_object_find(object_generic, obj_info.obj_id);
        }

        if(object != NULL)
        {
          instance = lwm2m_instance_find(object->instance_info, obj_info.obj_inst_id);
          if(instance != NULL)
          {
            instance_info.no_object_info = 1;
            tx_byte_allocate(byte_pool_m2m, (VOID **)&instance_info.id_info, sizeof(qapi_Net_LWM2M_Id_Info_v2_t), TX_NO_WAIT);
            if(instance_info.id_info == NULL)
            {
              send_response_message(handler, op_data, QAPI_NET_LWM2M_500_INTERNAL_SERVER_E);
              return QAPI_ERR_NO_MEMORY;
            }
            memset(instance_info.id_info, 0x00, sizeof(qapi_Net_LWM2M_Id_Info_v2_t));
            instance_info.id_info->id_set |= QAPI_NET_LWM2M_OBJECT_ID_E;
            instance_info.id_info->id_set |= QAPI_NET_LWM2M_INSTANCE_ID_E;
            instance_info.id_info->object_ID = obj_info.obj_id;
            instance_info.id_info->instance_ID = obj_info.obj_inst_id;

            object->instance_info = lwm2m_instance_remove(object->instance_info, &instance, obj_info.obj_inst_id);
            if(!qapi_Net_LWM2M_Free_Single_Instance_Info(instance))
            {
              IOT_DEBUG("Instance id %d cleared ", obj_info.obj_inst_id);
              object->no_instances --;
              if(object->object_ID == LWM2M_GENERIC_SENSOR_OBJECT_ID)
              {
                obj_inst_created_generic[obj_info.obj_inst_id] = 0;
              }
              else if(object->object_ID == LWM2M_PRESENCE_SENSOR_OBJECT_ID)
              {
                obj_inst_created_presence[obj_info.obj_inst_id] = 0;
              }
              send_response_message(handler, op_data, QAPI_NET_LWM2M_202_DELETED_E);
              result = QAPI_OK;
            }
            else
            {
              result = QAPI_ERROR;
            }

            if(result == QAPI_OK)
            {
              result = qapi_Net_LWM2M_Delete_Object_Instance_v2(handler, &instance_info);
            }

            if(instance_info.id_info)
            {
              tx_byte_release(instance_info.id_info);
              instance_info.id_info = NULL;
            }
            return result;
          }
          else
          {
            IOT_DEBUG("Instance id %d not valid", obj_info.obj_inst_id);
          }
        }
        else
        {
          IOT_DEBUG("Object id %d not found", obj_info.obj_id);
          send_response_message(handler, op_data, QAPI_NET_LWM2M_404_NOT_FOUND_E);
          return QAPI_ERR_NO_ENTRY;
        }
      }
    }
    else
    {
     if(ext_obj_delete_all(handler,op_data) == QAPI_OK)
      {
        send_response_message(handler, op_data, QAPI_NET_LWM2M_202_DELETED_E);
        return QAPI_OK;
      }
    }
  }
  IOT_DEBUG("Instance id not deleted");
  send_response_message(handler, op_data, QAPI_NET_LWM2M_400_BAD_REQUEST_E);
  return result;
}

qapi_Net_LWM2M_Instance_Info_v2_t *obj_create_generic_sensor(qapi_Net_LWM2M_Data_v2_t *lwm2m_data)
{
	int i = 0;
	qapi_Net_LWM2M_Data_v2_t *object = NULL;
	qapi_Net_LWM2M_Instance_Info_v2_t *instance = NULL;
	qapi_Net_LWM2M_Resource_Info_t *res1 = NULL;
	qapi_Net_LWM2M_Resource_Info_t *res2 = NULL;
	qapi_Net_LWM2M_Resource_Info_t *res3 = NULL;
	qapi_Net_LWM2M_Resource_Info_t *res4 = NULL;
	qapi_Net_LWM2M_Resource_Info_t *res5 = NULL;
	qapi_Net_LWM2M_Resource_Info_t *res6 = NULL;
	qapi_Net_LWM2M_Resource_Info_t *res7 = NULL;
	qapi_Net_LWM2M_Resource_Info_t *res8 = NULL;

	object = lwm2m_object_find(object_generic, LWM2M_GENERIC_SENSOR_OBJECT_ID);
	if (object == NULL)
	{
		IOT_DEBUG("Object not found.");
		return NULL;
	}

	for (i = 0; i < MAX_EXT_INSTANCE; i++)
	{
		if (obj_inst_created_generic[i] == 0)
		{
			obj_inst_index_generic = i;
			break;
		}
	}

	if (i == MAX_EXT_INSTANCE)
	{
		IOT_DEBUG("Maximum instances reached for object.");
		return NULL;
	}

	tx_byte_allocate(byte_pool_m2m, (VOID **)&instance, sizeof(qapi_Net_LWM2M_Instance_Info_v2_t), TX_NO_WAIT);
	if (instance == NULL)
	{
		IOT_DEBUG("Memory allocation failure.\n");
		return NULL;
	}

	memset(instance, 0x00, sizeof(qapi_Net_LWM2M_Instance_Info_v2_t));
	instance->instance_ID = obj_inst_index_generic;
	instance->next = NULL;
	
	instance->no_resources++;
	
	tx_byte_allocate(byte_pool_m2m, (VOID **)&instance->resource_info, sizeof(qapi_Net_LWM2M_Resource_Info_t), TX_NO_WAIT);
	if (instance->resource_info == NULL)
	{
		IOT_DEBUG("Memory allocation failure.\n");
		return NULL;
	}
	
	memset(instance->resource_info, 0x00, sizeof(qapi_Net_LWM2M_Resource_Info_t));
	instance->resource_info->resource_ID   		= RES_M_SENSOR_VALUE;
	instance->resource_info->type          		= QAPI_NET_LWM2M_TYPE_INTEGER_E;
	instance->resource_info->value.asInteger 	= 30;
	instance->resource_info->next          		= NULL;
	
	instance->no_resources++;
	
	tx_byte_allocate(byte_pool_m2m, (VOID **)&res1, sizeof(qapi_Net_LWM2M_Resource_Info_t), TX_NO_WAIT);
	if (res1 == NULL)
	{
		IOT_DEBUG("Memory allocation failure.\n");
		return NULL;
	}
	
	memset(res1, 0x00, sizeof(qapi_Net_LWM2M_Resource_Info_t));
	res1->resource_ID             = RES_O_SENSOR_UNITS;
	res1->type                    = QAPI_NET_LWM2M_TYPE_STRING_E;
	res1->value.asBuffer.length   = strlen("Celcius") + 1;
	
	tx_byte_allocate(byte_pool_m2m, (VOID **)&res1->value.asBuffer.buffer, res1->value.asBuffer.length, TX_NO_WAIT);
	if (res1->value.asBuffer.buffer == NULL)
	{
		IOT_DEBUG("Memory allocation failure.\n");
		return NULL;
	}
	
	memcpy(res1->value.asBuffer.buffer, "Celcius", res1->value.asBuffer.length);
	res1->next                    = NULL;
	instance->resource_info->next = res1;
	
	instance->no_resources++;
	
	tx_byte_allocate(byte_pool_m2m, (VOID **)&res2, sizeof(qapi_Net_LWM2M_Resource_Info_t), TX_NO_WAIT);
	if (res2 == NULL)
	{
		IOT_DEBUG("Memory allocation failure.\n");
		return NULL;
	}
	
	memset(res2, 0x00, sizeof(qapi_Net_LWM2M_Resource_Info_t));
	res2->resource_ID             = RES_O_MIN_MEASURED_VAL;
	res2->type                    = QAPI_NET_LWM2M_TYPE_FLOAT_E;
	res2->value.asFloat           = 10.0;
	res2->next                    = NULL;
	res1->next                    = res2;

	instance->no_resources++;
	
	tx_byte_allocate(byte_pool_m2m, (VOID **)&res3, sizeof(qapi_Net_LWM2M_Resource_Info_t), TX_NO_WAIT);
	if (res3 == NULL)
	{
	 	IOT_DEBUG("Memory allocation failure.\n");
	 	return NULL;
	}
	
	memset(res3, 0x00, sizeof(qapi_Net_LWM2M_Resource_Info_t));
	res3->resource_ID             = RES_O_MAX_MEASURED_VAL;
	res3->type                    = QAPI_NET_LWM2M_TYPE_FLOAT_E;
	res3->value.asFloat           = 40.0;
	res3->next                    = NULL;
	res2->next                    = res3;

	instance->no_resources++;

	tx_byte_allocate(byte_pool_m2m, (VOID **)&res4, sizeof(qapi_Net_LWM2M_Resource_Info_t), TX_NO_WAIT);
	if (res4 == NULL)
	{
		IOT_DEBUG("Memory allocation failure.\n");
		return NULL;
	}
	
	memset(res4, 0x00, sizeof(qapi_Net_LWM2M_Resource_Info_t));
	res4->resource_ID             = RES_M_MIN_RANGE_VAL;
	res4->type                    = QAPI_NET_LWM2M_TYPE_FLOAT_E;
	res4->value.asFloat           = 0.0;
	res4->next                    = NULL;
	res3->next                    = res4;

	instance->no_resources++;
	
	tx_byte_allocate(byte_pool_m2m, (VOID **)&res5, sizeof(qapi_Net_LWM2M_Resource_Info_t), TX_NO_WAIT);
	if (res5 == NULL)
	{
		IOT_DEBUG("Memory allocation failure.\n");
		return NULL;
	}
	
	memset(res5, 0x00, sizeof(qapi_Net_LWM2M_Resource_Info_t));
	res5->resource_ID             = RES_M_MAX_RANGE_VAL;
	res5->type                    = QAPI_NET_LWM2M_TYPE_FLOAT_E;
	res5->value.asFloat           = 50.0;
	res5->next                    = NULL;
	res4->next                    = res5;

	instance->no_resources++;
	
	tx_byte_allocate(byte_pool_m2m, (VOID **)&res6, sizeof(qapi_Net_LWM2M_Resource_Info_t), TX_NO_WAIT);
	if (res6 == NULL)
	{
		IOT_DEBUG("Memory allocation failure.\n");
		return NULL;
	}
	memset(res6, 0x00, sizeof(qapi_Net_LWM2M_Resource_Info_t));

	res6->resource_ID             = RES_O_RESET_MEASURED_VAL;
	res6->type                    = QAPI_NET_LWM2M_TYPE_OPAQUE_E;
	res6->next                    = NULL;
	res5->next                    = res6;

	instance->no_resources++;
	
	tx_byte_allocate(byte_pool_m2m, (VOID **)&res7, sizeof(qapi_Net_LWM2M_Resource_Info_t), TX_NO_WAIT);
	if (res7 == NULL)
	{
		IOT_DEBUG("Memory allocation failure.\n");
		return NULL;
	}
	
	memset(res7, 0x00, sizeof(qapi_Net_LWM2M_Resource_Info_t));
	res7->resource_ID             = RES_M_SENSOR_STATE;
	res7->type                    = QAPI_NET_LWM2M_TYPE_BOOLEAN_E;
	res7->value.asBoolean         = true;
	res7->next                    = NULL;
	res6->next                    = res7;

	instance->no_resources++;
	
	tx_byte_allocate(byte_pool_m2m, (VOID **)&res8, sizeof(qapi_Net_LWM2M_Resource_Info_t), TX_NO_WAIT);
	if (res8 == NULL)
	{
		IOT_DEBUG("Memory allocation failure.\n");
		return NULL;
	}
	
	memset(res8, 0x00, sizeof(qapi_Net_LWM2M_Resource_Info_t));
	res8->resource_ID             = RES_O_MEAN_VAL;
	res8->type                    = QAPI_NET_LWM2M_TYPE_INTEGER_E;
	res8->value.asInteger         = 25;
	res8->next                    = NULL;
	res7->next                    = res8;

	object->no_instances++;

	lwm2m_data->object_ID = LWM2M_GENERIC_SENSOR_OBJECT_ID;
	lwm2m_data->no_instances = 1;

	return instance;
}

qapi_Status_t ext_obj_delete_all
(
	qapi_Net_LWM2M_App_Handler_t handler, 
	qapi_Net_LWM2M_Server_Data_t op_data
)
{
  int i = 0;
  qapi_Net_LWM2M_Instance_Info_v2_t *instance = NULL;
  qapi_Net_LWM2M_Data_v2_t *object = NULL;
  qapi_Net_LWM2M_Object_Info_v2_t instance_info = {0};

  if(handler == ext_object_handler_app)
  {
    object = lwm2m_object_find(object_generic, LWM2M_GENERIC_SENSOR_OBJECT_ID);
    if(object != NULL)
    {
      for(i =0; i< MAX_EXT_INSTANCE; i++)
      {
        if(obj_inst_created_generic[i] == 1)
        {
          instance = lwm2m_instance_find(object->instance_info, i);
          if(instance != NULL)
          {
            object->instance_info = lwm2m_instance_remove(object->instance_info, &instance, i);
            if(!qapi_Net_LWM2M_Free_Single_Instance_Info(instance))
            {
              IOT_DEBUG("Instance id %d cleared ", i);
              object->no_instances --;
              obj_inst_created_generic[i] = 0;
              instance = NULL;

              instance_info.no_object_info = 1;
              tx_byte_allocate(byte_pool_m2m, (VOID **)&instance_info.id_info, sizeof(qapi_Net_LWM2M_Id_Info_v2_t), TX_NO_WAIT);
              if(instance_info.id_info == NULL)
              {
                return QAPI_ERR_NO_MEMORY;
              }
              memset(instance_info.id_info, 0x00, sizeof(qapi_Net_LWM2M_Id_Info_t));
              instance_info.id_info->id_set |= QAPI_NET_LWM2M_OBJECT_ID_E;
              instance_info.id_info->id_set |= QAPI_NET_LWM2M_INSTANCE_ID_E;
              instance_info.id_info->object_ID = 1001;
              instance_info.id_info->instance_ID = i;

              qapi_Net_LWM2M_Delete_Object_Instance_v2(handler, &instance_info);

              if(instance_info.id_info)
              {
                tx_byte_release(instance_info.id_info);
                instance_info.id_info = NULL;
              }
            }
          }
        }
      }
      IOT_DEBUG("LWM2M_LOG: Deleted all instances.\n");
      return QAPI_OK;
    }
  }
  
  return QAPI_ERROR;
}

qapi_Status_t ext_obj_handle_event
(
	qapi_Net_LWM2M_App_Handler_t handler, 
	qapi_Net_LWM2M_Server_Data_t op_data
)
{
  int i = 0;
  qapi_Net_LWM2M_Instance_Info_v2_t *instance = NULL;
  qapi_Net_LWM2M_Data_v2_t *object = NULL;

  if(handler == NULL)
  {
    IOT_DEBUG("Application handle is not valid for current application");
    return QAPI_ERR_INVALID_PARAM;
  }

  if(op_data.event == QAPI_NET_LWM2M_SLEEP_E)
  {
    lwm2m_client_in_sleep_state = true;
  }

  if(op_data.event == QAPI_NET_LWM2M_WAKEUP_E)
  {
    lwm2m_client_in_sleep_state = false;
  }

  if(op_data.event == QAPI_NET_LWM2M_DEVICE_FACTORY_RESET_E)
  {
    if(handler == ext_object_handler_app)
    {
      ext_object_handler_app = NULL;
      stop_ext_obs_notify_timer(handler);

      object = lwm2m_object_find(object_generic, LWM2M_GENERIC_SENSOR_OBJECT_ID);
      if(object != NULL)
      {
        for(i =0; i< MAX_EXT_INSTANCE; i++)
        {
          if(obj_inst_created_generic[i] == 1)
          {
            instance = lwm2m_instance_find(object->instance_info, i);
            if(instance != NULL)
            {
              object->instance_info = lwm2m_instance_remove(object->instance_info, &instance, i);
              if(!qapi_Net_LWM2M_Free_Single_Instance_Info(instance))
              {
                IOT_DEBUG("Instance id %d cleared ", i);
                object->no_instances --;
                obj_inst_created_generic[i] = 0;
                instance = NULL;
              }
            }
          }
        }
      }
    }

    IOT_DEBUG("LWM2M_LOG: Extensible object application de-registered successfully.\n");
  }

  if(op_data.event)
  {
    ext_obj_event = op_data.event;
    return QAPI_OK;
  }

  return QAPI_ERROR;
}

/****************************************************************************************
                               Lwm2m all commands interface
*****************************************************************************************/

/**
 * @fn	   lwm2m_ext_object_init()
 *
 * @brief  This fucntion is used to create Extensible object application task, all requested
 *         lwm2m commands will be processed in task loop.
 *
 * @return QAPI_OK	   Success case
 *		   QAPI_ERROR  Failure case
 */
qapi_Status_t lwm2m_ext_object_init(void)
{
	int ret = -1;
	
	ext_obj_cmd_t* cmd_ptr = NULL;
    qapi_Net_LWM2M_Ext_t *ext_param = NULL;
	char *ext_thread_stack_pointer = NULL;
	
    /* Allocate the stack for extensible object application */
    tx_byte_allocate(byte_pool_m2m, (VOID **) &ext_thread_stack_pointer, 8192, TX_NO_WAIT);
    
    /* Create the extensible object application thread.  */
	if (TX_SUCCESS != txm_module_object_allocate((VOID *)&m2m_thread_hdl, sizeof(TX_THREAD))) 
	{
		return -1;
	}

    ret = tx_thread_create(m2m_thread_hdl, "ext_object", m2m_ext_obj_task_entry,
                           0, ext_thread_stack_pointer, 8192,
                           145, 145, TX_NO_TIME_SLICE, TX_AUTO_START);

    if (ret != TX_SUCCESS)
    {
		IOT_DEBUG("Failed to create thread for command (%d).\n", ret);
		return QAPI_ERROR;
    }
	else
	{
		app_reg_status = true;
	}
	
    qapi_Timer_Sleep(3, QAPI_TIMER_UNIT_SEC, true);

	tx_byte_allocate(byte_pool_m2m, (VOID **)&ext_param, sizeof(qapi_Net_LWM2M_Ext_t), TX_NO_WAIT);

	if (ext_param == NULL)
	{
		IOT_DEBUG("Cannot assign memory for extensible object parameter");
		return QAPI_ERROR;
	}

	memset(ext_param, 0x00, sizeof(qapi_Net_LWM2M_Ext_Obj_t));
	ext_param->app_data.ul_op.msg_type = QAPI_NET_LWM2M_EXT_REGISTER_APP_E;

	IOT_DEBUG("application register status - %d", app_reg_status);

	if ((cmd_ptr = ext_obj_get_cmd_buffer()) == NULL)
	{
		IOT_DEBUG("Cannot obtain command buffer");
		return QAPI_ERROR;
	}
	else
	{
		cmd_ptr->cmd_hdr.cmd_id	= EXT_OBJ_APP_ORIGINATED_EVT;
		cmd_ptr->cmd_data.data	= (void *)ext_param;

		if (app_reg_status == false)
		{
			IOT_DEBUG("Application not registerd");
			tx_byte_release(ext_param);
			return QAPI_OK;
		}
		
		IOT_DEBUG("Enqueue the command into the command queue. ");
		ext_obj_enqueue_cmd(cmd_ptr);
	}

	m2m_ext_obj_set_signal();

	return QAPI_OK;
}


qapi_Status_t lwm2m_create_ext_obj(void)
{
	ext_obj_cmd_t* cmd_ptr = NULL;
	qapi_Net_LWM2M_Ext_t *ext_param = NULL;

	tx_byte_allocate(byte_pool_m2m, (VOID **)&ext_param, sizeof(qapi_Net_LWM2M_Ext_t), TX_NO_WAIT);

	if (ext_param == NULL)
	{
		IOT_DEBUG("Cannot assign memory for extensible object parameter");
		return QAPI_ERROR;
	}

	memset(ext_param, 0x00, sizeof(qapi_Net_LWM2M_Ext_Obj_t));
	ext_param->app_data.ul_op.msg_type = QAPI_NET_LWM2M_EXT_CREATE_OBJ_E;

	if ((cmd_ptr = ext_obj_get_cmd_buffer()) == NULL)
	{
		IOT_DEBUG("Cannot obtain command buffer");
		return QAPI_ERROR;
	}
	else
	{
		cmd_ptr->cmd_hdr.cmd_id	= EXT_OBJ_APP_ORIGINATED_EVT;
		cmd_ptr->cmd_data.data	= (void *)ext_param;

		if (app_reg_status == false)
		{
			IOT_DEBUG("Application not registerd");
			tx_byte_release(ext_param);
			return QAPI_OK;
		}

		IOT_DEBUG("Enqueue the command into the command queue. ");
		ext_obj_enqueue_cmd(cmd_ptr);
	}

	m2m_ext_obj_set_signal();

	return QAPI_OK;
}

qapi_Status_t lwm2m_create_ext_obj_inst(void)
{
	ext_obj_cmd_t* cmd_ptr = NULL;
	qapi_Net_LWM2M_Ext_t *ext_param = NULL;

	tx_byte_allocate(byte_pool_m2m, (VOID **)&ext_param, sizeof(qapi_Net_LWM2M_Ext_t), TX_NO_WAIT);
	if (ext_param == NULL)
	{
		IOT_DEBUG("Cannot assign memory for extensible object parameter");
		return QAPI_ERROR;
	}

	memset(ext_param, 0x00, sizeof(qapi_Net_LWM2M_Ext_Obj_t));
	ext_param->app_data.ul_op.msg_type = QAPI_NET_LWM2M_EXT_CREATE_OBJ_INSTANCE_E;
	ext_param->app_data.ul_op.obj_info.obj_mask |= QAPI_NET_LWM2M_OBJECT_ID_E;
	ext_param->app_data.ul_op.obj_info.obj_id = LWM2M_GENERIC_SENSOR_OBJECT_ID;
	
    if ((cmd_ptr = ext_obj_get_cmd_buffer()) == NULL)
    {
		IOT_DEBUG("Cannot obtain command buffer");
		return QAPI_ERROR;
    }
    else
	{
		cmd_ptr->cmd_hdr.cmd_id	= EXT_OBJ_APP_ORIGINATED_EVT;
		cmd_ptr->cmd_data.data	= (void *)ext_param;

		if (app_reg_status == false)
		{
			IOT_DEBUG("Application not registerd ");
			tx_byte_release(ext_param);
			return QAPI_ERROR;
		}
		
		IOT_DEBUG("Enqueue the command into the command queue. ");
		ext_obj_enqueue_cmd(cmd_ptr);
	}
  
    m2m_ext_obj_set_signal();
    return QAPI_OK;
}

qapi_Status_t lwm2m_delete_ext_obj_inst(int obj_id, int inst_id)
{
	ext_obj_cmd_t* cmd_ptr = NULL;
	qapi_Net_LWM2M_Ext_t *ext_param = NULL;

	tx_byte_allocate(byte_pool_m2m, (VOID **)&ext_param, sizeof(qapi_Net_LWM2M_Ext_t), TX_NO_WAIT);
	if (ext_param == NULL)
	{
		IOT_DEBUG("Cannot assign memory for extensible object parameter");
		return QAPI_ERROR;
	}

	memset(ext_param, 0x00, sizeof(qapi_Net_LWM2M_Ext_Obj_t));

	if (obj_id != 0)
	{
		tx_byte_release(ext_param);
	}
  
	ext_param->app_data.ul_op.msg_type = QAPI_NET_LWM2M_EXT_DELETE_OBJ_INSTANCE_E;
	ext_param->app_data.ul_op.obj_info.obj_mask |= QAPI_NET_LWM2M_OBJECT_ID_E;
	ext_param->app_data.ul_op.obj_info.obj_mask |= QAPI_NET_LWM2M_INSTANCE_ID_E;
	ext_param->app_data.ul_op.obj_info.obj_id = LWM2M_GENERIC_SENSOR_OBJECT_ID;
	ext_param->app_data.ul_op.obj_info.obj_inst_id = inst_id;

	if ((cmd_ptr = ext_obj_get_cmd_buffer()) == NULL)
	{
		IOT_DEBUG("Cannot obtain command buffer");
		return QAPI_OK;
	}
	else
	{
		cmd_ptr->cmd_hdr.cmd_id	= EXT_OBJ_APP_ORIGINATED_EVT;
		cmd_ptr->cmd_data.data	= (void *)ext_param;

		if (app_reg_status == false)
		{
			IOT_DEBUG("Application not registerd ");
			tx_byte_release(ext_param);
			return QAPI_OK;
		}
		
		IOT_DEBUG("Enqueue the command into the command queue. ");
		ext_obj_enqueue_cmd(cmd_ptr);
	}

	m2m_ext_obj_set_signal();
	return QAPI_OK;
}

qapi_Status_t lwm2m_ext_read(int obj_id, int inst_id, int res_id)
{
	qapi_Net_LWM2M_Data_v2_t *object = NULL;
	qapi_Net_LWM2M_Instance_Info_v2_t *instance = NULL;
	qapi_Net_LWM2M_Instance_Info_v2_t *instance_start = NULL;
	qapi_Net_LWM2M_Resource_Info_t *resource = NULL;
	int i;
	uint16_t resList1[] = {
							RES_M_SENSOR_VALUE,
							RES_O_SENSOR_UNITS,
							RES_O_MIN_MEASURED_VAL,
							RES_O_MAX_MEASURED_VAL,
							RES_M_MIN_RANGE_VAL,
							RES_M_MAX_RANGE_VAL,
							RES_M_SENSOR_STATE,
							RES_O_MEAN_VAL
							};
	uint16_t resList2[] = {
							RES_M_DIG_INPUT_STATE,
							RES_O_DIG_INPUT_COUNTER,
							RES_O_SENSOR_TYPE,
							RES_O_BUSY_TO_CLEAR_DELAY,
							RES_O_CLEAR_TO_BUSY_DELAY,
						   	};
	
	int nRes;

 

	if (ext_object_handler_app == NULL)
	{
		IOT_DEBUG("Application not registered.\n");
		return QAPI_ERROR;
	}

	if (object_generic == NULL)
	{
		IOT_DEBUG("Object not created for application.\n");
		return QAPI_ERROR;
	}

	obj_id = LWM2M_GENERIC_SENSOR_OBJECT_ID;
	object = lwm2m_object_find(object_generic, obj_id);
 
	if (object == NULL)
	{
		IOT_DEBUG("Not a valid object id.\n");
		return QAPI_OK;
	}

  	if (inst_id != -1)
	{
		instance = lwm2m_instance_find(object->instance_info, inst_id);
		if (instance == NULL)
		{
			IOT_DEBUG("Not a valid instance id.\n");
			return QAPI_OK;
		}

		if (res_id != -1)
		{
			resource = lwm2m_resource_find(instance->resource_info, res_id);
			if (resource == NULL)
			{
				IOT_DEBUG("Not a valid resource id.\n");
				return QAPI_OK;
			}

			if (lwm2m_check_readable(obj_id, res_id) == false)
			{
				IOT_DEBUG("Read on resource %d not allowed", res_id);
				return QAPI_OK;
			}

			IOT_DEBUG("Read on / %d / %d / %d", obj_id, inst_id, res_id);
			
			switch(resource->type)
			{
				case QAPI_NET_LWM2M_TYPE_STRING_E:
				{
					IOT_DEBUG("Resource %d : Value %s \n",resource->resource_ID, resource->value.asBuffer.buffer);
					IOT_DEBUG("Resource %d : Value %s \n",resource->resource_ID, resource->value.asBuffer.buffer);
					break;
				}				
				case QAPI_NET_LWM2M_TYPE_INTEGER_E:
				{
					IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, (int)resource->value.asInteger);
					IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, (int)resource->value.asInteger);
					break;
				}	
				case QAPI_NET_LWM2M_TYPE_FLOAT_E:
				{
					IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, (int)resource->value.asFloat);
					IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, (int)resource->value.asFloat);
					break;
				}	
				case QAPI_NET_LWM2M_TYPE_BOOLEAN_E:
				{
					IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, resource->value.asBoolean);
					IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, resource->value.asBoolean);
					break;
				}
				default:
					break;
			}

			return QAPI_OK;
		}
		else
		{
			IOT_DEBUG("Read on / %d / %d ", obj_id, inst_id);

			if (obj_id == LWM2M_GENERIC_SENSOR_OBJECT_ID)
			{
				nRes = sizeof(resList1)/sizeof(uint16_t);

				for (i = 0; i<nRes; i++)
				{
					resource = lwm2m_resource_find(instance->resource_info, resList1[i]);
					if (resource == NULL)
					{
						IOT_DEBUG("Not a valid resource id.\n");
						return QAPI_OK;
					}

					switch (resource->type)
					{
						case QAPI_NET_LWM2M_TYPE_STRING_E:
						{
							IOT_DEBUG("Resource %d : Value %s \n",resource->resource_ID, resource->value.asBuffer.buffer);
							IOT_DEBUG("Resource %d : Value %s \n",resource->resource_ID, resource->value.asBuffer.buffer);
							break;
						}
						case QAPI_NET_LWM2M_TYPE_INTEGER_E:
						{
							IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, (int)resource->value.asInteger);
							IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, (int)resource->value.asInteger);
							break;
						}
						case QAPI_NET_LWM2M_TYPE_FLOAT_E:
						{
							IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, (int)resource->value.asFloat);
							IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, (int)resource->value.asFloat);
							break;
						}
						case QAPI_NET_LWM2M_TYPE_BOOLEAN_E:
						{
							IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, resource->value.asBoolean);
							IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, resource->value.asBoolean);
							break;
						}
						default:
							break;
					}
				}
				
				return QAPI_OK;
			}
			else
			{
				nRes = sizeof(resList2)/sizeof(uint16_t);

				for (i = 0; i<nRes; i++)
				{
					resource = lwm2m_resource_find(instance->resource_info, resList2[i]);
					if (resource == NULL)
					{
						IOT_DEBUG("Not a valid resource id.\n");
						return QAPI_OK;
					}

					switch(resource->type)
					{
						case QAPI_NET_LWM2M_TYPE_STRING_E:
						{
							IOT_DEBUG("Resource %d : Value %s \n",resource->resource_ID, resource->value.asBuffer.buffer);
							IOT_DEBUG("Resource %d : Value %s \n",resource->resource_ID, resource->value.asBuffer.buffer);
							break;
						}
						case QAPI_NET_LWM2M_TYPE_INTEGER_E:
						{
							IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, (int)resource->value.asInteger);
							IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, (int)resource->value.asInteger);
							break;
						}
						case QAPI_NET_LWM2M_TYPE_FLOAT_E:
						{
							IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, (int)resource->value.asFloat);
							IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, (int)resource->value.asFloat);
							break;
						}
						case QAPI_NET_LWM2M_TYPE_BOOLEAN_E:
						{
							IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, resource->value.asBoolean);
							IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, resource->value.asBoolean);
							break;
						}
						default:
							break;
					}
				}
				
				return QAPI_OK;
			}
		}
	}
  	else
	{
		instance_start = object->instance_info;
		while(object->instance_info != NULL)
		{
			instance = lwm2m_instance_find(object->instance_info, object->instance_info->instance_ID);
			if (instance == NULL)
			{
				IOT_DEBUG("Not a valid instance id.\n");
				return QAPI_OK;
			}

			IOT_DEBUG("Read on / %d / %d ", obj_id, inst_id);

			if (obj_id == LWM2M_GENERIC_SENSOR_OBJECT_ID)
			{
				nRes = sizeof(resList1)/sizeof(uint16_t);

				for(i = 0; i<nRes; i++)
				{
					resource = lwm2m_resource_find(instance->resource_info, resList1[i]);
					if (resource == NULL)
					{
						IOT_DEBUG("Not a valid resource id.\n");
						return QAPI_OK;
					}

					switch (resource->type)
					{
						case QAPI_NET_LWM2M_TYPE_STRING_E:
						{
							IOT_DEBUG("Resource %d : Value %s \n",resource->resource_ID, resource->value.asBuffer.buffer);
							IOT_DEBUG("Resource %d : Value %s \n",resource->resource_ID, resource->value.asBuffer.buffer);
							break;
						}
						case QAPI_NET_LWM2M_TYPE_INTEGER_E:
						{
							IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, (int)resource->value.asInteger);
							IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, (int)resource->value.asInteger);
							break;
						}
						case QAPI_NET_LWM2M_TYPE_FLOAT_E:
						{
							IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, (int)resource->value.asFloat);
							IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, (int)resource->value.asFloat);
							break;
						}
						case QAPI_NET_LWM2M_TYPE_BOOLEAN_E:
						{
							IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, resource->value.asBoolean);
							IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, resource->value.asBoolean);
							break;
						}
						default:
							break;
					}
				}
			}
			else
			{
				nRes = sizeof(resList2)/sizeof(uint16_t);

				for (i = 0; i<nRes; i++)
				{
					resource = lwm2m_resource_find(instance->resource_info, resList2[i]);
					if (resource == NULL)
					{
						IOT_DEBUG("Not a valid resource id.\n");
						return QAPI_OK;
					}

					switch (resource->type)
					{
						case QAPI_NET_LWM2M_TYPE_STRING_E:
						{
							IOT_DEBUG("Resource %d : Value %s \n",resource->resource_ID, resource->value.asBuffer.buffer);
							IOT_DEBUG("Resource %d : Value %s \n",resource->resource_ID, resource->value.asBuffer.buffer);
							break;
						}
						case QAPI_NET_LWM2M_TYPE_INTEGER_E:
						{
							IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, (int)resource->value.asInteger);
							IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, (int)resource->value.asInteger);
							break;
						}
						case QAPI_NET_LWM2M_TYPE_FLOAT_E:
						{
							IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, (int)resource->value.asFloat);
							IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, (int)resource->value.asFloat);
							break;
						}
						case QAPI_NET_LWM2M_TYPE_BOOLEAN_E:
						{
							IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, resource->value.asBoolean);
							IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, resource->value.asBoolean);
							break;
						}
						default:
							break;
					}
				}
			}
			object->instance_info = object->instance_info->next;
		}
		
		object->instance_info = instance_start;
		return QAPI_OK;
	}
}

qapi_Status_t lwm2m_ext_write(int obj_id, int inst_id, int res_id, char *value)
{
	qapi_Net_LWM2M_Data_v2_t *object = NULL;
	qapi_Net_LWM2M_Instance_Info_v2_t *instance = NULL;
	qapi_Net_LWM2M_Resource_Info_t *resource = NULL;
	char str[255];

    if (ext_object_handler_app == NULL)
    {
		IOT_DEBUG("Application not registered.\n");
		return QAPI_ERROR;
    }
	
    if (object_generic == NULL)
    {
		IOT_DEBUG("Object not created for application.\n");
		return QAPI_ERROR;
    }
	
    obj_id = LWM2M_GENERIC_SENSOR_OBJECT_ID;
    object = lwm2m_object_find(object_generic, obj_id);

	strlcpy(str, value, sizeof(str));
	
	if (object == NULL)
	{
		IOT_DEBUG("Object not found.\n");
		return QAPI_OK;
	}

	instance = lwm2m_instance_find(object->instance_info, inst_id);
	if(instance == NULL)
	{
		IOT_DEBUG("Not a valid instance id.\n");
		return QAPI_OK;
	}

	resource = lwm2m_resource_find(instance->resource_info, res_id);
	if(resource == NULL)
	{
		IOT_DEBUG("Not a valid resource id.\n");
		return QAPI_OK;
	}

	if (check_writable(obj_id, res_id) == false)
	{
		IOT_DEBUG("Write on resource %d not allowed", res_id);
		return QAPI_OK;
	}

	IOT_DEBUG("Write on / %d / %d / %d", obj_id, inst_id, res_id);
	switch(resource->type)
	{
		case QAPI_NET_LWM2M_TYPE_STRING_E:
		{
			memcpy(resource->value.asBuffer.buffer, value, strlen((char*)value));
			IOT_DEBUG("Resource %d : Value %s \n",resource->resource_ID, resource->value.asBuffer.buffer);
			IOT_DEBUG("Resource %d : Value %s \n",resource->resource_ID, resource->value.asBuffer.buffer);
			break;
		}
		case QAPI_NET_LWM2M_TYPE_INTEGER_E:
		{
			resource->value.asInteger = (int64_t)*value;
			IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, (int)resource->value.asInteger);
			IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, (int)resource->value.asInteger);
			break;
		}
		case QAPI_NET_LWM2M_TYPE_FLOAT_E:
		{
			resource->value.asFloat = (double)*value;
			IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID,(int) resource->value.asFloat);
			IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID,(int) resource->value.asFloat);
			break;
		}
		case QAPI_NET_LWM2M_TYPE_BOOLEAN_E:
		{
			resource->value.asBoolean = (bool)*value;
			IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, resource->value.asBoolean);
			IOT_DEBUG("Resource %d : Value %d \n",resource->resource_ID, resource->value.asBoolean);
			break;
		}
		default:
			break;
	}

	return QAPI_OK;
}

qapi_Status_t lwm2m_ext_exec(int obj_id, int inst_id, int res_id)
{
	qapi_Net_LWM2M_Data_v2_t *object = NULL;
	qapi_Net_LWM2M_Instance_Info_v2_t *instance = NULL;
	qapi_Net_LWM2M_Resource_Info_t *resource = NULL;
	qapi_Net_LWM2M_Resource_Info_t *resource1 = NULL;
	qapi_Net_LWM2M_Resource_Info_t *resource2 = NULL;
	qapi_Net_LWM2M_Resource_Info_t *resource3 = NULL;
	qapi_Net_LWM2M_Resource_Info_t *resource4 = NULL;

	if (ext_object_handler_app == NULL)
	{
		IOT_DEBUG("Application not registered.\n");
		return QAPI_ERROR;
	}

	if (object_generic == NULL)
	{
		IOT_DEBUG("Object not created for application.\n");
		return QAPI_ERROR;
	}

	obj_id = LWM2M_GENERIC_SENSOR_OBJECT_ID;
	object = lwm2m_object_find(object_generic, obj_id);

	if (object == NULL)
	{
		IOT_DEBUG("Not a valid object id.\n");
		return QAPI_OK;
	}

	instance = lwm2m_instance_find(object->instance_info, inst_id);
	if (instance == NULL)
	{
		IOT_DEBUG("Not a valid instance id.\n");
		return QAPI_OK;
	}

	resource = lwm2m_resource_find(instance->resource_info, res_id);
	if (resource == NULL)
	{
		IOT_DEBUG("Not a valid resource id.\n");
		return QAPI_OK;
	}

	if (check_executable(obj_id,res_id) == false)
	{
		IOT_DEBUG("Execute on resource %d not allowed", res_id);
		return QAPI_OK;
	}

	IOT_DEBUG("Execute on / %d / %d / %d", obj_id, inst_id, res_id);

	if (obj_id == LWM2M_GENERIC_SENSOR_OBJECT_ID)
	{
		resource1 = lwm2m_resource_find(instance->resource_info, RES_O_MIN_MEASURED_VAL);
		resource2 = lwm2m_resource_find(instance->resource_info, RES_M_MIN_RANGE_VAL);
		resource3 = lwm2m_resource_find(instance->resource_info, RES_O_MAX_MEASURED_VAL);
		resource4 = lwm2m_resource_find(instance->resource_info, RES_M_MAX_RANGE_VAL);
		
		if (resource1 != NULL && resource2 != NULL)
		{
			resource1->value.asFloat = resource2->value.asFloat;
		}
		
		if (resource3 != NULL && resource4 != NULL)
		{
			resource3->value.asFloat = resource4->value.asFloat;
		}
	}
	else
	{
		resource1 = lwm2m_resource_find(instance->resource_info, RES_O_DIG_INPUT_COUNTER);
		if (resource1 != NULL)
		{
			resource1->value.asInteger = 0;
		}
	}
	return QAPI_OK;
}

void lwm2m_display_template(void)
{ 
	IOT_DEBUG("Object id is %d \n", LWM2M_GENERIC_SENSOR_OBJECT_ID);
	IOT_DEBUG("Object is multi-instance \n");
	IOT_DEBUG("Res_id   Res_name 				  Res_instances  Mandatory");
	IOT_DEBUG("	Type	Operation Description\n");
	IOT_DEBUG("0 	   Sensor value 			  Single		 Yes	  ");
	IOT_DEBUG("	Integer R		  Last or current measured sensor value\n");
	IOT_DEBUG("1 	   Sensor units 			  Single		 No 	  ");
	IOT_DEBUG("	String	R		  Measurement unit definition e.g. Celcius for tempwerature.\n");
	IOT_DEBUG("2 	   Min measured value		  Single		 No 	  ");
	IOT_DEBUG("	Float	R		  Minimum value measured since power on or reset \n");
	IOT_DEBUG("3 	   Max measured value		  Single		 No 	  ");
	IOT_DEBUG("	Float	R		  Maximum value measured since power on or reset \n");
	IOT_DEBUG("4 	   Min range value			  Single		 Yes	  ");
	IOT_DEBUG("	Float	R/W 	  Minimum threshold value. \n");
	IOT_DEBUG("5 	   Max range value			  Single		 Yes	  ");
	IOT_DEBUG("	Float	R/W 	  Maximum threshold value. \n");
	IOT_DEBUG("6  Reset min and max mesaured value Single		 No 	  ");
	IOT_DEBUG("	Opaque	E		  Reset minimum and maximum measured value. \n");
	IOT_DEBUG("7 	   Sensor state 			  Single		 Yes	  ");
	IOT_DEBUG("	Boolean R/W 	  Sensor state on/off.. \n");
	IOT_DEBUG("8 	   Mean value				  Single		 No 	  ");
	IOT_DEBUG("  Integer R		   Mean measured value. \n");
}

qapi_Status_t lwm2m_deinit_ext_obj(void)
{
	ext_obj_cmd_t* cmd_ptr = NULL;
	qapi_Net_LWM2M_Ext_t *ext_param = NULL;


	tx_byte_allocate(byte_pool_m2m, (VOID **)&ext_param, sizeof(qapi_Net_LWM2M_Ext_t), TX_NO_WAIT);
	if (ext_param == NULL)
	{
		IOT_DEBUG("Cannot assign memory for extensible object parameter");
		return QAPI_ERROR;
	}

	memset(ext_param, 0x00, sizeof(qapi_Net_LWM2M_Ext_Obj_t));
	ext_param->app_data.ul_op.msg_type = QAPI_NET_LWM2M_EXT_DEREGISTER_APP_E;

	if((cmd_ptr = ext_obj_get_cmd_buffer()) == NULL)
	{
		IOT_DEBUG("Cannot obtain command buffer");
		return QAPI_ERROR;
	}
	else
	{
		cmd_ptr->cmd_hdr.cmd_id 			 = EXT_OBJ_APP_ORIGINATED_EVT;
		cmd_ptr->cmd_data.data				 = (void *)ext_param;

		if (app_reg_status == false)
		{
			IOT_DEBUG("Application not registerd ");
			tx_byte_release(ext_param);
			return QAPI_OK;
		}
		
		IOT_DEBUG("Enqueue the command into the command queue. ");
		ext_obj_enqueue_cmd(cmd_ptr);
	}

	m2m_ext_obj_set_signal();

	return QAPI_OK;
}

/* @func 
 * 		quectel_task_entry
 * @brief 
 *		initiate lwm2m_ext task and process related signals
 */
int quectel_task_entry(void)
{
    uint32 m2m_event = 0;
	int result = 0;

    /* extensible lwm2m object must be initiated after lwm2m client connection */
    qapi_Timer_Sleep(30, QAPI_TIMER_UNIT_SEC, true);

	/* create a memory pool */
    txm_module_object_allocate(&byte_pool_m2m, sizeof(TX_BYTE_POOL));
	tx_byte_pool_create(byte_pool_m2m, "lwm2m_mem_pool", m2m_free_mem, BYTE_POOL_SIZE);

	/* Initial uart for debug */
//	m2m_uart_dbg_init();

    IOT_DEBUG("extessible LwM2M client task start....\n");
    txm_module_object_allocate(&m2m_signal_handle, sizeof(TX_EVENT_FLAGS_GROUP));
    tx_event_flags_create(m2m_signal_handle, "dss_signal_event");
	tx_event_flags_set(m2m_signal_handle, 0x0, TX_AND);

    qapi_Timer_Sleep(3, QAPI_TIMER_UNIT_SEC, true);

	/* Creat extensible m2m task for processing ext object command */
	lwm2m_ext_object_init();

	/* display exmple object template */
	lwm2m_display_template();
	
	/* create extensible object */
	m2m_create_object();

	/* Create instance for extensible object */
	lwm2m_create_ext_obj_inst();

	qapi_Timer_Sleep(3, QAPI_TIMER_UNIT_SEC, true);

	/* Read resouce of object */
	lwm2m_ext_read(LWM2M_GENERIC_SENSOR_OBJECT_ID, 0, 1);
	
	while(1)
    {
	   /* Process extessible lwm2m client task signal */
       tx_event_flags_get(m2m_signal_handle, M2M_EXT_TASK_EXIT, TX_OR, &m2m_event, TX_WAIT_FOREVER);

	   if (m2m_event & M2M_EXT_TASK_EXIT)
	   {
	   		IOT_DEBUG("extessible LwM2M client task exit.");
			tx_event_flags_set(m2m_signal_handle, ~M2M_EXT_TASK_EXIT, TX_AND);
			break;
	   }
    }
    return result;
}
#endif /* __EXAMPLE_LWM2M_EXT__ */
