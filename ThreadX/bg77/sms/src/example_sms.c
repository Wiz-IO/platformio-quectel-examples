/******************************************************************************
*@file    example_sms.c
*@brief   example of SMS
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/

#if defined(__EXAMPLE_SMS__)
#include "txm_module.h"
#include "qapi_diag.h"
#include "qapi_timer.h"
#include "qapi_uart.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "qapi_device_info.h"
#include "qapi_quectel.h"
#include "qapi_status.h"
#include "example_sms.h"


#define QMI_CLNT_WAIT_SIG      (0x1 << 5)
#define QMI_CLNT_TIMER_SIG     (0x1 << 6)
#define MAX_QMI_ATTEMPTS  5
#define QMI_WMS_TIMEOUT_MS 10000
#define SEND_MSG_ADDR "08010"
#define SEND_MSG_DATA "hello"
#define SERVICE_CENTER_ADDRESS "+8613800210500"

TX_BYTE_POOL *byte_pool_sms;
#define SMS_BYTE_POOL_SIZE		16*8*1024
UCHAR free_memory_sms[SMS_BYTE_POOL_SIZE];

/* uart rx tx buffer */
static char *rx_buff = NULL; /*!!! should keep this buffer as 4K Bytes */
static char *tx_buff = NULL;

/* uart config para*/
QT_UART_CONF_PARA uart_conf = 
{
	NULL,
	QT_QAPI_UART_PORT_01,
	NULL,
	0,
	NULL,
	0,
	115200
};

/***************************************************************
 *						         FUNC										 *
 ***************************************************************/
/*
@func
	quectel_task_entry
@brief
	Entry function for task.
*/
int quectel_task_entry
(
    void
)
{
    int ret = -1;
    int i;
    qapi_Status_t   status = QAPI_ERROR;
    uint16_t        Service_state = 0;
    uint16_t        index = 0, index1 = 1;
    qapi_QT_SMS_Message_Rcvd_t msg_info;
    qapi_QT_SMS_Message_Content_t send_message;
    qapi_QT_SMS_Para_t  sms_para;
	char            msg_buffer[128] = {0};
    uint16_t        used = 0;
    char            sms_cpms_info[128] = {0}; 
    qapi_QT_SMS_CPMS_Set_t sms_cpms;
    qapi_QT_Chset_Type_e cscs_val = ALPHA_MAX ;
    qapi_QT_SMS_Message_Store_t msg_store_num;
    wms_message_tag_type_enum_v01 msg_status = WMS_MESSAGE_TAG_TYPE_ENUM_MIN_ENUM_VAL_V01;
    char sca[48]={0};
    qapi_QT_SMS_CSCA_Type_e sca_type = QT_UNKNOW_NUMBER;

    qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_SEC, true);

    IOT_DEBUG("QT# quectel_task_entry Start");

    ret = txm_module_object_allocate(&byte_pool_sms, sizeof(TX_BYTE_POOL));
  	if(ret != TX_SUCCESS)
  	{
  		IOT_DEBUG("txm_module_object_allocate [byte_pool_sensor] failed, %d", ret);
    	return ret;
  	}

	ret = tx_byte_pool_create(byte_pool_sms, "Sensor application pool", free_memory_sms, SMS_BYTE_POOL_SIZE);
  	if(ret != TX_SUCCESS)
  	{
  		IOT_DEBUG("tx_byte_pool_create [byte_pool_sensor] failed, %d", ret);
    	return ret;
  	}

  	ret = tx_byte_allocate(byte_pool_sms, (VOID *)&rx_buff, 4*1024, TX_NO_WAIT);
  	if(ret != TX_SUCCESS)
  	{
  		IOT_DEBUG("tx_byte_allocate [rx_buff] failed, %d", ret);
    	return ret;
  	}

  	ret = tx_byte_allocate(byte_pool_sms, (VOID *)&tx_buff, 4*1024, TX_NO_WAIT);
  	if(ret != TX_SUCCESS)
  	{
  		IOT_DEBUG("tx_byte_allocate [tx_buff] failed, %d", ret);
    	return ret;
  	}

	uart_conf.tx_buff = tx_buff;
	uart_conf.rx_buff = rx_buff;
	uart_conf.tx_len = 4096;
	uart_conf.rx_len = 4096;

	/* uart 1 init */
	uart_init(&uart_conf);
	/* start uart 1 receive */
	uart_recv(&uart_conf);
	/* prompt task running */
	qt_uart_dbg(uart_conf.hdlr,"SMS Example...\r\n");

/********************************Get SMS service status********************************/
    //Must get sms service ready first, Service_state=1
	qt_uart_dbg(uart_conf.hdlr,"GET SMS service status...\r\n");

    do
    {
	    qapi_Timer_Sleep(3, QAPI_TIMER_UNIT_SEC, true);
        status = qapi_QT_SMS_Get_Service_Ready_Status(&Service_state);
        qt_uart_dbg(uart_conf.hdlr,"SMS service state:%d",Service_state);
    }while( status != QAPI_OK || Service_state != 1 );

    qt_uart_dbg(uart_conf.hdlr,"#SMS service state:%d \r\n",Service_state);


 /********************************Read msg********************************/
 	qt_uart_dbg(uart_conf.hdlr,"Read SMS index:%d \r\n",index);

    status = qapi_QT_SMS_Message_Read(index, &msg_info);
    if( status != QAPI_OK )
    {
	    qt_uart_dbg(uart_conf.hdlr,"MSG read error \r\n");
    }
    else
    {
        qt_uart_dbg(uart_conf.hdlr,"SMS status:%d, data:%s sender_num:%s\r\n",msg_info.status, msg_info.sms_info.data,(char*)msg_info.sms_info.sender_num);
        IOT_DEBUG("SMS data_len:%d, data:%s, sender_num:%s", msg_info.sms_info.data_len,msg_info.sms_info.data,msg_info.sms_info.sender_num);
    }


/********************************Send msg********************************/ 
    qt_uart_dbg(uart_conf.hdlr,"Send SMS...\r\n");
    memset(&send_message, 0, sizeof(qapi_QT_SMS_Message_Content_t));
    memcpy(send_message.address, SEND_MSG_ADDR, strlen(SEND_MSG_ADDR));
    memcpy(send_message.message, SEND_MSG_DATA, strlen(SEND_MSG_DATA));

    status = qapi_QT_SMS_Message_Send(&send_message);
    if( status != QAPI_OK )
    {
        IOT_DEBUG("Send sms fail");
        qt_uart_dbg(uart_conf.hdlr,"Send SMS fail\r\n");
    }
    else
    {
        qt_uart_dbg(uart_conf.hdlr,"Send SMS success \r\n");
    }

/********************************Delete msg********************************/ 
    qt_uart_dbg(uart_conf.hdlr,"Delete SMS index:%d ... \r\n",index1);

    status = qapi_QT_SMS_Message_Delete(index1);
    if( status != QAPI_OK )
    {
      IOT_DEBUG("Delete sms fail");
      qt_uart_dbg(uart_conf.hdlr,"Delete SMS fail \r\n");
    }

/********************************Get sms_pms********************************/

    qt_uart_dbg(uart_conf.hdlr,"Get sms_cpms... \r\n");
    memset(&sms_cpms_info,0,sizeof(qapi_QT_SMS_Para_t));
    status = qapi_QT_SMS_CPMS_Get(&sms_cpms_info);
    if( status != QAPI_OK )
    {
      IOT_DEBUG("Get sms_cpms fail");
      qt_uart_dbg(uart_conf.hdlr,"Get sms_cpms fail \r\n");
    }
    else
    {
      IOT_DEBUG("Get sms_cpms:%s",sms_cpms_info);
      qt_uart_dbg(uart_conf.hdlr,"Get sms_cpms:%s \r\n",sms_cpms_info);
    }


/********************************Set sms_pms********************************/
    qt_uart_dbg(uart_conf.hdlr,"Set sms_cpms... \r\n");

    sms_cpms.mem1 = QT_WMS_MEMORY_STORE_ME;
    sms_cpms.mem2 = QT_WMS_MEMORY_STORE_ME;
    sms_cpms.mem3 = QT_WMS_MEMORY_STORE_ME;


    status = qapi_QT_SMS_CPMS_Set(&sms_cpms);
    if( status != QAPI_OK )
    {
      IOT_DEBUG("Set sms_cpms fail");
      qt_uart_dbg(uart_conf.hdlr,"Set sms_cpms fail \r\n");
    }

    
/********************************Get sms_para********************************/
    qt_uart_dbg(uart_conf.hdlr,"Get sms_para... \r\n");
    status = qapi_QT_SMS_Para_Get(&sms_para);
    if( status != QAPI_OK )
    {
      IOT_DEBUG("Get sms_para fail");
      qt_uart_dbg(uart_conf.hdlr,"Get sms_para fail \r\n");
    }
    else
    {
      IOT_DEBUG("Get sms_para vp:%d, pid:%d, dcs:%d \r\n",sms_para.vp,sms_para.pid,sms_para.dcs);
      qt_uart_dbg(uart_conf.hdlr,"Get sms_para vp:%d, pid:%d, dcs:%d \r\n",sms_para.vp,sms_para.pid,sms_para.dcs);
    }


/********************************Set sms_para********************************/
    qt_uart_dbg(uart_conf.hdlr,"Set sms_para... \r\n");

    sms_para.vp = 167;
    sms_para.pid = 0;
    sms_para.dcs = 240;
    
    status = qapi_QT_SMS_Para_Set(&sms_para);
    if( status != QAPI_OK )
    {
      IOT_DEBUG("Set sms_para fail");
      qt_uart_dbg(uart_conf.hdlr,"Set sms_para fail \r\n");
    }


/********************************Get Character********************************/
    qt_uart_dbg(uart_conf.hdlr,"Get Character... \r\n");

    status = qapi_QT_SMS_Chset_Get(&cscs_val);
    if( status != QAPI_OK )
    {
      IOT_DEBUG("Get Character fail");
      qt_uart_dbg(uart_conf.hdlr,"Get Character fail \r\n");
    }
    else
    {
      qt_uart_dbg(uart_conf.hdlr,"Get Character:%d \r\n",cscs_val);
    }


/********************************Set Character********************************/
    qt_uart_dbg(uart_conf.hdlr,"Set Character... \r\n");
    cscs_val = ALPHA_GSM;

    status = qapi_QT_SMS_Chset_Set(cscs_val);
    if( status != QAPI_OK )
    {
      IOT_DEBUG("Set Character fail");
      qt_uart_dbg(uart_conf.hdlr,"Set Character fail \r\n");
    }
    else
    {
      qt_uart_dbg(uart_conf.hdlr,"Set Character sucess \r\n");

    }

    
/********************************Get sms_store_num********************************/
    qt_uart_dbg(uart_conf.hdlr,"Get sms_store_num... \r\n");

    memset(&msg_store_num,0,sizeof(qapi_QT_SMS_Message_Store_t));
    status = qapi_QT_SMS_Rcvd_Num(&msg_store_num);
    if( status != QAPI_OK )
    {
      IOT_DEBUG("Get sms_store_num fail");
      qt_uart_dbg(uart_conf.hdlr,"Get sms_store_num fail \r\n");
    }
    else
    {
      IOT_DEBUG("Get sms_store_num used num:%d, total num:%d ",msg_store_num.used_num,msg_store_num.max_num);
      qt_uart_dbg(uart_conf.hdlr,"Get sms_store_num used num:%d, total num:%d \r\n",msg_store_num.used_num,msg_store_num.max_num);
    }


/********************************Get sms_status********************************/
    qt_uart_dbg(uart_conf.hdlr,"Get sms_status... \r\n");

    status = qapi_QT_SMS_Message_Status_Get(index,&msg_status);
    if( status != QAPI_OK)
    {
      IOT_DEBUG("Get sms_status fail");
      qt_uart_dbg(uart_conf.hdlr,"Get sms_status fail \r\n");
    }
    else
    {
        IOT_DEBUG("Get sms_status:%d ",msg_status);
        qt_uart_dbg(uart_conf.hdlr,"Get sms_status:%d \r\n",msg_status);
    }

/********************************Get csca********************************/
    qt_uart_dbg(uart_conf.hdlr,"Get csca... \r\n");

    status = qapi_QT_SMS_CSCA_Get(sca,&sca_type);
    if( status != QAPI_OK)
    {
      IOT_DEBUG("Get csca fail");
      qt_uart_dbg(uart_conf.hdlr,"Get csca fail \r\n");
    }
    else
    {
        IOT_DEBUG("Get csca type:%d number:%s\r\n",sca_type,sca);
        qt_uart_dbg(uart_conf.hdlr,"Get csca type:%d number:%s\r\n",sca_type,sca);        
    }

 
/********************************Set csca********************************/
    qt_uart_dbg(uart_conf.hdlr,"Set csca... \r\n");

	memset(sca,0,48);
    memcpy(sca, SERVICE_CENTER_ADDRESS,strlen(SERVICE_CENTER_ADDRESS));
    status = qapi_QT_SMS_CSCA_Set(sca,sca_type);
    if( status != QAPI_OK)
    {
      IOT_DEBUG("Set csca fail");
      qt_uart_dbg(uart_conf.hdlr,"Set csca fail \r\n");
    }
    else
    {
        IOT_DEBUG("Set csca_type:%d csca_number:%s \r\n",sca_type,sca);
        qt_uart_dbg(uart_conf.hdlr,"Set csca_type:%d csca_number:%s \r\n",sca_type,sca);
    }


/********************************SIM Example End********************************/
    qt_uart_dbg(uart_conf.hdlr,"SIM Example End \r\n");

    return 0;
}

#endif

