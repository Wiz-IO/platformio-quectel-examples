#include "SDKinfo_interface.h"

//
//  Property names and data for DigitalTwin read-only properties for this interface.
//
static const char DT_SdkLanguage_Property[] = "language";
static const char DT_SdkVersion_Property[] = "version";
static const char DT_SdkVendor_Property[] = "vendor";
static const char DT_SdkLanguage[] = "\"C\"";
static const char DT_SdkVendor[] = "\"Microsoft\"";

typedef struct SDKINFO_INTERFACE_STATE_TAG
{
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE interfaceClientHandle;

} SDKINFO_INTERFACE_STATE;

static SDKINFO_INTERFACE_STATE appState;

// DigitalTwinSampleSdkInfo_PropertyCallback is invoked when a property is updated (or failed) going to server.
// In this sample, we route ALL property callbacks to this function and just have the userContextCallback set
// to the propertyName.  Product code will potentially have context stored in this userContextCallback.
static void DTSdkInfo_PropertyCallback(DIGITALTWIN_CLIENT_RESULT dtReportedStatus, void* userContextCallback)
{
    if (dtReportedStatus == DIGITALTWIN_CLIENT_OK)
    {
        LogInfo("SDK_INFO: Property callback property=<%s> succeeds", (const char*)userContextCallback);
    }
    else
    {
        LogError("SDK_INFO: Property callback property=<%s> fails, error=<%s>", (const char*)userContextCallback, MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, dtReportedStatus));
    }
}


//
// DTSdkInfo_ReportPropertyAsync is a helper function to report a SdkInfo's properties.
// It invokes underlying DigitalTwin API for reporting properties and sets up its callback on completion.
//
static DIGITALTWIN_CLIENT_RESULT DTSdkInfo_ReportPropertyAsync(DIGITALTWIN_INTERFACE_CLIENT_HANDLE interfaceHandle, const char* propertyName, const char* propertyData)
{
    DIGITALTWIN_CLIENT_RESULT result = DIGITALTWIN_CLIENT_ERROR;

    result = DigitalTwin_InterfaceClient_ReportPropertyAsync(interfaceHandle, propertyName,
        (const unsigned char*)propertyData, strlen(propertyData), NULL,
        DTSdkInfo_PropertyCallback, (void*)propertyName);

    if (result != DIGITALTWIN_CLIENT_OK)
    {
        LogError("SDK_INFO: Reporting property=<%s> failed, error=<%s>", propertyName, MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, result));
    }
    else
    {
        LogInfo("SDK_INFO: Queued async report read only property for %s", propertyName);
    }

    return result;
}

// DigitalTwinSampleSdkInfo_ReportSdkInfoAsync() reports all SdkInfo properties to the service
static DIGITALTWIN_CLIENT_RESULT DTSdkInfo_ReportSdkInfoAsync(DIGITALTWIN_INTERFACE_CLIENT_HANDLE interfaceHandle)
{
    DIGITALTWIN_CLIENT_RESULT result;

    char versionString[32];
    sprintf(versionString, "\"%s\"", DigitalTwin_Client_GetVersionString());

    // NOTE: Future versions of SDK will support ability to send mulitiple properties in a single
    // send.  For now, one at a time is sufficient albeit less effecient.

    if (((result = DTSdkInfo_ReportPropertyAsync(interfaceHandle, DT_SdkLanguage_Property, DT_SdkLanguage)) != DIGITALTWIN_CLIENT_OK) ||
        ((result = DTSdkInfo_ReportPropertyAsync(interfaceHandle, DT_SdkVersion_Property, versionString)) != DIGITALTWIN_CLIENT_OK) ||
        ((result = DTSdkInfo_ReportPropertyAsync(interfaceHandle, DT_SdkVendor_Property, DT_SdkVendor)) != DIGITALTWIN_CLIENT_OK))
    {
        LogError("SDK_INFO: Reporting properties failed.");
    }
    else
    {
        LogInfo("SDK_INFO: Queing of all properties to be reported has succeeded");
    }

    return result;
}


static void SDKinfoInterface_InterfaceRegisteredCallback(DIGITALTWIN_CLIENT_RESULT dtInterfaceStatus, void* userInterfaceContext)
{
    LogInfo("SDKinfoInterface_InterfaceRegisteredCallback with status=<%s>, userContext=<%p>", MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, dtInterfaceStatus), userInterfaceContext);

	SDKINFO_INTERFACE_STATE *sdkInfoState = (SDKINFO_INTERFACE_STATE*)userInterfaceContext;

	if (dtInterfaceStatus == DIGITALTWIN_CLIENT_OK)
    {
        // Once the interface is registered, send our reported properties to the service.  
        // It *IS* safe to invoke most DigitalTwin API calls from a callback thread like this, though it 
        // is NOT safe to create/destroy/register interfaces now.
        LogInfo("SDKINFO_INTERFACE: Interface successfully registered.");
		DTSdkInfo_ReportSdkInfoAsync(sdkInfoState->interfaceClientHandle);
    }
    else if (dtInterfaceStatus == DIGITALTWIN_CLIENT_ERROR_INTERFACE_UNREGISTERING)
    {
        // Once an interface is marked as unregistered, it cannot be used for any DigitalTwin SDK calls.
        LogInfo("SDKINFO_INTERFACE: Interface received unregistering callback.");
    }
    else
    {
        LogError("SDKINFO_INTERFACE: Interface received failed, status=<%s>.", MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, dtInterfaceStatus));
    }
}

DIGITALTWIN_INTERFACE_CLIENT_HANDLE SDKinfoInterface_Create()
{
    DIGITALTWIN_INTERFACE_CLIENT_HANDLE interfaceHandle;
    DIGITALTWIN_CLIENT_RESULT result;

    memset(&appState, 0, sizeof(SDKINFO_INTERFACE_STATE));

    if ((result = DigitalTwin_InterfaceClient_Create(SDKinfoInterfaceId,  SDKinfoInterfaceInstanceName, SDKinfoInterface_InterfaceRegisteredCallback, (void*)&appState, &interfaceHandle)) != DIGITALTWIN_CLIENT_OK)
    {
        LogError("DEVICEINFO_INTERFACE: Unable to allocate interface client handle for interfaceId=<%s>, interfaceInstanceName=<%s>, error=<%s>", SDKinfoInterfaceId, SDKinfoInterfaceInstanceName, MU_ENUM_TO_STRING(DIGITALTWIN_CLIENT_RESULT, result));
        interfaceHandle = NULL;
    }

    else
    {
        LogInfo("SDKINFO_INTERFACE: Created DIGITALTWIN_INTERFACE_CLIENT_HANDLE successfully for interfaceId=<%s>, interfaceInstanceName=<%s>, handle=<%p>", SDKinfoInterfaceId, SDKinfoInterfaceInstanceName, interfaceHandle);
        appState.interfaceClientHandle = interfaceHandle;
		//LogInfo("deviceinfo interface - appState.interfaceClientHandle:%d........\n",appState.interfaceClientHandle);
    }

    return interfaceHandle;
}


//
// DTSdkInfo_Close is invoked when the sample device is shutting down.
//
void DTSdkInfo_Close(DIGITALTWIN_INTERFACE_CLIENT_HANDLE interfaceHandle)
{
    // On shutdown, in general the first call made should be to DigitalTwin_InterfaceClient_Destroy.
    // This will block if there are any active callbacks in this interface, and then
    // mark the underlying handle such that no future callbacks shall come to it.
    DigitalTwin_InterfaceClient_Destroy(interfaceHandle);

    // After DigitalTwin_InterfaceClient_Destroy returns, it is safe to assume
    // no more callbacks shall arrive for this interface and it is OK to free
    // resources callbacks otherwise may have needed.
}


