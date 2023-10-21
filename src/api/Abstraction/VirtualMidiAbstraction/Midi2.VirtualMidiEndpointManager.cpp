// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "midi2.VirtualMidiAbstraction.h"

using namespace wil;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

#define MAX_DEVICE_ID_LEN 200 // size in chars

GUID AbstractionLayerGUID = __uuidof(Midi2VirtualMidiAbstraction);

_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiEndpointManager::Initialize(
    IUnknown* midiDeviceManager, 
    LPCWSTR configurationJson
)
{
    OutputDebugString(L"" __FUNCTION__ " Enter");

    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF(E_INVALIDARG, nullptr == midiDeviceManager);

    RETURN_IF_FAILED(midiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_MidiDeviceManager));

    m_transportAbstractionId = AbstractionLayerGUID;   // this is needed so MidiSrv can instantiate the correct transport
    m_containerId = m_transportAbstractionId;                           // we use the transport ID as the container ID for convenience


    if (configurationJson != nullptr)
    {
        try
        {
            OutputDebugString(configurationJson);

            m_jsonObject = json::JsonObject::Parse(configurationJson);
        }
        catch (...)
        {
            OutputDebugString(L"Exception processing json for virtual MIDI abstraction");
        }

    }
    else
    {
        OutputDebugString(L"Configuration json is null for virtual MIDI abstraction");
    }


    RETURN_IF_FAILED(CreateParentDevice());
    RETURN_IF_FAILED(CreateConfiguredEndpoints(configurationJson));


    return S_OK;
}




void SwMidiParentDeviceCreateCallback(__in HSWDEVICE /*hSwDevice*/, __in HRESULT CreationResult, __in_opt PVOID pContext, __in_opt PCWSTR pszDeviceInstanceId)
{
    if (pContext == nullptr)
    {
        // TODO: Should log this.

        return;
    }

    PPARENTDEVICECREATECONTEXT creationContext = (PPARENTDEVICECREATECONTEXT)pContext;


    // interface registration has started, assume failure
    creationContext->MidiParentDevice->SwDeviceState = SWDEVICESTATE::Failed;

    LOG_IF_FAILED(CreationResult);

    if (SUCCEEDED(CreationResult))
    {
        // success, mark the port as created
        creationContext->MidiParentDevice->SwDeviceState = SWDEVICESTATE::Created;

        // get the new device instance ID. This is usually modified from what we started with
        creationContext->MidiParentDevice->InstanceId = std::wstring(pszDeviceInstanceId);
    }
    else
    {
        OutputDebugString(L"" __FUNCTION__ " - CreationResult FAILURE");
    }

    // success or failure, signal we have completed.
    creationContext->creationCompleted.SetEvent();
}



HRESULT
CMidi2VirtualMidiEndpointManager::CreateParentDevice()
{
    // the parent device parameters are set by the transport (this)

    std::wstring parentDeviceName{ TRANSPORT_PARENT_DEVICE_NAME };
    std::wstring parentDeviceId{ TRANSPORT_PARENT_ID };

    SW_DEVICE_CREATE_INFO CreateInfo = {};
    CreateInfo.cbSize = sizeof(CreateInfo);
    CreateInfo.pszInstanceId = parentDeviceId.c_str();
    CreateInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    CreateInfo.pszDeviceDescription = parentDeviceName.c_str();

    SW_DEVICE_CREATE_INFO* createInfo = (SW_DEVICE_CREATE_INFO*)&CreateInfo;

    if (m_parentDevice != nullptr)
    {
        return S_OK;
    }

    m_parentDevice = std::make_unique<MidiEndpointParentDeviceInfo>();

    RETURN_IF_NULL_ALLOC(m_parentDevice);

    PARENTDEVICECREATECONTEXT creationContext;

    // lambdas can only be converted to a function pointer if they
    // don't do capture, so copy everything into the CREATECONTEXT
    // to share with the SwDeviceCreate callback.
    creationContext.MidiParentDevice = m_parentDevice.get();

    //creationContext.InterfaceDevProperties = (DEVPROPERTY*)InterfaceDevProperties;
    //creationContext.IntPropertyCount = IntPropertyCount;

    m_parentDevice->SwDeviceState = SWDEVICESTATE::CreatePending;

    //m_parentDevice->InstanceId = createInfo->pszInstanceId;

    DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;

    DEVPROPERTY deviceDevProperties[] = {
        {{DEVPKEY_Device_PresenceNotForDevice, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)), &devPropTrue},
        {{DEVPKEY_Device_NoConnectSound, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)),&devPropTrue}
    };


    std::wstring rootDeviceId = LOOPBACK_PARENT_ROOT;
    std::wstring enumeratorName = TRANSPORT_ENUMERATOR;

    createInfo->pContainerId = &m_containerId;

    RETURN_IF_FAILED(SwDeviceCreate(
        enumeratorName.c_str(),             // this really should come from the service
        rootDeviceId.c_str(),               // root device
        createInfo,
        ARRAYSIZE(deviceDevProperties),     // count of properties
        (DEVPROPERTY*)deviceDevProperties,  // pointer to properties
        SwMidiParentDeviceCreateCallback,   // callback
        &creationContext,
        wil::out_param(m_parentDevice->SwDevice)));

    // wait for creation to complete
    creationContext.creationCompleted.wait();

    // confirm we were able to register the interface
    RETURN_HR_IF(E_FAIL, m_parentDevice->SwDeviceState != SWDEVICESTATE::Created);

    return S_OK;
}


#define MIDI_VIRTUAL_ENDPOINTS_ARRAY_KEY L"endpoints"

#define MIDI_VIRTUAL_ENDPOINT_PROPERTY_KEY L"key"
#define MIDI_VIRTUAL_ENDPOINT_PROPERTY_NAME L"name"
#define MIDI_VIRTUAL_ENDPOINT_PROPERTY_UNIQUEID L"uniqueIdentifier"
#define MIDI_VIRTUAL_ENDPOINT_PROPERTY_MULTICLIENT L"supportsMultClient"

#define MIDI_JSON_ADD_NODE L"add"
#define MIDI_JSON_UPDATE_NODE L"update"
#define MIDI_JSON_REMOVE_NODE L"remove"


json::JsonObject GetAddNode(json::JsonObject parent)
{
    if (parent.HasKey(MIDI_JSON_ADD_NODE))
    {
        return parent.GetNamedObject(MIDI_JSON_ADD_NODE);
    }
    else
    {
        return nullptr;
    }

}

_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiEndpointManager::CreateConfiguredEndpoints(std::wstring configurationJson)
{
    // if nothing to configure, that's ok
    if (configurationJson.empty()) return S_OK;

    try
    {
        auto jsonObject = json::JsonObject::Parse(configurationJson);


        // check to see if we have an "add" node. No point in checking for "update" or "remove" for initial configuration
        auto addNode = GetAddNode(jsonObject);

        if (addNode != nullptr && addNode.HasKey(MIDI_VIRTUAL_ENDPOINTS_ARRAY_KEY))
        {
            auto endpoints = addNode.GetNamedArray(MIDI_VIRTUAL_ENDPOINTS_ARRAY_KEY);

            for (auto endpointElement : endpoints)
            {
                // GetObjectW here is because wingdi redefines it to GetObject. It's a stupid preprocessor conflict
                try
                {
                    auto endpoint = endpointElement.GetObjectW();

                    //  auto key = endpoint.GetNamedString(MIDI_VIRTUAL_ENDPOINT_PROPERTY_KEY, L"");
                    auto name = endpoint.GetNamedString(MIDI_VIRTUAL_ENDPOINT_PROPERTY_NAME, L"");
                    auto uniqueIdentifier = endpoint.GetNamedString(MIDI_VIRTUAL_ENDPOINT_PROPERTY_UNIQUEID, L"");
                    //   auto supportsMultiClient = endpoint.GetNamedBoolean(MIDI_VIRTUAL_ENDPOINT_PROPERTY_MULTICLIENT, true);

                    auto instanceId = TRANSPORT_MNEMONIC L"_" + uniqueIdentifier;

                    // TODO: Need to add this to the table for routing, and also add the other properties to the function
                    CreateEndpoint((std::wstring)instanceId, (std::wstring)name);
                }
                catch (...)
                {
                    // couldn't get an object. Garbage data
                    OutputDebugString(L"" __FUNCTION__ " Exception getting endpoint properties from json");

                    return E_FAIL;
                }
            }
        }
        else
        {
            // nothing to add
        }


    }
    catch (...)
    {
        // exception parsing the json. It's likely empty or malformed

        OutputDebugString(L"" __FUNCTION__ " Exception parsing JSON");

        TraceLoggingWrite(
            MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this")
        );

        return E_FAIL;
    }

    return S_OK;
}

// this will be called from the runtime endpoint creation interface

_Use_decl_annotations_
HRESULT 
CMidi2VirtualMidiEndpointManager::CreateEndpoint(std::wstring const instanceId, std::wstring const name)
{
    //put all of the devproperties we want into arrays and pass into ActivateEndpoint:

    std::wstring mnemonic(TRANSPORT_MNEMONIC);
    MidiFlow const flow = MidiFlow::MidiFlowBidirectional;

    DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;

    DEVPROPERTY interfaceDevProperties[] = {
        {{DEVPKEY_DeviceInterface_FriendlyName, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((name.length() + 1) * sizeof(WCHAR)), (PVOID)name.c_str()},
        {{PKEY_MIDI_TransportSuppliedEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((name.length() + 1) * sizeof(WCHAR)), (PVOID)name.c_str()},
        {{PKEY_MIDI_UmpLoopback, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)),&devPropTrue},
        {{PKEY_MIDI_AbstractionLayer, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_GUID, static_cast<ULONG>(sizeof(GUID)), (PVOID)&AbstractionLayerGUID },        // essential to instantiate the right endpoint types
        {{PKEY_MIDI_TransportMnemonic, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((mnemonic.length() + 1) * sizeof(WCHAR)), (PVOID)mnemonic.c_str()},
    };

    DEVPROPERTY deviceDevProperties[] = {
        {{DEVPKEY_Device_PresenceNotForDevice, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)), &devPropTrue},
        {{DEVPKEY_Device_NoConnectSound, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)),&devPropTrue}
    };

    SW_DEVICE_CREATE_INFO createInfo = {};
    createInfo.cbSize = sizeof(createInfo);

    createInfo.pszInstanceId = instanceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = name.c_str();


    RETURN_IF_FAILED(m_MidiDeviceManager->ActivateEndpoint(
        std::wstring(m_parentDevice->InstanceId).c_str(),       // parent instance Id
        true,                                                   // UMP-only
        flow,                                                   // MIDI Flow
        ARRAYSIZE(interfaceDevProperties),
        ARRAYSIZE(deviceDevProperties),
        (PVOID)interfaceDevProperties,
        (PVOID)deviceDevProperties,
        (PVOID)&createInfo));

    return S_OK;
}



HRESULT
CMidi2VirtualMidiEndpointManager::Cleanup()
{
    OutputDebugString(L"" __FUNCTION__ " Enter");

    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    return S_OK;
}


//
//_Use_decl_annotations_
//HRESULT
//CMidi2VirtualMidiEndpointManager::ApplyConfiguration(
//    LPCWSTR /*configurationJson*/,
//    LPWSTR /*resultJson*/
//)
//{
//    return E_NOTIMPL;
//}


