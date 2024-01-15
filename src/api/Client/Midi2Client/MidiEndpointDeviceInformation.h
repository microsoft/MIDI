// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiEndpointDeviceInformation.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiEndpointDeviceInformation : MidiEndpointDeviceInformationT<MidiEndpointDeviceInformation>
    {
        MidiEndpointDeviceInformation() = default;

        static midi2::MidiEndpointDeviceInformation CreateFromId(_In_ winrt::hstring const& id) noexcept;

        //static winrt::hstring UniversalMidiPacketBidirectionalInterfaceClassId() noexcept { return L"" /* STRING_DEVINTERFACE_UNIVERSALMIDIPACKET_BIDI */; }

        // these would be more robust if they did an enumeration lookup on the loopback/ping properties
        static winrt::hstring DiagnosticsLoopbackAEndpointId() noexcept { return MIDI_DIAGNOSTICS_LOOPBACK_BIDI_ID_A; }
        static winrt::hstring DiagnosticsLoopbackBEndpointId() noexcept { return MIDI_DIAGNOSTICS_LOOPBACK_BIDI_ID_B; }
        static winrt::hstring DiagnosticsInternalPingEndpointId() noexcept { return MIDI_DIAGNOSTICS_PING_BIDI_ID; }

        static winrt::hstring EndpointInterfaceClass() noexcept { return STRING_DEVINTERFACE_UNIVERSALMIDIPACKET_BIDI; }

        static collections::IVectorView<midi2::MidiEndpointDeviceInformation> FindAll(
            _In_ midi2::MidiEndpointDeviceInformationSortOrder const& sortOrder, 
            _In_ midi2::MidiEndpointDeviceInformationFilter const& endpointFilter) noexcept;

        static collections::IVectorView<midi2::MidiEndpointDeviceInformation> FindAll(
            _In_ midi2::MidiEndpointDeviceInformationSortOrder const& sortOrder) noexcept;

        static collections::IVectorView<midi2::MidiEndpointDeviceInformation> FindAll() noexcept;

        static collections::IVectorView<winrt::hstring> GetAdditionalPropertiesList() noexcept;

        static winrt::Windows::Devices::Enumeration::DeviceWatcher CreateWatcher(
            _In_ midi2::MidiEndpointDeviceInformationFilter const& endpointFilter) noexcept;

        static bool MidiEndpointDeviceInformation::DeviceMatchesFilter(
            _In_ midi2::MidiEndpointDeviceInformation const& deviceInformation,
            _In_ midi2::MidiEndpointDeviceInformationFilter const& endpointFilter) noexcept;

        winrt::hstring Id() const noexcept;
        winrt::guid ContainerId() const noexcept { return GetGuidProperty(L"System.Devices.ContainerId", winrt::guid{}); }
        winrt::hstring DeviceInstanceId() const noexcept { return GetStringProperty(L"System.Devices.DeviceInstanceId", L""); }

        winrt::Windows::Devices::Enumeration::DeviceInformation GetParentDeviceInformation() const noexcept;
        winrt::Windows::Devices::Enumeration::DeviceInformation GetContainerInformation() const noexcept;

        //winrt::Windows::Devices::Enumeration::DeviceInformation DeviceInformation() const noexcept { return m_deviceInformation; }

        winrt::hstring Name() const noexcept;

        winrt::hstring TransportSuppliedName() const noexcept { return m_transportSuppliedEndpointName; }  // todo: may need to update this later
        winrt::hstring EndpointSuppliedName() const noexcept { return GetStringProperty(STRING_PKEY_MIDI_EndpointProvidedName, L""); }
        winrt::hstring UserSuppliedName() const noexcept { return GetStringProperty(STRING_PKEY_MIDI_UserSuppliedEndpointName, L""); }

        winrt::hstring TransportId() const noexcept { return GetGuidPropertyAsString(STRING_PKEY_MIDI_AbstractionLayer, L""); }
        winrt::hstring TransportMnemonic() const noexcept { return GetStringProperty(STRING_PKEY_MIDI_TransportMnemonic, L""); }

        winrt::hstring TransportSuppliedSerialNumber() const noexcept { return GetStringProperty(STRING_PKEY_MIDI_SerialNumber, L"");}
        winrt::hstring ManufacturerName() const noexcept { return GetStringProperty(STRING_PKEY_MIDI_ManufacturerName, L""); }

        bool SupportsMultiClient() const noexcept { return GetBoolProperty(STRING_PKEY_MIDI_SupportsMulticlient, true); }

        midi2::MidiEndpointNativeDataFormat NativeDataFormat() const noexcept;
        midi2::MidiEndpointDevicePurpose EndpointPurpose() const noexcept;


        bool HasStaticFunctionBlocks() const noexcept { return GetBoolProperty(STRING_PKEY_MIDI_FunctionBlocksAreStatic, false); }
        uint8_t FunctionBlockCount() const noexcept { return GetByteProperty(STRING_PKEY_MIDI_FunctionBlockCount, (uint8_t)0); }
        collections::IMapView<uint8_t, midi2::MidiFunctionBlock> FunctionBlocks() const noexcept;

        collections::IVectorView<midi2::MidiGroupTerminalBlock> GroupTerminalBlocks() const noexcept;


        winrt::hstring ProductInstanceId() const noexcept { return GetStringProperty(STRING_PKEY_MIDI_EndpointProvidedProductInstanceId, L""); }
        uint8_t SpecificationVersionMajor() const noexcept { return GetByteProperty(STRING_PKEY_MIDI_EndpointUmpVersionMajor, (uint8_t)0); }
        uint8_t SpecificationVersionMinor() const noexcept { return GetByteProperty(STRING_PKEY_MIDI_EndpointUmpVersionMinor, (uint8_t)0); }

        bool SupportsMidi10Protocol() const noexcept { return GetBoolProperty(STRING_PKEY_MIDI_EndpointSupportsMidi1Protocol, false); }
        bool SupportsMidi20Protocol() const noexcept { return GetBoolProperty(STRING_PKEY_MIDI_EndpointSupportsMidi2Protocol, false); }
        midi2::MidiProtocol ConfiguredProtocol() const noexcept;

        bool SupportsReceivingJRTimestamps() const noexcept { return GetBoolProperty(STRING_PKEY_MIDI_EndpointSupportsReceivingJRTimestamps, false); }
        bool SupportsSendingJRTimestamps() const noexcept { return GetBoolProperty(STRING_PKEY_MIDI_EndpointSupportsSendingJRTimestamps, false); }
        bool ConfiguredToReceiveJRTimestamps() const noexcept { return GetBoolProperty(STRING_PKEY_MIDI_EndpointConfiguredToReceiveJRTimestamps, false); }
        bool ConfiguredToSendJRTimestamps() const noexcept { return GetBoolProperty(STRING_PKEY_MIDI_EndpointConfiguredToSendJRTimestamps, false); }



        // MIDI Device Identity (sysex stuff)
        winrt::com_array<uint8_t> DeviceIdentitySystemExclusiveId() const noexcept;
        uint8_t DeviceIdentityDeviceFamilyLsb() const noexcept { return m_deviceIdentity.DeviceFamilyLsb; }
        uint8_t DeviceIdentityDeviceFamilyMsb() const noexcept { return m_deviceIdentity.DeviceFamilyMsb; }
        uint8_t DeviceIdentityDeviceFamilyModelNumberLsb() const noexcept { return m_deviceIdentity.DeviceFamilyModelNumberLsb; }
        uint8_t DeviceIdentityDeviceFamilyModelNumberMsb() const noexcept { return m_deviceIdentity.DeviceFamilyModelNumberMsb; }
        winrt::com_array<uint8_t> DeviceIdentitySoftwareRevisionLevel() const noexcept;


        bool RequiresNoteOffTranslation() { return GetBoolProperty(STRING_PKEY_MIDI_RequiresNoteOffTranslation, false); }
        bool SupportsMidiPolyphonicExpression() { return GetBoolProperty(STRING_PKEY_MIDI_SupportsMidiPolyphonicExpression, false); }
        uint16_t RecommendedCCAutomationIntervalMS() { return GetUInt16Property(STRING_PKEY_MIDI_RecommendedCCAutomationIntervalMS, 0); }

        winrt::hstring Description() const noexcept { return GetStringProperty(STRING_PKEY_MIDI_UserSuppliedDescription, L""); }
        winrt::hstring LargeImagePath() const noexcept { return GetStringProperty(STRING_PKEY_MIDI_UserSuppliedLargeImagePath, L""); }
        winrt::hstring SmallImagePath() const noexcept { return GetStringProperty(STRING_PKEY_MIDI_UserSuppliedSmallImagePath, L""); }



        collections::IMapView<winrt::hstring, IInspectable> Properties() { return m_properties.GetView(); }


        bool UpdateFromDeviceInformation(
            _In_ winrt::Windows::Devices::Enumeration::DeviceInformation const& deviceInformation) noexcept;

        bool UpdateFromDeviceInformationUpdate(
            _In_ winrt::Windows::Devices::Enumeration::DeviceInformationUpdate const& deviceInformationUpdate) noexcept;

        void InternalUpdateFromDeviceInformation(
            _In_ winrt::Windows::Devices::Enumeration::DeviceInformation const& info) noexcept;

    private:
        winrt::hstring GetStringProperty(
            _In_ winrt::hstring key,
            _In_ winrt::hstring defaultValue) const noexcept;

        winrt::guid GetGuidProperty(
            _In_ winrt::hstring key,
            _In_ winrt::guid defaultValue) const noexcept;

        winrt::hstring GetGuidPropertyAsString(
            _In_ winrt::hstring key,
            _In_ winrt::hstring defaultValue) const noexcept;


        uint8_t GetByteProperty(
            _In_ winrt::hstring key,
            _In_ uint8_t defaultValue) const noexcept;

        uint64_t GetUInt64Property(
            _In_ winrt::hstring key,
            _In_ uint64_t defaultValue) const noexcept;

        uint32_t GetUInt32Property(
            _In_ winrt::hstring key,
            _In_ uint32_t defaultValue) const noexcept;

        uint16_t GetUInt16Property(
            _In_ winrt::hstring key,
            _In_ uint16_t defaultValue) const noexcept;


        int64_t GetInt64Property(
            _In_ winrt::hstring key,
            _In_ int64_t defaultValue) const noexcept;

        int32_t GetInt32Property(
            _In_ winrt::hstring key,
            _In_ int32_t defaultValue) const noexcept;

        int16_t GetInt16Property(
            _In_ winrt::hstring key,
            _In_ int16_t defaultValue) const noexcept;


        bool GetBoolProperty(
            _In_ winrt::hstring key,
            _In_ bool defaultValue) const noexcept;

        foundation::IReferenceArray<uint8_t> GetBinaryProperty(
            _In_ winrt::hstring key) const noexcept;



        // TODO: This should come from a property in the bag, not something here to get out of sync
        winrt::hstring m_transportSuppliedEndpointName{};

        winrt::hstring m_id{};


        collections::IMap<winrt::hstring, IInspectable> m_properties =
            winrt::single_threaded_map< winrt::hstring, IInspectable>();


        collections::IMap<uint8_t, midi2::MidiFunctionBlock> m_functionBlocks =
            winrt::single_threaded_map<uint8_t, midi2::MidiFunctionBlock>();

        collections::IVector<midi2::MidiGroupTerminalBlock> m_groupTerminalBlocks{ winrt::single_threaded_vector<midi2::MidiGroupTerminalBlock>() };


        MidiDeviceIdentityProperty m_deviceIdentity;

        void ReadDeviceIdentity();
        void ReadFunctionBlocks();
        void ReadGroupTerminalBlocks();


        void AddOrUpdateFunctionBlock(_In_ foundation::IReferenceArray<uint8_t> refArray);


    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiEndpointDeviceInformation : MidiEndpointDeviceInformationT<MidiEndpointDeviceInformation, implementation::MidiEndpointDeviceInformation>
    {
    };
}
