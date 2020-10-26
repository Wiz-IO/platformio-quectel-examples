#ifndef __QUECTEL_HTTP_H__
#define __QUECTEL_HTTP_H__
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#if defined(__EXAMPLE_HTTP__)
#include "qapi_fs_types.h"
#include "qapi_dss.h"

#include "qapi_timer.h"
#include "qapi_diag.h"
#include "qapi_ssl.h"

/*===========================================================================
                             DEFINITION
===========================================================================*/
#define DEF_SRC_TYPE    SOCK_STREAM	//SOCK_STREAM(TCP) SOCK_DGRAM(UDP)
#define DEF_LOC_ADDR	"127.0.0.1"
#define DEF_SRV_ADDR    "220.180.239.212"
#define DEF_SRV_PORT    (8498)
#define DEF_BACKLOG_NUM (5)

#define QUEC_APN_LEN	  		(QAPI_DSS_CALL_INFO_APN_MAX_LEN + 1)
#define QUEC_APN_USER_LEN 		(QAPI_DSS_CALL_INFO_USERNAME_MAX_LEN + 1)
#define QUEC_APN_PASS_LEN 		(QAPI_DSS_CALL_INFO_PASSWORD_MAX_LEN + 1)
#define QUEC_DEV_NAME_LEN 		(QAPI_DSS_CALL_INFO_DEVICE_NAME_MAX_LEN + 1)

#define lower(c) (('A' <= c && c <= 'Z') ? c + ('a' - 'A') : c)
#define LEFT_SHIFT_OP(N)		(1 << (N))
#define _htons(x) \
        ((unsigned short)((((unsigned short)(x) & 0x00ff) << 8) | \
                  (((unsigned short)(x) & 0xff00) >> 8)))

#define HTTP_DEFAULT_PORT 			80
#define HTTPS_DEFAULT_PORT  		443
#define HTTP_DOMAIN_NAME_LENGTH     150
#define HTTP_URL_LENGTH             700
#define HTTP_MAX_PATH_LENGTH        100
#define HTTP_READ_BYTES				1024

#define QUECTEL_HTTPS_SUPPORT		/* HTTPS support */

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

typedef enum 
{
	HTTP_DL_SUCCESS		= LEFT_SHIFT_OP(0),
	HTTP_DL_FAIL	= LEFT_SHIFT_OP(1),
} http_Signal_Evt_e;


typedef void(*http_dl_cb)(int error_id);
typedef struct {
    char url[HTTP_URL_LENGTH+1];
    uint32 url_length;
	char filename[HTTP_MAX_PATH_LENGTH+1];
    http_dl_cb cb;
}http_dl_info;

typedef enum
{
	HTTP_SESSION_IDLE,
	HTTP_SESSION_INIT,
	HTTP_SESSION_CONN,
	HTTP_SESSION_DOWNLOADING,
	HTTP_SESSION_DL_FAIL,
	HTTP_SESSION_DL_FIN,
	HTTP_SESSION_DISCONN
} http_session_status_e;

typedef struct
{
    http_session_status_e session_state;
    
	uint8 	data_retry;
	int32     reason_code;

	uint32    start_pos;
	uint32 	last_pos;
	uint32    range_size;
} http_session_policy_t;

#ifdef QUECTEL_HTTPS_SUPPORT
#define FOTA_HTTPS_CERT_NUM		(3)

typedef enum
{
	QHTTPS_SSL_FORMAT_INV = -1,
	QHTTPS_SSL_FORMAT_PEM = 0,
	QHTTPS_SSL_FORMAT_DER,
	QHTTPS_SSL_FORMAT_P7B,
	QHTTPS_SSL_FORMAT_BIN,
	QHTTPS_SSL_FORMAT_MAX = 8
} ssl_cert_format_e;

typedef enum
{
	QHTTPS_SSL_NO_METHOD = -1,
	QHTTPS_SSL_ONEWAY_METHOD,
	QHTTPS_SSL_TWOWAY_METHOD,
	QHTTPS_SSL_MAX_METHOD
} ssl_verify_method_e;

/* TLS/DTLS Instance structure */
typedef struct ssl_inst
{
	qapi_Net_SSL_Obj_Hdl_t sslCtx;
	qapi_Net_SSL_Con_Hdl_t sslCon;
	qapi_Net_SSL_Config_t config;
	qapi_Net_SSL_Role_t role;
} SSL_INST;
#endif	/* QUECTEL_FOTA_HTTPS_SUPPORT */

typedef enum NW_SIG_EVENTS
{
	NW_SIG_EVT_UNENABLE = LEFT_SHIFT_OP(0),
	NW_SIG_EVT_ENABLE =  LEFT_SHIFT_OP(1)
}NW_Signal_Evt_e;


/*===========================================================================
                             DECLARATION
===========================================================================*/
int32 http_inet_ntoa(const qapi_DSS_Addr_t inaddr, uint8 *buf, int32 buflen);
void http_show_sysinfo(void);
int http_netctrl_start(void);
int http_store_read_from_EFS_file(const char *name, void **buffer_ptr, size_t *buffer_size);

#endif /*__EXAMPLE_HTTP__*/
#endif /*__QUECTEL_HTTP_H__*/

