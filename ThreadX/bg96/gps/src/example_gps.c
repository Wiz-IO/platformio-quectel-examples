/******************************************************************************
*@file    example_gps.c
*@brief   example of gps operation
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


/**************************************************************************
*                                 GLOBAL
***************************************************************************/

/*==========================================================================
  Signals used for waiting in test app for callbacks
===========================================================================*/
static TX_EVENT_FLAGS_GROUP* gps_signal_handle; 
static uint32 gps_tracking_id;

#define LOCATION_TRACKING_FIXED_BIT (0x1 << 0)
#define LOCATION_TRACKING_RSP_OK_BIT (0x1 << 1)
#define LOCATION_TRACKING_RSP_FAIL_BIT (0x1 << 2)
qapi_Location_t location;

static char tx_buff[4096];
QT_UART_CONF_PARA uart1_conf =
{
	NULL,
	QT_UART_PORT_02,
	tx_buff,
	sizeof(tx_buff),
	NULL,
	0,
	115200
};
#define GPS_USER_BUFFER_SIZE 4096
static uint8_t GPSuserBuffer[GPS_USER_BUFFER_SIZE];
qapi_loc_client_id loc_clnt;
/**************************************************************************
*                                 FUNCTION
***************************************************************************/

/*==========================================================================
LOCATION API REGISTERED CALLBACKS
===========================================================================*/
static void location_capabilities_callback(qapi_Location_Capabilities_Mask_t capabilities)
{

    qt_uart_dbg(uart1_conf.hdlr,"Location mod has tracking capability:%d\n",capabilities);
}

static void location_response_callback(qapi_Location_Error_t err, uint32_t id)
{
    qt_uart_dbg(uart1_conf.hdlr,"LOCATION RESPONSE CALLBACK err=%u id=%u", err, id); 
	if(err == QAPI_LOCATION_ERROR_SUCCESS)
	{
		tx_event_flags_set(gps_signal_handle, LOCATION_TRACKING_RSP_OK_BIT, TX_OR);	
	}
	else
	{
		tx_event_flags_set(gps_signal_handle, LOCATION_TRACKING_RSP_FAIL_BIT, TX_OR);	
	}
}
static void location_collective_response_cb(size_t count, qapi_Location_Error_t* err, uint32_t* ids)
{
    
}

static void location_tracking_callback(qapi_Location_t loc)
{
    if(loc.flags & QAPI_LOCATION_HAS_LAT_LONG_BIT) 
    {
        memcpy(&location,&loc,sizeof(qapi_Location_t));
        tx_event_flags_set(gps_signal_handle, LOCATION_TRACKING_FIXED_BIT, TX_OR);
    }    
}

qapi_Location_Callbacks_t location_callbacks= {
    sizeof(qapi_Location_Callbacks_t),
    location_capabilities_callback,
    location_response_callback,
    location_collective_response_cb,
    location_tracking_callback,
    NULL,
    NULL,
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
    ret = (qapi_Location_Error_t)qapi_Loc_Set_User_Buffer(loc_clnt, (uint8_t*)GPSuserBuffer, GPS_USER_BUFFER_SIZE);
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

int quectel_task_entry(void)
{
    uint32 signal = 0;
    uint32 count = 0;
    qapi_Location_Options_t Location_Options = {sizeof(qapi_Location_Options_t),
                                                1000,//update lat/lon frequency
                                                0};
    txm_module_object_allocate(&gps_signal_handle, sizeof(TX_EVENT_FLAGS_GROUP));
	qapi_Timer_Sleep(1000, QAPI_TIMER_UNIT_MSEC, true);//need delay 1s, otherwise maybe there is no callback.
	uart_init(&uart1_conf);
    qt_uart_dbg(uart1_conf.hdlr,"QT# quectel_task_entry ~~~");  
	
    location_init();
    qapi_Loc_Start_Tracking(loc_clnt, &Location_Options, &gps_tracking_id);
	tx_event_flags_get(gps_signal_handle, LOCATION_TRACKING_RSP_OK_BIT|LOCATION_TRACKING_RSP_FAIL_BIT, TX_OR_CLEAR, &signal, TX_WAIT_FOREVER);
	if(signal&LOCATION_TRACKING_RSP_OK_BIT)
	{
		qt_uart_dbg(uart1_conf.hdlr,"wating for tracking...");
	}
	else if(signal&LOCATION_TRACKING_RSP_FAIL_BIT)
	{
		qt_uart_dbg(uart1_conf.hdlr,"start tracking failed");
		return -1;
	}
	while(1)
	{
        tx_event_flags_get(gps_signal_handle, LOCATION_TRACKING_FIXED_BIT, TX_OR_CLEAR, &signal, TX_WAIT_FOREVER);
        if(signal & LOCATION_TRACKING_FIXED_BIT)
        {
            qt_uart_dbg(uart1_conf.hdlr,"LAT: %d.%d LON: %d.%d ALT: %d.%d ACC: %d.%d",
            (int)location.latitude, ((int)(location.latitude*100000))%100000,
            (int)location.longitude, ((int)(location.longitude*100000))%100000,
            (int)location.altitude, ((int)(location.altitude*100))%100,
            (int)location.accuracy, ((int)(location.accuracy*100))%100);

            qt_uart_dbg(uart1_conf.hdlr,"SPEED: %d.%d BEAR: %d.%d TIME: %llu FLAGS: %x",
            (int)location.speed, ((int)(location.speed*100))%100,
            (int)location.bearing, ((int)(location.bearing*100))%100,location.timestamp,
            location.flags);    
            count++;
        }
        if(count > 10)
        {
             qt_uart_dbg(uart1_conf.hdlr,"Stop gps sucsess");
             location_deinit();
             break;
        }

    }
	return 0;
}

#endif /*__EXAMPLE_GPS__*/

