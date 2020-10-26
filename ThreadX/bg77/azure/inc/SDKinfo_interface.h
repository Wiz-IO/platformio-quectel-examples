
#ifndef SDKINFO_INTERFACE_H
#define SDKINFO_INTERFACE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "digitaltwin_interface_client.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/threadapi.h"

#include "digitaltwin_client_helper.h"
#include "digitaltwin_serializer.h"
#include "parson.h"
#include "../sample_device_impl.h"

#ifdef __cplusplus
extern "C"
{
#endif


static const char SDKinfoInterfaceId[] = "urn:azureiot:Client:SDKInformation:1";
static const char SDKinfoInterfaceInstanceName[] = "urn_azureiot_Client_SDKInformation";

#endif  // SENSOR_INTERFACE_H


