// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "Enumeration.MidiEndpointDeviceInformation.g.h"

namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{
    struct MidiEndpointDeviceInformation : MidiEndpointDeviceInformationT<MidiEndpointDeviceInformation>
    {
        MidiEndpointDeviceInformation() = default;

        static midi2enum::MidiEndpointDeviceInformation CreateFromEndpointDeviceId(_In_ winrt::hstring const& endpointDeviceId) noexcept;

        static winrt::guid EndpointInterfaceClass() noexcept { return internal::StringToGuid(STRING_DEVINTERFACE_UNIVERSALMIDIPACKET_BIDI); }

        static collections::IVectorView<midi2enum::MidiEndpointDeviceInformation> FindAll(
            _In_ midi2enum::MidiEndpointDeviceInformationSortOrder const& sortOrder,
            _In_ midi2enum::MidiEndpointDeviceInformationFilters const& endpointFilters) noexcept;

        static collections::IVectorView<midi2enum::MidiEndpointDeviceInformation> FindAll(
            _In_ midi2enum::MidiEndpointDeviceInformationSortOrder const& sortOrder) noexcept;

        static collections::IVectorView<midi2enum::MidiEndpointDeviceInformation> FindAll() noexcept;

        static collections::IVectorView<midi2enum::MidiEndpointDeviceInformation> FindAllForContainer(
            _In_ winrt::guid const& containerId) noexcept;

        static collections::IVectorView<winrt::hstring> GetAdditionalPropertiesList() noexcept;

        static bool DeviceMatchesFilter(
            _In_ midi2enum::MidiEndpointDeviceInformation const& deviceInformation,
            _In_ midi2enum::MidiEndpointDeviceInformationFilters const& endpointFilters) noexcept;


        winrt::hstring EndpointDeviceId() const noexcept { return m_id; }

        winrt::guid ContainerId() const noexcept { return internal::GetDeviceInfoProperty<winrt::guid>(m_properties, L"System.Devices.ContainerId", winrt::guid{}); }
        winrt::hstring DeviceInstanceId() const noexcept { return internal::GetDeviceInfoProperty<winrt::hstring>(m_properties, L"System.Devices.DeviceInstanceId", L""); }

        winrt::hstring ParentDeviceInstanceId() const noexcept { return m_parentDeviceInstanceId; }

        midi2enum::MidiParentDeviceInformation GetParentDeviceInformation() const noexcept;
        winrt::Windows::Devices::Enumeration::DeviceInformation GetContainerDeviceInformation() const noexcept;

        //foundation::DateTime DeclaredProductInstanceIdLastUpdateTime() const noexcept { return GetDateTimeProperty(STRING_PKEY_MIDI_EndpointProvidedProductInstanceIdLastUpdateTime, foundation::DateTime{}); }
        //foundation::DateTime DeclaredEndpointSuppliedNameLastUpdateTime() const noexcept { return GetDateTimeProperty(STRING_PKEY_MIDI_EndpointProvidedNameLastUpdateTime, foundation::DateTime{}); }
        foundation::DateTime DeclaredStreamConfigurationLastUpdateTime() const noexcept { return GetDateTimeProperty(STRING_PKEY_MIDI_EndpointConfigurationLastUpdateTime, foundation::DateTime{}); }
        foundation::DateTime DeclaredEndpointInfoLastUpdateTime() const noexcept { return GetDateTimeProperty(STRING_PKEY_MIDI_EndpointInformationLastUpdateTime, foundation::DateTime{}); }
        foundation::DateTime DeclaredDeviceIdentityLastUpdateTime() const noexcept { return GetDateTimeProperty(STRING_PKEY_MIDI_DeviceIdentityLastUpdateTime, foundation::DateTime{}); }
        foundation::DateTime DeclaredFunctionBlocksLastUpdateTime() const noexcept { return GetDateTimeProperty(STRING_PKEY_MIDI_FunctionBlocksLastUpdateTime, foundation::DateTime{}); }

        winrt::hstring Name() const noexcept;

        midi2enum::MidiEndpointDevicePurpose EndpointPurpose() const noexcept;


        collections::IVectorView<midi2enum::MidiFunctionBlock> GetDeclaredFunctionBlocks() const noexcept;
        collections::IVectorView<midi2enum::MidiGroupTerminalBlock> GetGroupTerminalBlocks() const noexcept;

        midi2enum::MidiDeclaredEndpointInfo GetDeclaredEndpointInfo() const noexcept;
        midi2enum::MidiDeclaredDeviceIdentity GetDeclaredDeviceIdentity() const noexcept;
        midi2enum::MidiDeclaredStreamConfiguration GetDeclaredStreamConfiguration() const noexcept;

        midi2enum::MidiEndpointUserSuppliedInfo GetUserSuppliedInfo() const noexcept;
        midi2enum::MidiEndpointTransportSuppliedInfo GetTransportSuppliedInfo() const noexcept;


        collections::IMapView<winrt::hstring, foundation::IInspectable> Properties() { return m_properties.GetView(); }

        midi2enum::Midi1PortNamingApproach Midi1PortNamingApproach() const noexcept;

        collections::IVectorView<midi2enum::Midi1PortNameTableEntry> GetNameTable() const noexcept;

        winrt::hstring ToString();


        bool UpdateFromDeviceInformation(
            _In_ enumeration::DeviceInformation const& deviceInformation) noexcept;

        bool UpdateFromDeviceInformationUpdate(
            _In_ enumeration::DeviceInformationUpdate const& deviceInformationUpdate) noexcept;

        void InternalUpdateFromDeviceInformation(
            _In_ enumeration::DeviceInformation const& info) noexcept;

    private:
        winrt::hstring m_parentDeviceInstanceId{};
        winrt::hstring m_mediaDriverParentDeviceInstanceId{};

   //     static winrt::hstring GetMidi1PortDeviceSelector(_In_ midi2enum::Midi1PortFlow const portFlow);

        // we keep these here so they can be returned from functions, but we re-query these each time
 //       collections::IVector<legacy::MidiLegacyPortDeviceInformation> m_midi1SourcePorts{ winrt::single_threaded_vector<legacy::MidiLegacyPortDeviceInformation>() };
 //       collections::IVector<legacy::MidiLegacyPortDeviceInformation> m_midi1DestinationPorts{ winrt::single_threaded_vector<legacy::MidiLegacyPortDeviceInformation>() };

        foundation::DateTime GetDateTimeProperty(
            _In_ winrt::hstring key,
            _In_ foundation::DateTime defaultValue) const noexcept;

        foundation::IReferenceArray<uint8_t> GetBinaryProperty(
            _In_ winrt::hstring key) const noexcept;

        winrt::hstring m_id{};

        collections::IMap<winrt::hstring, foundation::IInspectable> m_properties 
            { winrt::multi_threaded_map<winrt::hstring, foundation::IInspectable>() };

        // these don't change, so fine to keep them as a class var
        collections::IVector<midi2enum::MidiGroupTerminalBlock> m_groupTerminalBlocks
            { winrt::multi_threaded_vector<midi2enum::MidiGroupTerminalBlock>() };

        void ReadGroupTerminalBlocks();


    public:
        bool IsMuted() const noexcept { return internal::GetDeviceInfoProperty<bool>(m_properties, STRING_PKEY_MIDI_IsMuted, false); }
        bool IsMidi1PortCreationEnabled() const noexcept { return internal::GetDeviceInfoProperty<bool>(m_properties, STRING_PKEY_MIDI_CreateMidi1PortsForEndpoint, true); }

    };
}
namespace winrt::Windows::Devices::Midi2::Enumeration::factory_implementation
{
    struct MidiEndpointDeviceInformation : MidiEndpointDeviceInformationT<MidiEndpointDeviceInformation, implementation::MidiEndpointDeviceInformation>
    {
    };
}
