// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "CapabilityInquiry.MidiDeviceInfo.g.h"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    struct MidiDeviceInfo : MidiDeviceInfoT<MidiDeviceInfo>
    {
        MidiDeviceInfo() = default;

        MidiDeviceInfo(_In_ midi2enum::MidiDeclaredDeviceIdentity const& identity) noexcept
            : m_identity(identity) {}

        MidiDeviceInfo(
            _In_ midi2enum::MidiDeclaredDeviceIdentity const& identity,
            _In_ winrt::hstring const& manufacturer,
            _In_ winrt::hstring const& family,
            _In_ winrt::hstring const& model) noexcept
            : m_identity(identity)
            , m_manufacturer(manufacturer)
            , m_family(family)
            , m_model(model) {}

        midi2enum::MidiDeclaredDeviceIdentity Identity() const noexcept { return m_identity; }
        void Identity(_In_ midi2enum::MidiDeclaredDeviceIdentity const& value) noexcept { m_identity = value; }

        winrt::hstring Manufacturer() const noexcept { return m_manufacturer; }
        void Manufacturer(_In_ winrt::hstring const& value) noexcept { m_manufacturer = value; }

        winrt::hstring Family() const noexcept { return m_family; }
        void Family(_In_ winrt::hstring const& value) noexcept { m_family = value; }

        winrt::hstring Model() const noexcept { return m_model; }
        void Model(_In_ winrt::hstring const& value) noexcept { m_model = value; }

        winrt::hstring Version() const noexcept { return m_version; }
        void Version(_In_ winrt::hstring const& value) noexcept { m_version = value; }

        json::JsonObject GetJson() noexcept;
        static ci::MidiDeviceInfo FromJson(_In_ json::JsonObject const& jsonObject) noexcept;

        winrt::hstring ToString();

    private:
        midi2enum::MidiDeclaredDeviceIdentity m_identity{ nullptr };

        winrt::hstring m_manufacturer{};
        winrt::hstring m_family{};
        winrt::hstring m_model{};
        winrt::hstring m_version{};
    };
}

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::factory_implementation
{
    struct MidiDeviceInfo : MidiDeviceInfoT<MidiDeviceInfo, implementation::MidiDeviceInfo>
    {
    };
}
