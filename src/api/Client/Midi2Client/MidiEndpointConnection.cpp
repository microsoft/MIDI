// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiEndpointConnection.h"
#include "MidiEndpointConnection.g.cpp"

#include "midi_service_interface.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    hstring MidiEndpointConnection::GetDeviceSelector()
    {
        // TODO: get this selector from Julia. It will return all UMP Endpoints the system recognizes
        // Additional selectors will be made available through the SDK, as they are just string queries
        // Right now, this is the selector from Gary's MidiKsEnum code

        return L"System.Devices.InterfaceClassGuid:=\"{6994AD04-93EF-11D0-A3CC-00A0C9223196}\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True";
    }



}
