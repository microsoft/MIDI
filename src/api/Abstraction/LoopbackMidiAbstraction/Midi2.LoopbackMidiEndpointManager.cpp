// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "midi2.LoopbackMidiAbstraction.h"

using namespace wil;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

#define MAX_DEVICE_ID_LEN 200 // size in chars

GUID AbstractionLayerGUID = __uuidof(Midi2LoopbackMidiAbstraction);


_Use_decl_annotations_
HRESULT
CMidi2LoopbackMidiEndpointManager::Initialize(
    IUnknown* MidiDeviceManager, 
    IUnknown* MidiEndpointProtocolManager
)
{
    TraceLoggingWrite(
        MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF(E_INVALIDARG, nullptr == MidiDeviceManager);
    RETURN_HR_IF(E_INVALIDARG, nullptr == MidiEndpointProtocolManager);

    RETURN_IF_FAILED(MidiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_MidiDeviceManager));
    RETURN_IF_FAILED(MidiEndpointProtocolManager->QueryInterface(__uuidof(IMidiEndpointProtocolManagerInterface), (void**)&m_MidiProtocolManager));


    m_TransportAbstractionId = AbstractionLayerGUID;    // this is needed so MidiSrv can instantiate the correct transport
    m_ContainerId = m_TransportAbstractionId;           // we use the transport ID as the container ID for convenience

    RETURN_IF_FAILED(CreateParentDevice());

    return S_OK;
}



HRESULT
CMidi2LoopbackMidiEndpointManager::CreateParentDevice()
{
    TraceLoggingWrite(
        MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    // the parent device parameters are set by the transport (this)
    std::wstring parentDeviceName{ TRANSPORT_PARENT_DEVICE_NAME };
    std::wstring parentDeviceId{ internal::NormalizeDeviceInstanceIdWStringCopy(TRANSPORT_PARENT_ID) };

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

    m_parentDeviceId = internal::NormalizeDeviceInstanceIdWStringCopy(newDeviceId);


    TraceLoggingWrite(
        MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(newDeviceId, "New parent device instance id")
    );

    return S_OK;
}



_Use_decl_annotations_
HRESULT
CMidi2LoopbackMidiEndpointManager::DeleteEndpointPair(
    _In_ std::shared_ptr<MidiLoopbackDeviceDefinition> definitionA,
    _In_ std::shared_ptr<MidiLoopbackDeviceDefinition> definitionB)
{
    TraceLoggingWrite(
        MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    // TODO


    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2LoopbackMidiEndpointManager::DeleteSingleEndpoint(
    std::shared_ptr<MidiLoopbackDeviceDefinition> definition
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, definition);

    TraceLoggingWrite(
        MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(definition->AssociationId.c_str(), "association id"),
        TraceLoggingWideString(definition->EndpointUniqueIdentifier.c_str(), "unique identifier"),
        TraceLoggingWideString(definition->InstanceIdPrefix.c_str(), "prefix"),
        TraceLoggingWideString(definition->EndpointName.c_str(), "name"),
        TraceLoggingWideString(definition->EndpointDescription.c_str(), "description")
    );


    // TODO


    return S_OK;
}



_Use_decl_annotations_
HRESULT
CMidi2LoopbackMidiEndpointManager::CreateSingleEndpoint(
    std::shared_ptr<MidiLoopbackDeviceDefinition> definition
    )
{
    RETURN_HR_IF_NULL(E_INVALIDARG, definition);

    TraceLoggingWrite(
        MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(definition->AssociationId.c_str(), "association id"),
        TraceLoggingWideString(definition->EndpointUniqueIdentifier.c_str(), "unique identifier"),
        TraceLoggingWideString(definition->InstanceIdPrefix.c_str(), "prefix"),
        TraceLoggingWideString(definition->EndpointName.c_str(), "name"),
        TraceLoggingWideString(definition->EndpointDescription.c_str(), "description")
        );


    RETURN_HR_IF_MSG(E_INVALIDARG, definition->EndpointName.empty(), "Empty endpoint name");
    RETURN_HR_IF_MSG(E_INVALIDARG, definition->InstanceIdPrefix.empty(), "Empty endpoint prefix");
    RETURN_HR_IF_MSG(E_INVALIDARG, definition->EndpointUniqueIdentifier.empty(), "Empty endpoint unique id");


    //put all of the devproperties we want into arrays and pass into ActivateEndpoint:

    std::wstring mnemonic(TRANSPORT_MNEMONIC);

    DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;
    //   DEVPROP_BOOLEAN devPropFalse = DEVPROP_FALSE;

    std::wstring endpointName = definition->EndpointName;
    std::wstring endpointDescription = definition->EndpointDescription;

    std::vector<DEVPROPERTY> interfaceDeviceProperties{};

    bool requiresMetadataHandler = false;
    bool multiClient = true;
    bool generateIncomingTimestamps = true;

    // no user or in-protocol data in this case
    std::wstring friendlyName = internal::CalculateEndpointDevicePrimaryName(endpointName, L"", L"");

    // all the standard properties we define for endpoints
    if (internal::AddStandardEndpointProperties(
        interfaceDeviceProperties,
        m_TransportAbstractionId,
        MidiEndpointDevicePurposePropertyValue::NormalMessageEndpoint,
        friendlyName,
        mnemonic,
        endpointName,
        endpointDescription,
        L"",
        L"",
        definition->EndpointUniqueIdentifier,
        MidiDataFormat::MidiDataFormat_UMP,
        MIDI_PROP_NATIVEDATAFORMAT_UMP,
        multiClient,
        requiresMetadataHandler,
        generateIncomingTimestamps
        ))
    {
        // additional properties for this abstraction

        // we clear this because it's not used for this abstraction. 
        interfaceDeviceProperties.push_back(internal::BuildEmptyDevProperty(PKEY_MIDI_AssociatedUMP));

        // this is needed for the loopback endpoints to have a relationship with each other
        interfaceDeviceProperties.push_back(internal::BuildWStringDevProperty(PKEY_MIDI_VirtualMidiEndpointAssociator, definition->AssociationId));
    }
    else
    {
        TraceLoggingWrite(
            MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Unable to build standard endpoint properties list", "message")
        );

        return E_FAIL;
    }

    // Device properties

    DEVPROPERTY deviceDevProperties[] = {
        {{DEVPKEY_Device_PresenceNotForDevice, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)), &devPropTrue},
        {{DEVPKEY_Device_NoConnectSound, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)),&devPropTrue}
    };

    SW_DEVICE_CREATE_INFO createInfo = {};
    createInfo.cbSize = sizeof(createInfo);

    // build the instance id, which becomes the middle of the SWD id
    std::wstring instanceId = internal::NormalizeDeviceInstanceIdWStringCopy(
        definition->InstanceIdPrefix + definition->EndpointUniqueIdentifier);

    createInfo.pszInstanceId = instanceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = endpointName.c_str();


    const ULONG deviceInterfaceIdMaxSize = 255;
    wchar_t newDeviceInterfaceId[deviceInterfaceIdMaxSize]{ 0 };

    RETURN_IF_FAILED(m_MidiDeviceManager->ActivateEndpoint(
        (PCWSTR)m_parentDeviceId.c_str(),                       // parent instance Id
        true,                                                   // UMP-only
        MidiFlow::MidiFlowBidirectional,                        // MIDI Flow
        (ULONG)interfaceDeviceProperties.size(),
        ARRAYSIZE(deviceDevProperties),
        (PVOID)interfaceDeviceProperties.data(),
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

    // we need this for removal later
    definition->CreatedShortClientInstanceId = instanceId;

    definition->CreatedEndpointInterfaceId = internal::NormalizeEndpointInterfaceIdWStringCopy(newDeviceInterfaceId);

    //MidiEndpointTable::Current().AddCreatedEndpointDevice(entry);
    //MidiEndpointTable::Current().AddCreatedClient(entry.VirtualEndpointAssociationId, entry.CreatedClientEndpointId);

    return S_OK;
}





_Use_decl_annotations_
HRESULT 
CMidi2LoopbackMidiEndpointManager::CreateEndpointPair(
    std::shared_ptr<MidiLoopbackDeviceDefinition> definitionA,
    std::shared_ptr<MidiLoopbackDeviceDefinition> definitionB
)
{
    TraceLoggingWrite(
        MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );







    return S_OK;
}



HRESULT
CMidi2LoopbackMidiEndpointManager::Cleanup()
{
    TraceLoggingWrite(
        MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    // destroy and release all the devices we have created

//    LOG_IF_FAILED(AbstractionState::Current().GetEndpointTable()->Cleanup());

    return S_OK;
}

