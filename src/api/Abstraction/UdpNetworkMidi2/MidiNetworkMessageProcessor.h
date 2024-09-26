// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once



class MidiNetworkMessageProcessor
{
public:
    HRESULT Initialize(/* TODO: Callback functions as needed */);

    // TODO: Will this always send one at a time, or do we queue them?
    HRESULT SendUmpWithForwardErrorCorrection();


    HRESULT Cleanup();

private:

    // should this be the type that contains the retransmit history, FEC, etc?

    HRESULT ProcessIncomingPacket();

//    DatagramSocket m_socket{ nullptr };

    // TODO: Map of client connections and their sessions



};