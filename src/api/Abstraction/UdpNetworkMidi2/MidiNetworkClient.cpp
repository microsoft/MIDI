// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"


_Use_decl_annotations_
HRESULT 
MidiNetworkClient::Initialize(MidiNetworkUdpClientDefinition& clientDefinition)
{
    UNREFERENCED_PARAMETER(clientDefinition);

    return S_OK;
}

HRESULT 
MidiNetworkClient::Cleanup()
{


    return S_OK;
}

HRESULT
MidiNetworkClient::ProcessIncomingPacket()
{

    return S_OK;
}

HRESULT 
MidiNetworkClient::EstablishNewSession()
{

    return S_OK;
}
