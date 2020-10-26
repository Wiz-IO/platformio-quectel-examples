/******************************************************************************
*@file    example_network.c
*@brief   example of network operation
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2019 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#if defined(__EXAMPLE_NETWORK__)
/**************************************************************************
*                                 INCLUDE
***************************************************************************/
#include <stdbool.h>
#include "qapi_uart.h"
#include "qapi_diag.h"

#include "quectel_utils.h"
#include "qapi_timer.h"
#include "example_network.h"
#include "qapi_quectel.h"
#include "quectel_uart_apis.h"
#include <locale.h>

/**************************************************************************
*                                 FUNCTION
***************************************************************************/
TX_BYTE_POOL *byte_pool_nw;
#define NW_BYTE_POOL_SIZE		16*1024
UCHAR free_memory_nw[NW_BYTE_POOL_SIZE];

static unsigned long nw_task_run_couter = 0;

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

static void tcp_uart_rx_cb(uint32_t num_bytes, void *cb_data)
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


static void tcp_uart_tx_cb(uint32_t num_bytes, void *cb_data)
{
	IOT_DEBUG("QT# uart_tx_cb, send [%d]", num_bytes);
}

void network_uart_dbg_init()
{
	qapi_Status_t status;
	qapi_UART_Open_Config_t uart_cfg;
	
  	if (TX_SUCCESS != tx_byte_allocate(byte_pool_nw, (VOID *)&uart_rx_buff,4*1024, TX_NO_WAIT))
  	{
  		IOT_DEBUG("tx_byte_allocate [uart_rx_buff] failed!");
    	return;
  	}

  	if (TX_SUCCESS != tx_byte_allocate(byte_pool_nw, (VOID *)&uart_tx_buff, 4*1024, TX_NO_WAIT))
  	{
  		IOT_DEBUG("tx_byte_allocate [uart_tx_buff] failed!");
    	return;
  	}

    uart_conf.rx_buff = uart_rx_buff;
	uart_conf.tx_buff = uart_tx_buff;
	uart_conf.tx_len = 4096;
	uart_conf.rx_len = 4096;

	/* debug uart init 			*/
	uart_cfg.baud_Rate			= 115200;
	uart_cfg.enable_Flow_Ctrl	= QAPI_FCTL_OFF_E;
	uart_cfg.bits_Per_Char		= QAPI_UART_8_BITS_PER_CHAR_E;
	uart_cfg.enable_Loopback	= 0;
	uart_cfg.num_Stop_Bits		= QAPI_UART_1_0_STOP_BITS_E;
	uart_cfg.parity_Mode		= QAPI_UART_NO_PARITY_E;
	uart_cfg.rx_CB_ISR			= (qapi_UART_Callback_Fn_t)&tcp_uart_rx_cb;
	uart_cfg.tx_CB_ISR			= (qapi_UART_Callback_Fn_t)&tcp_uart_tx_cb;

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

void nw_uart_debug_print(const char* fmt, ...) 
{
	va_list arg_list;
    char dbg_buffer[128] = {0};
    
	va_start(arg_list, fmt);
    vsnprintf((char *)(dbg_buffer), sizeof(dbg_buffer), (char *)fmt, arg_list);
    va_end(arg_list);
		
    qapi_UART_Transmit(uart_conf.hdlr, dbg_buffer, strlen(dbg_buffer), NULL);
    //qapi_Timer_Sleep(50, QAPI_TIMER_UNIT_MSEC, true);   //Do not add delay unless necessary
}

/*
@func
  quectel_task_entry
@brief
  Entry function for task. 
*/
int quectel_task_entry(void)
{
	int ret = -1;
	qapi_Status_t status;
	qapi_QT_NW_CFUN_MODE_e fun = 0;
	qapi_QT_NW_Band_Params_t band_pref;
	unsigned long long catm_band_h, nb_band_h;
	qapi_QT_NW_RAT_PREF_e mode;
	qapi_QT_NW_RAT_SCAN_ORDER_e order;
	qapi_QT_NW_SRV_DOMAIN_PREF_e domain;
	unsigned char pdp_context_number = 1; //profile1
	qapi_QT_NW_DS_Profile_PDP_Context_t profile;
	qapi_QT_NW_Alloc_PSM_Cfg_t psm_cfg;
	qapi_QT_NW_RAT_e rat_mode=0;  //emtc 
	qapi_QT_NW_Alloc_eDRX_Cfg_t edrx_cfg;

	qapi_QT_NW_CFUN_MODE_e set_fun = 0; //low power
	qapi_QT_NW_RAT_PREF_e set_mode = 1; //Cat-M only
	qapi_QT_NW_RAT_SCAN_ORDER_e set_order = 1; //order = CatM->GSM->NB
	qapi_QT_NW_SRV_DOMAIN_PREF_e set_domain = 1; //PS only
	qapi_QT_NW_Band_Params_t set_band_pref;
	unsigned long long set_catm_band_h = 0x2, set_nb_band_h = 0x2;
	unsigned char set_pdp_context_number = 2; //profile2
	qapi_QT_NW_DS_Profile_PDP_Context_t set_profile;
	qapi_QT_NW_Req_PSM_Cfg_t set_psm_cfg;
	qapi_QT_NW_Req_eDRX_Cfg_t set_edrx_cfg;

	qapi_QT_NW_Lapi_Config_Profile_t lapi_cfg;
	unsigned short nas_config_data;
	unsigned short capability_value;
	unsigned long fgi_value;

	/* sleep 5 seconds */
	qapi_Timer_Sleep(5, QAPI_TIMER_UNIT_SEC, true);

	ret = txm_module_object_allocate(&byte_pool_nw, sizeof(TX_BYTE_POOL));
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("txm_module_object_allocate [byte_pool_sensor] failed, %d", ret);
		return ret;
	}

	ret = tx_byte_pool_create(byte_pool_nw, "Sensor NW pool", free_memory_nw, NW_BYTE_POOL_SIZE);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_pool_create [byte_pool_sensor] failed, %d", ret);
		return ret;
	}
		
	network_uart_dbg_init();

	while (1)
	{
		nw_task_run_couter ++;

		/* sleep 5 seconds */
		qapi_Timer_Sleep(5, QAPI_TIMER_UNIT_SEC, true);

		//After 2 seconds, reset the module;
		qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);
		
		status = qapi_QT_Phone_Func_Get(&fun);
		if(QAPI_QT_ERR_OK == status)
		{
			IOT_DEBUG("CFUN=%d.", fun);
			nw_uart_debug_print("CFUN=%d.\n", fun);
		}
		else
		{
			IOT_DEBUG("qapi_QT_Phone_Func_Get() ERROR.");
			nw_uart_debug_print("qapi_QT_Phone_Func_Get() ERROR.\n");
		}

		//After 2 seconds, reset the module;
		qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);
		
		status = qapi_QT_NW_Rat_Pref_Get(&mode);
		if(QAPI_QT_ERR_OK == status)
		{
			IOT_DEBUG("[NETWORK] RAT mode=%d.", mode);
			nw_uart_debug_print("[NETWORK] RAT mode=%d.\n", mode);
		}
		else
		{
			IOT_DEBUG("qapi_QT_NW_Rat_Pref_Get() ERROR.");
			nw_uart_debug_print("qapi_QT_NW_Rat_Pref_Get() ERROR.\n");
		}

		//After 2 seconds, reset the module;
		qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);
		
		status = qapi_QT_NW_Band_Pref_Get(&band_pref);
		if(QAPI_QT_ERR_OK == status)
		{
			QAPI_MSG_SPRINTF(MSG_SSID_LINUX_DATA, MSG_LEGACY_HIGH,"[NETWORK] gsm_band=%d, catm_band_low_b64= %x, nb_band_low_b64=%x.", band_pref.gsm_band, band_pref.catm_band_low, band_pref.nb_band_low);
			nw_uart_debug_print("[NETWORK] gsm_band=%x, catm_band_low_b64= %llx, nb_band_low_b64=%llx.\n", band_pref.gsm_band, band_pref.catm_band_low, band_pref.nb_band_low);
		}
		else
		{
			IOT_DEBUG("qapi_QT_NW_Band_Pref_Get() ERROR.");
			nw_uart_debug_print("qapi_QT_NW_Band_Pref_Get() ERROR.\n");
		}

		//After 2 seconds, reset the module;
		qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);
		
		status = qapi_QT_NW_Extend_Band_Pref_Get(&catm_band_h, &nb_band_h);
		if(QAPI_QT_ERR_OK == status)
		{
			IOT_DEBUG("[NETWORK] catm_band_high_b64= %llx, nb_band_high_b64=%llx.", catm_band_h, nb_band_h);
			nw_uart_debug_print("[NETWORK] catm_band_high_b64= %llx, nb_band_high_b64=%llx.\n", catm_band_h, nb_band_h);
		}
		else
		{
			IOT_DEBUG("qapi_QT_NW_Extend_Band_Pref_Get() ERROR.");
			nw_uart_debug_print("qapi_QT_NW_Extend_Band_Pref_Get() ERROR.\n");
		}		

		//After 2 seconds, reset the module;
		qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);
		
		status = qapi_QT_NW_Rat_Scan_Pre_Get(&order);
		if(QAPI_QT_ERR_OK == status)
		{
			IOT_DEBUG("[NETWORK] RAT order=%d.", order);
			nw_uart_debug_print("[NETWORK] RAT order=%d.\n", order);
		}
		else
		{
			IOT_DEBUG("qapi_QT_NW_Rat_Scan_Pre_Get() ERROR.");
			nw_uart_debug_print("qapi_QT_NW_Rat_Scan_Pre_Get() ERROR.\n");
		}

		//After 2 seconds, reset the module;
		qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);
		
		status = qapi_QT_NW_Srv_Domain_Pref_Get(&domain);
		if(QAPI_QT_ERR_OK == status)
		{
			IOT_DEBUG("[NETWORK] Service domain=%d.", domain);
			nw_uart_debug_print("[NETWORK] Service domain=%d.\n", domain);
		}
		else
		{
			IOT_DEBUG("qapi_QT_NW_Srv_Domain_Pref_Get() ERROR.");
			nw_uart_debug_print("qapi_QT_NW_Srv_Domain_Pref_Get() ERROR.\n");
		}

		//After 2 seconds, reset the module;
		qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);
		
		status = qapi_QT_NW_PDP_Cfg_Get(&pdp_context_number, &profile);
		if(QAPI_QT_ERR_OK == status)
		{
			IOT_DEBUG("[NETWORK] pdp_type=%d, apn=%s, user_name=%s, pass_word=%s, auth=%d.", profile.pdp_type, profile.apn, profile.user_name, profile.pass_word, profile.auth_type);
			nw_uart_debug_print("[NETWORK] pdp_type=%d, apn=%s, user_name=%s, pass_word=%s, auth=%d.\n", profile.pdp_type, profile.apn, profile.user_name, profile.pass_word, profile.auth_type);
		}
		else
		{
			IOT_DEBUG("qapi_QT_NW_PDP_Cfg_Get() ERROR.");
			nw_uart_debug_print("qapi_QT_NW_PDP_Cfg_Get() ERROR.\n");
		}

		//After 2 seconds, reset the module;
		qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);
		
		status = qapi_QT_NW_PSM_Cfg_Get(&psm_cfg);
		if(QAPI_QT_ERR_OK == status)
		{
			IOT_DEBUG("[NETWORK] psm_enable=%d, psm_t3412=%d, psm_t3324=%d", psm_cfg.alloc_psm_enabled, psm_cfg.alloc_periodic_tau_timer_value, psm_cfg.alloc_active_timer_value);
			nw_uart_debug_print("[NETWORK] psm_enable=%d, psm_t3412=%d, psm_t3324=%d\n", psm_cfg.alloc_psm_enabled, psm_cfg.alloc_periodic_tau_timer_value, psm_cfg.alloc_active_timer_value);
		}
		else
		{
			IOT_DEBUG("qapi_QT_NW_PSM_Cfg_Get() ERROR.");
			nw_uart_debug_print("qapi_QT_NW_PSM_Cfg_Get() ERROR.\n");
		}

		//After 2 seconds, reset the module;
		qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);
		
		status = qapi_QT_NW_eDRX_Cfg_Get(&rat_mode, &edrx_cfg);
		if(QAPI_QT_ERR_OK == status)
		{
			IOT_DEBUG("[NETWORK].edrx_enable=%d, edrx_cycle=%d, ptw_cycle=%d", edrx_cfg.alloc_edrx_enable, edrx_cfg.alloc_edrx_cycle, edrx_cfg.alloc_ptw_cycle);
			nw_uart_debug_print("[NETWORK].edrx_enable=%d, edrx_cycle=%d, ptw_cycle=%d.\n", edrx_cfg.alloc_edrx_enable, edrx_cfg.alloc_edrx_cycle, edrx_cfg.alloc_ptw_cycle);
		}
		else
		{
			IOT_DEBUG("qapi_QT_NW_eDRX_Cfg_Get() ERROR.");
			nw_uart_debug_print("qapi_QT_NW_eDRX_Cfg_Get() ERROR.\n");
		}

		status = qapi_QT_NW_Lapi_Cfg_Get(&lapi_cfg);
		if(QAPI_QT_ERR_OK == status)
		{
			IOT_DEBUG("[NETWORK].lapi_mode=%d, lapi_enable=%d.\n", lapi_cfg.lapi_mode,lapi_cfg.lapi_enable);
			nw_uart_debug_print("[NETWORK].lapi_mode=%d, lapi_enable=%d.\n", lapi_cfg.lapi_mode,lapi_cfg.lapi_enable);
		}
		else
		{
			IOT_DEBUG("qapi_QT_NW_Lapi_Cfg_Get() ERROR.\n");
			nw_uart_debug_print("qapi_QT_NW_Lapi_Cfg_Get() ERROR.\n");
		}

		status = qapi_QT_NW_NAS_Cfg_Get(&nas_config_data);
		if(QAPI_QT_ERR_OK == status)
		{
			IOT_DEBUG("[NETWORK]nas_config_data=%d.\n", nas_config_data);
			nw_uart_debug_print("[NETWORK]nas_config_data=%d.\n", nas_config_data);
		}
		else
		{
			IOT_DEBUG("qapi_QT_NW_Nas_Cfg_Get() ERROR.\n");
			nw_uart_debug_print("qapi_QT_NW_Nas_Cfg_Get() ERROR.\n");
		}

		status = qapi_QT_NW_NCC_Cfg_Get(&capability_value);
		if(QAPI_QT_ERR_OK == status)
		{
			IOT_DEBUG("[NETWORK]capability_value=%d.\n", capability_value);
			nw_uart_debug_print("[NETWORK]capability_value=%d.\n", capability_value);
		}
		else
		{
			IOT_DEBUG("qapi_QT_NW_Ncc_Cfg_Get() ERROR.\n");
			nw_uart_debug_print("qapi_QT_NW_Ncc_Cfg_Get() ERROR.\n");
		}

		status = qapi_QT_NW_FGI_Cfg_Get(&fgi_value);
		if(QAPI_QT_ERR_OK == status)
		{
			IOT_DEBUG("[NETWORK]fgi_value=%x.\n", fgi_value);
			nw_uart_debug_print("[NETWORK]fgi_value=%d.\n", fgi_value);
		}
		else
		{
			IOT_DEBUG("qapi_QT_NW_FGI_Cfg_Get() ERROR.\n");
			nw_uart_debug_print("qapi_QT_NW_FGI_Cfg_Get() ERROR.\n");
		}
		
		/* sleep 10 seconds */
		qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_SEC, true);


		set_psm_cfg.req_psm_enable = 1;
		set_psm_cfg.req_periodic_tau_timer_value = 300;
		set_psm_cfg.req_active_timer_value = 60;

		qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);
		
		status = qapi_QT_NW_PSM_Cfg_Set(&set_psm_cfg);
		if(QAPI_QT_ERR_OK == status)
		{
			IOT_DEBUG("[NETWORK] enable PSM successfully.");
			nw_uart_debug_print("[NETWORK] enable PSM successfully.\n");
		}
		else
		{
			IOT_DEBUG("qapi_QT_NW_PSM_Cfg_Set() ERROR.");
			nw_uart_debug_print("qapi_QT_NW_PSM_Cfg_Set() ERROR.\n");
		}

		set_edrx_cfg.req_edrx_enable = 1; //enable
		set_edrx_cfg.rat_mode = 0;        //emtc
		set_edrx_cfg.req_edrx_cycle = 5;
		set_edrx_cfg.req_ptw_cycle = 8;

		//After 2 seconds, reset the module;
		qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);
		
		status = qapi_QT_NW_eDRX_Cfg_Set(&set_edrx_cfg);
		if(QAPI_QT_ERR_OK == status)
		{
			IOT_DEBUG("[NETWORK] enable eDRX successfully.");
			nw_uart_debug_print("[NETWORK] enable eDRX successfully.\n");
		}
		else
		{
			IOT_DEBUG("qapi_QT_NW_eDRX_Cfg_Set() ERROR.");
			nw_uart_debug_print("qapi_QT_NW_eDRX_Cfg_Set() ERROR.\n");
		}
		
		set_profile.pdp_type = 1; //ipv6
		strcpy((char*)set_profile.apn, "internet");
		strcpy((char*)set_profile.user_name, "quectel");
		strcpy((char*)set_profile.pass_word, "123456");
		set_profile.auth_type = 1; //chap
		
		//After 2 seconds, reset the module;
		qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);
		
		status = qapi_QT_NW_PDP_Cfg_Set(&set_pdp_context_number, &set_profile);     //set profile2
		if(QAPI_QT_ERR_OK == status)
		{
			IOT_DEBUG("[NETWORK] PDP profile config successfully.");
			nw_uart_debug_print("[NETWORK] PDP profile config successfully.\n");
		}
		else
		{
			IOT_DEBUG("qapi_QT_NW_PDP_Cfg_Get() ERROR.");
			nw_uart_debug_print("qapi_QT_NW_PDP_Cfg_Get() ERROR.\n");
		}

		//After 2 seconds, reset the module;
		qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);

		set_band_pref.gsm_band = 2;
		set_band_pref.catm_band_low = 5;
		set_band_pref.nb_band_low = 8;
		
		status = qapi_QT_NW_Band_Pref_Set(&set_band_pref);  //set band(B1 ~ B64) of catm as B3 and band(B1 ~ B64) of nb as B8.
		if(QAPI_QT_ERR_OK == status)
		{
			IOT_DEBUG("[NETWORK] Normal band config successfully.");
			nw_uart_debug_print("[NETWORK] Normal band config successfully.\n");
		}
		else
		{
			IOT_DEBUG("qapi_QT_NW_Band_Pref_Set() ERROR.");
			nw_uart_debug_print("qapi_QT_NW_Band_Pref_Set() ERROR.\n");
		}

		//After 2 seconds, reset the module;
		qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);
		
		status = qapi_QT_NW_Extend_Band_Pref_Set(&set_catm_band_h, &set_nb_band_h); //set band(B65 ~ B128) of catm as B66 and band(B65 ~ B128) of nb as B66.
		if(QAPI_QT_ERR_OK == status)
		{
			IOT_DEBUG("[NETWORK] Extend band config successfully.");
			nw_uart_debug_print("[NETWORK] Extend band config successfully.\n");
		}
		else
		{
			IOT_DEBUG("qapi_QT_NW_Extend_Band_Pref_Set() ERROR.");
			nw_uart_debug_print("qapi_QT_NW_Extend_Band_Pref_Set() ERROR.\n");
		}	

		//After 2 seconds, reset the module;
		qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);
		
		status = qapi_QT_NW_Rat_Pref_Set(&set_mode);           // set catm only
		if(QAPI_QT_ERR_OK == status)
		{
			IOT_DEBUG("[NETWORK] Preferred RAT config successfully.");
			nw_uart_debug_print("[NETWORK] Preferred RAT config successfully.\n");
		}
		else
		{
			IOT_DEBUG("qapi_QT_NW_Rat_Pref_Get() ERROR.");
			nw_uart_debug_print("qapi_QT_NW_Rat_Pref_Get() ERROR.\n");
		}

		//After 2 seconds, reset the module;
		qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);
		
		status = qapi_QT_NW_Srv_Domain_Pref_Set(&set_domain);   //set ps only
		if(QAPI_QT_ERR_OK == status)
		{
			IOT_DEBUG("Set PS only successfully.");
			nw_uart_debug_print("Set PS only successfully.\n");
		}
		else
		{
			IOT_DEBUG("qapi_QT_NW_Srv_Domain_Pref_Set() ERROR.");
			nw_uart_debug_print("qapi_QT_NW_Srv_Domain_Pref_Set() ERROR.\n");
		}

		//After 2 seconds, reset the module;
		qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);
		
		status = qapi_QT_NW_Rat_Scan_Pre_Set(&set_order);      //order of nwtwork scanning is CatM->GSM->NB
		if(QAPI_QT_ERR_OK == status)
		{
			IOT_DEBUG("[NETWORK] Set preferred RAT scan successfully.");
			nw_uart_debug_print("[NETWORK] Set preferred RAT scan successfully.\n");
		}
		else
		{
			IOT_DEBUG("qapi_QT_NW_Rat_Scan_Pre_Set() ERROR.");
			nw_uart_debug_print("qapi_QT_NW_Rat_Scan_Pre_Set() ERROR.\n");
		}

		//After 2 seconds, reset the module;
		qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);
		
		status = qapi_QT_Phone_Func_Set(&set_fun);           //set low power
		if(QAPI_QT_ERR_OK == status)
		{
			IOT_DEBUG("[NETWORK] Set low power successfully.");
			nw_uart_debug_print("[NETWORK] Set low power successfully.\n");
		}
		else
		{
			IOT_DEBUG("qapi_QT_Phone_Func_Set() ERROR.");
			nw_uart_debug_print("qapi_QT_Phone_Func_Set() ERROR.\n");
		}

		lapi_cfg.lapi_mode = 2;
		lapi_cfg.lapi_enable = 1;
		status = qapi_QT_NW_Lapi_Cfg_Set(&lapi_cfg);
		if(QAPI_QT_ERR_OK == status)
		{
			IOT_DEBUG("[NETWORK] Lapi config successfully.\n");
			nw_uart_debug_print("[NETWORK]Lapi config successfully.\n");
		}
		else
		{
			IOT_DEBUG("qapi_QT_NW_Lapi_Cfg_Set() ERROR.\n");
			nw_uart_debug_print("qapi_QT_NW_Lapi_Cfg_Set() ERROR.\n");
		}

		nas_config_data = 5;
		status = qapi_QT_NW_NAS_Cfg_Set(&nas_config_data);
		if(QAPI_QT_ERR_OK == status)
		{
			IOT_DEBUG("[NETWORK] Nas config successfully.\n");
			nw_uart_debug_print("[NETWORK]Nas config successfully.\n");
		}
		else
		{
			IOT_DEBUG("qapi_QT_NW_Nas_Cfg_Set() ERROR.\n");
			nw_uart_debug_print("qapi_QT_NW_Nas_Cfg_Set() ERROR.\n");
		}

		capability_value = 13;
		status = qapi_QT_NW_NCC_Cfg_Set(&capability_value);
		if(QAPI_QT_ERR_OK == status)
		{
			IOT_DEBUG("[NETWORK] Ncc config successfully.\n");
			nw_uart_debug_print("[NETWORK]Ncc config successfully.\n");
		}
		else
		{
			IOT_DEBUG("qapi_QT_NW_Ncc_Cfg_Set() ERROR.");
			nw_uart_debug_print("qapi_QT_NW_Ncc_Cfg_Set() ERROR.\n");
		}

		fgi_value = 0xe001002;
		status = qapi_QT_NW_FGI_Cfg_Set(&fgi_value);
		if(QAPI_QT_ERR_OK == status)
		{
			IOT_DEBUG("[NETWORK] Fgi config successfully.\n");
			nw_uart_debug_print("[NETWORK]Fgi config successfully.\n");

		}
		else
		{
			IOT_DEBUG("qapi_QT_NW_FGI_Cfg_Set() ERROR.\n");
			nw_uart_debug_print("qapi_QT_NW_FGI_Cfg_Set() ERROR.\n");
		}

		if(nw_task_run_couter)
		{
			//After 1000 seconds, reset the module;
			qapi_Timer_Sleep(1000, QAPI_TIMER_UNIT_SEC, true);
		}
		
	}
}

#endif /*__EXAMPLE_NETWORK__*/

