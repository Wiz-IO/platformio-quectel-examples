/******************************************************************************
*@file    example_network.h
*@brief   example of network operation
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2019 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#ifndef __EXAMPLE_NETWORK_H__
#define __EXAMPLE_NETWORK_H__

#if defined(__EXAMPLE_NETWORK__)

#define QT_DS_PROFILE_MAX_APN_STRING_LEN (101)
#define QT_DS_PROFILE_MAX_USERNAME_LEN (128)
#define QT_DS_PROFILE_MAX_PASSWORD_LEN (128)

typedef enum {
	QT_NW_CFUN_MIN_FUNC = 0,  
	QT_NW_CFUN_FUNN_FUNC = 1,
	QT_NW_CFUN_SHUT_DOWN = 2,
	QT_NW_CFUN_RESET = 3,
	QT_NW_CFUN_FTM = 4,

	QT_NW_CFUN_MAX
} qapi_QT_NW_CFUN_MODE_e;

typedef enum {
	QT_NW_PREF_GSM = 0,
	QT_NW_PREF_CATM = 1,
	QT_NW_PREF_GSM_CATM = 2,
	QT_NW_PREF_CATNB = 3,
	QT_NW_PREF_GSM_CATNB = 4,
	QT_NW_PREF_CATM_CATNB = 5,
	QT_NW_PREF_GSM_CATM_CATNB = 6,
	QT_NW_PREF_RAT_MAX
}qapi_QT_NW_RAT_PREF_e;

typedef enum {
	QT_NW_PREF_SCAN_CATM_CATNB_GSM = 0,
	QT_NW_PREF_SCAN_CATM_GSM_CATNB = 1,
	QT_NW_PREF_SCAN_CATNB_CATM_GSM = 2,
	QT_NW_PREF_SCAN_CATNB_GSM_CATM = 3,
	QT_NW_PREF_SCAN_GSM_CATM_CATNB = 4,
	QT_NW_PREF_SCAN_GSM_CATNB_CATM = 5,
	QT_NW_PREF_RAT_SCAN_ORDER_MAX
}qapi_QT_NW_RAT_SCAN_ORDER_e;

typedef enum {
	QT_NW_PREF_CS_ONLY = 0,
	QT_NW_PREF_PS_ONLY = 1,
	QT_NW_PREF_CS_PS = 2,
	QT_NW_PREF_SRV_DOMAIN_MAX
} qapi_QT_NW_SRV_DOMAIN_PREF_e;

typedef enum {
	QT_NW_GSM_BAND_EGSM = 0,
	QT_NW_GSM_BAND_PGSM = 1,
	QT_NW_GSM_BAND_PCS_1900 = 2,
	QT_NW_GSM_BAND_DCS_1800 = 3,
	QT_NW_GSM_BAND_CELL_850 = 4,
	QT_NW_GSM_BAND_MAX
}qapi_QT_GSM_BAND_e;

typedef enum
{
	QT_NW_DS_PROFILE_PDP_IPV4 = 1,
	QT_NW_DS_PROFILE_PDP_IPV6 = 2,
	QT_NW_DS_PROFILE_PDP_IPV4V6 = 3,
	QT_NW_DS_PROFILE_PDP_MAX
}qapi_QT_NW_DS_PROFILE_PDP_TYPE_e;

typedef enum {
	QT_NW_DS_PROFILE_AUTH_PAP = 0,
	QT_NW_DS_PROFILE_AUTH_CHAP = 1,
	QT_NW_DS_PROFILE_AUTH_PAP_CHAP = 2,
	QT_NW_DS_PROFILE_AUTH_TYPE_MAX
}qapi_QT_NW_DS_PROFILE_AUTH_TYPE_e;

typedef struct {
	unsigned char gsm_band;
	unsigned long long catm_band_low;
	unsigned long long nb_band_low;
}qapi_QT_NW_Band_Params_t;

typedef enum {
	QT_NW_EMTC = 0,
	QT_NW_NB_IOT = 1,
}qapi_QT_NW_RAT_e;

typedef struct {
	unsigned short year;
	unsigned char month;
	unsigned char day;
	unsigned char hour;
	unsigned char minute;
	unsigned char second;
	unsigned char time_zone;
}qapi_QT_Real_Time_Cfg_Params_t;

typedef struct {
	unsigned short arfcn;
	unsigned short mcc;
	unsigned short mnc;
	unsigned short lac;
	unsigned long cell_id;
	qapi_QT_GSM_BAND_e band;
	unsigned char bsic;
	unsigned char rxlev;
	unsigned short drx;
	unsigned long c1;
	unsigned long c2;
}qapi_QT_NW_GSM_Meas_Info_t;

typedef struct {
	unsigned long earfcn;
	unsigned short mcc;
	unsigned short mnc;
	unsigned short tac;
	unsigned long cell_id;
	unsigned char freq_band;
	unsigned short pci;
	unsigned short rsrp;
	unsigned short rsrq;
	unsigned short rssi;
	unsigned short sinr;
}qapi_QT_NW_LTE_Meas_Info_t;

typedef struct {
	qapi_QT_NW_DS_PROFILE_PDP_TYPE_e pdp_type;
	qapi_QT_NW_DS_PROFILE_AUTH_TYPE_e auth_type;
	unsigned char apn[QT_DS_PROFILE_MAX_APN_STRING_LEN + 1];
	unsigned char user_name[QT_DS_PROFILE_MAX_USERNAME_LEN + 1];
	unsigned char pass_word[QT_DS_PROFILE_MAX_PASSWORD_LEN + 1];
}qapi_QT_NW_DS_Profile_PDP_Context_t;

typedef struct {
	unsigned char req_psm_enable;
	unsigned long req_active_timer_value;
	unsigned long req_periodic_tau_timer_value;
}qapi_QT_NW_Req_PSM_Cfg_t;

typedef struct {
	unsigned char alloc_psm_enabled;
	unsigned long alloc_active_timer_value;
	unsigned long alloc_periodic_tau_timer_value;
}qapi_QT_NW_Alloc_PSM_Cfg_t;

typedef struct {
	unsigned char req_edrx_enable;
	qapi_QT_NW_RAT_e rat_mode;
	unsigned char req_ptw_cycle;
	unsigned char req_edrx_cycle;
}qapi_QT_NW_Req_eDRX_Cfg_t;

typedef struct {
	unsigned char alloc_edrx_enable;
	unsigned char alloc_ptw_cycle;
	unsigned char alloc_edrx_cycle;
}qapi_QT_NW_Alloc_eDRX_Cfg_t;


#endif /*__EXAMPLE_NETWORK__*/

#endif /*__EXAMPLE_NETWORK_H__*/

