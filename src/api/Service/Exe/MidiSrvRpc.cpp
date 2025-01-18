// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"
#include <filesystem>

#define SAFE_COTASKMEMFREE(p) \
    if (NULL != p) { \
        CoTaskMemFree(p); \
        (p) = NULL; \
    }
    
void __RPC_FAR* __RPC_API midl_user_allocate(size_t size)
{
    return (void*)new (std::nothrow) BYTE[size];
}

void __RPC_USER midl_user_free(void __RPC_FAR* p)
{
    delete[] (BYTE*)p;
}

// used so apps have a method they can call to spin up the service and
// be able to do things like device enumeration which rely on the service
// to be running, but do not actually trigger service start
BOOL 
MidiSrvVerifyConnectivity(
    handle_t /*bindingHandle*/)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    std::shared_ptr<CMidiSessionTracker> sessionTracker;

    auto coInit = wil::CoInitializeEx(COINIT_MULTITHREADED);

    if (FAILED(g_MidiService->GetSessionTracker(sessionTracker)))
    {
        return false;
    }

    return sessionTracker->VerifyConnectivity();
}

HRESULT MidiSrvCreateClient(
    /* [in] */ handle_t bindingHandle,
    /* [string][in] */ __RPC__in_string LPCWSTR midiDevice,
    /* [in] */ __RPC__in PMIDISRV_CLIENTCREATION_PARAMS creationParams,
    /* [in] */ __RPC__in GUID sessionId,
    /* [out] */ __RPC__deref_out_opt PMIDISRV_CLIENT *client
    )
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    std::shared_ptr<CMidiClientManager> clientManager;
    PMIDISRV_CLIENT createdClient {nullptr};

    std::shared_ptr<CMidiSrvTraceLogger> traceLogger = nullptr;
    RETURN_IF_FAILED(g_MidiService->GetTraceLogger(traceLogger));

    std::wstring processName{};

    auto coInit = wil::CoInitializeEx(COINIT_MULTITHREADED);

    auto cleanupOnExit = wil::scope_exit([&]() {
        if (createdClient)
        {
            MIDL_user_free(createdClient);
        }
    });

    auto logCreateClientOnFailure = wil::ThreadFailureCallback([&](wil::FailureInfo const& failure) noexcept {
        if (traceLogger != nullptr)
        {
            DWORD EmptyProcessId{0};
            traceLogger->LogMidi2CreateClient(
                failure.hr,
                midiDevice,
                processName.c_str(),
                creationParams->CallingApi,
                creationParams->Flow,
                creationParams->DataFormat,
                EmptyProcessId);
        }
        return false;
    });

    // allocate a midl object to fill in and return to the client
    createdClient = (PMIDISRV_CLIENT) MIDL_user_allocate(sizeof(MIDISRV_CLIENT));
    RETURN_IF_NULL_ALLOC(createdClient);

    ZeroMemory(createdClient, sizeof(MIDISRV_CLIENT));

    DWORD clientProcessId{0};
    wil::unique_handle clientProcessHandle;

    RETURN_IF_FAILED(HRESULT_FROM_RPCSTATUS(I_RpcBindingInqLocalClientPID(bindingHandle, &clientProcessId)));

    // get the client PID, impersonate the client to get the client process handle, and then
    // revert back to self.
    RETURN_IF_FAILED(HRESULT_FROM_RPCSTATUS(RpcImpersonateClient(bindingHandle)));
    clientProcessHandle.reset(OpenProcess(PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION, FALSE, clientProcessId));
    RETURN_IF_FAILED(HRESULT_FROM_RPCSTATUS(RpcRevertToSelf()));
    RETURN_LAST_ERROR_IF_NULL(clientProcessHandle);

    std::wstring modulePath{ 0 };
    modulePath.resize(MAX_PATH + 1);

    auto numPathCharacters = GetModuleFileNameEx(clientProcessHandle.get(), NULL, modulePath.data(), (DWORD)modulePath.capacity());
    if (numPathCharacters > 0)
    {
        processName = (std::filesystem::path(modulePath).filename()).c_str();
    }

    // Client manager creates the client, fills in the MIDISRV_CLIENT information
    RETURN_IF_FAILED(g_MidiService->GetClientManager(clientManager));
    RETURN_IF_FAILED(clientManager->CreateMidiClient(midiDevice, sessionId, clientProcessId, clientProcessHandle, creationParams, createdClient, false));

    // Success, transfer the MIDISRV_CLIENT data to the caller.
    *client = createdClient;
    createdClient = nullptr;

    if (traceLogger != nullptr)
    {
        traceLogger->LogMidi2CreateClient(S_OK,
            midiDevice,
            processName.c_str(),
            creationParams->CallingApi,
            creationParams->Flow,
            creationParams->DataFormat,
            clientProcessId
        );
    }

    return S_OK;
}

HRESULT MidiSrvDestroyClient(
    /* [in] */ handle_t /* bindingHandle */,
    /* [in] */ __RPC__in MidiClientHandle clientHandle)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    std::shared_ptr<CMidiClientManager> clientManager;

    std::shared_ptr<CMidiSrvTraceLogger> traceLogger = nullptr;
    RETURN_IF_FAILED(g_MidiService->GetTraceLogger(traceLogger));

    auto logCreateClientOnFailure = wil::ThreadFailureCallback([&](wil::FailureInfo const& failure) noexcept {
        if (traceLogger != nullptr)
        {
            traceLogger->LogMidi2DestroyClient(failure.hr);
        }
        return false;
    });

    auto coInit = wil::CoInitializeEx(COINIT_MULTITHREADED);

    // Client manager creates the client, fills in the MIDISRV_CLIENT information
    RETURN_IF_FAILED(g_MidiService->GetClientManager(clientManager));
    RETURN_IF_FAILED(clientManager->DestroyMidiClient(clientHandle));

    traceLogger->LogMidi2DestroyClient(S_OK);

    return S_OK;
}


HRESULT 
MidiSrvUpdateConfiguration(
    /* [in] */ handle_t bindingHandle,
    /*[in, string]*/ __RPC__in_string LPCWSTR configurationJson,
    __RPC__out LPWSTR* responseJson)
{
    UNREFERENCED_PARAMETER(bindingHandle);

    // in case of any HRESULT error, the response is supposed to be nullptr
    *responseJson = nullptr;

    RETURN_HR_IF_NULL(E_INVALIDARG, configurationJson);
    //RETURN_HR_IF_NULL(E_INVALIDARG, responseJson);

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(configurationJson, "JSON")
    );

    // Send it to the configuration manager and get it broken apart

    std::shared_ptr<CMidiConfigurationManager> configurationManager;
    std::shared_ptr<CMidiDeviceManager> deviceManager;

    auto coInit = wil::CoInitializeEx(COINIT_MULTITHREADED);

    RETURN_IF_FAILED(g_MidiService->GetConfigurationManager(configurationManager));
    RETURN_IF_FAILED(g_MidiService->GetDeviceManager(deviceManager));

    auto configEntries = configurationManager->GetTransportSettingsFromJsonString(configurationJson);

    if (configEntries.size() != 1)
    {
        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(nullptr, "this"),
            TraceLoggingWideString(L"Only one config entry allowed per call", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        return E_FAIL;
    }

    LPWSTR response;

    RETURN_IF_FAILED(deviceManager->UpdateTransportConfiguration(
        configEntries.begin()->first,
        configEntries.begin()->second.c_str(),
        &response));

    // TODO: Should use tryparse here instead
    auto responseObject = json::JsonObject::Parse(response);

    internal::JsonStringifyObjectToOutParam(responseObject, responseJson);

    // TODO: Now check to see if it has settings for anything else, and send those along to be processed

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Exit success", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}


HRESULT
MidiSrvRegisterSession(
    /* [in] */ handle_t bindingHandle,
    __RPC__in GUID sessionId,
    __RPC__in_string LPCWSTR sessionName,
    __RPC__deref_out_opt PMIDISRV_CONTEXT_HANDLE* contextHandle
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    std::shared_ptr<CMidiSessionTracker> sessionTracker;

    auto coInit = wil::CoInitializeEx(COINIT_MULTITHREADED);
    DWORD clientProcessId{0};
    wil::unique_handle clientProcessHandle;

    RETURN_IF_FAILED(g_MidiService->GetSessionTracker(sessionTracker));

    RETURN_IF_FAILED(HRESULT_FROM_RPCSTATUS(I_RpcBindingInqLocalClientPID(bindingHandle, &clientProcessId)));
    RETURN_IF_FAILED(HRESULT_FROM_RPCSTATUS(RpcImpersonateClient(bindingHandle)));
    clientProcessHandle.reset(OpenProcess(PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION, FALSE, clientProcessId));
    RETURN_IF_FAILED(HRESULT_FROM_RPCSTATUS(RpcRevertToSelf()));

    RETURN_LAST_ERROR_IF_NULL(clientProcessHandle);

    RETURN_IF_FAILED(sessionTracker->AddClientSession(sessionId, sessionName, clientProcessId, clientProcessHandle, contextHandle));

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Exit success", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}

HRESULT
MidiSrvUpdateSessionName(
    /* [in] */ handle_t bindingHandle,
    __RPC__in PMIDISRV_CONTEXT_HANDLE contextHandle,
    __RPC__in GUID sessionId,
    __RPC__in_string LPCWSTR sessionName
)
{
    UNREFERENCED_PARAMETER(contextHandle);

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    std::shared_ptr<CMidiSessionTracker> sessionTracker;

    auto coInit = wil::CoInitializeEx(COINIT_MULTITHREADED);

    RETURN_IF_FAILED(g_MidiService->GetSessionTracker(sessionTracker));

    DWORD clientProcessId{0};
    RETURN_IF_FAILED(HRESULT_FROM_RPCSTATUS(I_RpcBindingInqLocalClientPID(bindingHandle, &clientProcessId)));

    RETURN_IF_FAILED(sessionTracker->UpdateClientSessionName(sessionId, sessionName, clientProcessId));

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Exit success", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}

HRESULT
MidiSrvDeregisterSession(
    /* [in] */ handle_t bindingHandle,
    __RPC__in PMIDISRV_CONTEXT_HANDLE contextHandle,
    __RPC__in GUID sessionId
)
{
    UNREFERENCED_PARAMETER(contextHandle);

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    std::shared_ptr<CMidiSessionTracker> sessionTracker;

    auto coInit = wil::CoInitializeEx(COINIT_MULTITHREADED);

    RETURN_IF_FAILED(g_MidiService->GetSessionTracker(sessionTracker));

    DWORD clientProcessId{0};
    RETURN_IF_FAILED(HRESULT_FROM_RPCSTATUS(I_RpcBindingInqLocalClientPID(bindingHandle, &clientProcessId)));

    RETURN_IF_FAILED(sessionTracker->RemoveClientSession(sessionId, clientProcessId));

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Exit success", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}


// Server context run-down routing. This is responsible for cleaning
// up connections when the client doesn't close them correctly (like
// if it crashes out)
void 
__RPC_USER PMIDISRV_CONTEXT_HANDLE_rundown(
    __RPC__in PMIDISRV_CONTEXT_HANDLE phContext
)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter context handle rundown", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    std::shared_ptr<CMidiSessionTracker> sessionTracker;

    auto coInit = wil::CoInitializeEx(COINIT_MULTITHREADED);

    auto trHR = g_MidiService->GetSessionTracker(sessionTracker);

    if (SUCCEEDED(trHR))
    {
        //UNREFERENCED_PARAMETER(phContext);
        LOG_IF_FAILED(sessionTracker->RemoveClientSessionInternal(phContext));
    }

}

HRESULT
MidiSrvGetSessionList(
    /* [in] */ handle_t bindingHandle,
    __RPC__out LPWSTR* sessionListJson
)
{
    UNREFERENCED_PARAMETER(bindingHandle);

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    std::shared_ptr<CMidiSessionTracker> sessionTracker;

    auto coInit = wil::CoInitializeEx(COINIT_MULTITHREADED);

    RETURN_IF_FAILED(g_MidiService->GetSessionTracker(sessionTracker));

    RETURN_IF_FAILED(sessionTracker->GetSessionList(sessionListJson));

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Exit success", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;

}

HRESULT 
MidiSrvGetTransportList(
    /*[in]*/ handle_t bindingHandle,
    __RPC__out LPWSTR* transportListJson
)
{
    UNREFERENCED_PARAMETER(bindingHandle);

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    std::shared_ptr<CMidiConfigurationManager> configManager;

    auto coInit = wil::CoInitializeEx(COINIT_MULTITHREADED);

    RETURN_IF_FAILED(g_MidiService->GetConfigurationManager(configManager));

    auto allMetadata = configManager->GetAllEnabledTransportMetadata();

    // TODO: This code should probably live in the configuration manager instead of the RPC layer

    json::JsonObject rootObject{};

    for (auto &metadata : allMetadata)
    {
        // add to result object

        json::JsonObject transportObject;
       
        if (metadata.Name != NULL) transportObject.SetNamedValue(MIDI_SERVICE_JSON_TRANSPORT_PLUGIN_INFO_NAME_PROPERTY_KEY, json::JsonValue::CreateStringValue(metadata.Name));
        if (metadata.TransportCode != NULL) transportObject.SetNamedValue(MIDI_SERVICE_JSON_TRANSPORT_PLUGIN_INFO_TRANSPORT_CODE_PROPERTY_KEY, json::JsonValue::CreateStringValue(metadata.TransportCode));
        if (metadata.Description != NULL) transportObject.SetNamedValue(MIDI_SERVICE_JSON_TRANSPORT_PLUGIN_INFO_DESCRIPTION_PROPERTY_KEY, json::JsonValue::CreateStringValue(metadata.Description));
        if (metadata.Author != NULL) transportObject.SetNamedValue(MIDI_SERVICE_JSON_TRANSPORT_PLUGIN_INFO_AUTHOR_PROPERTY_KEY, json::JsonValue::CreateStringValue(metadata.Author));
        if (metadata.SmallImagePath != NULL) transportObject.SetNamedValue(MIDI_SERVICE_JSON_TRANSPORT_PLUGIN_INFO_SMALL_IMAGE_PATH_PROPERTY_KEY, json::JsonValue::CreateStringValue(metadata.SmallImagePath));
        if (metadata.Version != NULL) transportObject.SetNamedValue(MIDI_SERVICE_JSON_TRANSPORT_PLUGIN_INFO_VERSION_PROPERTY_KEY, json::JsonValue::CreateStringValue(metadata.Version));
        transportObject.SetNamedValue(MIDI_SERVICE_JSON_TRANSPORT_PLUGIN_INFO_IS_RT_CREATABLE_APPS_PROPERTY_KEY, json::JsonValue::CreateBooleanValue(metadata.Flags & MetadataFlags_IsRuntimeCreatableByApps));
        transportObject.SetNamedValue(MIDI_SERVICE_JSON_TRANSPORT_PLUGIN_INFO_IS_RT_CREATABLE_SETTINGS_PROPERTY_KEY, json::JsonValue::CreateBooleanValue(metadata.Flags & MetadataFlags_IsRuntimeCreatableBySettings));
        transportObject.SetNamedValue(MIDI_SERVICE_JSON_TRANSPORT_PLUGIN_INFO_IS_SYSTEM_MANAGED_PROPERTY_KEY, json::JsonValue::CreateBooleanValue(metadata.Flags & MetadataFlags_IsSystemManaged));
        transportObject.SetNamedValue(MIDI_SERVICE_JSON_TRANSPORT_PLUGIN_INFO_IS_CLIENT_CONFIGURABLE_PROPERTY_KEY, json::JsonValue::CreateBooleanValue(metadata.Flags & MetadataFlags_IsClientConfigurable));
//        if (metadata.ClientConfigurationAssemblyName != NULL) transportObject.SetNamedValue(MIDI_SERVICE_JSON_TRANSPORT_PLUGIN_INFO_CLIENT_CONFIG_ASSEMBLY_PROPERTY_KEY, json::JsonValue::CreateStringValue(metadata.ClientConfigurationAssemblyName));

        // add the transport metadata to the root, using the transport id as the key
        rootObject.SetNamedValue(internal::GuidToString(metadata.TransportId).c_str(), transportObject);

        // We need to free all the strings in the struct. 
        SAFE_COTASKMEMFREE(metadata.Name);
        SAFE_COTASKMEMFREE(metadata.TransportCode);
        SAFE_COTASKMEMFREE(metadata.Description);
        SAFE_COTASKMEMFREE(metadata.Author);
        SAFE_COTASKMEMFREE(metadata.SmallImagePath);
        SAFE_COTASKMEMFREE(metadata.Version);
//        SAFE_COTASKMEMFREE(metadata.ClientConfigurationAssemblyName);

    }

    // stringify json to output parameter

    internal::JsonStringifyObjectToOutParam(rootObject, transportListJson);

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Exit success", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    return S_OK;
}

//HRESULT
//MidiSrvGetTransportList(
//    /*[in]*/ handle_t bindingHandle,
//    /*[in]*/ DWORD transportMetadataStructSize,
//    __RPC__in PTRANSPORTMETADATA transportList,
//    __RPC__inout DWORD* transportCount
//)
//{
//    TraceLoggingWrite(
//        MidiSrvTelemetryProvider::Provider(),
//        MIDI_TRACE_EVENT_INFO,
//        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
//        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
//        TraceLoggingWideString(L"Enter")
//    );
//
//    UNREFERENCED_PARAMETER(bindingHandle);
//    RETURN_HR_IF_NULL(E_INVALIDARG, transportCount);
//
//    std::shared_ptr<CMidiConfigurationManager> configManager;
//
//    auto coInit = wil::CoInitializeEx(COINIT_MULTITHREADED);
//
//    RETURN_IF_FAILED(g_MidiService->GetConfigurationManager(configManager));
//    auto allMetadata = configManager->GetAllEnabledTransportMetadata();
//
//    DWORD structCount = allMetadata.size();
//
//    // if null is passed in, we just measure and return the data
//    if (transportList == nullptr)
//    {
//        // return the count
//        if (transportMetadataStructSize == sizeof(TRANSPORTMETADATA))
//        {
//            *transportCount = structCount;
//
//            return S_OK;
//        }
//        else
//        {
//            // unexpected struct size
//            RETURN_IF_FAILED(E_INVALIDARG);
//        }
//    }
//    else
//    {
//        // transportList is not nullptr
//
//        if (*transportCount != structCount)
//        {
//            // transport count has changed in between calls
//            RETURN_IF_FAILED(E_INVALIDARG);
//        }
//
//        // transportList is valid pointer, struct count is expected number, size is as expected
//        // copy the metadata over into the provided buffer
//
//       
//
//    }
//
//
//
//
//    // TODO: This code should probably live in the configuration manager instead of the RPC layer
//
//
//    TraceLoggingWrite(
//        MidiSrvTelemetryProvider::Provider(),
//        MIDI_TRACE_EVENT_INFO,
//        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
//        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
//        TraceLoggingWideString(L"Exit success")
//    );
//
//    return S_OK;
//}



HRESULT
MidiSrvGetTransformList(
    /*[in]*/ handle_t bindingHandle,
    __RPC__out LPWSTR* transformListJson
)
{
    UNREFERENCED_PARAMETER(bindingHandle);
    //UNREFERENCED_PARAMETER(transformListJson);

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(nullptr, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    json::JsonObject rootObject{};


    // TODO. Right now, this returns an empty list because transform configuration is not yet in place


    // stringify json to output parameter
    internal::JsonStringifyObjectToOutParam(rootObject, transformListJson);


    return S_OK;
}
