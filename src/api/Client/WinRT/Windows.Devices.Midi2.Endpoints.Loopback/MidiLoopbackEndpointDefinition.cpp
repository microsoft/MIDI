// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiLoopbackEndpointDefinition.h"
#include "MidiLoopbackEndpointDefinition.g.cpp"

namespace winrt::Windows::Devices::Midi2::Endpoints::Loopback::implementation
{
    MidiLoopbackEndpointDefinition::MidiLoopbackEndpointDefinition(hstring const& name, hstring const& uniqueId)
    {
        throw hresult_not_implemented();
    }
    MidiLoopbackEndpointDefinition::MidiLoopbackEndpointDefinition(hstring const& name, hstring const& uniqueId, hstring const& description)
    {
        throw hresult_not_implemented();
    }
    hstring MidiLoopbackEndpointDefinition::Name()
    {
        throw hresult_not_implemented();
    }
    void MidiLoopbackEndpointDefinition::Name(hstring const& value)
    {
        throw hresult_not_implemented();
    }
    hstring MidiLoopbackEndpointDefinition::UniqueId()
    {
        throw hresult_not_implemented();
    }
    void MidiLoopbackEndpointDefinition::UniqueId(hstring const& value)
    {
        throw hresult_not_implemented();
    }
    hstring MidiLoopbackEndpointDefinition::Description()
    {
        throw hresult_not_implemented();
    }
    void MidiLoopbackEndpointDefinition::Description(hstring const& value)
    {
        throw hresult_not_implemented();
    }
}
