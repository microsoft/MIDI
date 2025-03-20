// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiUniversalSystemExclusiveMessageBuilder.h"
#include "Messages.MidiUniversalSystemExclusiveMessageBuilder.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::Messages::implementation
{
    _Use_decl_annotations_
    midi2::MidiMessage64 MidiUniversalSystemExclusiveMessageBuilder::BuildIdentityRequest(
        uint64_t timestamp, 
        midi2::MidiUniversalSystemExclusiveChannel const& systemExclusiveChannel)
    {
        UNREFERENCED_PARAMETER(timestamp);
        UNREFERENCED_PARAMETER(systemExclusiveChannel);

        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    collections::IVector<midi2::MidiMessage64> MidiUniversalSystemExclusiveMessageBuilder::BuildIdentityRequestReply(
        uint64_t timestamp, 
        midi2::MidiDeclaredDeviceIdentity const& identity)
    {
        UNREFERENCED_PARAMETER(timestamp);
        UNREFERENCED_PARAMETER(identity);

        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    midi2::MidiMessage64 MidiUniversalSystemExclusiveMessageBuilder::BuildWait(
        uint64_t timestamp)
    {
        UNREFERENCED_PARAMETER(timestamp);

        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    midi2::MidiMessage64 MidiUniversalSystemExclusiveMessageBuilder::BuildCancel(
        uint64_t timestamp)
    {
        UNREFERENCED_PARAMETER(timestamp);

        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    midi2::MidiMessage64 MidiUniversalSystemExclusiveMessageBuilder::BuildNAK(
        uint64_t timestamp)
    {
        UNREFERENCED_PARAMETER(timestamp);

        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    midi2::MidiMessage64 MidiUniversalSystemExclusiveMessageBuilder::BuildACK(
        uint64_t timestamp)
    {
        UNREFERENCED_PARAMETER(timestamp);

        throw hresult_not_implemented();
    }
}
