/******************************************************************************
*@file    example_rtc.h
*@brief   example of rtc operation
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#ifndef __QUECTEL_RTC_H__
#define __QUECTEL_RTC_H__

#if defined(__EXAMPLE_RTC__)
void qt_rtc_init(void);
void qt_rtc_show_time(qapi_PM_Rtc_Julian_Type_t *rtc_date);
void qt_rtc_get_time(qapi_PM_Rtc_Julian_Type_t *rtc_date);
void qt_rtc_rw_alarm_time(qapi_PM_Rtc_Cmd_Type_t cmd,
					 qapi_PM_Rtc_Alarm_Type_t what_alarm,
					 qapi_PM_Rtc_Julian_Type_t *rtc_date);
#endif /*__EXAMPLE_RTC__*/
#endif /*__QUECTEL_RTC_H__*/

