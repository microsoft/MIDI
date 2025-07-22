// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiEndpointConnectionBasicSettings.h"
#include "MidiEndpointConnectionBasicSettings.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    MidiEndpointConnectionBasicSettings::MidiEndpointConnectionBasicSettings(bool waitForEndpointReceiptOnSend) noexcept
    {
        m_waitForEndpointReceiptOnSend = waitForEndpointReceiptOnSend;
    }

    _Use_decl_annotations_
    MidiEndpointConnectionBasicSettings::MidiEndpointConnectionBasicSettings(bool waitForEndpointReceiptOnSend, bool const autoReconnect) noexcept
            : MidiEndpointConnectionBasicSettings(waitForEndpointReceiptOnSend)
    {
        m_autoReconnect = autoReconnect;
    }



    MidiEndpointConnectionBasicSettings::~MidiEndpointConnectionBasicSettings() noexcept
    {
        // nothing to do here
    }

}
