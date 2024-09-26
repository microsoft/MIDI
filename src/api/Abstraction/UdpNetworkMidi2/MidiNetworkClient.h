// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

struct MidiNetworkUdpClientDefinition
{
    int foo;
    // TODO: Authentication and User Authentication

};



class MidiNetworkClient
{
public:
    HRESULT Initialize(_In_ MidiNetworkUdpClientDefinition& clientDefinition);

    HRESULT Cleanup();

private:
    // this will need to take the incoming packet and then route it to the 
    // correct session based on the client IP/Port sending the message, or 
    // start up a new session if appropriate
    HRESULT ProcessIncomingPacket();

    HRESULT EstablishNewSession();


//    DatagramSocket m_socket{ nullptr };

    // TODO: Map of client connections and their sessions



};