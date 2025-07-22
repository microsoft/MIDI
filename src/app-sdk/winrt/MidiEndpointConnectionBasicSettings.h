// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "MidiEndpointConnectionBasicSettings.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{
    struct MidiEndpointConnectionBasicSettings : MidiEndpointConnectionBasicSettingsT<MidiEndpointConnectionBasicSettings>
    {
        MidiEndpointConnectionBasicSettings() = default;
        ~MidiEndpointConnectionBasicSettings() noexcept;


        MidiEndpointConnectionBasicSettings(_In_ bool const waitForEndpointReceiptOnSend) noexcept;
        MidiEndpointConnectionBasicSettings(_In_ bool const waitForEndpointReceiptOnSend, _In_ bool const autoReconnect) noexcept;

        bool WaitForEndpointReceiptOnSend() const noexcept { return m_waitForEndpointReceiptOnSend; }
        bool AutoReconnect() const noexcept { return m_autoReconnect; }

        winrt::hstring SettingsJson() const noexcept { return L"{}"; }

    private:
        bool m_waitForEndpointReceiptOnSend{ false };
        bool m_autoReconnect{ false };
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::factory_implementation
{
    struct MidiEndpointConnectionBasicSettings : MidiEndpointConnectionBasicSettingsT<MidiEndpointConnectionBasicSettings, implementation::MidiEndpointConnectionBasicSettings>
    {
    };
}
