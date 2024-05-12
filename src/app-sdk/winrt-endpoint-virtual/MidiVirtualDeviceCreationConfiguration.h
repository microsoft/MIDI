// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiVirtualDeviceCreationConfiguration.g.h"


namespace winrt::Microsoft::Devices::Midi2::Endpoints::Virtual::implementation
{
    struct MidiVirtualDeviceCreationConfiguration : MidiVirtualDeviceCreationConfigurationT<MidiVirtualDeviceCreationConfiguration>
    {
        MidiVirtualDeviceCreationConfiguration() = default;

        MidiVirtualDeviceCreationConfiguration(
            _In_ winrt::hstring name,
            _In_ winrt::hstring description,
            _In_ winrt::hstring manufacturer,
            midi2::MidiDeclaredEndpointInfo declaredEndpointInfo
        );

        MidiVirtualDeviceCreationConfiguration(
            _In_ winrt::hstring name,
            _In_ winrt::hstring description,
            _In_ winrt::hstring manufacturer,
            midi2::MidiDeclaredEndpointInfo declaredEndpointInfo,
            midi2::MidiDeclaredDeviceIdentity declaredDeviceIdentity
            );

        MidiVirtualDeviceCreationConfiguration(
            _In_ winrt::hstring name,
            _In_ winrt::hstring description,
            _In_ winrt::hstring manufacturer,
            midi2::MidiDeclaredEndpointInfo declaredEndpointInfo,
            midi2::MidiDeclaredDeviceIdentity declaredDeviceIdentity,
            midi2::MidiEndpointUserSuppliedInfo userSuppliedInfo
        );

        bool IsFromConfigurationFile() const noexcept { return false; }
        winrt::guid TransportId() const noexcept { return virt::MidiVirtualDeviceManager::AbstractionId(); }
        winrt::hstring GetConfigurationJson() const noexcept;


        winrt::hstring Name() const noexcept { return m_endpointName; }
        void Name(_In_ winrt::hstring const& value) noexcept { m_endpointName = internal::TrimmedHStringCopy(value); }

        winrt::hstring Description() const noexcept { return m_description; }
        void Description(_In_ winrt::hstring const& value) noexcept { m_description = internal::TrimmedHStringCopy(value); }

        winrt::hstring Manufacturer() const noexcept { return m_manufacturer; }
        void Manufacturer(_In_ winrt::hstring const& value) noexcept { m_manufacturer = internal::TrimmedHStringCopy(value); }

        midi2::MidiDeclaredDeviceIdentity DeclaredDeviceIdentity() const noexcept { return m_declaredDeviceIdentity; }
        void DeclaredDeviceIdentity(_In_ midi2::MidiDeclaredDeviceIdentity const& value) noexcept { m_declaredDeviceIdentity = value; }

        midi2::MidiDeclaredEndpointInfo DeclaredEndpointInfo() const noexcept { return m_declaredEndpointInfo; }
        void DeclaredEndpointInfo(_In_ midi2::MidiDeclaredEndpointInfo const& value) noexcept { m_declaredEndpointInfo = value; }

        midi2::MidiEndpointUserSuppliedInfo UserSuppliedInfo() const noexcept { return m_userSuppliedInfo; }
        void UserSuppliedInfo(_In_ midi2::MidiEndpointUserSuppliedInfo const& value) noexcept { m_userSuppliedInfo = value; }

        collections::IVector<midi2::MidiFunctionBlock> FunctionBlocks() { return m_functionBlocks; }


    private:
        winrt::hstring m_endpointName{};
        winrt::hstring m_description{};
        winrt::hstring m_manufacturer{};

        midi2::MidiDeclaredDeviceIdentity m_declaredDeviceIdentity{};
        midi2::MidiDeclaredEndpointInfo m_declaredEndpointInfo{};
        midi2::MidiEndpointUserSuppliedInfo m_userSuppliedInfo{};

        collections::IVector<midi2::MidiFunctionBlock>
            m_functionBlocks{ winrt::multi_threaded_vector<midi2::MidiFunctionBlock>() };

    };
}
namespace winrt::Microsoft::Devices::Midi2::Endpoints::Virtual::factory_implementation
{
    struct MidiVirtualDeviceCreationConfiguration : MidiVirtualDeviceCreationConfigurationT<MidiVirtualDeviceCreationConfiguration, implementation::MidiVirtualDeviceCreationConfiguration>
    {
    };
}
