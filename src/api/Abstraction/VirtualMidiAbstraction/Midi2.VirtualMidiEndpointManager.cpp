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






json::JsonObject GetJsonCreateNode(json::JsonObject Parent)
{
    if (Parent.HasKey(MIDI_VIRT_JSON_INSTRUCTION_CREATE_KEY))
    {
        return Parent.GetNamedObject(MIDI_VIRT_JSON_INSTRUCTION_CREATE_KEY);
    }
    else
    {
        return nullptr;
    }
}

winrt::hstring GetJsonStringValue(
    json::JsonObject Parent,
    winrt::hstring Key,
    winrt::hstring Default)
{
    if (Parent.HasKey(Key))
    {
        return Parent.GetNamedString(Key);
    }
    else
    {
        return Default;
    }
}












_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiEndpointManager::Initialize(
    IUnknown* MidiDeviceManager, 
    IUnknown* /*midiEndpointProtocolManager*/,
    LPCWSTR ConfigurationJson
)
{
    OutputDebugString(L"" __FUNCTION__ " Enter");

    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF(E_INVALIDARG, nullptr == MidiDeviceManager);

    RETURN_IF_FAILED(MidiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_MidiDeviceManager));

    m_TransportAbstractionId = AbstractionLayerGUID;    // this is needed so MidiSrv can instantiate the correct transport
    m_ContainerId = m_TransportAbstractionId;           // we use the transport ID as the container ID for convenience

    RETURN_IF_FAILED(CreateParentDevice());

    // TODO: This has a bit of a smell. Revisit.
    MidiEndpointTable::Current().Initialize(this);


    if (ConfigurationJson != nullptr)
    {
        try
        {
            std::wstring json{ ConfigurationJson };

            if (!json.empty())
            {
                json::JsonObject jsonObject = json::JsonObject::Parse(json);

                LOG_IF_FAILED(ApplyJson(jsonObject));
            }
        }
        catch (...)
        {
            OutputDebugString(L"Exception processing json for virtual MIDI abstraction");
            
            // we return S_OK here because otherwise this prevents the service from starting up.
            return S_OK;
        }
    }
    else
    {
        // empty / null is fine. We just continue on.

        OutputDebugString(L"Configuration json is null for virtual MIDI abstraction");

        return S_OK;
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiEndpointManager::ApplyJson(json::JsonObject jsonObject)
{
    // we're expecting a chunk of json here with children for creating, updating, and removing virtual devices

    if (jsonObject.HasKey(MIDI_VIRT_JSON_INSTRUCTION_CREATE_KEY))
    {
        auto array = jsonObject.GetNamedArray(MIDI_VIRT_JSON_INSTRUCTION_CREATE_KEY);

        // under this should be a set of objects, each representing a single device to modify

        if (array)
        {

            for (uint32_t i = 0; i < array.Size(); i++)
            {
                auto jsonEntry = (array.GetObjectAt(i));

                if (jsonEntry)
                {
                    // add to our endpoint table
                    MidiVirtualDeviceEndpointEntry deviceEntry;

                    deviceEntry.VirtualEndpointAssociationId = GetJsonStringValue(jsonEntry, MIDI_VIRT_JSON_DEVICE_PROPERTY_ASSOCIATION_IDENTIFIER, L"");
                    deviceEntry.ShortUniqueId = GetJsonStringValue(jsonEntry, MIDI_VIRT_JSON_DEVICE_PROPERTY_SHORT_UNIQUE_ID, L"");
                    deviceEntry.BaseEndpointName = GetJsonStringValue(jsonEntry, MIDI_VIRT_JSON_DEVICE_PROPERTY_NAME, L"");
                    deviceEntry.Description = GetJsonStringValue(jsonEntry, MIDI_VIRT_JSON_DEVICE_PROPERTY_DESCRIPTION, L"");

                    // if no association id, or it already exists in the table, bail

                    // if no unique Id, bail or maybe generate one

                    // if a unique id and it's larger than the max length, truncate it

                    // create the device-side endpoint
                    LOG_IF_FAILED(CreateDeviceSideEndpoint(deviceEntry));
                }

            }
        }
    }


    return S_OK;
}




HRESULT
CMidi2VirtualMidiEndpointManager::CreateParentDevice()
{
    OutputDebugString(L"" __FUNCTION__);

    // the parent device parameters are set by the transport (this)
    std::wstring parentDeviceName{ TRANSPORT_PARENT_DEVICE_NAME };
    std::wstring parentDeviceId{ TRANSPORT_PARENT_ID };

    SW_DEVICE_CREATE_INFO createInfo = {};
    createInfo.cbSize = sizeof(createInfo);
    createInfo.pszInstanceId = parentDeviceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = parentDeviceName.c_str();
    createInfo.pContainerId = &m_ContainerId;

    const ULONG deviceIdMaxSize = 255;
    wchar_t newDeviceId[deviceIdMaxSize]{ 0 };

    RETURN_IF_FAILED(m_MidiDeviceManager->ActivateVirtualParentDevice(
        0,
        nullptr,
        &createInfo,
        (PWSTR)newDeviceId,
        deviceIdMaxSize
    ));

    m_parentDeviceId = std::wstring(newDeviceId);

    OutputDebugString(__FUNCTION__ L" New parent device id: ");
    OutputDebugString(newDeviceId);
    OutputDebugString(L"\n");

    return S_OK;
}





_Use_decl_annotations_
HRESULT CMidi2VirtualMidiEndpointManager::CreateClientVisibleEndpoint(
    MidiVirtualDeviceEndpointEntry& entry
)
{
    OutputDebugString(__FUNCTION__ L": Enter\n");

    //put all of the devproperties we want into arrays and pass into ActivateEndpoint:

    std::wstring mnemonic(TRANSPORT_MNEMONIC);

    DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;
    //   DEVPROP_BOOLEAN devPropFalse = DEVPROP_FALSE;
    BYTE nativeDataFormat = MIDI_PROP_NATIVEDATAFORMAT_UMP;
    BYTE supportedDataFormat = (BYTE)MidiDataFormat::MidiDataFormat_UMP;

    // this purpose is important because it controls how this shows up to clients
    auto endpointPurpose = (uint32_t)MidiEndpointDevicePurposePropertyValue::NormalMessageEndpoint;

    OutputDebugString(__FUNCTION__ L": Building DEVPROPERTY interfaceDevProperties[]\n");

    std::wstring endpointName = entry.BaseEndpointName;
    std::wstring endpointDescription = entry.Description;

    DEVPROPERTY interfaceDevProperties[] = {

        {{PKEY_MIDI_AssociatedUMP, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_EMPTY, 0, nullptr},

        {{PKEY_MIDI_SupportedDataFormats, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BYTE, static_cast<ULONG>(sizeof(BYTE)), &supportedDataFormat},

        {{PKEY_MIDI_SupportsMulticlient, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)),& devPropTrue},
        {{PKEY_MIDI_EndpointRequiresMetadataHandler, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)), &devPropTrue},

            // this is needed for virtual endpoints (client and device) to have a relationship
        {{PKEY_MIDI_VirtualMidiEndpointAssociator, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((entry.VirtualEndpointAssociationId.length() + 1) * sizeof(WCHAR)), (PVOID)entry.VirtualEndpointAssociationId.c_str()},


        {{DEVPKEY_DeviceInterface_FriendlyName, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((endpointName.length() + 1) * sizeof(WCHAR)), (PVOID)endpointName.c_str()},
        {{PKEY_MIDI_TransportSuppliedEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((endpointName.length() + 1) * sizeof(WCHAR)), (PVOID)endpointName.c_str()},

        {{PKEY_MIDI_EndpointDevicePurpose, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_UINT32, static_cast<ULONG>(sizeof(endpointPurpose)),(PVOID)&endpointPurpose},
        {{PKEY_MIDI_UserSuppliedDescription, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((endpointDescription.length() + 1) * sizeof(WCHAR)), (PVOID)endpointDescription.c_str() },
        {{PKEY_MIDI_NativeDataFormat, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BYTE, static_cast<ULONG>(sizeof(BYTE)), (PVOID)&nativeDataFormat},

        {{PKEY_MIDI_GenerateIncomingTimestamp, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)), (PVOID)&devPropTrue},


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

    OutputDebugString(__FUNCTION__ L": Building SW_DEVICE_CREATE_INFO\n");

    SW_DEVICE_CREATE_INFO createInfo = {};
    createInfo.cbSize = sizeof(createInfo);


    std::wstring instanceId = MIDI_VIRT_INSTANCE_ID_CLIENT_PREFIX + entry.ShortUniqueId;


    createInfo.pszInstanceId = instanceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = endpointName.c_str();


    const ULONG deviceInterfaceIdMaxSize = 255;
    wchar_t newDeviceInterfaceId[deviceInterfaceIdMaxSize]{ 0 };


    OutputDebugString(__FUNCTION__ L": About to ActivateEndpoint\n");

    RETURN_IF_FAILED(m_MidiDeviceManager->ActivateEndpoint(
        (PCWSTR)m_parentDeviceId.c_str(),                       // parent instance Id
        true,                                                   // UMP-only
        MidiFlow::MidiFlowBidirectional,                        // MIDI Flow
        ARRAYSIZE(interfaceDevProperties),
        ARRAYSIZE(deviceDevProperties),
        (PVOID)interfaceDevProperties,
        (PVOID)deviceDevProperties,
        (PVOID)&createInfo,
        (LPWSTR)&newDeviceInterfaceId,
        deviceInterfaceIdMaxSize));


    // now delete all the properties that have been discovered in-protocol
    // we have to do this because they end up cached by PNP and come back
    // when you recreate a device with the same Id. This is a real problem 
    // if you are testing function blocks or endpoint properties with this
    // loopback transport.
    m_MidiDeviceManager->DeleteAllEndpointInProtocolDiscoveredProperties(newDeviceInterfaceId);

    // TODO: Invoke the protocol negotiator to now capture updated endpoint info.

    entry.CreatedClientEndpointId = newDeviceInterfaceId;

    //MidiEndpointTable::Current().AddCreatedEndpointDevice(entry);
    //MidiEndpointTable::Current().AddCreatedClient(entry.VirtualEndpointAssociationId, entry.CreatedClientEndpointId);



    OutputDebugString(__FUNCTION__ L": Complete\n");

    return S_OK;
}


_Use_decl_annotations_
HRESULT CMidi2VirtualMidiEndpointManager::CreateDeviceSideEndpoint(
    MidiVirtualDeviceEndpointEntry &entry
)
{
    OutputDebugString(__FUNCTION__ L": Enter\n");

    //put all of the devproperties we want into arrays and pass into ActivateEndpoint:

    std::wstring mnemonic(TRANSPORT_MNEMONIC);

    DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;
    DEVPROP_BOOLEAN devPropFalse = DEVPROP_FALSE;
    BYTE nativeDataFormat = MIDI_PROP_NATIVEDATAFORMAT_UMP;
    BYTE supportedDataFormat = (BYTE)MidiDataFormat::MidiDataFormat_UMP;


    // this purpose is important because it controls how this shows up to clients
    auto endpointPurpose = (uint32_t)MidiEndpointDevicePurposePropertyValue::VirtualDeviceResponder;

    OutputDebugString(__FUNCTION__ L": Building DEVPROPERTY interfaceDevProperties[]\n");

    std::wstring endpointName = entry.BaseEndpointName + L" (Virtual MIDI Device)";
    std::wstring endpointDescription = entry.Description + L" (for use only by the device host application)";

    DEVPROPERTY interfaceDevProperties[] = {
        {{PKEY_MIDI_AssociatedUMP, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_EMPTY, 0, nullptr},

        {{PKEY_MIDI_SupportedDataFormats, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BYTE, static_cast<ULONG>(sizeof(BYTE)), &supportedDataFormat},
            
            // the device side connection is NOT multi-client. Keep it just to the device app
        {{PKEY_MIDI_SupportsMulticlient, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropFalse)),& devPropFalse},
 
        // device side shouldn't have a metadata handler
        {{PKEY_MIDI_EndpointRequiresMetadataHandler, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropFalse)), &devPropFalse},

            // this is needed for virtual endpoints (client and device) to have a relationship
        {{PKEY_MIDI_VirtualMidiEndpointAssociator, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((entry.VirtualEndpointAssociationId.length() + 1) * sizeof(WCHAR)), (PVOID)entry.VirtualEndpointAssociationId.c_str()},



        {{DEVPKEY_DeviceInterface_FriendlyName, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((endpointName.length() + 1) * sizeof(WCHAR)), (PVOID)endpointName.c_str()},
        {{PKEY_MIDI_TransportSuppliedEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((endpointName.length() + 1) * sizeof(WCHAR)), (PVOID)endpointName.c_str()},

        {{PKEY_MIDI_EndpointDevicePurpose, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_UINT32, static_cast<ULONG>(sizeof(endpointPurpose)),(PVOID)&endpointPurpose},
        {{PKEY_MIDI_UserSuppliedDescription, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((endpointDescription.length() + 1) * sizeof(WCHAR)), (PVOID)endpointDescription.c_str() },
        {{PKEY_MIDI_NativeDataFormat, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BYTE, static_cast<ULONG>(sizeof(BYTE)), (PVOID)&nativeDataFormat},

        {{PKEY_MIDI_GenerateIncomingTimestamp, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)), (PVOID)&devPropTrue},


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

    OutputDebugString(__FUNCTION__ L": Building SW_DEVICE_CREATE_INFO\n");

    SW_DEVICE_CREATE_INFO createInfo = {};
    createInfo.cbSize = sizeof(createInfo);


    std::wstring instanceId = MIDI_VIRT_INSTANCE_ID_DEVICE_PREFIX + entry.ShortUniqueId;


    createInfo.pszInstanceId = instanceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = endpointName.c_str();


    const ULONG deviceInterfaceIdMaxSize = 255;
    wchar_t newDeviceInterfaceId[deviceInterfaceIdMaxSize]{ 0 };


    OutputDebugString(__FUNCTION__ L": About to ActivateEndpoint\n");

    RETURN_IF_FAILED(m_MidiDeviceManager->ActivateEndpoint(
        (PCWSTR)m_parentDeviceId.c_str(),                       // parent instance Id
        true,                                                   // UMP-only
        MidiFlow::MidiFlowBidirectional,                        // MIDI Flow
        ARRAYSIZE(interfaceDevProperties),
        ARRAYSIZE(deviceDevProperties),
        (PVOID)interfaceDevProperties,
        (PVOID)deviceDevProperties,
        (PVOID)&createInfo,
        (LPWSTR)&newDeviceInterfaceId,
        deviceInterfaceIdMaxSize));


    // now delete all the properties that have been discovered in-protocol
    // we have to do this because they end up cached by PNP and come back
    // when you recreate a device with the same Id. This is a real problem 
    // if you are testing function blocks or endpoint properties with this
    // loopback transport.
    m_MidiDeviceManager->DeleteAllEndpointInProtocolDiscoveredProperties(newDeviceInterfaceId);

    entry.CreatedDeviceEndpointId = newDeviceInterfaceId;

    MidiEndpointTable::Current().AddCreatedEndpointDevice(entry);

    // todo: store the interface id and use it for matches later instead of the current partial string match

    OutputDebugString(__FUNCTION__ L": Complete\n");

    return S_OK;

}

// this will be called from the runtime endpoint creation interface




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


    // destroy and release all the devices we have created


    MidiEndpointTable::Current().Cleanup();


    return S_OK;
}

