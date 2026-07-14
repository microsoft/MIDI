// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "Transports.Virtual.MidiVirtualDeviceCreationConfig.g.h"


namespace winrt::Windows::Devices::Midi2::Transports::Virtual::implementation
{
    struct MidiVirtualDeviceCreationConfig : MidiVirtualDeviceCreationConfigT<MidiVirtualDeviceCreationConfig>
    {
        MidiVirtualDeviceCreationConfig() = default;

        MidiVirtualDeviceCreationConfig(
            _In_ winrt::hstring const& name,
            _In_ winrt::hstring const& description,
            _In_ winrt::hstring const& manufacturer,
            _In_ midi2enum::MidiDeclaredEndpointInfo const& declaredEndpointInfo
        );

        MidiVirtualDeviceCreationConfig(
            _In_ winrt::hstring const& name,
            _In_ winrt::hstring const& description,
            _In_ winrt::hstring const& manufacturer,
            _In_ midi2enum::MidiDeclaredEndpointInfo const& declaredEndpointInfo,
            _In_ midi2enum::MidiDeclaredDeviceIdentity const& declaredDeviceIdentity
            );

        MidiVirtualDeviceCreationConfig(
            _In_ winrt::hstring const& name,
            _In_ winrt::hstring const& description,
            _In_ winrt::hstring const& manufacturer,
            _In_ midi2enum::MidiDeclaredEndpointInfo const& declaredEndpointInfo,
            _In_ midi2enum::MidiDeclaredDeviceIdentity const& declaredDeviceIdentity,
            _In_ midi2enum::MidiEndpointUserSuppliedInfo const& userSuppliedInfo
        );

        bool IsFromCurrentConfigFile() const noexcept { return false; }
        winrt::guid TransportId() const noexcept { return virt::MidiVirtualDeviceManager::TransportId(); }
        json::JsonObject ConfigJson() const noexcept;


        winrt::hstring Name() const noexcept { return m_endpointName; }
        void Name(_In_ winrt::hstring const& value) noexcept { m_endpointName = internal::TrimmedHStringCopy(value); }

        winrt::hstring Description() const noexcept { return m_description; }
        void Description(_In_ winrt::hstring const& value) noexcept { m_description = internal::TrimmedHStringCopy(value); }

        winrt::hstring Manufacturer() const noexcept { return m_manufacturer; }
        void Manufacturer(_In_ winrt::hstring const& value) noexcept { m_manufacturer = internal::TrimmedHStringCopy(value); }

        bool CreateOnlyUmpEndpoints() const noexcept { return m_umpOnly; }
        void CreateOnlyUmpEndpoints(_In_ bool const value) { m_umpOnly = value; }


        midi2enum::MidiDeclaredDeviceIdentity DeclaredDeviceIdentity() const noexcept { return m_declaredDeviceIdentity; }
        void DeclaredDeviceIdentity(_In_ midi2enum::MidiDeclaredDeviceIdentity const& value) noexcept { m_declaredDeviceIdentity = value; }

        midi2enum::MidiDeclaredEndpointInfo DeclaredEndpointInfo() const noexcept { return m_declaredEndpointInfo; }
        void DeclaredEndpointInfo(_In_ midi2enum::MidiDeclaredEndpointInfo const& value) noexcept { m_declaredEndpointInfo = value; }

        midi2enum::MidiEndpointUserSuppliedInfo UserSuppliedInfo() const noexcept { return m_userSuppliedInfo; }
        void UserSuppliedInfo(_In_ midi2enum::MidiEndpointUserSuppliedInfo const& value) noexcept { m_userSuppliedInfo = value; }

        collections::IVector<midi2enum::MidiFunctionBlock> FunctionBlocks() { return m_functionBlocks; }

        winrt::guid AssociationId() const noexcept { return m_associationId; }

    private:
        winrt::hstring m_endpointName{};
        winrt::hstring m_description{};
        winrt::hstring m_manufacturer{};

        bool m_umpOnly{ false };

        winrt::guid m_associationId{ foundation::GuidHelper::CreateNewGuid() };

        midi2enum::MidiDeclaredDeviceIdentity m_declaredDeviceIdentity{};
        midi2enum::MidiDeclaredEndpointInfo m_declaredEndpointInfo{};
        midi2enum::MidiEndpointUserSuppliedInfo m_userSuppliedInfo{};

        collections::IVector<midi2enum::MidiFunctionBlock>
            m_functionBlocks{ winrt::multi_threaded_vector<midi2enum::MidiFunctionBlock>() };

    };
}
namespace winrt::Windows::Devices::Midi2::Transports::Virtual::factory_implementation
{
    struct MidiVirtualDeviceCreationConfig : MidiVirtualDeviceCreationConfigT<MidiVirtualDeviceCreationConfig, implementation::MidiVirtualDeviceCreationConfig>
    {
    };
}
