// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "MidiEndpointNameTable.h"
#include "Feature_Servicing_MIDI2PortNamingRework.h"

using namespace wil;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

GUID TransportLayerGUID = TRANSPORT_LAYER_GUID;


_Use_decl_annotations_
HRESULT
CMidi2MidiSynthEndpointManager::Initialize(
    IMidiDeviceManager* midiDeviceManager,
    IMidiEndpointProtocolManager* midiEndpointProtocolManager
)
{
    TraceLoggingWrite(
        MidiSynthTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF(E_INVALIDARG, nullptr == midiDeviceManager);
    RETURN_HR_IF(E_INVALIDARG, nullptr == midiEndpointProtocolManager);

    RETURN_IF_FAILED(midiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManager), (void**)&m_midiDeviceManager));
    RETURN_IF_FAILED(midiEndpointProtocolManager->QueryInterface(__uuidof(IMidiEndpointProtocolManager), (void**)&m_midiProtocolManager));

    m_transportId = TransportLayerGUID;     // this is needed so MidiSrv can instantiate the correct transport
    m_containerId = m_transportId;          // we use the transport ID as the container ID for convenience

    RETURN_IF_FAILED(CreateParentDevice());

    m_initialized = true;

    // The synthesizer is always present when it is switched on, so unlike a transport which
    // discovers hardware this settles its one endpoint immediately. Configuration has already been
    // applied by this point: the service initializes a transport's configuration manager and hands
    // it the saved settings before it activates the endpoint manager.
    RETURN_IF_FAILED(SyncEndpointToSettings());

    return S_OK;
}


HRESULT
CMidi2MidiSynthEndpointManager::SyncEndpointToSettings()
{
    RETURN_HR_IF(E_UNEXPECTED, !m_initialized);

    auto device = TransportState::Current().GetDevice();
    RETURN_HR_IF_NULL(E_UNEXPECTED, device);

    bool const wanted = device->Settings().Enabled;

    bool exists{ false };
    {
        auto lock = m_endpointLock.lock_shared();
        exists = !m_endpointDeviceInterfaceId.empty();
    }

    if (wanted == exists)
    {
        return S_OK;
    }

    TraceLoggingWrite(
        MidiSynthTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingBool(wanted, "enabled")
    );

    return wanted ? CreateEndpoint() : RemoveEndpoint();
}


HRESULT
CMidi2MidiSynthEndpointManager::RemoveEndpoint()
{
    RETURN_HR_IF_NULL(E_POINTER, m_midiDeviceManager);

    std::wstring instanceId;
    {
        auto lock = m_endpointLock.lock_exclusive();

        if (m_endpointShortInstanceId.empty())
        {
            return S_FALSE;
        }

        instanceId = m_endpointShortInstanceId;

        // Cleared before the removal rather than after, so a settings change arriving while the
        // service tears the device pipe down cannot decide the endpoint still exists.
        m_endpointShortInstanceId.clear();
        m_endpointDeviceInterfaceId.clear();
    }

    RETURN_IF_FAILED(m_midiDeviceManager->RemoveEndpoint(instanceId.c_str()));

    TraceLoggingWrite(
        MidiSynthTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Endpoint removed", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(instanceId.c_str(), "instance id")
    );

    return S_OK;
}


HRESULT
CMidi2MidiSynthEndpointManager::CreateParentDevice()
{
    TraceLoggingWrite(
        MidiSynthTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    // this happens before initialization is complete, so don't gate on m_initialized here

    RETURN_HR_IF_NULL(E_POINTER, m_midiDeviceManager);

    std::wstring parentDeviceName{ internal::ResourceGetWString(IDS_TRANSPORT_PARENT_DEVICE_NAME) };
    std::wstring parentDeviceId{ internal::NormalizeDeviceInstanceIdWStringCopy(TRANSPORT_PARENT_ID) };

    SW_DEVICE_CREATE_INFO createInfo = {};
    createInfo.cbSize = sizeof(createInfo);
    createInfo.pszInstanceId = parentDeviceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = parentDeviceName.c_str();
    createInfo.pContainerId = &m_containerId;

    wil::unique_cotaskmem_string newDeviceId;

    RETURN_IF_FAILED(m_midiDeviceManager->ActivateVirtualParentDevice(
        0,
        nullptr,
        &createInfo,
        &newDeviceId
    ));

    m_parentDeviceId = internal::NormalizeDeviceInstanceIdWStringCopy(newDeviceId.get());

    TraceLoggingWrite(
        MidiSynthTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(newDeviceId.get(), "New parent device instance id")
    );

    return S_OK;
}


HRESULT
CMidi2MidiSynthEndpointManager::CreateEndpoint()
{
    RETURN_HR_IF(E_UNEXPECTED, !m_initialized);
    RETURN_HR_IF_NULL(E_POINTER, m_midiDeviceManager);

    TraceLoggingWrite(
        MidiSynthTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    std::wstring transportCode{ TRANSPORT_CODE };
    std::wstring uniqueIdentifier{ MIDI_SYNTH_ENDPOINT_UNIQUE_ID };

    std::wstring endpointName{ internal::ResourceGetWString(IDS_ENDPOINT_NAME) };
    std::wstring endpointDescription{ internal::ResourceGetWString(IDS_ENDPOINT_DESCRIPTION) };

    // A nameless endpoint is worse than no endpoint: it would still take a WinMM port number and
    // would be unidentifiable in every application that lists it.
    RETURN_HR_IF_MSG(E_UNEXPECTED, endpointName.empty(), "Endpoint name string resource is missing");

    // The specification states the name limit as a UTF-8 byte count, not a character count
    if (Feature_Servicing_MIDI2EndpointNameUtf8ByteLimit::IsEnabled())
    {
        endpointName = internal::TruncateToUtf8ByteCount(endpointName, MIDI_STREAM_MESSAGE_ENDPOINT_NAME_MAX_LENGTH);
    }

    // no user or in-protocol data in this case
    std::wstring friendlyName = internal::CalculateEndpointDevicePrimaryName(endpointName, L"", L"");

    std::vector<DEVPROPERTY> interfaceDevProperties{};

    SW_DEVICE_CREATE_INFO createInfo = {};
    createInfo.cbSize = sizeof(createInfo);

    // build the instance id, which becomes the middle of the SWD id
    std::wstring instanceId = internal::NormalizeDeviceInstanceIdWStringCopy(
        std::wstring{ MIDI_SYNTH_INSTANCE_ID_PREFIX } + uniqueIdentifier);

    createInfo.pszInstanceId = instanceId.c_str();
    createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
    createInfo.pszDeviceDescription = friendlyName.c_str();

    MIDIENDPOINTCOMMONPROPERTIES commonProperties{};
    commonProperties.TransportId = m_transportId;
    commonProperties.EndpointDeviceType = MidiEndpointDeviceType::MidiEndpointDeviceType_MidiSynthesizer;
    commonProperties.FriendlyName = friendlyName.c_str();
    commonProperties.TransportCode = transportCode.c_str();
    commonProperties.EndpointName = endpointName.c_str();
    commonProperties.EndpointDescription = endpointDescription.c_str();
    commonProperties.CustomEndpointName = nullptr;
    commonProperties.CustomEndpointDescription = nullptr;
    commonProperties.UniqueIdentifier = uniqueIdentifier.c_str();
    commonProperties.ManufacturerName = TRANSPORT_MANUFACTURER;
    commonProperties.SupportedDataFormats = MidiDataFormats::MidiDataFormats_UMP;
    commonProperties.NativeDataFormat = MidiDataFormats::MidiDataFormats_UMP;

    UINT32 capabilities{ 0 };
    capabilities |= MidiEndpointCapabilities_SupportsMidi1Protocol;
    capabilities |= MidiEndpointCapabilities_SupportsMidi2Protocol;
    capabilities |= MidiEndpointCapabilities_SupportsMultiClient;
    capabilities |= MidiEndpointCapabilities_GenerateIncomingTimestamps;
    commonProperties.Capabilities = (MidiEndpointCapabilities)capabilities;

    // One group terminal block in each direction, so WinMM and WinRT MIDI 1.0 get a single input
    // and a single output port rather than sixteen of each.
    std::vector<internal::GroupTerminalBlockInternal> blocks{};

    internal::GroupTerminalBlockInternal gtbIn;
    gtbIn.Number = 1;                                       // gtb numbers start at 1
    gtbIn.GroupCount = 1;
    gtbIn.FirstGroupIndex = MIDI_SYNTH_GROUP_INDEX;
    gtbIn.Protocol = 0x00;                                  // unknown, so the endpoint protocol decides
    gtbIn.Direction = MIDI_GROUP_TERMINAL_BLOCK_INPUT;      // MIDI Out from the user's perspective
    gtbIn.Name = friendlyName;
    blocks.push_back(gtbIn);

    internal::GroupTerminalBlockInternal gtbOut;
    gtbOut.Number = 2;
    gtbOut.GroupCount = 1;
    gtbOut.FirstGroupIndex = MIDI_SYNTH_GROUP_INDEX;
    gtbOut.Protocol = 0x00;
    gtbOut.Direction = MIDI_GROUP_TERMINAL_BLOCK_OUTPUT;    // MIDI In from the user's perspective
    gtbOut.Name = friendlyName;
    blocks.push_back(gtbOut);

    std::vector<std::byte> groupTerminalBlockData;
    if (internal::WriteGroupTerminalBlocksToPropertyDataPointer(blocks, groupTerminalBlockData))
    {
        interfaceDevProperties.push_back({ { PKEY_MIDI_GroupTerminalBlocks, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BINARY, (ULONG)groupTerminalBlockData.size(), (PVOID)groupTerminalBlockData.data() });
    }

    // Sort out MIDI 1.0 endpoint names
    WindowsMidiServicesNamingLib::MidiEndpointNameTable nameTable{};

    LOG_IF_FAILED(nameTable.PopulateAllEntriesForNativeUmpDevice(L"", blocks));

    if (Feature_Servicing_MIDI2PortNamingRework::IsEnabled())
    {
        // There is no MIDI 1.0 driver behind this, so there is no older name to match against.
        nameTable.SetPortNamesHaveLegacyEquivalent(false);

        LOG_IF_FAILED(nameTable.RebuildNewStyleNames(friendlyName, false));
    }

    LOG_IF_FAILED(nameTable.WriteProperties(interfaceDevProperties));

    wil::unique_cotaskmem_string newDeviceInterfaceId;

    RETURN_IF_FAILED(m_midiDeviceManager->ActivateEndpoint(
        (PCWSTR)m_parentDeviceId.c_str(),                       // parent instance Id
        false,                                                  // UMP-only. When false, WinMM MIDI 1.0 ports are created
        MidiFlow::MidiFlowBidirectional,
        &commonProperties,
        (ULONG)interfaceDevProperties.size(),
        (ULONG)0,
        interfaceDevProperties.data(),
        nullptr,
        &createInfo,
        &newDeviceInterfaceId));

    auto const endpointInterfaceId =
        internal::NormalizeEndpointInterfaceIdWStringCopy(newDeviceInterfaceId.get());

    {
        auto lock = m_endpointLock.lock_exclusive();

        m_endpointShortInstanceId = instanceId;
        m_endpointDeviceInterfaceId = endpointInterfaceId;
    }

    RETURN_IF_FAILED(WriteDeviceIdentity(endpointInterfaceId));

    TraceLoggingWrite(
        MidiSynthTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Endpoint activated", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointInterfaceId.c_str(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    return S_OK;
}


// Must run after ActivateEndpoint, not as part of it. ActivateEndpoint appends its own
// DEVPROP_TYPE_EMPTY entries for every property that in-protocol discovery owns, identity
// included, and those come after the transport's in the same array, so anything written there is
// deleted on the way in. This synthesizer answers no UMP Stream discovery, so it declares the
// identity itself, from the same values the SysEx Identity Reply and Property Exchange use.
HRESULT
CMidi2MidiSynthEndpointManager::WriteDeviceIdentity(std::wstring const& endpointInterfaceId)
{
    RETURN_HR_IF_NULL(E_POINTER, m_midiDeviceManager);
    RETURN_HR_IF(E_UNEXPECTED, endpointInterfaceId.empty());

    MidiSynth::SynthIdentity const identity{};

    MidiDeviceIdentityProperty deviceIdentity{};
    deviceIdentity.ManufacturerSysExIdByte1 = identity.ManufacturerSysExId[0];
    deviceIdentity.ManufacturerSysExIdByte2 = identity.ManufacturerSysExId[1];
    deviceIdentity.ManufacturerSysExIdByte3 = identity.ManufacturerSysExId[2];
    deviceIdentity.DeviceFamilyLsb = static_cast<uint8_t>(identity.FamilyCode & 0x7F);
    deviceIdentity.DeviceFamilyMsb = static_cast<uint8_t>((identity.FamilyCode >> 7) & 0x7F);
    deviceIdentity.DeviceFamilyModelNumberLsb = static_cast<uint8_t>(identity.FamilyMemberCode & 0x7F);
    deviceIdentity.DeviceFamilyModelNumberMsb = static_cast<uint8_t>((identity.FamilyMemberCode >> 7) & 0x7F);
    deviceIdentity.SoftwareRevisionLevelByte1 = identity.SoftwareRevision[0];
    deviceIdentity.SoftwareRevisionLevelByte2 = identity.SoftwareRevision[1];
    deviceIdentity.SoftwareRevisionLevelByte3 = identity.SoftwareRevision[2];
    deviceIdentity.SoftwareRevisionLevelByte4 = identity.SoftwareRevision[3];

    FILETIME identityUpdateTime{};
    GetSystemTimePreciseAsFileTime(&identityUpdateTime);

    // The synthesizer is a singleton, so its product instance id is the same unique identifier the
    // endpoint is named after. The specification limits this to printable ASCII without spaces,
    // which this already is.
    std::wstring const productInstanceId{ MIDI_SYNTH_ENDPOINT_UNIQUE_ID };

    static_assert(
        ARRAYSIZE(MIDI_SYNTH_ENDPOINT_UNIQUE_ID) - 1 <= MIDI_MAX_UMP_PRODUCT_INSTANCE_ID_BYTE_COUNT,
        "The product instance id must fit the UMP stream message that carries it.");

    DEVPROPERTY props[]
    {
        { { PKEY_MIDI_DeviceIdentity, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BINARY, (ULONG)sizeof(deviceIdentity), (PVOID)&deviceIdentity },

        { { PKEY_MIDI_DeviceIdentityLastUpdateTime, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_FILETIME, (ULONG)sizeof(FILETIME), (PVOID)&identityUpdateTime },

        { { PKEY_MIDI_EndpointProvidedProductInstanceId, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_STRING,
            (ULONG)((productInstanceId.length() + 1) * sizeof(wchar_t)),
            (PVOID)productInstanceId.c_str() },

        { { PKEY_MIDI_EndpointProvidedProductInstanceIdLastUpdateTime, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_FILETIME, (ULONG)sizeof(FILETIME), (PVOID)&identityUpdateTime },
    };

    RETURN_IF_FAILED(m_midiDeviceManager->UpdateEndpointProperties(
        endpointInterfaceId.c_str(), ARRAYSIZE(props), props));

    return S_OK;
}


HRESULT
CMidi2MidiSynthEndpointManager::Shutdown()
{
    TraceLoggingWrite(
        MidiSynthTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    LOG_IF_FAILED(TransportState::Current().Shutdown());

    m_midiDeviceManager.reset();
    m_midiProtocolManager.reset();

    m_initialized = false;

    return S_OK;
}
