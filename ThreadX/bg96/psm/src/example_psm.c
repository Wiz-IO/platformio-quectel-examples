/******************************************************************************
*@file    example_psm.c
*@brief   example of psm client
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#if defined(__EXAMPLE_PSM__)
#include "txm_module.h"
#include "qapi_diag.h"
#include "qapi_timer.h"
#include "qapi_uart.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "qapi_fs_types.h"
#include "qapi_fs.h"
#include "example_psm.h"
#include "qapi_psm.h"


/**************************************************************************
*                                 DEFINE
***************************************************************************/
TX_BYTE_POOL *byte_pool_gpio;
#define GPIO_BYTE_POOL_SIZE		16*8*1024
UCHAR free_memory_Uart[GPIO_BYTE_POOL_SIZE];

/* uart rx tx buffer */
static char *rx_buff = NULL; /*!!! should keep this buffer as 4K Bytes */
static char *tx_buff = NULL;

/* uart config para*/
static QT_UART_CONF_PARA uart_conf;



/**************************************************************************
*								  PSM
************************************************************************/

int health_check_fail;
int backoff;
int psm_complete;
int is_modem_loaded;
psm_status_msg_type msgs[1];
int   test_active_timer = -1;

int   time_in_psm = PSM_DEFAULT_TEST_PSM_TIME;
int   is_modem_required = TRUE;

int id= -1 ;
qapi_DSS_Hndl_t   dss_handle;
int    is_data_event_cycle = TRUE;
TX_MUTEX               *c_mutex;
TX_EVENT_FLAGS_GROUP   *c_cond;

ULONG         free_memory_psmt[PSMT_BYTE_POOL_SIZE];
TX_BYTE_POOL *byte_pool_psmt;
char              apn[128];
char              ip_addr[128];

TX_MUTEX               *mutex;
TX_EVENT_FLAGS_GROUP   *cond;

TX_THREAD              *psm_data_thread;

/*===========================================================================

						   Static & global Variable Declarations

===========================================================================*/
/*
@func
  quectel_dbg_uart_init
@brief
  Entry function for task. 
*/
/*=========================================================================*/
void quectel_dbg_uart_init(qapi_UART_Port_Id_e port_id)
{
	uart_conf.hdlr	  = NULL;
	uart_conf.port_id = port_id;
	uart_conf.tx_buff = tx_buff;
	uart_conf.tx_len  = sizeof(tx_buff);
	uart_conf.rx_buff = rx_buff;
	uart_conf.rx_len  = sizeof(rx_buff);
	uart_conf.baudrate= 115200;

	/* uart init */
	uart_init(&uart_conf);

	/* start uart receive */
	uart_recv(&uart_conf);
}



/**************************************************************************
*                                 GLOBAL
***************************************************************************/
void psm_demo_app_call_back_func(psm_status_msg_type *psm_status)
{
	qt_uart_dbg(uart_conf.hdlr, "psm_status->status: %d", psm_status->status);
	qt_uart_dbg(uart_conf.hdlr, "psm_status->client_id: %d", psm_status->client_id);
	qt_uart_dbg(uart_conf.hdlr, "psm_status->reason: %d", psm_status->reason);
	qt_uart_dbg(uart_conf.hdlr, "psm_status: %x\n",psm_status);
    if ( psm_status )
    {
        // to avoid fludding of logs, don't print health check.
        if ( psm_status->status != PSM_STATUS_HEALTH_CHECK &&
             psm_status->status != PSM_STATUS_NONE )
        {
           qt_uart_dbg(uart_conf.hdlr,"APPLICATION ID: %d\n", psm_status->client_id);
           qt_uart_dbg(uart_conf.hdlr, "PSM STATUS:    %s\n",status_string[psm_status->status].err_name);
           qt_uart_dbg(uart_conf.hdlr, "REJECT REASON: %s\n",reject_string[psm_status->reason].err_name);
        }
		backoff = TRUE;
        switch(psm_status->status)
        {
            // special handling for some cases like modem loaded, not loaded etc.
            case PSM_STATUS_MODEM_LOADED:
                is_modem_loaded = true;
                break;

            case PSM_STATUS_MODEM_NOT_LOADED:
                is_modem_loaded = false;
                break;

            case PSM_STATUS_HEALTH_CHECK:
                if (!health_check_fail )
                {
                    qapi_PSM_Client_Hc_Ack(id);
                }
                break;

            case PSM_STATUS_NW_OOS:
                if ( backoff )
                {
                    qapi_PSM_Client_Enter_Backoff(id);
                }
                break;

            case PSM_STATUS_NW_LIMITED_SERVICE:
                if ( backoff )
                {
                    qapi_PSM_Client_Enter_Backoff(id);
                }
                break;

            case PSM_STATUS_COMPLETE:
                psm_complete = true;
				tx_event_flags_set(c_cond, PSMT_CONN_NET_MASK, TX_OR);
                qt_uart_dbg(uart_conf.hdlr,"Received PSM complete\n");
                break;

            default:
                break;
        }
    }
}

/**************************************************************************
*                          psm_test_enter_psm_auto
***************************************************************************/

int psm_test_enter_psm_auto(void)
{
    int             result = FAIL;
    psm_info_type   psm_info;

   qt_uart_dbg(uart_conf.hdlr,"Enter PSM process executing. Test app work is done.");

    memset(&psm_info, 0, sizeof(psm_info));
    psm_info.psm_time_info.time_format_flag = PSM_TIME_IN_SECS;
    psm_info.psm_wakeup_type = PSM_WAKEUP_MEASUREMENT_NW_ACCESS;

	test_active_timer = 60;	// set defuatl awake timer, 5min
	qt_uart_dbg(uart_conf.hdlr,"psm_test_enter_psm_auto: test_active_timer: %d", test_active_timer);

    if (test_active_timer >= 0)
    {
        psm_info.active_time_in_secs = test_active_timer;
    }
    else
    {
        psm_info.active_time_in_secs = PSM_DEFAULT_TEST_ACTIVE_TIMER;
    }

    psm_info.psm_time_info.psm_duration_in_secs = time_in_psm;
    if (is_modem_required == FALSE)
    {
        psm_info.psm_wakeup_type = PSM_WAKEUP_MEASUREMENT_ONLY;
    }

    qt_uart_dbg(uart_conf.hdlr,"Enter into Power save Mode");
    result = qapi_PSM_Client_Enter_Psm(id, &psm_info);

    qt_uart_dbg(uart_conf.hdlr,"Result: %s", result ? "FAIL":"SUCCESS");
    if ( result != PSM_ERR_NONE )
    {
        qt_uart_dbg(uart_conf.hdlr,"Reason: %s", error_string[result].err_name);
    }

    return 0;
}


/**************************************************************************
*                          psm_test_start_lte_data_call
***************************************************************************/

int psm_test_start_lte_data_call(void)
{
	qapi_Status_t rc = 0;
    qapi_DSS_Call_Param_Value_t param_info;

     qt_uart_dbg(uart_conf.hdlr,"LTE Data call function entered");

    if ( dss_handle != NULL )
    {
        /* set data call param */
        param_info.buf_val = NULL;
        param_info.num_val = QAPI_DSS_RADIO_TECH_UNKNOWN;
         qt_uart_dbg(uart_conf.hdlr,"Setting tech to Automatic");
        qapi_DSS_Set_Data_Call_Param(dss_handle, QAPI_DSS_CALL_INFO_TECH_PREF_E, &param_info);


        param_info.buf_val = apn;
        param_info.num_val = strlen(apn);
        qt_uart_dbg(uart_conf.hdlr,"Setting APN - %s", apn);
        qapi_DSS_Set_Data_Call_Param(dss_handle, QAPI_DSS_CALL_INFO_APN_NAME_E, &param_info);

        param_info.buf_val = NULL;
        param_info.num_val = QAPI_DSS_IP_VERSION_4;
        qt_uart_dbg(uart_conf.hdlr,"Setting family to IP");
        qapi_DSS_Set_Data_Call_Param(dss_handle, QAPI_DSS_CALL_INFO_IP_VERSION_E, &param_info);

        /* set data call param for UMTS*/
        param_info.buf_val = NULL;
        param_info.num_val = 1;
        qt_uart_dbg(uart_conf.hdlr,"setting profile number to: %d", 1);
        qapi_DSS_Set_Data_Call_Param(dss_handle, QAPI_DSS_CALL_INFO_UMTS_PROFILE_IDX_E, &param_info);

        rc= qapi_DSS_Start_Data_Call(dss_handle);
        qt_uart_dbg(uart_conf.hdlr,"Start data call executed  %d",rc);

    }
    else
    {
         qt_uart_dbg(uart_conf.hdlr,"dss_handle is NULL");
    }
    return 0;
}


/**************************************************************************
*                        psm_test_register_application
***************************************************************************/

static int psm_test_register_application(void)
{
    qapi_Status_t  result = QAPI_OK;
    do
    {
		result =qapi_PSM_Client_Register(&id, (psm_client_cb_type)psm_demo_app_call_back_func, &msgs[0]);
		qt_uart_dbg(uart_conf.hdlr,"Result: %s", result ? "FAIL": "SUCCESS");

		if ( result == QAPI_OK )
        {
            break;
        }
        qapi_Timer_Sleep(100, QAPI_TIMER_UNIT_MSEC, true);
    }while(1);
    qt_uart_dbg(uart_conf.hdlr,"Result: %s", result ? "FAIL": "SUCCESS");
    qt_uart_dbg(uart_conf.hdlr,"Application Id: %lu", id);
    return result;
}

/**************************************************************************
*                          psm_test_net_event_cb
***************************************************************************/

void psm_test_net_event_cb( qapi_DSS_Hndl_t hndl,
                                     void * user_data,
                                     qapi_DSS_Net_Evt_t evt,
                                     qapi_DSS_Evt_Payload_t *payload_ptr )
{

    psm_data_test_event_params_type  params;

    memset(&params, 0, sizeof(params));
    params.evt = evt;
    params.hndl = hndl;

    switch( params.evt )
    {
        case QAPI_DSS_EVT_NET_NO_NET_E:
            tx_event_flags_set(c_cond, PSMT_NO_NET_MASK, TX_OR);
            break;

        case QAPI_DSS_EVT_NET_IS_CONN_E:
            tx_event_flags_set(c_cond, PSMT_CONN_NET_MASK, TX_OR);
            break;

        default:
            break;
    }

    qt_uart_dbg(uart_conf.hdlr,"Data test event: %d sent to data test thread.",params.evt);

}


/**************************************************************************
*                          psm_test_init
***************************************************************************/

void psm_test_init(void)
{

	if( TRUE == is_data_event_cycle )
	  {
		  qt_uart_dbg(uart_conf.hdlr,"just before dss_init");

		  if (QAPI_DSS_SUCCESS !=  qapi_DSS_Init(QAPI_DSS_MODE_GENERAL))
		  {
			  qt_uart_dbg(uart_conf.hdlr,"%s","dss_init failed !!");
		  }
		  else
		  {
			   qt_uart_dbg(uart_conf.hdlr,"dss_init successful");
		  }
		  do
		  {
			  /* obtain data service handle */
			  qt_uart_dbg(uart_conf.hdlr,"Registering Callback DSS_Handle");
			  qapi_DSS_Get_Data_Srvc_Hndl(psm_test_net_event_cb, NULL, &dss_handle);
			  qt_uart_dbg(uart_conf.hdlr,"dss_handle %d", dss_handle);

			  if ( dss_handle != NULL )
			  {
				  qt_uart_dbg(uart_conf.hdlr,"Got valid Data handle. Break.");
				  break;
			  }
			  qapi_Timer_Sleep(100, QAPI_TIMER_UNIT_MSEC, true);

		  }while(1);
	  }
	  else
	  {
		   qt_uart_dbg(uart_conf.hdlr,"Data service handle registration not required");
	  }
}


/**************************************************************************
*                          psm_test_start_lte_data_call
***************************************************************************/

int psm_test_auto(void)
{
    int    result = FAIL;
    ULONG   c_actual_flags;

    qt_uart_dbg(uart_conf.hdlr,"\n\n========= Automated Test Application =========");
  
    result = psm_test_register_application();

    qt_uart_dbg(uart_conf.hdlr,"Register application: %d", result);

    if( TRUE == is_modem_required )
    {
        qt_uart_dbg(uart_conf.hdlr,"Wait for Device to acquire LTE service");
        psm_test_init();

        if( TRUE == is_data_event_cycle )
        {
            qt_uart_dbg(uart_conf.hdlr,"Data event cycle. Performing Data call.");
            psm_test_start_lte_data_call();
        }
        else
        {
            qt_uart_dbg(uart_conf.hdlr,"Not a Data event cycle. No Data call");
            psm_test_enter_psm_auto();
        }

    }
    else
    {
        qt_uart_dbg(uart_conf.hdlr,"Modem not required in this cycle. Send ready");
        psm_test_enter_psm_auto();
    }
	qt_uart_dbg(uart_conf.hdlr,"Ready to exit the PSM, psm_complete: %d", psm_complete);

    do
    {
         tx_event_flags_get(c_cond, 0x1, TX_OR_CLEAR, &c_actual_flags, TX_WAIT_FOREVER);
    }while (!psm_complete);

    qt_uart_dbg(uart_conf.hdlr,"Exiting as PSM is complete");

    return result;
}

/**************************************************************************
*                  psm_data_damtest_task_entry
***************************************************************************/
int psm_data_damtest_task_entry
(
    ULONG thread_input
)
{

    ULONG   actual_flags = NULL;
    UINT    status;

    do
    {
        status = tx_event_flags_get(c_cond,(PSMT_NO_NET_MASK | PSMT_CONN_NET_MASK), TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER);
		qt_uart_dbg(uart_conf.hdlr,"tx_event_flags_get  status   %d", status);

        if( status != TX_SUCCESS )
        {
            break;
        }

        switch(actual_flags)
        {
            case PSMT_NO_NET_MASK:
				  qt_uart_dbg(uart_conf.hdlr,"DSS Event Connected. PSMT_NO_NET_MASK");
                break;

            case PSMT_CONN_NET_MASK:
                qt_uart_dbg(uart_conf.hdlr,"DSS Event Connected. Attempting Ping");
                psm_test_enter_psm_auto();
                break;

            default:
                break;
        }

    } while(TRUE);

    return 0;
}


/* DAM PSM Client */
int quectel_task_entry(void)
{

	int ret = -1;
    CHAR *data_thread_stack_pointer = NULL;
	qapi_PSM_Status_t  result = QAPI_OK;

	qapi_Status_t state;

	qapi_Timer_Sleep(1, QAPI_TIMER_UNIT_SEC, true);
	

	ret = txm_module_object_allocate(&byte_pool_gpio, sizeof(TX_BYTE_POOL));
	if(ret != TX_SUCCESS)
	{
		return ret;
	}

	ret = tx_byte_pool_create(byte_pool_gpio, "Sensor1 application pool",free_memory_Uart, GPIO_BYTE_POOL_SIZE);
	if(ret != TX_SUCCESS)
	{
		return ret;
	}
	
	ret = tx_byte_allocate(byte_pool_gpio, (VOID *)&rx_buff, 4*1024, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		return ret;
	}

	ret = tx_byte_allocate(byte_pool_gpio, (VOID *)&tx_buff, 4*1024, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		return ret;
	}

	quectel_dbg_uart_init(QAPI_UART_PORT_002_E);
    qapi_Timer_Sleep(50, QAPI_TIMER_UNIT_SEC, true);
	qt_uart_dbg(uart_conf.hdlr,"\r\n===quectel_psm_damtest_task_entry start===\r\n");


	txm_module_object_allocate(&c_cond, sizeof(TX_EVENT_FLAGS_GROUP));
    tx_event_flags_create(c_cond, "event flag PSMT");



    txm_module_object_allocate(&byte_pool_psmt, sizeof(TX_BYTE_POOL));
    tx_byte_pool_create(byte_pool_psmt, "byte pool PSM", free_memory_psmt, PSMT_BYTE_POOL_SIZE);

    /* Allocate the stack for data thread.  */
    txm_module_object_allocate(&psm_data_thread, sizeof(TX_THREAD));
    tx_byte_allocate(byte_pool_psmt, (VOID **) &data_thread_stack_pointer, PSM_DATA_STACK_SIZE, TX_NO_WAIT);

    /* Create the data thread.  */
    tx_thread_create(psm_data_thread, "PSMT Data Thread", psm_data_damtest_task_entry,
                          0, data_thread_stack_pointer, PSM_DATA_STACK_SIZE,
                          148, 148, TX_NO_TIME_SLICE, TX_AUTO_START);

     psm_test_auto();

    return 0;
}






void psm_user_space_dispatcher(UINT cb_id, void *app_cb,
                               UINT cb_param1, UINT cb_param2,
                               UINT cb_param3, UINT cb_param4)
{
    psm_client_cb_type CB_Function;

    switch(cb_id)
    {
        case TXM_QAPI_PSM_CLIENT_CB_FUNC:
            /* Client callback */
            CB_Function = (psm_client_cb_type) app_cb;
            (CB_Function)((psm_status_msg_type *) cb_param1);
            break;

        default:
            break;
    }
}


#endif /*__EXAMPLE_PSM__*/

