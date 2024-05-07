// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once


// Routing
// 
// Loop (out to in)
// -------------------------------
// Output of Endpoint A is sent to the Input of Virtual Endpoint B (C, D, E, F...)
// (SendMessage goes to Callback)
// Requires that Endpoint A is virtual
// Requires that Endpoints B... are virtual
// 
// Loop (in to out)
// -------------------------------
// Input of Any Endpoint A is set to the Output of Any Endpoint B (C, D, E, F...)
// (Callback goes to SendMidiMessage)
// Requires
// 
// 
// Direct Copy (out to out)
// -------------------------------
// Output of Virtual Endpoint A is sent to the Output of Virtual Endpoint B (C, D, E, F...)
// (SendMessages goes to SendMidiMessage)
//
// Direct Copy (in to in)
// -------------------------------
// Input of Virtual Endpoint A is sent to the input of Virtual Endpoint B (C, D, E, F...)
// (Callback goes to Callback)
//

// What we have control over
// We can hook incoming messages from any endpoint (Virtual or not) through normal callback
// We can send messages to any endpoint (Virtual or not) through normal SendMidiMessage
// We can handle the SendMidiMessage call on any virtual endpoint and send it anywhere



struct MidiRoute
{
    GUID Id;

    // This can do One to Many, but not any sort of merging
    
    // Sources
    std::shared_ptr<IMidiCallback> SourceCallback;                          // for any endpoint type
    HRESULT SendMidiMessage(PVOID message, UINT size, LONGLONG timestamp);  // for virtual

    // Destinations
    std::vector<std::shared_ptr<IMidiBiDi>> DestinationsSendMessage;        // for any endpoint type
    std::vector<std::shared_ptr<IMidiCallback>> DestinationsCallback;       // only for virtual
};




