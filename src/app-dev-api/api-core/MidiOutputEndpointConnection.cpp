// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiOutputEndpointConnection.h"
#include "MidiOutputEndpointConnection.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{

    hstring MidiOutputEndpointConnection::GetDeviceSelectorForOutput()
    {
        // TODO
        return L"";
    }

    void MidiOutputEndpointConnection::Start()
    {

    }


}
