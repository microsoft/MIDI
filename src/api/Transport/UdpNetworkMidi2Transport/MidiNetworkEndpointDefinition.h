// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

enum MidiNetworkEndpointType
{
    UDP_Host,
    UDP_Client,
};


struct MidiNetworkEndpointDefinition
{
    MidiNetworkEndpointType EndpointType;

    std::wstring Name;
    std::wstring ProductInstanceId;

};


