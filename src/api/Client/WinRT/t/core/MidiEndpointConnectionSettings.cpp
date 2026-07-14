// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiEndpointConnectionSettings.h"
#include "MidiEndpointConnectionSettings.g.cpp"

namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
        MidiEndpointConnectionSettings::MidiEndpointConnectionSettings(bool waitForEndpointReceiptOnSend) noexcept
    {
        m_waitForEndpointReceiptOnSend = waitForEndpointReceiptOnSend;
    }

    _Use_decl_annotations_
        MidiEndpointConnectionSettings::MidiEndpointConnectionSettings(bool waitForEndpointReceiptOnSend, bool const autoReconnect) noexcept
            : MidiEndpointConnectionSettings(waitForEndpointReceiptOnSend)
    {
        m_autoReconnect = autoReconnect;
    }



    MidiEndpointConnectionSettings::~MidiEndpointConnectionSettings() noexcept
    {
        // nothing to do here
    }

}
