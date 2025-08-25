// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "MidiEndpointDeviceInformation.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{
    struct MidiEndpointDeviceInformation : MidiEndpointDeviceInformationT<MidiEndpointDeviceInformation>
    {
        MidiEndpointDeviceInformation() = default;

        static midi2::MidiEndpointDeviceInformation CreateFromEndpointDeviceId(_In_ winrt::hstring const& endpointDeviceId) noexcept;

        static midi2::MidiEndpointDeviceInformation CreateFromAssociatedMidi1PortDeviceId(_In_ winrt::hstring const& deviceId) noexcept;

        static winrt::guid EndpointInterfaceClass() noexcept { return internal::StringToGuid(STRING_DEVINTERFACE_UNIVERSALMIDIPACKET_BIDI); }

        static collections::IVectorView<midi2::MidiEndpointDeviceInformation> FindAll(
            _In_ midi2::MidiEndpointDeviceInformationSortOrder const& sortOrder, 
            _In_ midi2::MidiEndpointDeviceInformationFilters const& endpointFilters) noexcept;

        static collections::IVectorView<midi2::MidiEndpointDeviceInformation> FindAll(
            _In_ midi2::MidiEndpointDeviceInformationSortOrder const& sortOrder) noexcept;

        static collections::IVectorView<midi2::MidiEndpointDeviceInformation> FindAll() noexcept;

        static midi2::MidiEndpointDeviceInformation CreateFromAssociatedMidi1PortNumber(_In_ uint32_t const portNumber, _In_ midi2::Midi1PortFlow const portFlow) noexcept;
        static winrt::hstring FindEndpointDeviceIdForAssociatedMidi1PortNumber(_In_ uint32_t const portNumber, _In_ midi2::Midi1PortFlow const portFlow) noexcept;

        static collections::IVectorView<midi2::MidiEndpointDeviceInformation> FindAllForAssociatedMidi1PortName(_In_ winrt::hstring const& portName, _In_ midi2::Midi1PortFlow const portFlow) noexcept;
        static collections::IVectorView<winrt::hstring> FindAllEndpointDeviceIdsForAssociatedMidi1PortName(_In_ winrt::hstring const& portName, _In_ midi2::Midi1PortFlow const portFlow) noexcept;






        static collections::IVectorView<winrt::hstring> GetAdditionalPropertiesList() noexcept;

        static bool DeviceMatchesFilter(
            _In_ midi2::MidiEndpointDeviceInformation const& deviceInformation,
            _In_ midi2::MidiEndpointDeviceInformationFilters const& endpointFilters) noexcept;


        winrt::hstring EndpointDeviceId() const noexcept { return m_id; }

        winrt::guid ContainerId() const noexcept { return internal::GetDeviceInfoProperty<winrt::guid>(m_properties, L"System.Devices.ContainerId", winrt::guid{}); }
        winrt::hstring DeviceInstanceId() const noexcept { return internal::GetDeviceInfoProperty<winrt::hstring>(m_properties, L"System.Devices.DeviceInstanceId", L""); }

        winrt::Windows::Devices::Enumeration::DeviceInformation GetParentDeviceInformation() const noexcept;
        winrt::Windows::Devices::Enumeration::DeviceInformation GetContainerDeviceInformation() const noexcept;

        //foundation::DateTime DeclaredProductInstanceIdLastUpdateTime() const noexcept { return GetDateTimeProperty(STRING_PKEY_MIDI_EndpointProvidedProductInstanceIdLastUpdateTime, foundation::DateTime{}); }
        //foundation::DateTime DeclaredEndpointSuppliedNameLastUpdateTime() const noexcept { return GetDateTimeProperty(STRING_PKEY_MIDI_EndpointProvidedNameLastUpdateTime, foundation::DateTime{}); }
        foundation::DateTime DeclaredStreamConfigurationLastUpdateTime() const noexcept { return GetDateTimeProperty(STRING_PKEY_MIDI_EndpointConfigurationLastUpdateTime, foundation::DateTime{}); }
        foundation::DateTime DeclaredEndpointInfoLastUpdateTime() const noexcept { return GetDateTimeProperty(STRING_PKEY_MIDI_EndpointInformationLastUpdateTime, foundation::DateTime{}); }
        foundation::DateTime DeclaredDeviceIdentityLastUpdateTime() const noexcept { return GetDateTimeProperty(STRING_PKEY_MIDI_DeviceIdentityLastUpdateTime, foundation::DateTime{}); }
        foundation::DateTime DeclaredFunctionBlocksLastUpdateTime() const noexcept { return GetDateTimeProperty(STRING_PKEY_MIDI_FunctionBlocksLastUpdateTime, foundation::DateTime{}); }

        winrt::hstring Name() const noexcept;

        midi2::MidiEndpointDevicePurpose EndpointPurpose() const noexcept;


        collections::IVectorView<midi2::MidiFunctionBlock> GetDeclaredFunctionBlocks() const noexcept;

        collections::IVectorView<midi2::MidiGroupTerminalBlock> GetGroupTerminalBlocks() const noexcept;


        midi2::MidiDeclaredEndpointInfo GetDeclaredEndpointInfo() const noexcept;
        midi2::MidiDeclaredDeviceIdentity GetDeclaredDeviceIdentity() const noexcept;
        midi2::MidiDeclaredStreamConfiguration GetDeclaredStreamConfiguration() const noexcept;

        midi2::MidiEndpointUserSuppliedInfo GetUserSuppliedInfo() const noexcept;
        midi2::MidiEndpointTransportSuppliedInfo GetTransportSuppliedInfo() const noexcept;


        collections::IMapView<winrt::hstring, IInspectable> Properties() { return m_properties.GetView(); }

        collections::IVectorView<midi2::MidiEndpointAssociatedPortDeviceInformation> FindAllAssociatedMidi1PortsForThisEndpoint(_In_ midi2::Midi1PortFlow const portFlow) noexcept;
        collections::IVectorView<midi2::MidiEndpointAssociatedPortDeviceInformation> FindAllAssociatedMidi1PortsForThisEndpoint(_In_ midi2::Midi1PortFlow const portFlow, _In_ bool const useCachedPortInformationIfAvailable) noexcept;

        midi2::MidiEndpointAssociatedPortDeviceInformation FindAssociatedMidi1PortForGroupForThisEndpoint(_In_ midi2::MidiGroup const& group, _In_ midi2::Midi1PortFlow const portFlow) noexcept;
        midi2::MidiEndpointAssociatedPortDeviceInformation FindAssociatedMidi1PortForGroupForThisEndpoint(_In_ midi2::MidiGroup const& group, _In_ midi2::Midi1PortFlow const portFlow, _In_ bool const useCachedPortInformationIfAvailable) noexcept;

        midi2::Midi1PortNamingApproach Midi1PortNamingApproach() const noexcept;

        collections::IVectorView<midi2::Midi1PortNameTableEntry> GetNameTable() const noexcept;

        winrt::hstring ToString();

        bool UpdateFromDeviceInformation(
            _In_ winrt::Windows::Devices::Enumeration::DeviceInformation const& deviceInformation) noexcept;

        bool UpdateFromDeviceInformationUpdate(
            _In_ winrt::Windows::Devices::Enumeration::DeviceInformationUpdate const& deviceInformationUpdate) noexcept;

        void InternalUpdateFromDeviceInformation(
            _In_ winrt::Windows::Devices::Enumeration::DeviceInformation const& info) noexcept;

    private:
        static winrt::hstring GetMidi1PortDeviceSelector(_In_ midi2::Midi1PortFlow const portFlow);

        collections::IVector<midi2::MidiEndpointAssociatedPortDeviceInformation>* GetMidi1PortCache(_In_ midi2::Midi1PortFlow const portFlow);
        collections::IVector<midi2::MidiEndpointAssociatedPortDeviceInformation>* FindAndCacheAssociatedMidi1PortInformation(_In_ midi2::Midi1PortFlow const portFlow, _In_ bool const refreshCache) noexcept;

        // we keep these here so they can be returned from functions, but we re-query these each time
        collections::IVector<midi2::MidiEndpointAssociatedPortDeviceInformation> m_midi1SourcePorts{ winrt::single_threaded_vector<midi2::MidiEndpointAssociatedPortDeviceInformation>() };
        collections::IVector<midi2::MidiEndpointAssociatedPortDeviceInformation> m_midi1DestinationPorts{ winrt::single_threaded_vector<midi2::MidiEndpointAssociatedPortDeviceInformation>() };

        foundation::DateTime GetDateTimeProperty(
            _In_ winrt::hstring key,
            _In_ foundation::DateTime defaultValue) const noexcept;

        foundation::IReferenceArray<uint8_t> GetBinaryProperty(
            _In_ winrt::hstring key) const noexcept;

        winrt::hstring m_id{};

        collections::IMap<winrt::hstring, foundation::IInspectable> m_properties 
            { winrt::multi_threaded_map<winrt::hstring, foundation::IInspectable>() };

        // these don't change, so fine to keep them as a class var
        collections::IVector<midi2::MidiGroupTerminalBlock> m_groupTerminalBlocks
            { winrt::multi_threaded_vector<midi2::MidiGroupTerminalBlock>() };

        void ReadGroupTerminalBlocks();
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::factory_implementation
{
    struct MidiEndpointDeviceInformation : MidiEndpointDeviceInformationT<MidiEndpointDeviceInformation, implementation::MidiEndpointDeviceInformation>
    {
    };
}
