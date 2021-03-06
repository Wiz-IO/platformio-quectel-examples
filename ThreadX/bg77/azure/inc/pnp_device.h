/*********************************************************************************************
 * This code was automatically generated by Digital Twin Code Generator tool 0.6.7.
 *
 * Generated Date: 12/24/2019
 *********************************************************************************************/

#ifndef PNP_DEVICE
#define PNP_DEVICE

#include "iothub.h"
#include "iothub_client_version.h"
#include "azure_c_shared_utility/tickcounter.h"
#include "azure_c_shared_utility/shared_util_options.h"
#include "azure_c_shared_utility/http_proxy_io.h"

#include "iothub_device_client_ll.h"
#include "iothub_client_options.h"
#include "azure_prov_client/prov_device_ll_client.h"
#include "azure_prov_client/prov_security_factory.h"
#include "azure_prov_client/prov_transport_mqtt_client.h"
#include "azure_prov_client/internal/prov_transport_mqtt_common.h"


#include "digitaltwin_device_client_ll.h"
#include "digitaltwin_interface_client.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/tickcounter.h"
#include "utilities/digitaltwin_client_helper.h"

#include "utilities/deviceinfo_interface.h"

#include "utilities/sensor_interface.h"

#include "sample_device_impl.h"

#ifdef __cplusplus
extern "C"
{
#endif

int pnp_device_initialize( char *symm_auth,const char* trustedCert);

void pnp_device_run(char* lat, char* lon);

void pnp_device_close();

DIGITALTWIN_INTERFACE_CLIENT_HANDLE SDKinfoInterface_Create();

bool IsSendingEnds();
#ifdef __cplusplus
}
#endif

#endif // PNP_DEVICE
