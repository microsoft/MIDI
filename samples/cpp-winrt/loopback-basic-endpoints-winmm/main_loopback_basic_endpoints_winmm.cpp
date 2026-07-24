// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code

#include <iostream>
#include <Windows.h>
#include <mmeapi.h>


#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.Enumeration.h>
#include <winrt/Windows.Devices.Midi2.Enumeration.Legacy.h>
#include <winrt/Windows.Devices.Midi2.Transports.BasicLoopback.h>
#include <winrt/Windows.Devices.Midi2.Utilities.Messages.h>

using namespace winrt::Windows::Devices::Midi2;                                 // Core SDK
using namespace winrt::Windows::Devices::Midi2::Enumeration;                    
using namespace winrt::Windows::Devices::Midi2::Enumeration::Legacy;            
using namespace winrt::Windows::Devices::Midi2::Transports::BasicLoopback;      // For basic / MIDI 1.0 loopback endpoints
using namespace winrt::Windows::Devices::Midi2::Utilities::Messages;            // For message utilities and strong types


// where you find types like IAsyncOperation, IInspectable, etc.
namespace foundation = winrt::Windows::Foundation;

// we'll use these to keep track of the ids of the created endpoints
winrt::hstring m_endpointId{};
winrt::guid m_associationId = winrt::Windows::Foundation::GuidHelper::CreateNewGuid();

bool CreateLoopbackEndpoints()
{
    std::cout << "Creating loopback endpoints." << std::endl;

    MidiBasicLoopbackEndpointDefinition definition;

    definition.Name(L"Sample App Loopback");
    definition.Description(L"The description is optional, but is displayed to users. This becomes the transport-defined description.");
    definition.UniqueId(L"5150-8675309-OU812");    // if left blank, one will be generated

    MidiBasicLoopbackCreationConfig creationConfig(m_associationId, definition);

    auto response = MidiBasicLoopbackManager::CreateTransientLoopback(creationConfig);

    if (response.Success())
    {
        std::wcout << L"Endpoint created successfully" << std::endl << std::endl;

        std::wcout
            << L"Loopback Endpoint: " << std::endl
            << L" - " << response.CreatedLoopbackEntry().Name().c_str() << std::endl
            << L" - " << response.CreatedLoopbackEntry().EndpointDeviceId().c_str() << std::endl << std::endl;

        m_endpointId = response.CreatedLoopbackEntry().EndpointDeviceId();

        auto midi1ports = MidiLegacyPortDeviceInformation::FindAllForAssociatedEndpoint(m_endpointId);

        if (midi1ports.Size() > 1)
        {
            std::wcout << L"Found " << midi1ports.Size() << L" MIDI 1.0 ports for the loopback endpoint." << std::endl;
            for (auto port : midi1ports)
            {
                std::wcout
                    << L" - " << port.Name().c_str() << std::endl
                    << L" - " << port.Number() << std::endl;

                // NOTE: If the PC this is running on has CoolSoft MIDIMapper or VirtualMIDISynth installed
                // or any other winmm .drv that manipulates the WinMM port numbers, our midisrv-returned
                // port numbers are not going to be correct. Although this smells like a bug, there's not
                // likely to be anything we can do about that, other than advice those who need to use those
                // drivers to switch to legacy API mode. You can find more information, and any updates on 
                // this behavior, here: https://github.com/microsoft/MIDI/issues/1062

                if (port.Flow() == Midi1PortFlow::MidiMessageDestination)
                {
                    MIDIOUTCAPSW caps{ 0 };

                    midiOutGetNumDevs();    // this should not be needed after June CFRs
                    auto result = midiOutGetDevCaps(port.Number(), &caps, sizeof(caps));

                    if (result == MMSYSERR_NOERROR)
                    {
                        std::wcout
                            << L" - caps.szPname: " << caps.szPname << std::endl
                    }
                    else
                    {
                        std::wcout << L" - midiOutGetDevCaps failed with error: " << result << std::endl;
                    }
                }
                else
                {
                    MIDIINCAPSW caps{ 0 };

                    midiInGetNumDevs();    // this should not be needed after June CFRs
                    auto result = midiInGetDevCaps(port.Number(), &caps, sizeof(caps));

                    if (result == MMSYSERR_NOERROR)
                    {
                        std::wcout
                            << L" - caps.szPname: " << caps.szPname << std::endl
                    }
                    else
                    {
                        std::wcout << L" - midiInGetDevCaps failed with error: " << result << std::endl;
                    }
                }

            }
        }
        else
        {
            std::wcout << L"Missing pair of MIDI 1.0 ports." << std::endl;
            return false;
        }

    }
    else
    {
        std::wcout << L"Failed to create loopback endpoints." << std::endl; 
        std::wcout << L"This can happen if you control-C or crash out of the sample before the loopbacks are removed." << std::endl;
        std::wcout << L"If that's the case, restart MidiSrv, or change the unique Ids above." << std::endl;
        std::wcout << L"Error Code (please see the enum for description) " << static_cast<uint32_t>(response.ErrorCode()) << std::endl;
        std::wcout << L"Error Message: " << response.ErrorMessage().c_str() << std::endl;
    }


    // Success here is a boolean for success/fail
    return response.Success();
}


int main()
{
    // initialize the thread before calling the bootstrapper or any WinRT code. You may also
    // be able to leave this out and call RoInitialize() or CoInitializeEx() before creating
    // the initializer.
    //winrt::init_apartment(winrt::apartment_type::single_threaded);

    // MTA by default
    winrt::init_apartment();

    if (!MidiApi::EnsureServiceAvailable())
    {
        std::wcout << L"Could not demand-start the MIDI service" << std::endl;
        return 1;
    }


    // create the MIDI session, giving us access to Windows MIDI Services. An app may open 
    // more than one session. If so, the session name should be meaningful to the user, like
    // the name of a browser tab, or a project.

    std::wcout << std::endl << L"Creating session..." << std::endl;

    auto session = MidiSession::Create(L"Loopback Sample Session");

    if (CreateLoopbackEndpoints())
    {
        



        auto endpoint = session.CreateEndpointConnection(m_endpointId);
        std::wcout << L"Connected to endpoint: " << std::wstring{ m_endpointId.c_str() } << std::endl;


        // Wire up an event handler to receive the message. There is a single event handler type, but the
        // MidiMessageReceivedEventArgs class provides the different ways to access the data
        // Your event handlers should return quickly as they are called synchronously.

        auto MessageReceivedHandler = [&](IMidiMessageReceivedEventSource const& /*sender*/, MidiMessageReceivedEventArgs const& args)
            {
                // there are several ways to get the message data from the arguments. If you want to use
                // strongly-typed UMP classes, then you may start with the GetUmp() method. The GetXXX calls 
                // are all generating something within the function, so you want to call them once and then
                // keep the result around in a variable if you plan to refer to it multiple times. In 
                // contrast, the FillXXX functions will update values in provided (pre-allocated) types
                // passed in to the functions.
                auto ump = args.GetMessagePacket();

                std::wcout << std::endl;
                std::wcout << L"Received UMP" << std::endl;
                std::wcout << L"- Current Timestamp: " << std::dec << MidiClock::Now() << std::endl;
                std::wcout << L"- UMP Timestamp:     " << std::dec << ump.Timestamp() << std::endl;
                std::wcout << L"- UMP Msg Type:      0x" << std::hex << static_cast<uint32_t>(ump.MessageType()) << std::endl;
                std::wcout << L"- UMP Packet Type:   0x" << std::hex << static_cast<uint32_t>(ump.PacketType()) << std::endl;
                std::wcout << L"- Message:           " << std::wstring{ MidiMessageHelper::GetMessageDisplayNameFromFirstWord(args.PeekFirstWord()).c_str() } << std::endl;

                // if you wish to cast the IMidiUmp to a specific Ump Type, you can do so using .as<T> WinRT extension

                if (ump.PacketType() == MidiPacketType::UniversalMidiPacket32)
                {
                    // we'll use the Ump32 type here. This is a runtimeclass that the strongly-typed 
                    // 32-bit messages derive from. There are also MidiUmp64/96/128 classes.
                    auto ump32 = ump.as<MidiMessage32>();

                    std::wcout << L"- Word 0:            0x" << std::hex << ump32.Word0() << std::endl;
                }

                std::wcout << std::endl;

            };

        // the returned token is used to deregister the event later.
        auto eventRevokeToken = endpoint.MessageReceived(MessageReceivedHandler);

        std::wcout << std::endl << L"Opening endpoint connection" << std::endl;

        // once you have wired up all your event handlers, added any filters/listeners, etc.
        // You can open the connection.
        endpoint.Open();


        std::wcout << std::endl << L"Creating MIDI 1.0 Channel Voice 32-bit UMP..." << std::endl;

        auto ump32 = MidiMessageBuilder::BuildMidi1ChannelVoiceMessage(
            MidiClock::Now(),                           // use current timestamp
            MidiGroup(5),                               // group 6
            Midi1ChannelVoiceMessageStatus::NoteOn,     // 9
            MidiChannel(3),                             // channel 4
            120,                                        // note 120 - hex 0x78
            100);                                       // velocity 100 hex 0x64

        // here you would set other values in the UMP word(s)

        std::wcout << L"Sending single UMP " << std::hex << ump32.Word0() << " ..." << std::endl;
        std::wcout << std::endl << L" ** Wait for the sent UMP to arrive, and then press enter to cleanup. **" << std::endl;

        auto ump = ump32.as<IMidiUniversalPacket>();
        auto sendResult = endpoint.SendSingleMessagePacket(ump);          // could also use the SendWords methods, etc.

        if (MidiEndpointConnection::SendMessageFailed(sendResult))
        {
            std::wcout << std::endl << L"Send message failed..." << std::endl;
        }

        system("pause");

        std::wcout << std::endl << L"Deregistering event handler..." << std::endl;

        if (eventRevokeToken)
        {
            // deregister the event by passing in the revoke token
            endpoint.MessageReceived(eventRevokeToken);
        }

        std::wcout << L"Disconnecting UMP Endpoint Connection..." << std::endl;


        session.DisconnectEndpointConnection(endpoint.ConnectionId());

        // close the session, detaching all Windows MIDI Services resources and closing all connections
        // You can also disconnect individual Endpoint Connections when you are done with them, as we did above
        session.Close();


        // remove the loopback endpoints
        // If you don't do this, they will stay active, and the next attempt
        // to create them will fail because the unique Ids are already in use

        MidiBasicLoopbackRemovalConfig removalConfig(m_associationId);

        auto removalResponse = MidiBasicLoopbackManager::RemoveTransientLoopback(removalConfig);

        if (removalResponse.Success())
        {
            std::wcout << L"Loopback endpoints removed." << std::endl;
        }
        else
        {
            std::wcout << L"There was a problem removing the endpoints." << std::endl;
            std::wcout << L"Error Code (see enum for description): " << static_cast<uint32_t>(removalResponse.ErrorCode()) << std::endl;
            std::wcout << L"Error Message: " << std::wstring{ removalResponse.ErrorMessage().c_str() } << std::endl;
        }
    }

    std::wcout << L"Cleaning up WinRT / COM apartment..." << std::endl;
    winrt::uninit_apartment();

}
