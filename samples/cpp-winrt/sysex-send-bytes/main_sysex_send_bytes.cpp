// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: converting MIDI 1.0 bytes you already have in memory
// into UMP words, and sending them.
//
// This is the path to use when your application already holds MIDI 1.0
// bytestream data, which is the usual situation when porting from WinMM. You
// hand the bytes to MidiMessageConverter and send the words it gives back.
//
// The converter state object is the important part. It remembers that you are
// in the middle of a System Exclusive message between calls. Without it, a
// continuation buffer is parsed as though it were the start of a new message.
// If you are sending one complete message at a time you can use the overload
// without a state object, but keeping one costs nothing and is correct in both
// cases.

#include <iostream>
#include <iomanip>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.Diagnostics.h>
#include <winrt/Windows.Devices.Midi2.Utilities.Messages.h>

using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Devices::Midi2::Diagnostics;
using namespace winrt::Windows::Devices::Midi2::Utilities::Messages;

namespace collections = winrt::Windows::Foundation::Collections;


// The endpoint to send to. Leave empty to use a diagnostic loopback endpoint so
// the sample is safe to run unchanged.
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

    MidiSession session = MidiSession::Create(L"SysEx Send Bytes Sample");

    MidiEndpointConnection connection = session.CreateEndpointConnection(destinationEndpointId);

    if (connection == nullptr || !connection.Open())
    {
        std::wcout << L"Could not open a connection to " << destinationEndpointId.c_str() << std::endl;
        return 1;
    }

    MidiGroup destinationGroup{ DestinationGroupIndex };

    // A Universal Non-Real Time Identity Request. This asks a device to say what
    // it is, and is one of the few System Exclusive messages which is safe to
    // send to hardware you do not know anything about.
    //
    //   F0    start of System Exclusive
    //   7E    Universal Non-Real Time
    //   7F    device id, 7F meaning "all devices"
    //   06    General Information sub-id
    //   01    Identity Request
    //   F7    end of System Exclusive
    std::vector<uint8_t> midi1Bytes{ 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 };

    // The converter takes any IIterable<UInt8>, so a WinRT vector over your bytes
    // is all that is needed here.
    collections::IVector<uint8_t> byteVector =
        winrt::single_threaded_vector<uint8_t>(std::vector<uint8_t>(midi1Bytes));

    MidiBytestreamToUmpMessageConverterState converterState;

    // allowRunningStatus is false here because our buffer carries a complete
    // message with its own status byte. Set it to true when you are feeding a
    // stream which relies on running status, as MIDI 1.0 hardware often does.
    collections::IVector<uint32_t> words =
        MidiMessageConverter::ConvertMidi1CompleteMessageBytesToUmpWords(
            destinationGroup,
            byteVector,
            false,
            converterState);

    std::wcout << midi1Bytes.size() << L" MIDI 1.0 byte(s) became "
        << words.Size() << L" UMP word(s):" << std::endl;

    for (uint32_t const& word : words)
    {
        std::wcout << L"  0x" << std::hex << std::uppercase
            << std::setw(8) << std::setfill(L'0') << word << std::dec << std::endl;
    }

    // All of these words belong to one logical transfer, so they go out together
    // with a single timestamp. Use the constant rather than a literal zero, so the
    // intent is obvious and you are not depending on the value staying what it is.
    MidiSendMessageResults result =
        connection.SendMultipleMessagesWordList(MidiClock::TimestampConstantSendImmediately(), words);

    if (MidiEndpointConnection::SendMessageSucceeded(result))
    {
        std::wcout << std::endl << L"Sent." << std::endl;
    }
    else
    {
        std::wcout << std::endl << L"Send failed." << std::endl;
    }

    session.DisconnectEndpointConnection(connection.ConnectionId());

    return 0;
}
