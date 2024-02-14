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
    _In_ MidiLoopbackDeviceDefinition const& definitionA,
    _In_ MidiLoopbackDeviceDefinition const& definitionB)
{
    TraceLoggingWrite(
        MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    // we can't really do much with the return values here other than log them.

    LOG_IF_FAILED(DeleteSingleEndpoint(definitionA));
    LOG_IF_FAILED(DeleteSingleEndpoint(definitionB));

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2LoopbackMidiEndpointManager::DeleteSingleEndpoint(
    MidiLoopbackDeviceDefinition const& definition
)
{

    TraceLoggingWrite(
        MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(definition.AssociationId.c_str(), "association id"),
        TraceLoggingWideString(definition.EndpointUniqueIdentifier.c_str(), "unique identifier"),
        TraceLoggingWideString(definition.InstanceIdPrefix.c_str(), "prefix"),
        TraceLoggingWideString(definition.EndpointName.c_str(), "name"),
        TraceLoggingWideString(definition.EndpointDescription.c_str(), "description")
    );

    return m_MidiDeviceManager->DeactivateEndpoint(definition.CreatedShortClientInstanceId.c_str());
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
        TraceLoggingWideString(definition->InstanceIdPrefix.c_str(), "prefix"),
        TraceLoggingWideString(definition->EndpointUniqueIdentifier.c_str(), "unique identifier"),
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

    // no user or in-protocol data in this case
    std::wstring friendlyName = internal::CalculateEndpointDevicePrimaryName(endpointName, L"", L"");


    bool requiresMetadataHandler = false;
    bool multiClient = true;
    bool generateIncomingTimestamps = true;


    TraceLoggingWrite(
        MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(definition->AssociationId.c_str(), "association id"),
        TraceLoggingWideString(definition->EndpointUniqueIdentifier.c_str(), "unique identifier"),
        TraceLoggingWideString(L"Adding endpoint properties"),
        TraceLoggingWideString(friendlyName.c_str(), "friendlyName"),
        TraceLoggingWideString(mnemonic.c_str(), "mnemonic"),
        TraceLoggingWideString(endpointName.c_str(), "endpointName"),
        TraceLoggingWideString(endpointDescription.c_str(), "endpointName")
    );

    // this is needed for the loopback endpoints to have a relationship with each other
    interfaceDeviceProperties.push_back(DEVPROPERTY{ {PKEY_MIDI_VirtualMidiEndpointAssociator, DEVPROP_STORE_SYSTEM, nullptr},
        DEVPROP_TYPE_STRING, (ULONG)(sizeof(wchar_t) * (definition->AssociationId.size() + 1)), (PVOID)definition->AssociationId.c_str() });

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
    createInfo.pszDeviceDescription = friendlyName.c_str();


    const ULONG deviceInterfaceIdMaxSize = 255;
    wchar_t newDeviceInterfaceId[deviceInterfaceIdMaxSize]{ 0 };

    TraceLoggingWrite(
        MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(definition->AssociationId.c_str(), "association id"),
        TraceLoggingWideString(definition->EndpointUniqueIdentifier.c_str(), "unique identifier"),
        TraceLoggingWideString(instanceId.c_str(), "instance id"),
        TraceLoggingWideString(L"Activating endpoint")
    );

    MIDIENDPOINTCOMMONPROPERTIES commonProperties;
    commonProperties.AbstractionLayerGuid = m_TransportAbstractionId;
    commonProperties.EndpointPurpose = MidiEndpointDevicePurposePropertyValue::NormalMessageEndpoint;
    commonProperties.FriendlyName = friendlyName.c_str();
    commonProperties.TransportMnemonic = mnemonic.c_str();
    commonProperties.TransportSuppliedEndpointName = endpointName.c_str();
    commonProperties.TransportSuppliedEndpointDescription = endpointDescription.c_str();
    commonProperties.UserSuppliedEndpointName = nullptr;
    commonProperties.UserSuppliedEndpointDescription = nullptr;
    commonProperties.UniqueIdentifier = definition->EndpointUniqueIdentifier.c_str();
    commonProperties.SupportedDataFormats = MidiDataFormat::MidiDataFormat_UMP;
    commonProperties.NativeDataFormat = MIDI_PROP_NATIVEDATAFORMAT_UMP;
    commonProperties.SupportsMultiClient = multiClient;
    commonProperties.RequiresMetadataHandler = requiresMetadataHandler;
    commonProperties.GenerateIncomingTimestamps = generateIncomingTimestamps;


    RETURN_IF_FAILED(m_MidiDeviceManager->ActivateEndpoint(
        (PCWSTR)m_parentDeviceId.c_str(),                       // parent instance Id
        true,                                                   // UMP-only
        MidiFlow::MidiFlowBidirectional,                        // MIDI Flow
        &commonProperties,
        (ULONG)interfaceDeviceProperties.size(),
        ARRAYSIZE(deviceDevProperties),
        (PVOID)interfaceDeviceProperties.data(),
        (PVOID)deviceDevProperties,
        (PVOID)&createInfo,
        (LPWSTR)&newDeviceInterfaceId,
        deviceInterfaceIdMaxSize));


    TraceLoggingWrite(
        MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(definition->AssociationId.c_str(), "association id"),
        TraceLoggingWideString(definition->EndpointUniqueIdentifier.c_str(), "unique identifier"),
        TraceLoggingWideString(newDeviceInterfaceId, "new device interface id"),
        TraceLoggingWideString(L"Endpoint activated")
    );


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

    TraceLoggingWrite(
        MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(definition->AssociationId.c_str(), "association id"),
        TraceLoggingWideString(definition->EndpointUniqueIdentifier.c_str(), "unique identifier"),
        TraceLoggingWideString(L"Done")
    );

    return S_OK;
}





_Use_decl_annotations_
HRESULT 
CMidi2LoopbackMidiEndpointManager::CreateEndpointPair(
    std::shared_ptr<MidiLoopbackDeviceDefinition> definitionA,
    std::shared_ptr<MidiLoopbackDeviceDefinition> definitionB,
    bool isFromConfigurationFile
)
{
    TraceLoggingWrite(
        MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    if (SUCCEEDED(CreateSingleEndpoint(definitionA)))
    {
        if (SUCCEEDED(CreateSingleEndpoint(definitionB)))
        {
            // all good now. Create the device table entry.

            auto associationId = definitionA->AssociationId;

            auto device = MidiLoopbackDevice{};

            device.IsFromConfigurationFile = isFromConfigurationFile;
            device.DefinitionA = *definitionA;
            device.DefinitionB = *definitionB;

            AbstractionState::Current().GetEndpointTable()->SetDevice(associationId, device);

        }
        else
        {
            // failed to create B. We need to remove A now

            TraceLoggingWrite(
                MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Failed to create loopback endpoint B. Removing A now.", "message")
            );

            // we can't do anything with the return value here
            DeleteSingleEndpoint(*definitionA);

            return E_FAIL;
        }
    }
    else
    {
        // failed to create A
        TraceLoggingWrite(
            MidiLoopbackMidiAbstractionTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Failed to create loopback endpoint A", "message")
        );

        return E_FAIL;
    }

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

