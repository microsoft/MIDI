// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Problem: MidiCapabilityInquirySession sent each MIDI-CI message one UMP at a time. With
// multi-client endpoints, two apps sending MIDI-CI to the same endpoint and group at the same
// time (for example two apps sending Discovery) had their System Exclusive 7 packets mixed
// together, so the receiver saw corrupted messages.
//
// Fix: the session sends each whole message in a single call through the connection's raw
// send, which the service delivers without splitting. Messages too long for one call are
// still split, between packets.
//
// Rollback: restores sending one UMP at a time.

class Feature_Servicing_MIDI2CapabilityInquiryWholeMessageSend
{
public:
    static bool IsEnabled()
    {
        return true;
    }
};
