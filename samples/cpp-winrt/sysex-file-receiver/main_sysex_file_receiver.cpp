// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: receiving a System Exclusive message and writing it
// to a .syx file.
//
// Under WinMM you would have allocated MIDIHDR buffers, called
// midiInPrepareHeader and midiInAddBuffer for each one, handled MIM_LONGDATA,
// requeued the buffer, and reassembled a message which arrived across several
// buffers.
//
// MidiSystemExclusiveReceiver does the reassembly. It raises BytesReceived when
// a message completes, or earlier if maximumBytesPerEvent bytes accumulate
// first, which is what keeps a very large dump from being buffered in memory
// all at once. When that happens, IsPartial is true on the event args.
//
// >>> SET SysExFilePath BELOW BEFORE RUNNING <<<

#include <iostream>
#include <fstream>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.Diagnostics.h>
#include <winrt/Windows.Devices.Midi2.Utilities.SysExTransfer.h>

using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Devices::Midi2::Diagnostics;
using namespace winrt::Windows::Devices::Midi2::Utilities::SysExTransfer;


// ============================================================================
// SET THESE VALUES
// ============================================================================

// Where to write what we receive. The file is overwritten if it already exists.
const std::wstring SysExFilePath = L"";

// The endpoint to listen to. Leave empty to use a diagnostic loopback endpoint.
// Pair this with the sysex-file-sender sample pointed at the same loopback to
// see the whole path work without any hardware.
const winrt::hstring SourceEndpointId = L"";

// Group 1 is index 0.
const uint8_t SourceGroupIndex = 0;

// Raise an event at least every this many bytes, even mid-message.
const uint32_t MaximumBytesPerEvent = 4096;

// ============================================================================


int main()
{
    winrt::init_apartment();

    if (SysExFilePath.empty())
    {
        std::wcout << L"Set SysExFilePath at the top of this file." << std::endl;
        return 1;
    }

    if (!MidiApi::EnsureServiceAvailable())
    {
        std::wcout << L"Could not demand-start the MIDI service." << std::endl;
        return 1;
    }

    std::ofstream outputFile(SysExFilePath, std::ios::binary | std::ios::trunc);

    if (!outputFile.is_open())
    {
        std::wcout << L"Could not open the output file for writing." << std::endl;
        return 1;
    }

    winrt::hstring sourceEndpointId = SourceEndpointId;

    if (sourceEndpointId.empty())
    {
        sourceEndpointId = MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId();
    }

    MidiSession session = MidiSession::Create(L"SysEx File Receiver Sample");

    MidiEndpointConnection connection = session.CreateEndpointConnection(sourceEndpointId);

    if (connection == nullptr)
    {
        std::wcout << L"Could not create a connection to " << sourceEndpointId.c_str() << std::endl;
        return 1;
    }

    MidiGroup sourceGroup{ SourceGroupIndex };

    MidiSystemExclusiveReceiver receiver(connection, sourceGroup, MaximumBytesPerEvent);

    // This handler runs on a service callback thread, so keep it short. Writing
    // to a file is already more work than you want here for a latency-sensitive
    // application; a real one would hand the bytes to a worker.
    receiver.BytesReceived([&outputFile](
        MidiSystemExclusiveReceiver const& sender,
        MidiSystemExclusiveReceivedEventArgs const& args)
        {
            winrt::Windows::Foundation::Collections::IVectorView<uint8_t> bytes = args.Bytes();

            for (uint8_t const& dataByte : bytes)
            {
                outputFile.put(static_cast<char>(dataByte));
            }

            std::wcout << L"Received " << bytes.Size() << L" byte(s)"
                << (args.IsPartial() ? L" (partial message)" : L" (message complete)")
                << L". Total messages: " << sender.CountMessagesReceived() << std::endl;
        });

    // Open the connection after the receiver is wired up. A device which starts
    // transmitting the moment you open it can otherwise get ahead of you.
    if (!connection.Open())
    {
        std::wcout << L"Could not open the connection." << std::endl;
        return 1;
    }

    if (!receiver.Start())
    {
        std::wcout << L"Could not start the receiver." << std::endl;
        return 1;
    }

    std::wcout << L"Listening on " << sourceEndpointId.c_str() << std::endl;
    std::wcout << L"Writing to " << SysExFilePath.c_str() << std::endl;
    std::wcout << std::endl << L"Press Enter to stop." << std::endl << std::endl;

    std::wcin.get();

    // Stop flushes anything still buffered, which may raise more events before
    // it returns, so the file stays open until after this call.
    receiver.Stop();

    std::wcout << std::endl << receiver.CountBytesReceived() << L" byte(s) in "
        << receiver.CountMessagesReceived() << L" message(s)." << std::endl;

    outputFile.close();

    session.DisconnectEndpointConnection(connection.ConnectionId());

    return 0;
}
