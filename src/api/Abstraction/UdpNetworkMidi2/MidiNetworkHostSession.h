// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// This represents Windows MIDI Services acting as a host
// to enable remote clients to connect to it.
//
// Each valid host session ends up as a UMP Endpoint in Windows
// The name of the connection is obtained from the remote client
//
// The lifetime of the UMP endpoint is based on the lifetime of the
// session
//


class MidiNetworkHostSession
{
public:
    HRESULT Initialize(/* TODO: Linked IMidiCallback and IMidiBiDi */);

    HRESULT ReceiveMidiMessagesFromNetwork(_In_ uint16_t const sequenceNumber, _In_ std::vector<uint32_t> const& words);
    HRESULT SendMidiMessageToNetwork();

    HRESULT Cleanup();


private:

    // client information

    // crypto nonce for this session for this client

    // https://learn.microsoft.com/en-us/uwp/api/windows.security.credentials.passwordvault?view=winrt-26100



};
