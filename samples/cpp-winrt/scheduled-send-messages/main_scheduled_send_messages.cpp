// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: scheduling messages to be sent in the future.
//
// There is no WinMM equivalent for this. Under WinMM, if you wanted a note to
// start 500 milliseconds from now, you had to keep a timer in your application
// and call midiOutShortMsg when it fired, which meant your timing was only as
// good as your thread scheduling on a busy machine.
//
// Here, you give the service a timestamp along with the message and the service
// sends it at that time. Jitter is handled below your application.
//
// The timestamp is not a duration and it is not relative to when you opened the
// connection. It is an absolute value on the same clock MidiClock::Now() reads,
// counted from system boot in timestamp ticks. To schedule, take Now() and
// offset it, which is what the MidiClock::OffsetTimestampBy... functions are for.

#include <iostream>
#include <thread>
#include <chrono>

#include <winrt/Windows.Foundation.h>

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.Diagnostics.h>
#include <winrt/Windows.Devices.Midi2.Utilities.Messages.h>

using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Devices::Midi2::Diagnostics;
using namespace winrt::Windows::Devices::Midi2::Utilities::Messages;


// The endpoint to send to. Leave empty to use a diagnostic loopback endpoint.
const winrt::hstring DestinationEndpointId = L"";

// Group 1 is index 0.
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

    MidiSession session = MidiSession::Create(L"Scheduled Send Sample");

    MidiEndpointConnection connection = session.CreateEndpointConnection(destinationEndpointId);

    if (connection == nullptr || !connection.Open())
    {
        std::wcout << L"Could not open a connection to " << destinationEndpointId.c_str() << std::endl;
        return 1;
    }

    MidiGroup group{ DestinationGroupIndex };

    // Read the clock once and schedule everything relative to that single value.
    // Calling Now() again for each message would let the gap between the calls
    // creep into your timing.
    uint64_t startTimestamp = MidiClock::Now();

    std::wcout << L"Scheduling four notes, 500 milliseconds apart." << std::endl;
    std::wcout << L"Current timestamp is " << startTimestamp << std::endl << std::endl;

    const uint8_t noteNumbers[] = { 60, 64, 67, 72 };

    for (int i = 0; i < 4; i++)
    {
        uint32_t offsetMilliseconds = static_cast<uint32_t>(i) * 500;

        // Offset the timestamp we captured, rather than reading the clock again.
        uint64_t noteOnTimestamp =
            MidiClock::OffsetTimestampByMilliseconds(startTimestamp, offsetMilliseconds);

        // The note off is scheduled 400 milliseconds after its note on, so the
        // notes do not run into each other.
        uint64_t noteOffTimestamp =
            MidiClock::OffsetTimestampByMilliseconds(startTimestamp, offsetMilliseconds + 400);

        MidiMessage32 noteOn = MidiMessageBuilder::BuildMidi1ChannelVoiceMessage(
            noteOnTimestamp,
            group,
            Midi1ChannelVoiceMessageStatus::NoteOn,
            MidiChannel(static_cast<uint8_t>(0)),
            noteNumbers[i],
            100);

        MidiMessage32 noteOff = MidiMessageBuilder::BuildMidi1ChannelVoiceMessage(
            noteOffTimestamp,
            group,
            Midi1ChannelVoiceMessageStatus::NoteOff,
            MidiChannel(static_cast<uint8_t>(0)),
            noteNumbers[i],
            0);

        connection.SendSingleMessagePacket(noteOn);
        connection.SendSingleMessagePacket(noteOff);

        std::wcout << L"Note " << (int)noteNumbers[i]
            << L" scheduled for +" << offsetMilliseconds << L" ms"
            << L" (timestamp " << noteOnTimestamp << L")" << std::endl;
    }

    // Everything above returned immediately. The messages are held by the service
    // and sent at their timestamps, so we have to stay alive long enough for the
    // last one to go out.
    std::wcout << std::endl << L"All messages handed to the service. Waiting for the last one." << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    std::wcout << L"Done." << std::endl;

    session.DisconnectEndpointConnection(connection.ConnectionId());

    return 0;
}
