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
        winrt::hstring ImageFileName() const noexcept { return m_imageFileName; }

        bool InternalInitialize(
            _In_ winrt::hstring const& endpointDeviceId, 
            _In_ winrt::hstring const& name, 
            _In_ winrt::hstring const& description,
            _In_ winrt::hstring const& imageFileName
            ) noexcept
        {
            m_endpointDeviceId = internal::NormalizeEndpointInterfaceIdHStringCopy(endpointDeviceId);
            m_name = internal::TrimmedHStringCopy(name);
            m_description = internal::TrimmedHStringCopy(description);

            // the service reports what the configuration file said, and that file is editable by
            // any standard user, so it is cleaned again on the way in
            m_imageFileName = winrt::hstring{ internal::CleanImageFileName(imageFileName.c_str()) };

            return true;
        }


    private:
        winrt::hstring m_endpointDeviceId{ };
        winrt::hstring m_name{ };
        winrt::hstring m_description{ };
        winrt::hstring m_imageFileName{ };

    };
}
