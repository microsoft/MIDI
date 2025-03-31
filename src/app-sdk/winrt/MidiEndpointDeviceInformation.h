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

        static winrt::guid EndpointInterfaceClass() noexcept { return internal::StringToGuid(STRING_DEVINTERFACE_UNIVERSALMIDIPACKET_BIDI); }

        static collections::IVectorView<midi2::MidiEndpointDeviceInformation> FindAll(
            _In_ midi2::MidiEndpointDeviceInformationSortOrder const& sortOrder, 
            _In_ midi2::MidiEndpointDeviceInformationFilters const& endpointFilters) noexcept;

        static collections::IVectorView<midi2::MidiEndpointDeviceInformation> FindAll(
            _In_ midi2::MidiEndpointDeviceInformationSortOrder const& sortOrder) noexcept;

        static collections::IVectorView<midi2::MidiEndpointDeviceInformation> FindAll() noexcept;

        static collections::IVectorView<winrt::hstring> GetAdditionalPropertiesList() noexcept;

        //static winrt::Windows::Devices::Enumeration::DeviceWatcher CreateWatcher(
        //    _In_ midi2::MidiEndpointDeviceInformationFilters const& endpointFilters) noexcept;

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

//        winrt::hstring TransportSuppliedName() const noexcept { return m_transportSuppliedEndpointName; }  // todo: may need to update this later

        midi2::MidiEndpointDevicePurpose EndpointPurpose() const noexcept;


        collections::IVectorView<midi2::MidiFunctionBlock> GetDeclaredFunctionBlocks() const noexcept;

        collections::IVectorView<midi2::MidiGroupTerminalBlock> GetGroupTerminalBlocks() const noexcept;


        midi2::MidiDeclaredEndpointInfo GetDeclaredEndpointInfo() const noexcept;
        midi2::MidiDeclaredDeviceIdentity GetDeclaredDeviceIdentity() const noexcept;
        midi2::MidiDeclaredStreamConfiguration GetDeclaredStreamConfiguration() const noexcept;

        midi2::MidiEndpointUserSuppliedInfo GetUserSuppliedInfo() const noexcept;
        midi2::MidiEndpointTransportSuppliedInfo GetTransportSuppliedInfo() const noexcept;


        collections::IMapView<winrt::hstring, IInspectable> Properties() { return m_properties.GetView(); }

        collections::IVectorView<midi2::MidiEndpointAssociatedPortDeviceInformation> GetAssociatedMidi1Ports(_In_ midi2::MidiPortFlow const portFlow) noexcept;


        winrt::hstring ToString();

        bool UpdateFromDeviceInformation(
            _In_ winrt::Windows::Devices::Enumeration::DeviceInformation const& deviceInformation) noexcept;

        bool UpdateFromDeviceInformationUpdate(
            _In_ winrt::Windows::Devices::Enumeration::DeviceInformationUpdate const& deviceInformationUpdate) noexcept;

        void InternalUpdateFromDeviceInformation(
            _In_ winrt::Windows::Devices::Enumeration::DeviceInformation const& info) noexcept;

    private:


        foundation::DateTime GetDateTimeProperty(
            _In_ winrt::hstring key,
            _In_ foundation::DateTime defaultValue) const noexcept;


        //template<typename T>
        //inline T GetProperty(
        //    winrt::hstring key,
        //    T defaultValue
        //) const noexcept
        //{
        //    if (!m_properties.HasKey(key)) return defaultValue;
        //    if (m_properties.Lookup(key) == nullptr) return defaultValue;

        //    std::optional<T> opt = m_properties.Lookup(key).try_as<T>();

        //    if (opt == std::nullopt)
        //    {
        //        return defaultValue;
        //    }
        //    else
        //    {
        //        return opt.value();
        //    }
        //}

        //template<typename T>
        //T GetProperty(
        //    _In_ winrt::hstring key,
        //    _In_ T defaultValue) const noexcept;

        //winrt::hstring GetStringProperty(
        //    _In_ winrt::hstring key,
        //    _In_ winrt::hstring defaultValue) const noexcept;

        //winrt::guid GetGuidProperty(
        //    _In_ winrt::hstring key,
        //    _In_ winrt::guid defaultValue) const noexcept;

        //winrt::hstring GetGuidPropertyAsString(
        //    _In_ winrt::hstring key,
        //    _In_ winrt::hstring defaultValue) const noexcept;

        //uint8_t GetByteProperty(
        //    _In_ winrt::hstring key,
        //    _In_ uint8_t defaultValue) const noexcept;

        //uint64_t GetUInt64Property(
        //    _In_ winrt::hstring key,
        //    _In_ uint64_t defaultValue) const noexcept;

        //uint32_t GetUInt32Property(
        //    _In_ winrt::hstring key,
        //    _In_ uint32_t defaultValue) const noexcept;

        //uint16_t GetUInt16Property(
        //    _In_ winrt::hstring key,
        //    _In_ uint16_t defaultValue) const noexcept;


        //int64_t GetInt64Property(
        //    _In_ winrt::hstring key,
        //    _In_ int64_t defaultValue) const noexcept;

        //int32_t GetInt32Property(
        //    _In_ winrt::hstring key,
        //    _In_ int32_t defaultValue) const noexcept;

        //int16_t GetInt16Property(
        //    _In_ winrt::hstring key,
        //    _In_ int16_t defaultValue) const noexcept;


        //bool GetBoolProperty(
        //    _In_ winrt::hstring key,
        //    _In_ bool defaultValue) const noexcept;

        foundation::IReferenceArray<uint8_t> GetBinaryProperty(
            _In_ winrt::hstring key) const noexcept;


        winrt::hstring m_id{};


        collections::IMap<winrt::hstring, foundation::IInspectable> m_properties 
            { winrt::multi_threaded_map<winrt::hstring, foundation::IInspectable>() };

        // these don't change, so fine to keep them as a class var
        collections::IVector<midi2::MidiGroupTerminalBlock> m_groupTerminalBlocks
            { winrt::single_threaded_vector<midi2::MidiGroupTerminalBlock>() };


        //MidiDeviceIdentityProperty m_deviceIdentity;

        //void ReadDeviceIdentity();
        //void ReadFunctionBlocks();
        void ReadGroupTerminalBlocks();


        //void AddOrUpdateFunctionBlock(_In_ foundation::IReferenceArray<uint8_t> refArray);


    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::factory_implementation
{
    struct MidiEndpointDeviceInformation : MidiEndpointDeviceInformationT<MidiEndpointDeviceInformation, implementation::MidiEndpointDeviceInformation>
    {
    };
}
