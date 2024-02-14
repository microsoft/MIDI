// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code

#include "pch.h"
#include <iostream>

#include <winrt/Windows.Devices.Midi2.h>    // normally you'd put this in pch.h, but here for visibility

using namespace winrt::Windows::Devices::Midi2;        // API

// standard WinRT enumeration support. This is how you find attached devices.
using namespace winrt::Windows::Devices::Enumeration;

// where you find types like IAsyncOperation, IInspectable, etc.
namespace foundation = winrt::Windows::Foundation;


// we'll use these to keep track of the ids of the created endpoints
winrt::hstring m_endpointAId{};
winrt::hstring m_endpointBId{};
winrt::guid m_associationId = winrt::Windows::Foundation::GuidHelper::CreateNewGuid();

bool CreateLoopbackEndpoints()
{
    std::cout << "Creating loopback endpoints." << std::endl;

    MidiServiceLoopbackEndpointDefinition definitionA;
    MidiServiceLoopbackEndpointDefinition definitionB;

    definitionA.Name(L"My App Loopback A");
    definitionA.Description(L"The first description is optional, but is displayed to users. This becomes the transport-defined description.");
    definitionA.UniqueId(L"8675309-OU812-5150");

    definitionB.Name(L"My App Loopback B");
    definitionB.Description(L"The second description is optional, but is displayed to users. This becomes the transport-defined description.");
    definitionB.UniqueId(L"3263827-OU812-5150"); // can be the same as the first one, but doesn't need to be.

    auto response = MidiService::CreateTemporaryLoopbackEndpoints(
        m_associationId,
        definitionA,
        definitionB
    );


    if (response.Success())
    {
        std::wcout << L"Endpoint A: " << response.EndpointDeviceIdA().c_str() << std::endl;
        std::wcout << L"Endpoint B: " << response.EndpointDeviceIdB().c_str() << std::endl;

        m_endpointAId = response.EndpointDeviceIdA();
        m_endpointBId = response.EndpointDeviceIdB();
    }
    else
    {
        std::cout << "Failed to create loopback endpoints." << std::endl;
    }

    return response.Success();
}


int main()
{
    winrt::init_apartment();

    // create the MIDI session, giving us access to Windows MIDI Services. An app may open 
    // more than one session. If so, the session name should be meaningful to the user, like
    // the name of a browser tab, or a project.

    std::cout << std::endl << "Creating session..." << std::endl;

    auto session = MidiSession::CreateSession(L"Sample Session");



    if (CreateLoopbackEndpoints())
    {
        auto sendEndpoint = session.CreateEndpointConnection(m_endpointAId);
        std::cout << "Connected to sending endpoint: " << winrt::to_string(m_endpointAId) << std::endl;

        auto receiveEndpoint = session.CreateEndpointConnection(m_endpointBId);
        std::cout << "Connected to receiving endpoint: " << winrt::to_string(m_endpointBId) << std::endl;

        // Wire up an event handler to receive the message. There is a single event handler type, but the
        // MidiMessageReceivedEventArgs class provides the different ways to access the data
        // Your event handlers should return quickly as they are called synchronously.

        auto MessageReceivedHandler = [&](foundation::IInspectable const& sender, MidiMessageReceivedEventArgs const& args)
            {
                // there are several ways to get the message data from the arguments. If you want to use
                // strongly-typed UMP classes, then you may start with the GetUmp() method. The GetXXX calls 
                // are all generating something within the function, so you want to call them once and then
                // keep the result around in a variable if you plan to refer to it multiple times. In 
                // contrast, the FillXXX functions will update values in provided (pre-allocated) types
                // passed in to the functions.
                auto ump = args.GetMessagePacket();

                std::cout << std::endl;
                std::cout << "Received UMP" << std::endl;
                std::cout << "- Current Timestamp: " << std::dec << MidiClock::Now() << std::endl;
                std::cout << "- UMP Timestamp:     " << std::dec << ump.Timestamp() << std::endl;
                std::cout << "- UMP Msg Type:      0x" << std::hex << (uint32_t)ump.MessageType() << std::endl;
                std::cout << "- UMP Packet Type:   0x" << std::hex << (uint32_t)ump.PacketType() << std::endl;
                std::cout << "- Message:           " << winrt::to_string(MidiMessageUtility::GetMessageFriendlyNameFromFirstWord(args.PeekFirstWord())) << std::endl;

                // if you wish to cast the IMidiUmp to a specific Ump Type, you can do so using .as<T> WinRT extension

                if (ump.PacketType() == MidiPacketType::UniversalMidiPacket32)
                {
                    // we'll use the Ump32 type here. This is a runtimeclass that the strongly-typed 
                    // 32-bit messages derive from. There are also MidiUmp64/96/128 classes.
                    auto ump32 = ump.as<MidiMessage32>();

                    std::cout << "- Word 0:            0x" << std::hex << ump32.Word0() << std::endl;
                }

                std::cout << std::endl;

            };

        // the returned token is used to deregister the event later.
        auto eventRevokeToken = receiveEndpoint.MessageReceived(MessageReceivedHandler);


        std::cout << std::endl << "Opening endpoint connection" << std::endl;

        // once you have wired up all your event handlers, added any filters/listeners, etc.
        // You can open the connection. Doing this will query the cache for the in-protocol 
        // endpoint information and function blocks. If not there, it will send out the requests
        // which will come back asynchronously with responses.
        sendEndpoint.Open();
        receiveEndpoint.Open();


        std::cout << std::endl << "Creating MIDI 1.0 Channel Voice 32-bit UMP..." << std::endl;

        auto ump32 = MidiMessageBuilder::BuildMidi1ChannelVoiceMessage(
            MidiClock::Now(), // use current timestamp
            5,      // group 5
            Midi1ChannelVoiceMessageStatus::NoteOn,     // 9
            3,      // channel 3
            120,    // note 120 - hex 0x78
            100);   // velocity 100 hex 0x64

        // here you would set other values in the UMP word(s)

        std::cout << "Sending single UMP..." << std::endl;

        auto ump = ump32.as<IMidiUniversalPacket>();
        auto sendResult = sendEndpoint.SendMessagePacket(ump);          // could also use the SendWords methods, etc.

        std::cout << std::endl << " ** Wait for the sent UMP to arrive, and then press enter to cleanup. **" << std::endl;

        system("pause");

        std::cout << std::endl << "Deregistering event handler..." << std::endl;

        // deregister the event by passing in the revoke token
        receiveEndpoint.MessageReceived(eventRevokeToken);

        std::cout << "Disconnecting UMP Endpoint Connection..." << std::endl;


        session.DisconnectEndpointConnection(sendEndpoint.ConnectionId());
        session.DisconnectEndpointConnection(receiveEndpoint.ConnectionId());

        // close the session, detaching all Windows MIDI Services resources and closing all connections
        // You can also disconnect individual Endpoint Connections when you are done with them, as we did above
        session.Close();


        // remove the loopback endpoints

        if (MidiService::RemoveTemporaryLoopbackEndpoints(m_associationId))
        {
            std::cout << "Endpoints removed." << std::endl;
        }
        else
        {
            std::cout << "There was a problem removing the endpoints." << std::endl;
        }
    }
}
