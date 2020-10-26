/******************************************************************************
*@file    example_gps.c
*@brief   example of gps
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#if defined(__EXAMPLE_GPS__)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stdarg.h"
#include "qapi_fs_types.h"
#include "qapi_uart.h"
#include "qapi_timer.h"
#include "qapi_diag.h"

#include "quectel_utils.h"
#include "qapi_location.h"
#include "txm_module.h"
#include "quectel_uart_apis.h"


#define GPS_BYTE_POOL_SIZE		16*8*1024
UCHAR free_memory_gps[GPS_BYTE_POOL_SIZE];
TX_BYTE_POOL *byte_pool_gps_task;
#define GPS_THREAD_PRIORITY   	180
#define GPS_THREAD_STACK_SIZE 	(1024 * 16)
TX_THREAD* GPS_thread_handle; 
char *GPS_thread_stack = NULL;

/**************************************************************************
*                                 GLOBAL
***************************************************************************/

/*==========================================================================
  Signals used for waiting in test app for callbacks
===========================================================================*/
typedef unsigned int	size_t;
typedef unsigned int 	uint32_t;


static TX_EVENT_FLAGS_GROUP* gps_signal_handle; 
static uint32 gps_tracking_id;

#define LOCATION_TRACKING_FIXED_BIT (0x1 << 0)
#define LOCATION_TRACKING_RSP_OK_BIT (0x1 << 1)
#define LOCATION_TRACKING_RSP_FAIL_BIT (0x1 << 2)
#define LOCATION_NMEA_RSP_OK_BIT (0x1 << 3)



qapi_Location_t location;

static char tx_buff[4096];
QT_UART_CONF_PARA uart1_conf =
{
	NULL,
	QT_QAPI_UART_PORT_01,
	tx_buff,
	sizeof(tx_buff),
	NULL,
	0,
	115200
};
#define GPS_USER_BUFFER_SIZE 4096
static uint8 GPSuserBuffer[GPS_USER_BUFFER_SIZE];
qapi_loc_client_id loc_clnt;

#define LOC_API_MAX_NMEA_STRING_LENGTH            200
/*8(GSV)+8(GSV)+3(GSA)+1(RMC)+1(GGA)+1(VTG)+2(Reseve)*/
#define LOC_API_MAX_GPS_NMEA_NUM            	  (16+3+1+1+2)


char nmeasrc_sentences[LOC_API_MAX_NMEA_STRING_LENGTH*LOC_API_MAX_GPS_NMEA_NUM];



typedef struct cb_info
{
  char                    *nmea;
  long               	   length;
  struct cb_info          *pNext;
} cb_info;

static long PushTailNode( cb_info **PHead, char *nmea, long length)
{
  cb_info *pNode = NULL,*pNode1 = NULL; 
  int ret = -1;
  if(PHead == NULL)
  {
    return FALSE;
  }
  ret = tx_byte_allocate(byte_pool_gps_task,(VOID *)&pNode, sizeof(cb_info), TX_NO_WAIT);
  if(ret == TX_SUCCESS)
  {
  		pNode->nmea      = nmea;
  		pNode->length    = length;
  		pNode->pNext     = NULL;

  		if (*PHead == NULL)
  		{
    		*PHead = pNode; 
  		}
  		else
  		{
    		pNode1 = *PHead;
    		while(pNode1->pNext != NULL)
    		{
      		pNode1 = pNode1->pNext;
    		}
    		pNode1->pNext = pNode;
  		}
  	}
    else
    {
    	return FALSE;
    }
	
  return TRUE;
}
static int PopHeadNode(cb_info **pHead, cb_info *pNode)
{
  cb_info *pTnode=NULL,*pTnode1=NULL;

  if((NULL == pNode) || (NULL == pHead))
  {
    return FALSE;
  }
  pTnode = *pHead;
  if (NULL != pTnode)
  {
    if(pTnode->nmea == NULL)
   	{
	 tx_byte_release(pTnode);                                 //this condition never appear but need to take care 
   	 return FALSE;
   	}
    pNode->nmea      = pTnode->nmea;
	pNode->length      = pTnode->length;
   	pNode->pNext     = NULL;
	if(pTnode->pNext !=  NULL)
	{
	  pTnode1 = pTnode->pNext;
	  *pHead = pTnode1;
	}
	else
	{
	 *pHead = NULL;
	}
	
	tx_byte_release(pTnode);
	
	pTnode = NULL;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}


cb_info*head = NULL;



/**************************************************************************
*                                 FUNCTION
***************************************************************************/

/*==========================================================================
LOCATION API REGISTERED CALLBACKS
===========================================================================*/

static void loc_gnss_nmea_callback(qapi_Gnss_Nmea_t gnssNmea)
{
	char *nmeatem = NULL;
	int ret = -1;
	
	ret = tx_byte_allocate(byte_pool_gps_task,(VOID **)&nmeatem, (gnssNmea.length+1), TX_NO_WAIT);

	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_allocate [rx_buff] failed, %d", ret);
		qt_uart_dbg(uart1_conf.hdlr,"tx_byte_allocate [rx_buff] failed, %d", ret);
	}
	else
	{
		memset(nmeatem, 0, gnssNmea.length+1);
	    memcpy(nmeatem,gnssNmea.nmea,gnssNmea.length);
	}	
	PushTailNode(&head,nmeatem,gnssNmea.length+1);
	tx_event_flags_set(gps_signal_handle, LOCATION_NMEA_RSP_OK_BIT, TX_OR);
}




static void location_capabilities_callback(qapi_Location_Capabilities_Mask_t capabilities)
{

    qt_uart_dbg(uart1_conf.hdlr,"Location mod has tracking capability:%d\n",capabilities);
}

static void location_response_callback(qapi_Location_Error_t err, uint32_t id)
{
    qt_uart_dbg(uart1_conf.hdlr,"LOCATION RESPONSE CALLBACK err=%u id=%u", err, id); 
}
static void location_collective_response_cb(size_t count, qapi_Location_Error_t* err, uint32_t* ids)
{
    
}

static void location_tracking_callback(qapi_Location_t location)
{
  qt_uart_dbg(uart1_conf.hdlr,"LAT: %d.%d LON: %d.%d ACC: %d.%d",
        (int)location.latitude, (abs((int)(location.latitude * 100000))) % 100000,
        (int)location.longitude, (abs((int)(location.longitude * 100000))) % 100000,
        (int)location.accuracy, (abs((int)(location.accuracy * 100))) % 100);
}

qapi_Location_Callbacks_t location_callbacks= {
    sizeof(qapi_Location_Callbacks_t),
    location_capabilities_callback,
    location_response_callback,
    location_collective_response_cb,
    location_tracking_callback,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    loc_gnss_nmea_callback,
    NULL
};

/*==========================================================================
LOCATION INIT / DEINIT APIs
===========================================================================*/
void location_init(void)
{
    qapi_Location_Error_t ret;

    tx_event_flags_create(gps_signal_handle, "ts_gps_app_events");

    ret = qapi_Loc_Init(&loc_clnt, &location_callbacks);
    if (ret == QAPI_LOCATION_ERROR_SUCCESS)
    {
        qt_uart_dbg(uart1_conf.hdlr,"LOCATION_INIT SUCCESS");
    }
    else
    {
        qt_uart_dbg(uart1_conf.hdlr,"LOCATION_INIT FAILED");
        
    }
    ret = (qapi_Location_Error_t)qapi_Loc_Set_User_Buffer(loc_clnt, (uint8*)GPSuserBuffer, GPS_USER_BUFFER_SIZE);
    if (ret != QAPI_LOCATION_ERROR_SUCCESS) {
        qt_uart_dbg(uart1_conf.hdlr,"Set user buffer failed ! (ret %d)", ret);
    }
}

void location_deinit(void)
{

    qapi_Loc_Stop_Tracking(loc_clnt, gps_tracking_id);
    qapi_Loc_Deinit(loc_clnt);
    tx_event_flags_delete(gps_signal_handle);
}
int quectel_gps_task_entry(void)
{
	uint32 signal = 0;
	uint32 count = 0;
	cb_info Node;
	int ret = -1;
		
	qapi_Location_Options_t Location_Options = {sizeof(qapi_Location_Options_t),
												1000,//update lat/lon frequency;Not currently supported 500ms,modify 1000ms
												0};
	txm_module_object_allocate(&gps_signal_handle, sizeof(TX_EVENT_FLAGS_GROUP));
	qapi_Timer_Sleep(1000, QAPI_TIMER_UNIT_MSEC, true);//need delay 1s, otherwise maybe there is no callback.
	uart_init(&uart1_conf);
	qt_uart_dbg(uart1_conf.hdlr,"QT# quectel_task_entry ~~~");	

	location_init();
	qapi_Loc_Start_Tracking(loc_clnt, &Location_Options, &gps_tracking_id);

	while(1)
	{
		tx_event_flags_get(gps_signal_handle, LOCATION_NMEA_RSP_OK_BIT, TX_OR_CLEAR, &signal, TX_WAIT_FOREVER);
		if(signal & LOCATION_NMEA_RSP_OK_BIT)
		{
			PopHeadNode(&head,&Node);
			qapi_UART_Transmit(uart1_conf.hdlr, Node.nmea, (Node.length - 1), NULL);
			tx_byte_release(Node.nmea);
			count++;
		}
	}
	
	return 0;
}

int quectel_task_entry(void)
{
	int ret = -1;

	ret = txm_module_object_allocate(&byte_pool_gps_task, sizeof(TX_BYTE_POOL));
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("txm_module_object_allocate [byte_pool_sensor] failed, %d", ret);
		return ret;
	}

	ret = tx_byte_pool_create(byte_pool_gps_task, "Sensor application pool", free_memory_gps, GPS_BYTE_POOL_SIZE);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_pool_create [byte_pool_sensor] failed, %d", ret);
		return ret;
	}

	ret = tx_byte_allocate(byte_pool_gps_task, (VOID *)&GPS_thread_stack, GPS_THREAD_STACK_SIZE, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_allocate [rx_buff] failed, %d", ret);
		return ret;
	}

	if(TX_SUCCESS != txm_module_object_allocate((VOID *)&GPS_thread_handle, sizeof(TX_THREAD))) 
	{
		IOT_INFO("[task] txm_module_object_allocate qt_sub1_thread_handle failed");
		return - 1;
	}

	/* create a new task : quectel_gps_task_entry */
	ret = tx_thread_create(GPS_thread_handle,
						   "TIMER Task Thread",
						   quectel_gps_task_entry,
						   NULL,
						   GPS_thread_stack,
						   GPS_THREAD_STACK_SIZE,
						   GPS_THREAD_PRIORITY,
						   GPS_THREAD_PRIORITY,
						   TX_NO_TIME_SLICE,
						   TX_AUTO_START
						   );
		  
	if(ret != TX_SUCCESS)
	{
		IOT_INFO("[task] Thread creation failed");
	}

	return 0;

}



#endif /*__EXAMPLE_GPS__*/

