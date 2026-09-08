// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: scheduling messages using the COM extensions.
//
// The scheduled-send-messages sample does the same thing through the WinRT
// connection API. This one does it through the COM extensions, which is the
// path most likely to be used by an application which already has its own UMP
// buffers and cannot afford an allocation per message.
//
// The important point is that scheduling is not a property of the WinRT
// projection. The timestamp is the first argument to SendMidiMessagesRaw, and
// it means exactly what it means everywhere else in this API: an absolute value
// on the MidiClock::Now() timeline, not a delay.
//
// Pass MidiClock::TimestampConstantSendImmediately(), or simply zero, when you
// want a message to go out as soon as possible.

#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

#include <winrt/Windows.Foundation.h>

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.Diagnostics.h>

#include <Unknwn.h>
#include "WindowsMidiServicesAppSdkComExtensions.h"

using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Devices::Midi2::Diagnostics;


// The endpoint to send to. Leave empty to use a diagnostic loopback endpoint.
const winrt::hstring DestinationEndpointId = L"";

// Group 1 is index 0. The group index lives in the second nibble of word 0.
const uint8_t DestinationGroupIndex = 0;


int main()
{
    winrt::init_apartment();

    if (!MidiApi::EnsureServiceAvailable())
    {
        std::wcout << L"Could not demand-start the MIDI service." << std::endl;
        return 1;
    }

    winrt::hstring destinationEndpointId = DestinationEndpointId;

    if (destinationEndpointId.empty())
    {
        destinationEndpointId = MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId();
    }

    MidiSession session = MidiSession::Create(L"Scheduled COM Extensions Sample");

    MidiEndpointConnection connection = session.CreateEndpointConnection(destinationEndpointId);

    if (connection == nullptr || !connection.Open())
    {
        std::wcout << L"Could not open a connection to " << destinationEndpointId.c_str() << std::endl;
        return 1;
    }

    // The COM extensions are reached by querying the connection for the raw
    // interface. This is the same object, not a second connection.
    winrt::com_ptr<IMidiEndpointConnectionRaw> rawConnection =
        connection.as<IMidiEndpointConnectionRaw>();

    if (rawConnection == nullptr)
    {
        std::wcout << L"Could not get the IMidiEndpointConnectionRaw interface." << std::endl;
        return 1;
    }

    // Never assume how many words fit in one transmission. Ask, per connection,
    // and do not cache the answer across releases.
    uint32_t maximumWords = rawConnection->GetSupportedMaxMidiWordsPerTransmission();

    std::wcout << L"This connection accepts up to " << maximumWords
        << L" words per transmission." << std::endl << std::endl;

    uint64_t startTimestamp = MidiClock::Now();

    const uint8_t noteNumbers[] = { 60, 64, 67, 72 };

    for (int i = 0; i < 4; i++)
    {
        uint32_t offsetMilliseconds = static_cast<uint32_t>(i) * 500;

        uint64_t noteOnTimestamp =
            MidiClock::OffsetTimestampByMilliseconds(startTimestamp, offsetMilliseconds);

        uint64_t noteOffTimestamp =
            MidiClock::OffsetTimestampByMilliseconds(startTimestamp, offsetMilliseconds + 400);

        // A MIDI 1.0 channel voice message is a single 32-bit UMP:
        //   nibble 0 : message type 2, MIDI 1.0 channel voice
        //   nibble 1 : group index
        //   nibble 2 : status, 9 for note on and 8 for note off
        //   nibble 3 : channel
        //   byte 2   : note number
        //   byte 3   : velocity
        uint32_t noteOnWord =
            0x20000000
            | (static_cast<uint32_t>(DestinationGroupIndex) << 24)
            | (0x9u << 20)
            | (0x0u << 16)
            | (static_cast<uint32_t>(noteNumbers[i]) << 8)
            | 100u;

        uint32_t noteOffWord =
            0x20000000
            | (static_cast<uint32_t>(DestinationGroupIndex) << 24)
            | (0x8u << 20)
            | (0x0u << 16)
            | (static_cast<uint32_t>(noteNumbers[i]) << 8)
            | 0u;

        // Each call carries one timestamp, so the note on and the note off are
        // sent separately here. Messages which share a timestamp can go in a
        // single call as one contiguous block of words.
        HRESULT sendResult = rawConnection->SendMidiMessagesRaw(noteOnTimestamp, 1, &noteOnWord);

        if (FAILED(sendResult))
        {
            std::wcout << L"Send failed with HRESULT 0x" << std::hex << sendResult << std::dec << std::endl;
            break;
        }

        sendResult = rawConnection->SendMidiMessagesRaw(noteOffTimestamp, 1, &noteOffWord);

        if (FAILED(sendResult))
        {
            std::wcout << L"Send failed with HRESULT 0x" << std::hex << sendResult << std::dec << std::endl;
            break;
        }

        std::wcout << L"Note " << (int)noteNumbers[i]
            << L" scheduled for +" << offsetMilliseconds << L" ms" << std::endl;
    }

    std::wcout << std::endl << L"All messages handed to the service. Waiting for the last one." << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    std::wcout << L"Done." << std::endl;

    session.DisconnectEndpointConnection(connection.ConnectionId());

    return 0;
}
