// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Loopback.MidiLoopbackEndpointEntry.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Loopback::implementation
{
    struct MidiLoopbackEndpointEntry : MidiLoopbackEndpointEntryT<MidiLoopbackEndpointEntry>
    {
        MidiLoopbackEndpointEntry() = default;

        winrt::hstring EndpointDeviceId() const noexcept { return m_endpointDeviceId; }
        winrt::hstring Name() const noexcept { return m_name; }
        winrt::hstring Description() const noexcept { return m_description; }

        bool InternalInitialize(
            _In_ winrt::hstring const& endpointDeviceId, 
            _In_ winrt::hstring const& name, 
            _In_ winrt::hstring const& description) noexcept
        {
            m_endpointDeviceId = internal::NormalizeEndpointInterfaceIdHStringCopy(endpointDeviceId);
            m_name = internal::TrimmedHStringCopy(name);
            m_description = internal::TrimmedHStringCopy(description);

            return true;
        }


    private:
        winrt::hstring m_endpointDeviceId{ };
        winrt::hstring m_name{ };
        winrt::hstring m_description{ };

    };
}
