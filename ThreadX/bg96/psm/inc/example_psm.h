#ifndef __EXAMPLE_PSM_H__
#define __EXAMPLE_PSM_H__

#include "stdio.h"
#include "qapi_dss.h"
#include "qapi_psm.h"
#include "qapi_psm_types.h"

#define PSM_DAM_TEST_LOG_FILE 	"/datatx/psmdam_log.txt"

#define TRUE                              1
#define FALSE                             0
#define SUCCESS                           0
#define FAIL                             -1
#define PSM_DEFAULT_DATA_EVENT_FREQUENCY  1
#define PSM_DEFAULT_TEST_ACTIVE_TIMER     0
#define PSM_DEFAULT_TEST_PSM_TIME        (150)
#define PSM_DEFAULT_PING_PACKET_SIZE      1000
#define PSM_PING_MAX_PACKET_SIZE         (65535)
#define PSM_MAX_LOG_MSG_SIZE              100

#define PSM_DATA_EVENT_PIPE_MAX_ELEMENTS  4
#define INVALID_FILE_HANDLE              (-1)
#define PSM_DATA_STACK_SIZE               4098
#define PSMT_BYTE_POOL_SIZE               17408 

#define PSMT_NO_NET_MASK                  0x00000001
#define PSMT_CONN_NET_MASK                0x00000010

typedef enum dss_net_evt_e
{
  DSS_EVT_INVALID           = QAPI_DSS_EVT_INVALID_E,            /* Invalid event */
  DSS_EVT_NET_IS_CONN       = QAPI_DSS_EVT_NET_IS_CONN_E,        /* Call connected */
  DSS_EVT_NET_NO_NET        = QAPI_DSS_EVT_NET_NO_NET_E,         /* Call disconnected */
  DSS_EVT_NET_RECONFIGURED  = QAPI_DSS_EVT_NET_RECONFIGURED_E,   /* Call reconfigured */
  DSS_EVT_NET_NEWADDR       = QAPI_DSS_EVT_NET_NEWADDR_E,        /* New address generated */
  DSS_EVT_NET_DELADDR       = QAPI_DSS_EVT_NET_DELADDR_E,        /* Delete generated */
  DSS_EVT_NIPD_DL_DATA      = QAPI_DSS_EVT_NIPD_DL_DATA_E,
  DSS_EVT_MAX               
} dss_net_evt_t;

typedef struct
{
  int   err;
  char *err_name;
}psm_err_map_type;

typedef struct
{
  dss_net_evt_t         evt;
  void                 *data;
  qapi_DSS_Hndl_t       hndl;
} psm_data_test_event_params_type;

const psm_err_map_type error_string[] = {
    {PSM_ERR_NONE, "NONE"},
    {PSM_ERR_FAIL, "FAIL"},
    {PSM_ERR_GENERIC_FAILURE, "GENERIC FAILURE"},
    {PSM_ERR_APP_NOT_REGISTERED, "APP NOT REGISTERED"},
    {PSM_ERR_WRONG_ARGUMENTS, "WRONG VALUES PASSED"},
    {PSM_ERR_IPC_FAILURE, "FAILED TO COMMUNICATE WITH SERVER"}
};


const psm_err_map_type reject_string[] = {
    {PSM_REJECT_REASON_NONE, "NONE"},
    {PSM_REJECT_REASON_NOT_ENABLED, "PSM FEATURE NOT ENABLED"},
    {PSM_REJECT_REASON_MODEM_NOT_READY, "MODEM NOT READY FOR PSM"},
    {PSM_REJECT_REASON_DURATION_TOO_SHORT, "ENTERED PSM DURATION IS TOO SHORT"},
};

const psm_err_map_type status_string[] = {
    {PSM_STATUS_NONE,               "NONE"},
    {PSM_STATUS_REJECT,             "REJECT"},
    {PSM_STATUS_READY,              "READY TO ENTER PSM MODE"},
    {PSM_STATUS_NOT_READY,          "NOT READY TO ENTER PSM MODE"},
    {PSM_STATUS_COMPLETE,           "ENTERED INTO PSM MODE"},
    {PSM_STATUS_DISCONNECTED,       "COMMUNICATION WITH SERVER LOST"},
    {PSM_STATUS_MODEM_LOADED,       "MODEM LOADED"},
    {PSM_STATUS_MODEM_NOT_LOADED,   "MODEM NOT LOADED"},
    {PSM_STATUS_NW_OOS,             "NW OOS"},
    {PSM_STATUS_NW_LIMITED_SERVICE, "NW LIMITED SERVICE"},
    {PSM_STATUS_HEALTH_CHECK,       "HEALT CHECK"},
    {PSM_STATUS_FEATURE_ENABLED,    "PSM FEATURE ENABLED"},
    {PSM_STATUS_FEATURE_DISABLED,   "PSM FEATURE DISABLED"},
};


void psm_user_space_dispatcher(UINT cb_id, void *app_cb,
                               UINT cb_param1, UINT cb_param2,
                               UINT cb_param3, UINT cb_param4);


#endif /* __EXAMPLE_PSM_H__ */
