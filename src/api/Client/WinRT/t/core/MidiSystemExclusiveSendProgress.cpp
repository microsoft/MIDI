// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiSystemExclusiveSendProgress.h"
#include "Utilities.SysExTransfer.MidiSystemExclusiveSendProgress.g.cpp"

namespace winrt::Windows::Devices::Midi2::Utilities::SysExTransfer::implementation
{
    _Use_decl_annotations_
    void MidiSystemExclusiveSendProgress::InternalInitialize(
        uint64_t const countBytesRead,
        uint64_t const countMessagesSent)
    {
        m_countBytesRead = countBytesRead;
        m_countMessagesSent = countMessagesSent;
    }


}
