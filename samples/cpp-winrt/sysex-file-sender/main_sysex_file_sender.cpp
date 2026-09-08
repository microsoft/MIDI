// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: sending a .syx file to a device.
//
// Under WinMM you would have read the file into a buffer, filled in a MIDIHDR,
// called midiOutPrepareHeader, midiOutLongMsg, and then midiOutUnprepareHeader,
// and you would have been responsible for not freeing the buffer too early.
//
// Here, MidiSystemExclusiveSender reads the MIDI 1.0 bytestream from a stream,
// converts it into SysEx 7 UMP messages, and paces the transfer for you.
//
// >>> YOU MUST SET THE TWO CONSTANTS BELOW BEFORE THIS SAMPLE WILL DO ANYTHING <<<
//
// We deliberately do not generate SysEx data or pick a device for you. Sending
// arbitrary System Exclusive data to a random device is a good way to change
// settings you did not mean to change, or to put a device into a mode you did
// not expect. Point this at hardware you own and understand.

#include <iostream>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.Diagnostics.h>
#include <winrt/Windows.Devices.Midi2.Utilities.SysExTransfer.h>
#include <winrt/Windows.Devices.Midi2.Utilities.Messages.h>

using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Devices::Midi2::Diagnostics;
using namespace winrt::Windows::Devices::Midi2::Utilities::SysExTransfer;
using namespace winrt::Windows::Devices::Midi2::Utilities::Messages;

namespace storage = winrt::Windows::Storage;
namespace streams = winrt::Windows::Storage::Streams;


// ============================================================================
// SET THESE TWO VALUES
// ============================================================================

// Full path to the .syx file you want to send. This file should contain raw
// MIDI 1.0 bytestream SysEx data, including the 0xF0 start and 0xF7 end bytes.
const winrt::hstring SysExFilePath = L"";

// The endpoint to send to. Leave this empty to use a diagnostic loopback
// endpoint, so that running the sample unchanged cannot disturb your hardware.
// Replace it with the endpoint device id of the device you actually want to
// send to. The get-vid-pid or static-enum-endpoints samples will show you the ids.
const winrt::hstring DestinationEndpointId = L"";

// Group 1 is index 0. Set this to the group your device expects.
const uint8_t DestinationGroupIndex = 0;

// ============================================================================


int main()
{
    winrt::init_apartment();

    if (SysExFilePath.empty())
    {
        std::wcout << L"Set SysExFilePath at the top of this file to the .syx file you want to send." << std::endl;
        return 1;
    }

    if (!MidiApi::EnsureServiceAvailable())
    {
        std::wcout << L"Could not demand-start the MIDI service." << std::endl;
        return 1;
    }

    // Open the file and get an input stream over it. The sender reads the
    // bytestream from here, so a large dump does not have to be held in memory.
    storage::StorageFile file{ nullptr };

    try
    {
        file = storage::StorageFile::GetFileFromPathAsync(SysExFilePath).get();
    }
    catch (winrt::hresult_error const& ex)
    {
        std::wcout << L"Could not open the file: " << ex.message().c_str() << std::endl;
        return 1;
    }

    streams::IRandomAccessStreamWithContentType fileStream = file.OpenReadAsync().get();
    streams::IInputStream inputStream = fileStream.GetInputStreamAt(0);

    // Resolve the destination here rather than in a global. Anything which calls
    // into WinRT must run after the apartment is initialized, so it does not
    // belong in a namespace-scope initializer.
    winrt::hstring destinationEndpointId = DestinationEndpointId;

    if (destinationEndpointId.empty())
    {
        destinationEndpointId = MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId();
    }

    MidiSession session = MidiSession::Create(L"SysEx File Sender Sample");

    MidiEndpointConnection connection = session.CreateEndpointConnection(destinationEndpointId);

    if (connection == nullptr)
    {
        std::wcout << L"Could not create a connection to " << destinationEndpointId.c_str() << std::endl;
        return 1;
    }

    if (!connection.Open())
    {
        std::wcout << L"Could not open the connection." << std::endl;
        return 1;
    }

    MidiGroup destinationGroup{ DestinationGroupIndex };

    // The converter state holds the partial-message and running status state for
    // this transfer. Each transfer needs its own, and it must live for the whole
    // transfer, otherwise a continuation buffer is parsed as a new message.
    MidiBytestreamToUmpMessageConverterState converterState;

    std::wcout << L"Sending " << SysExFilePath.c_str() << std::endl;
    std::wcout << L"     to " << destinationEndpointId.c_str() << std::endl;
    std::wcout << L"  group " << (int)destinationGroup.DisplayValue() << std::endl << std::endl;

    // The last two numeric arguments pace the transfer: send 10 messages, then
    // wait 5 milliseconds, and repeat. Many older devices need this time to keep
    // up with a large dump. Pass zero for either value to send with no pacing.
    MidiSystemExclusiveSender::SendBinarySysEx7ByteDataAsync(
        connection,
        destinationGroup,
        inputStream,
        10,
        5,
        converterState).get();

    std::wcout << L"Transfer complete." << std::endl;

    session.DisconnectEndpointConnection(connection.ConnectionId());

    return 0;
}
