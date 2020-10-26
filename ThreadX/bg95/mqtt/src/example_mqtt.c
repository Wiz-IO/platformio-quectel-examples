/******************************************************************************
*@file    example_mqtt.c
*@brief   example of mqtt
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/

#if defined(__EXAMPLE_MQTT__)
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "qapi_fs_types.h"
#include "qapi_status.h"
#include "qapi_socket.h"
#include "qapi_dss.h"
#include "qapi_netservices.h"
#include "qapi_net_status.h"
#include "qapi_uart.h"
#include "qapi_timer.h"
#include "qapi_diag.h"
#include "qapi_fs.h"
#include "qapi_mqtt.h"
#include "qapi_device_info.h"

#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "example_mqtt.h"

/**************************************************************************
*                                 DEFINE
***************************************************************************/
#define QL_DEF_APN	          "CMNET"
#define DSS_ADDR_INFO_SIZE	  5
#define DSS_ADDR_SIZE       16

#define BYTE_POOL_SIZE       (30720)

#define GET_ADDR_INFO_MIN(a, b) ((a) > (b) ? (b) : (a))

#define QUEC_MQTT_UART_DBG
#ifdef QUEC_MQTT_UART_DBG
#define MQTT_UART_DBG(...)	\
{\
	mqtt_uart_debug_print(__VA_ARGS__);	\
}
#else
#define MQTT_UART_DBG(...)
#endif

#define NW_SIGNAL_ENABLE //for checking network status by device_info

/**************************************************************************
*                                 GLOBAL
***************************************************************************/
qapi_Net_MQTT_Hndl_t    app_mqttcli_ctx = NULL;
qapi_Net_MQTT_Config_t  mqttdemo_cfg;

TX_BYTE_POOL *byte_pool_mqtt;
#define MQTT_BYTE_POOL_SIZE		10*8*1024
UCHAR free_memory_mqtt[MQTT_BYTE_POOL_SIZE];

void* mqtt_nw_signal_handle = NULL;	/* Related to nework indication */

qapi_Device_Info_Hndl_t device_info_hndl;

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

                           Static & global Variable Declarations

===========================================================================*/
static void mqtt_uart_rx_cb(uint32_t num_bytes, void *cb_data)
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

static void mqtt_uart_tx_cb(uint32_t num_bytes, void *cb_data)
{
	IOT_DEBUG("QT# uart_tx_cb, send [%d]", num_bytes);
}

void mqtt_uart_dbg_init()
{
	qapi_Status_t status;
	qapi_UART_Open_Config_t uart_cfg;
	
	if (TX_SUCCESS != tx_byte_allocate(byte_pool_mqtt, (VOID *)&uart_rx_buff, 4*1024, TX_NO_WAIT))
	{
		IOT_DEBUG("tx_byte_allocate [uart_rx_buff] failed!");
		return;
	}

	if (TX_SUCCESS != tx_byte_allocate(byte_pool_mqtt, (VOID *)&uart_tx_buff, 4*1024, TX_NO_WAIT))
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
	uart_cfg.enable_Flow_Ctrl = QAPI_FCTL_OFF_E;
	uart_cfg.bits_Per_Char		= QAPI_UART_8_BITS_PER_CHAR_E;
	uart_cfg.enable_Loopback	= 0;
	uart_cfg.num_Stop_Bits		= QAPI_UART_1_0_STOP_BITS_E;
	uart_cfg.parity_Mode		= QAPI_UART_NO_PARITY_E;
	uart_cfg.rx_CB_ISR			= (qapi_UART_Callback_Fn_t)&mqtt_uart_rx_cb;
	uart_cfg.tx_CB_ISR			= (qapi_UART_Callback_Fn_t)&mqtt_uart_tx_cb;

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

void mqtt_uart_debug_print(const char* fmt, ...) 
{
	va_list arg_list;
    char dbg_buffer[128] = {0};
    
	va_start(arg_list, fmt);
    vsnprintf((char *)(dbg_buffer), sizeof(dbg_buffer), (char *)fmt, arg_list);
    va_end(arg_list);
		
    qapi_UART_Transmit(uart_conf.hdlr, dbg_buffer, strlen(dbg_buffer), NULL);
    //qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_MSEC, true);   //Do not add delay unless necessary
}

void mqtt_cli_disconnect(void);

void mqtt_cli_connect_callback(qapi_Net_MQTT_Hndl_t mqtt, int32 reason)
{
    if(reason == QAPI_NET_MQTT_CONNECT_SUCCEEDED_E)
    {
        MQTT_UART_DBG("MQTT Connected, reason code=%d\n", reason);
    }
    else 
    {
        MQTT_UART_DBG("MQTT Connection failed, reason code=%d\n", reason);
        mqtt_cli_disconnect();
    }
}

void mqtt_cli_subscribe_callback(qapi_Net_MQTT_Hndl_t mqtt,
                                 int32 reason,
                                 const uint8 *topic,
                                 int32 topic_length,
                                 int32 qos,
                                 const void *sid)
{
    if(reason == QAPI_NET_MQTT_SUBSCRIBE_GRANTED_E)
    {
        MQTT_UART_DBG("Subscribe granted, topic=%s, qos=%d\n", topic, qos);
    }
    else 
    {
        MQTT_UART_DBG("Subscribe failed, reaon code=%d\n", reason);
    }  
}

void mqtt_cli_message_callback(qapi_Net_MQTT_Hndl_t mqtt,
                               int32 reason,
                               const uint8 *topic,
                               int32 topic_length,
                               const uint8 *msg,
                               int32 msg_length,
                               int32 qos,
                               const void *sid)
{
    MQTT_UART_DBG("Message Callback: qos=%d, topic=%s, msg=%s\n", qos, topic, msg);
}

void mqtt_cli_publish_callback(qapi_Net_MQTT_Hndl_t mqtt,
                               enum QAPI_NET_MQTT_MSG_TYPES msgtype,
                               int32_t qos,
                               uint16_t msg_id)
{
    char msg[10] = {0};

    if (msgtype == QAPI_NET_MQTT_PUBACK)
    {
        strlcpy(msg, "PUBACK", strlen("PUBACK")+1);
    }
    else if (msgtype == QAPI_NET_MQTT_PUBREC)
    {
        strlcpy(msg, "PUBREC", strlen("PUBREC")+1);
    }
    else if (msgtype == QAPI_NET_MQTT_PUBCOMP)
    {
        strlcpy(msg, "PUBCOMP", strlen("PUBCOMP")+1);
    }
    else
    {
        strlcpy(msg, "INVALID", strlen("INVALID")+1);
    }

MQTT_UART_DBG("Publish Callback type %s, QOS %d, msg Id %d\n", msg, qos, msg_id);
}

void mqtt_cli_connect(void)
{
    struct sockaddr_in *sin4;
    static uint32_t port_client = 0;
    int count = 1;
    int ret;
    char buff[32];

    MQTT_UART_DBG("mqtt_cli_connect entry\n");

  
    ret = qapi_Net_MQTT_New(&app_mqttcli_ctx);
    if(ret != QAPI_OK) 
    {
        MQTT_UART_DBG("Context not created\n");
        return;
    }
    else
    {
        MQTT_UART_DBG("Mqtt Context created success, ctx=0x%x\n", app_mqttcli_ctx);
    }

    qapi_Net_MQTT_Pass_Pool_Ptr(app_mqttcli_ctx, byte_pool_mqtt);
    qapi_Net_MQTT_Set_Connect_Callback(app_mqttcli_ctx, mqtt_cli_connect_callback);
    qapi_Net_MQTT_Set_Subscribe_Callback(app_mqttcli_ctx, mqtt_cli_subscribe_callback);
    qapi_Net_MQTT_Set_Message_Callback(app_mqttcli_ctx, mqtt_cli_message_callback);
    qapi_Net_MQTT_Set_Publish_Callback(app_mqttcli_ctx,mqtt_cli_publish_callback);

    sin4 = (struct sockaddr_in *)&mqttdemo_cfg.local;
    sin4->sin_family = AF_INET;
    sin4->sin_port = (uint16)_htons(port_client);
    sin4->sin_addr.s_addr = 0;

    sin4 = (struct sockaddr_in *)&mqttdemo_cfg.remote;

    while(count--) 
    {
        MQTT_UART_DBG("Connecting...\n");

        ret = qapi_Net_MQTT_Connect(app_mqttcli_ctx, &mqttdemo_cfg);
        if(ret != QAPI_OK) 
        {
            MQTT_UART_DBG("MQTT Connect Failed, Error type %d\n", ret);
        } 
        else 
        {
            MQTT_UART_DBG("MQTT Connect Successfull\n");
            break;
        }
    }
}

void mqtt_cli_publish(uint8 *topic, uint8 *msg, int qos, int retain)
{
    int ret = 0;
    uint8 msg_len = 0;
    uint16 msg_id = 1;

    if(msg) 
    {
        msg_len = strlen((const char *)msg);
    }
    MQTT_UART_DBG("Published trying...\n");
    ret = qapi_Net_MQTT_Publish(app_mqttcli_ctx, (uint8 *)topic, (uint8 *)msg, msg_len, qos, retain);
    if(ret) 
    {
        MQTT_UART_DBG("MQTT Publish Failed, Error type %d\n", ret);
    } 
    else 
    {
        MQTT_UART_DBG("Published Successfull\n");
    }
}

void mqtt_cli_subscribe(uint8 *topic, int qos)
{
    int ret;

    MQTT_UART_DBG("subscribing...\n");
    ret = qapi_Net_MQTT_Subscribe(app_mqttcli_ctx, (uint8 *)topic, qos);
    if(ret) 
    {
        MQTT_UART_DBG("MQTT Subscribe Failed, Error type %d\n", ret);
    } 
    else 
    {
        MQTT_UART_DBG("Subscribe Successfull\n");
    }
}

void mqtt_cli_unsubscribe(uint8 *topic)
{
    int ret;

    MQTT_UART_DBG("Unsubscribing...\n");
    ret = qapi_Net_MQTT_Unsubscribe(app_mqttcli_ctx, (uint8 *)topic);
    if(ret) 
    {
        MQTT_UART_DBG("MQTT Unsubscribe Failed, Error type %d\n", ret);
    } 
    else 
    {
        MQTT_UART_DBG("Unsubscribe Successfull\n");
    }
}

void mqtt_cli_disconnect(void)
{
  int ret;

  MQTT_UART_DBG("Disconnecting...\n");
  ret = qapi_Net_MQTT_Disconnect(app_mqttcli_ctx);
  if(ret) 
  {
    MQTT_UART_DBG("MQTT Disconnect Failed, Error type %d\n", ret);
  } 
  else 
  {
    MQTT_UART_DBG("Disconnect Successfull\n");
  }
}

/*************************************************************************************************************
 *  used to connect server
 **************************************************************************************************************/

int mqtt_cli_read_EFS_file(const char *name, void **buffer_ptr, size_t *buffer_size)
{
	int bytes_read;
	int efs_fd = -1;
	struct qapi_FS_Stat_Type_s stat;
	uint8 *file_buf = NULL;
	stat.st_size = 0;

	if ((!name) || (!buffer_ptr) ||(!buffer_size))
	{
		MQTT_UART_DBG("Reading SSL from EFS file failed!\n");
		return QAPI_ERROR;
	}

	if (qapi_FS_Open(name, QAPI_FS_O_RDONLY_E, &efs_fd) < 0)
	{
		MQTT_UART_DBG("Opening EFS EFS file %s failed\n", name);
		return QAPI_ERROR;
	}

	if (qapi_FS_Stat_With_Handle(efs_fd, &stat) < 0) 
	{
		MQTT_UART_DBG("Reading EFS file %s failed\n", name);
		return QAPI_ERROR;
	}

	MQTT_UART_DBG("Reading EFS file size %d \n", stat.st_size);
	
	tx_byte_allocate(byte_pool_mqtt, (VOID **) &file_buf, stat.st_size, TX_NO_WAIT);
	if (file_buf  == NULL) 
	{
		MQTT_UART_DBG("SSL_cert_store: QAPI_ERR_NO_MEMORY \n");
		return QAPI_ERR_NO_MEMORY;
	}

	qapi_FS_Read(efs_fd, file_buf, stat.st_size, &bytes_read);
	if ((bytes_read < 0) || (bytes_read != stat.st_size)) 
	{
		tx_byte_release(file_buf);
		MQTT_UART_DBG("SSL_cert_store: Reading EFS file error\n");
		return QAPI_ERROR;
	}

	*buffer_ptr = file_buf;
	*buffer_size = stat.st_size;

	qapi_FS_Close(efs_fd);

	return QAPI_OK;
}

qapi_Status_t conn_mqtt( )
{
  uint32 retain = false; 
  uint16 k_alive = 120;
  uint32 ssl_enabled = 0;

/* test with Telit Iot Platform */
  uint8 clean_session       = true;
  char  will_topic_ptr[]    = "will_topic";
  uint8 will_topic_len      = strlen(will_topic_ptr);
  char  will_message_ptr[]  = "will_msg";
  uint8 will_message_len    = strlen(will_message_ptr);
  uint8 will_qos = 1;
  char  username_ptr[]  = "ggq1992@126.com";
  uint8 username_len    = strlen(username_ptr);
  char  password_ptr[]  = "00quectel&mqtt";
  uint8 password_len    = strlen(password_ptr);
  char  client_id[]     = "12345";
  uint8 client_id_len   = strlen(client_id);
  struct sockaddr_in *sin4;
  struct ip46addr ipaddr;

  MQTT_UART_DBG("conn_mqtt entry\n");
  
  memset(&mqttdemo_cfg, 0, sizeof(mqttdemo_cfg));

  sin4 = (struct sockaddr_in *)&mqttdemo_cfg.remote;
  sin4->sin_family = AF_INET;
  sin4->sin_port = _htons(8100);
#if 1
  sin4->sin_addr.s_addr = inet_addr("220.180.239.212");  
#else
    ipaddr.type = AF_INET;
    get_ip_from_url(CLI_MQTT_SVR_NAME, &ipaddr);
    sin4->sin_addr.s_addr = ipaddr.a.addr4;
#endif
  
  strncpy((char *)mqttdemo_cfg.client_id, (char *)client_id, QAPI_NET_MQTT_MAX_CLIENT_ID_LEN);
  mqttdemo_cfg.client_id_len        = client_id_len;
  mqttdemo_cfg.nonblocking_connect  = false;
  mqttdemo_cfg.keepalive_duration   = k_alive; 
  mqttdemo_cfg.clean_session        = clean_session;
  mqttdemo_cfg.will_topic           = (uint8*)will_topic_ptr;
  mqttdemo_cfg.will_topic_len       = will_topic_len;
  mqttdemo_cfg.will_message         = (uint8*)will_message_ptr;
  mqttdemo_cfg.will_message_len     = will_message_len;
  mqttdemo_cfg.will_retained        = retain;
  mqttdemo_cfg.will_qos             = will_qos;
  mqttdemo_cfg.username             = (uint8*)username_ptr;
  mqttdemo_cfg.username_len         = username_len;  
  mqttdemo_cfg.password             = (uint8*)password_ptr;
  mqttdemo_cfg.password_len         = password_len;
#ifdef MQTT_CLIENT_SECURITY
    memset(&(mqttdemo_cfg.ssl_cfg), 0, sizeof(mqttdemo_cfg.ssl_cfg));

    if(ssl_enabled == 1)
    {
        uint8_t *cert_Buf = NULL; 
        uint32_t cert_Size = 0;
    	uint8_t *key_Buf = NULL;
    	uint32_t key_Size = 0;
        int result;
    	qapi_Net_SSL_Cert_Info_t cert_info;
    	qapi_NET_SSL_CA_Info_t calist_info[QAPI_NET_SSL_MAX_CA_LIST];
    	memset(&cert_info, 0, sizeof(cert_info));

        /* Read the CA certificate information */
        result = mqtt_cli_read_EFS_file(MQTTS_CA_CRT, (void **)&calist_info[0].ca_Buf, (size_t *)&calist_info[0].ca_Size);
        MQTT_UART_DBG("Read %s, result %d\r\n", MQTTS_CA_CRT, result);

        if (QAPI_OK != result) 
        {
             MQTT_UART_DBG("ERROR: Reading ca information from EFS failed!! \r\n");
             result = QAPI_ERROR;
        }

        cert_info.info.ca_List.ca_Info[0] = &calist_info[0];
        cert_info.info.ca_List.ca_Cnt = 1;
        cert_info.cert_Type = QAPI_NET_SSL_CA_LIST_E;

        result = qapi_Net_SSL_Cert_Convert_And_Store(&cert_info, MQTT_CALIST_BIN);
        MQTT_UART_DBG("%s qapi_Net_SSL_Cert_Convert_And_Store: %d\r\n", MQTT_CALIST_BIN, result);

        /* Read the client certificate information */
        result = mqtt_cli_read_EFS_file((char *)MQTTS_CLI_CRT, (void **)&cert_Buf, (size_t *)&cert_Size);
        MQTT_UART_DBG("Read %s, result %d\r\n", MQTTS_CLI_CRT, result);
        if (QAPI_OK != result) 
        {
             MQTT_UART_DBG("ERROR: Reading client certificate from EFS failed!! \r\n");
             result = QAPI_ERROR;
        }

        /* Read the client key information */
        result = mqtt_cli_read_EFS_file((char *)MQTTS_CLI_KEY, (void **)&key_Buf, (size_t *)&key_Size);
        MQTT_UART_DBG("Read %s, result %d\r\n", MQTTS_CLI_KEY, result);
        if (QAPI_OK != result) 
        {
             MQTT_UART_DBG("ERROR: Reading key information from EFS failed!! \r\n");
             result = QAPI_ERROR;
        }

        /* Update the client certificate information */
        cert_info.cert_Type = QAPI_NET_SSL_CERTIFICATE_E; 
        cert_info.info.cert.cert_Buf = cert_Buf;
        cert_info.info.cert.cert_Size = cert_Size;
        cert_info.info.cert.key_Buf = key_Buf;
        cert_info.info.cert.key_Size = key_Size;

        /* Convert and store the certificate */ 
        result = qapi_Net_SSL_Cert_Convert_And_Store(&cert_info, MQTT_CERT_BIN);
        MQTT_UART_DBG("%s qapi_Net_SSL_Cert_Convert_And_Store: %d\r\n", MQTT_CERT_BIN, result);

        mqttdemo_cfg.ca_list = MQTT_CALIST_BIN;
        mqttdemo_cfg.cert    = MQTT_CERT_BIN;
        mqttdemo_cfg.ssl_cfg.protocol  = QAPI_NET_SSL_PROTOCOL_TLS_1_2;
        mqttdemo_cfg.ssl_cfg.cipher[0] = QAPI_NET_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384; /*still need to be filled */
        mqttdemo_cfg.ssl_cfg.cipher[1] = QAPI_NET_TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384;
        mqttdemo_cfg.ssl_cfg.cipher[2] = QAPI_NET_TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256;
        mqttdemo_cfg.ssl_cfg.cipher[3] = QAPI_NET_TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256;
        mqttdemo_cfg.ssl_cfg.cipher[4] = QAPI_NET_TLS_ECDH_RSA_WITH_AES_256_CBC_SHA;
        mqttdemo_cfg.ssl_cfg.cipher[5] = QAPI_NET_TLS_ECDH_RSA_WITH_AES_128_CBC_SHA;
        mqttdemo_cfg.ssl_cfg.cipher[6] = QAPI_NET_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384;
        mqttdemo_cfg.ssl_cfg.cipher[7] = QAPI_NET_TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384;

        mqttdemo_cfg.ssl_cfg.verify.domain = 0;
        mqttdemo_cfg.ssl_cfg.verify.time_Validity = 0;
        mqttdemo_cfg.ssl_cfg.verify.send_Alert = 0;
        mqttdemo_cfg.ssl_cfg.verify.match_Name[0] = '\0';

        /* Frangment value will be taken only in 512 , 1024 , 2048 , 4096   */
        mqttdemo_cfg.ssl_cfg.max_Frag_Len = 4096;
        mqttdemo_cfg.ssl_cfg.max_Frag_Len_Neg_Disable = 0;
    }
#endif

    	MQTT_UART_DBG("client_len=%d, client_str=%s\n", mqttdemo_cfg.client_id_len, mqttdemo_cfg.client_id);
    if(mqttdemo_cfg.username)
        MQTT_UART_DBG("username_len=%d, username_str=%s\n", mqttdemo_cfg.username_len, mqttdemo_cfg.username);
    if(mqttdemo_cfg.password)
        MQTT_UART_DBG("pwd_len=%d, pwd_str=%s\n", mqttdemo_cfg.password_len, mqttdemo_cfg.password);


    mqtt_cli_connect();

  return QAPI_OK;
}

qapi_Status_t pub_mqtt( )
{
  char topic[] = "hello_topic";
  char msg[] = "pub test: hello";
  int qos = 1;
  int retain = 0;

  if(NULL == app_mqttcli_ctx) 
  {
    MQTT_UART_DBG("No MQTT Connection, Please do MQTT connection first \n");
    return QAPI_ERROR;
  }

  mqtt_cli_publish((uint8 *)topic, (uint8 *)msg, qos, retain);

  return QAPI_OK;
}

qapi_Status_t sub_mqtt()
{
  char topic[] = "hello_topic2";
  int qos = 1;

  if(NULL == app_mqttcli_ctx) 
  {
    MQTT_UART_DBG("No MQTT Connection, Please do MQTT connection first \n");
    return QAPI_ERROR;
  }

  mqtt_cli_subscribe((uint8 *)topic, qos);

  return QAPI_OK;
}

qapi_Status_t unsub_mqtt()
{
  char topic[] = "hello_topic";

  if(NULL == app_mqttcli_ctx) 
  {
    MQTT_UART_DBG("No MQTT Connection, Please do MQTT connection first \n");
    return QAPI_ERROR;
  }

  mqtt_cli_unsubscribe((uint8 *)topic);

  return QAPI_OK;
}

qapi_Status_t disconn_mqtt()
{

  if(NULL == app_mqttcli_ctx) 
  {
    MQTT_UART_DBG("No MQTT Connection, Please do MQTT connection first \n");
    return QAPI_ERROR;
  }

  mqtt_cli_disconnect();

  if(app_mqttcli_ctx != NULL)
    qapi_Net_MQTT_Destroy(app_mqttcli_ctx);

  return QAPI_OK;
}


/*===========================================================================
                             DEFINITION
===========================================================================*/
#define THREAD_STACK_SIZE    (1024 * 16)
#define THREAD_PRIORITY      (180)

#define CMD_BUF_SIZE  100

#define DEFAULT_PUB_TIME 5

/*===========================================================================
                           Global variable
===========================================================================*/
/* TCPClient dss thread handle */
#ifdef QAPI_TXM_MODULE
	static TX_THREAD *dss_thread_handle; 
#else
	static TX_THREAD _dss_thread_handle;
	static TX_THREAD *ts_thread_handle = &_dss_thread_handle;
#endif

static unsigned char tcp_dss_stack[THREAD_STACK_SIZE];

TX_EVENT_FLAGS_GROUP *tcp_signal_handle;

qapi_DSS_Hndl_t tcp_dss_handle = NULL;	            /* Related to DSS netctrl */

static char apn[QUEC_APN_LEN];					/* APN */
static char apn_username[QUEC_APN_USER_LEN];	/* APN username */
static char apn_passwd[QUEC_APN_PASS_LEN];		/* APN password */

/* @Note: If netctrl library fail to initialize, set this value will be 1,
 * We should not release library when it is 1. 
 */
signed char tcp_netctl_lib_status = DSS_LIB_STAT_INVALID_E;
unsigned char tcp_datacall_status = DSS_EVT_INVALID_E;

/*
@func
	dss_net_event_cb
@brief
	Initializes the DSS netctrl library for the specified operating mode.
*/
static void tcp_net_event_cb
( 
	qapi_DSS_Hndl_t 		hndl,
	void 				   *user_data,
	qapi_DSS_Net_Evt_t 		evt,
	qapi_DSS_Evt_Payload_t *payload_ptr 
)
{
	qapi_Status_t status = QAPI_OK;
	
	MQTT_UART_DBG("Data test event callback, event: %d\n", evt);

	switch (evt)
	{
		case QAPI_DSS_EVT_NET_IS_CONN_E:
		{
			MQTT_UART_DBG("Data Call Connected.\n");
			tcp_show_sysinfo();
			/* Signal main task */
  			tx_event_flags_set(tcp_signal_handle, DSS_SIG_EVT_CONN_E, TX_OR);
			tcp_datacall_status = DSS_EVT_NET_IS_CONN_E;
			
			break;
		}
		case QAPI_DSS_EVT_NET_NO_NET_E:
		{
			MQTT_UART_DBG("Data Call Disconnected.\n");
			
			if (DSS_EVT_NET_IS_CONN_E == tcp_datacall_status)
			{
				/* Release Data service handle and netctrl library */
				if (tcp_dss_handle)
				{
					qapi_DSS_Stop_Data_Call(tcp_dss_handle);
					status = qapi_DSS_Rel_Data_Srvc_Hndl(tcp_dss_handle);
					if (QAPI_OK == status)
					{
						MQTT_UART_DBG("Release data service handle success\n");
						tx_event_flags_set(tcp_signal_handle, DSS_SIG_EVT_EXIT_E, TX_OR);
					}
				}
				
				if (DSS_LIB_STAT_SUCCESS_E == tcp_netctl_lib_status)
				{
					qapi_DSS_Release(QAPI_DSS_MODE_GENERAL);
				}
			}
			else
			{
				/* DSS status maybe QAPI_DSS_EVT_NET_NO_NET_E before data call establishment */
				tx_event_flags_set(tcp_signal_handle, DSS_SIG_EVT_NO_CONN_E, TX_OR);
			}

			break;
		}
		default:
		{
			MQTT_UART_DBG("Data Call status is invalid.\n");
			
			/* Signal main task */
  			tx_event_flags_set(tcp_signal_handle, DSS_SIG_EVT_INV_E, TX_OR);
			tcp_datacall_status = DSS_EVT_INVALID_E;
			break;
		}
	}
}

void tcp_show_sysinfo(void)
{
	int i   = 0;
	int j 	= 0;
	unsigned int len = 0;
	uint8 buff[DSS_ADDR_SIZE] = {0}; 
	qapi_Status_t status;
	qapi_DSS_Addr_Info_t info_ptr[DSS_ADDR_INFO_SIZE];

	status = qapi_DSS_Get_IP_Addr_Count(tcp_dss_handle, &len);
	if (QAPI_ERROR == status)
	{
		MQTT_UART_DBG("Get IP address count error\n");
		return;
	}
		
	status = qapi_DSS_Get_IP_Addr(tcp_dss_handle, info_ptr, len);
	if (QAPI_ERROR == status)
	{
		MQTT_UART_DBG("Get IP address error\n");
		return;
	}
	
	j = GET_ADDR_INFO_MIN(len, DSS_ADDR_INFO_SIZE);
	
	for (i = 0; i < j; i++)
	{
		MQTT_UART_DBG("<--- static IP address information --->\n");
		tcp_inet_ntoa(info_ptr[i].iface_addr_s, buff, DSS_ADDR_SIZE);
		MQTT_UART_DBG("static IP: %s\n", buff);

		memset(buff, 0, sizeof(buff));
		tcp_inet_ntoa(info_ptr[i].gtwy_addr_s, buff, DSS_ADDR_SIZE);
		MQTT_UART_DBG("Gateway IP: %s\n", buff);

		memset(buff, 0, sizeof(buff));
		tcp_inet_ntoa(info_ptr[i].dnsp_addr_s, buff, DSS_ADDR_SIZE);
		MQTT_UART_DBG("Primary DNS IP: %s\n", buff);

		memset(buff, 0, sizeof(buff));
		tcp_inet_ntoa(info_ptr[i].dnss_addr_s, buff, DSS_ADDR_SIZE);
		MQTT_UART_DBG("Second DNS IP: %s\n", buff);
	}

	MQTT_UART_DBG("<--- End of system info --->\n");
}

/*
@func
	tcp_set_data_param
@brief
	Set the Parameter for Data Call, such as APN and network tech.
*/
static int tcp_set_data_param(void)
{
    qapi_DSS_Call_Param_Value_t param_info;
	
	/* Initial Data Call Parameter */
	memset(apn, 0, sizeof(apn));
	memset(apn_username, 0, sizeof(apn_username));
	memset(apn_passwd, 0, sizeof(apn_passwd));
	strlcpy(apn, QL_DEF_APN, QAPI_DSS_CALL_INFO_APN_MAX_LEN);

    if (NULL != tcp_dss_handle)
    {
        /* set data call param */
        param_info.buf_val = NULL;
        param_info.num_val = QAPI_DSS_RADIO_TECH_UNKNOWN;	//Automatic mode(or DSS_RADIO_TECH_LTE)
        MQTT_UART_DBG("Setting tech to Automatic\n");
        qapi_DSS_Set_Data_Call_Param(tcp_dss_handle, QAPI_DSS_CALL_INFO_TECH_PREF_E, &param_info);

		/* set apn */
        param_info.buf_val = apn;
        param_info.num_val = strlen(apn);
        MQTT_UART_DBG("Setting APN - %s\n", apn);
        qapi_DSS_Set_Data_Call_Param(tcp_dss_handle, QAPI_DSS_CALL_INFO_APN_NAME_E, &param_info);
#ifdef QUEC_CUSTOM_APN
		/* set apn username */
		param_info.buf_val = apn_username;
        param_info.num_val = strlen(apn_username);
        MQTT_UART_DBG("Setting APN USER - %s\n", apn_username);
        qapi_DSS_Set_Data_Call_Param(tcp_dss_handle, QAPI_DSS_CALL_INFO_USERNAME_E, &param_info);

		/* set apn password */
		param_info.buf_val = apn_passwd;
        param_info.num_val = strlen(apn_passwd);
        MQTT_UART_DBG("Setting APN PASSWORD - %s\n", apn_passwd);
        qapi_DSS_Set_Data_Call_Param(tcp_dss_handle, QAPI_DSS_CALL_INFO_PASSWORD_E, &param_info);
#endif
		/* set IP version(IPv4 or IPv6) */
        param_info.buf_val = NULL;
        param_info.num_val = QAPI_DSS_IP_VERSION_4;
        MQTT_UART_DBG("Setting family to IPv%d\n", param_info.num_val);
        qapi_DSS_Set_Data_Call_Param(tcp_dss_handle, QAPI_DSS_CALL_INFO_IP_VERSION_E, &param_info);
    }
    else
    {
        MQTT_UART_DBG("Dss handler is NULL!!!\n");
		return -1;
    }
	
    return 0;
}

/*
@func
	tcp_inet_ntoa
@brief
	utility interface to translate ip from "int" to x.x.x.x format.
*/
int32 tcp_inet_ntoa
(
	const qapi_DSS_Addr_t    inaddr, /* IPv4 address to be converted         */
	uint8                   *buf,    /* Buffer to hold the converted address */
	int32                    buflen  /* Length of buffer                     */
)
{
	uint8 *paddr  = (uint8 *)&inaddr.addr.v4;
	int32  rc = 0;

	if ((NULL == buf) || (0 >= buflen))
	{
		rc = -1;
	}
	else
	{
		if (-1 == snprintf((char*)buf, (unsigned int)buflen, "%d.%d.%d.%d",
							paddr[0],
							paddr[1],
							paddr[2],
							paddr[3]))
		{
			rc = -1;
		}
	}

	return rc;
} /* tcp_inet_ntoa() */

/*
@func
	tcp_netctrl_init
@brief
	Initializes the DSS netctrl library for the specified operating mode.
*/
static int tcp_netctrl_init(void)
{
	int ret_val = 0;
	qapi_Status_t status = QAPI_OK;

	MQTT_UART_DBG("Initializes the DSS netctrl library\n");

	/* Initializes the DSS netctrl library */
	if (QAPI_OK == qapi_DSS_Init(QAPI_DSS_MODE_GENERAL))
	{
		tcp_netctl_lib_status = DSS_LIB_STAT_SUCCESS_E;
		MQTT_UART_DBG("qapi_DSS_Init success\n");
	}
	else
	{
		/* @Note: netctrl library has been initialized */
		tcp_netctl_lib_status = DSS_LIB_STAT_FAIL_E;
		MQTT_UART_DBG("DSS netctrl library has been initialized.\n");
	}
	
	/* Registering callback tcp_dss_handleR */
	do
	{
		MQTT_UART_DBG("Registering Callback tcp_dss_handle\n");
		
		/* Obtain data service handle */
		status = qapi_DSS_Get_Data_Srvc_Hndl(tcp_net_event_cb, NULL, &tcp_dss_handle);
		MQTT_UART_DBG("tcp_dss_handle %d, status %d\n", tcp_dss_handle, status);
		
		if (NULL != tcp_dss_handle)
		{
			MQTT_UART_DBG("Registed tcp_dss_handler success\n");
			break;
		}

		/* Obtain data service handle failure, try again after 10ms */
		qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_MSEC, true);
	} while(1);

	return ret_val;
}

/*
@func
	tcp_netctrl_start
@brief
	Start the DSS netctrl library, and startup data call.
*/
int tcp_netctrl_start(void)
{
	int rc = 0;
	qapi_Status_t status = QAPI_OK;
		
	rc = tcp_netctrl_init();
	if (0 == rc)
	{
		/* Get valid DSS handler and set the data call parameter */
		tcp_set_data_param();
	}
	else
	{
		MQTT_UART_DBG("quectel_dss_init fail.\n");
		return -1;
	}

	MQTT_UART_DBG("qapi_DSS_Start_Data_Call start!!!.\n");
	status = qapi_DSS_Start_Data_Call(tcp_dss_handle);
	if (QAPI_OK == status)
	{
		MQTT_UART_DBG("Start Data service success.\n");
		return 0;
	}
	else
	{
		return -1;
	}
}

/*
@func
	tcp_netctrl_release
@brief
	Cleans up the DSS netctrl library and close data service.
*/
int tcp_netctrl_stop(void)
{
	qapi_Status_t stat = QAPI_OK;
	
	if (tcp_dss_handle)
	{
		stat = qapi_DSS_Stop_Data_Call(tcp_dss_handle);
		if (QAPI_OK == stat)
		{
			MQTT_UART_DBG("Stop data call success\n");
		}
	}
	
	return 0;
}	

/*
@func
	quec_dataservice_entry
@brief
	The entry of data service task.
*/
void quec_dataservice_thread(ULONG param)
{
	ULONG dss_event = 0;
	
	/* Start data call */
	tcp_netctrl_start();

	while (1)
	{
		/* Wait disconnect signal */
		tx_event_flags_get(tcp_signal_handle, DSS_SIG_EVT_DIS_E, TX_OR, &dss_event, TX_WAIT_FOREVER);
		if (dss_event & DSS_SIG_EVT_DIS_E)
		{
			/* Stop data call and release resource */
			tcp_netctrl_stop();
			MQTT_UART_DBG("Data service task exit.\n");
			break;
		}
	}

	MQTT_UART_DBG("Data Service Thread Exit!\n");
	return;
}

void dns_parser(const char *hostname, struct ip46addr *ipaddr, const char *iface)
{
	int32 e = 0;
	char ip_str[48];

	MQTT_UART_DBG("hostname: %s is resoling in interface: %s\n", hostname, iface);

	e = qapi_Net_DNSc_Reshost_on_iface(hostname, ipaddr, iface);
    MQTT_UART_DBG("reshost ret: %d\n", e);
    
	if (e)
	{
		MQTT_UART_DBG("Unable to resolve %s\n", hostname);
	}
	else
	{
		MQTT_UART_DBG("\n%s --> %s\n", hostname, inet_ntop(ipaddr->type, &ipaddr->a, ip_str, sizeof(ip_str)));
	}
}

void get_ip_from_url(const char *hostname, struct ip46addr *ipaddr)
{
	int i, j = 0;

	unsigned int len = 0;
	qapi_Status_t status;
	char iface[15] = {0};
	char first_dns[DSS_ADDR_SIZE] = {0};
	char second_dns[DSS_ADDR_SIZE] = {0};
	qapi_DSS_Addr_Info_t info_ptr[DSS_ADDR_INFO_SIZE];

	/* Get DNS server address */
	status = qapi_DSS_Get_IP_Addr_Count(tcp_dss_handle, &len);
	if (QAPI_ERROR == status)
	{
		MQTT_UART_DBG("Get IP address count error\n");
		return;
	}
		
	status = qapi_DSS_Get_IP_Addr(tcp_dss_handle, info_ptr, len);
	if (QAPI_ERROR == status)
	{
		MQTT_UART_DBG("Get IP address error\n");
		return;
	}
	
	j = GET_ADDR_INFO_MIN(len, DSS_ADDR_INFO_SIZE);
	MQTT_UART_DBG("@@@j = %d\n", j);
	
	for (i = 0; i < j; i++)
	{
		memset(first_dns, 0, sizeof(first_dns));
		tcp_inet_ntoa(info_ptr[i].dnsp_addr_s, (uint8*)first_dns, DSS_ADDR_SIZE);
		MQTT_UART_DBG("Primary DNS IP: %s\n", first_dns);

		memset(second_dns, 0, sizeof(second_dns));
		tcp_inet_ntoa(info_ptr[i].dnss_addr_s, (uint8*)second_dns, DSS_ADDR_SIZE);
		MQTT_UART_DBG("Second DNS IP: %s\n", second_dns);
	}

	/* Start DNS service */
	qapi_Net_DNSc_Command(QAPI_NET_DNS_START_E);
	MQTT_UART_DBG("Start DNSc.........");

	/* Get current active iface */
	memset(iface, 0, sizeof(iface));
	qapi_DSS_Get_Device_Name(tcp_dss_handle, iface, 15);
	MQTT_UART_DBG("device_name: %s\n", iface);

	/* Add dns server into corresponding interface */
	qapi_Net_DNSc_Add_Server_on_iface(first_dns, QAPI_NET_DNS_V4_PRIMARY_SERVER_ID, iface);
	qapi_Net_DNSc_Add_Server_on_iface(second_dns, QAPI_NET_DNS_V4_SECONDARY_SERVER_ID, iface);

	/* URL parser */
	dns_parser(hostname, ipaddr, iface);
}

#ifdef NW_SIGNAL_ENABLE
void mqtt_network_indication_cb(qapi_Device_Info_Hndl_t device_info_hndl, const qapi_Device_Info_t *dev_info)
{
	if(dev_info->id == QAPI_DEVICE_INFO_NETWORK_IND_E)
	{
		switch(dev_info->info_type)
		{
			case QAPI_DEVICE_INFO_TYPE_BOOLEAN_E:
				MQTT_UART_DBG( "~type[b] id[%d] status[%d]\n", dev_info->id, dev_info->u.valuebool);
				if(dev_info->u.valuebool == true)
				{
					tx_event_flags_set(mqtt_nw_signal_handle, NW_SIG_EVT_ENABLE, TX_OR);
				}
				else
				{
					tx_event_flags_set(mqtt_nw_signal_handle, NW_SIG_EVT_UNENABLE, TX_OR);
				}
			break;

			case QAPI_DEVICE_INFO_TYPE_INTEGER_E:
				MQTT_UART_DBG( "~type[i] id[%d] status[%d]\n", dev_info->id, dev_info->u.valueint);
			break;

			case QAPI_DEVICE_INFO_TYPE_BUFFER_E:
				MQTT_UART_DBG( "~type[s] id[%d] status[%s]\n", dev_info->id, dev_info->u.valuebuf.buf);
			break;

			default:
				MQTT_UART_DBG( "~[Invalid][%d]\n", dev_info->id);
			break;
		}
	}
}

void qt_mqtt_init_device_info(void)
{
	qapi_Status_t status = 0;

	status = qapi_Device_Info_Init_v2(&device_info_hndl);
	if(status != QAPI_OK)
	{
		MQTT_UART_DBG( "~ qapi_Device_Info_Init fail [%d]\n", status);
	}
	else
	{
		MQTT_UART_DBG( "~ qapi_Device_Info_Init OK [%d]\n", status);
	}
}
#endif /* NW_SIGNAL_ENABLE */

/*
@func
	quectel_task_entry
@brief
	The entry of data service task.
*/
int quectel_task_entry(void)
{

	int     ret = 0;
	ULONG   dss_event = 0;
	int32   sig_mask;
	ULONG   nw_event = 0;
	int32   nw_sig_mask;
	qapi_Status_t status = QAPI_OK;
	
#ifndef NW_SIGNAL_ENABLE 
	/* wait 10sec(set according to the time required for network registration) for device startup.
	if checking network status by device_info, do not need timer sleep. */
	qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_SEC, true);
#endif

	/* Create a byte memory pool. */
	txm_module_object_allocate(&byte_pool_mqtt, sizeof(TX_BYTE_POOL));
	tx_byte_pool_create(byte_pool_mqtt, "mqtt application pool", free_memory_mqtt, MQTT_BYTE_POOL_SIZE);

	/* Initial uart for debug */
	mqtt_uart_dbg_init();
	MQTT_UART_DBG("MQTT Task Start...\n");

#ifdef NW_SIGNAL_ENABLE
	txm_module_object_allocate(&mqtt_nw_signal_handle, sizeof(TX_EVENT_FLAGS_GROUP));
	tx_event_flags_create(mqtt_nw_signal_handle, "nw_signal_event");
	tx_event_flags_set(mqtt_nw_signal_handle, 0x0, TX_AND);

	qt_mqtt_init_device_info();
	status = qapi_Device_Info_Pass_Pool_Ptr(device_info_hndl, byte_pool_mqtt);
	MQTT_UART_DBG("[dev_info] status, status %d\n", status);
	status = qapi_Device_Info_Set_Callback_v2(device_info_hndl, QAPI_DEVICE_INFO_NETWORK_IND_E, mqtt_network_indication_cb);
	MQTT_UART_DBG("[dev_info] qapi_Device_Info_Set_Callback, status %d\n", status);

	nw_sig_mask = NW_SIG_EVT_UNENABLE | NW_SIG_EVT_ENABLE;
	while(1)
	{
		tx_event_flags_get(mqtt_nw_signal_handle, nw_sig_mask, TX_OR, &nw_event, TX_WAIT_FOREVER);

		if (nw_event & NW_SIG_EVT_UNENABLE)
		{
			MQTT_UART_DBG("waiting for network registering...\n");
			tx_event_flags_set(mqtt_nw_signal_handle, ~NW_SIG_EVT_UNENABLE, TX_AND);
		}
		else if (nw_event & NW_SIG_EVT_ENABLE)
		{
			MQTT_UART_DBG("network registered!\n");
			tx_event_flags_set(mqtt_nw_signal_handle, ~NW_SIG_EVT_ENABLE, TX_AND);
			break;
		}
	}
#endif /* NW_SIGNAL_ENABLE */

	/* Create event signal handle and clear signals */
	txm_module_object_allocate(&tcp_signal_handle, sizeof(TX_EVENT_FLAGS_GROUP));
	tx_event_flags_create(tcp_signal_handle, "dss_signal_event");
	tx_event_flags_set(tcp_signal_handle, 0x0, TX_AND);

	/* Start DSS thread, and detect iface status */
#ifdef QAPI_TXM_MODULE
	if (TX_SUCCESS != txm_module_object_allocate((VOID *)&dss_thread_handle, sizeof(TX_THREAD))) 
	{
        MQTT_UART_DBG("Txm module object allocate failed\n");
		return -1;
	}
#endif
	ret = tx_thread_create(dss_thread_handle, "TCPCLINET DSS Thread", quec_dataservice_thread, NULL,
							tcp_dss_stack, THREAD_STACK_SIZE, THREAD_PRIORITY, THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);
	if (ret != TX_SUCCESS)
	{
		MQTT_UART_DBG("Thread creation failed\n");
	}

	sig_mask = DSS_SIG_EVT_INV_E | DSS_SIG_EVT_NO_CONN_E | DSS_SIG_EVT_CONN_E | DSS_SIG_EVT_EXIT_E;
	
	while (1)
	{
		/* TCPClient signal process */
		tx_event_flags_get(tcp_signal_handle, sig_mask, TX_OR, &dss_event, TX_WAIT_FOREVER);
		MQTT_UART_DBG("SIGNAL EVENT IS [%d]\n", dss_event);
		
		if (dss_event & DSS_SIG_EVT_INV_E)
		{
			MQTT_UART_DBG("DSS_SIG_EVT_INV_E Signal\n");
			tx_event_flags_set(tcp_signal_handle, ~DSS_SIG_EVT_INV_E, TX_AND);
		}
		else if (dss_event & DSS_SIG_EVT_NO_CONN_E)
		{
			MQTT_UART_DBG("DSS_SIG_EVT_NO_CONN_E Signal\n");
			tx_event_flags_set(tcp_signal_handle, ~DSS_SIG_EVT_NO_CONN_E, TX_AND);
		}
		else if (dss_event & DSS_SIG_EVT_CONN_E)
		{
			MQTT_UART_DBG("DSS_SIG_EVT_CONN_E Signal\n");

			/* Connect to mqtt server */
          conn_mqtt();
        
          /* Subscribe topic*/
          sub_mqtt();

          /* Publish msg with a topic*/
          pub_mqtt();

          /* wait for the interaction to complete */
          qapi_Timer_Sleep(8, QAPI_TIMER_UNIT_SEC, true);
          
          /* Disconnect to mqtt server */
          disconn_mqtt();

          tx_event_flags_set(tcp_signal_handle, ~DSS_SIG_EVT_CONN_E, TX_AND);
		}
		else if (dss_event & DSS_SIG_EVT_EXIT_E)
		{
			MQTT_UART_DBG("DSS_SIG_EVT_EXIT_E Signal\n");
			tx_event_flags_set(tcp_signal_handle, ~DSS_SIG_EVT_EXIT_E, TX_AND);
			tx_event_flags_delete(tcp_signal_handle);
			break;
		}
		else
		{
			MQTT_UART_DBG("Unkonw Signal\n");
		}

		/* Clear all signals and wait next notification */
		tx_event_flags_set(tcp_signal_handle, 0x0, TX_AND);	//@Fixme:maybe not need
	}
	
	return 0;
}

#endif /*__EXAMPLE_MQTT__*/
