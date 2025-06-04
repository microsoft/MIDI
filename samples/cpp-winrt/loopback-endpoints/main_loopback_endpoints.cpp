﻿// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code

#include <iostream>

#include <winrt/Windows.Foundation.h>

#include <winrt/Microsoft.Windows.Devices.Midi2.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.Endpoints.Loopback.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.Messages.h>

using namespace winrt::Microsoft::Windows::Devices::Midi2;                          // Core SDK
using namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Loopback;     // For loopback endpoints
using namespace winrt::Microsoft::Windows::Devices::Midi2::Messages;                // For message utilities and strong types


// where you find types like IAsyncOperation, IInspectable, etc.
namespace foundation = winrt::Windows::Foundation;

// This include file has a wrapper for the bootstrapper code. You are welcome to
// use the .hpp as-is, or work the functionality into your code in whatever way
// makes the most sense for your application.
// 
// The namespace defined in the .hpp is not a WinRT namespace, just a regular C++ namespace
#include "winmidi/init/Microsoft.Windows.Devices.Midi2.Initialization.hpp"
namespace init = Microsoft::Windows::Devices::Midi2::Initialization;

// we'll use these to keep track of the ids of the created endpoints
winrt::hstring m_endpointAId{};
winrt::hstring m_endpointBId{};
winrt::guid m_associationId = winrt::Windows::Foundation::GuidHelper::CreateNewGuid();

bool CreateLoopbackEndpoints()
{
    std::cout << "Creating loopback endpoints." << std::endl;

    MidiLoopbackEndpointDefinition definitionA;
    MidiLoopbackEndpointDefinition definitionB;

    definitionA.Name = L"Sample App Loopback A";
    definitionA.Description = L"The first description is optional, but is displayed to users. This becomes the transport-defined description.";
    definitionA.UniqueId = L"8675309-OU812-5150";

    definitionB.Name = L"Sample App Loopback B";
    definitionB.Description = L"The second description is optional, but is displayed to users. This becomes the transport-defined description.";
    definitionB.UniqueId = L"3263827-OU812-5150"; // can be the same as the first one, but doesn't need to be.

    MidiLoopbackEndpointCreationConfig creationConfig(m_associationId, definitionA, definitionB);

    auto response = MidiLoopbackEndpointManager::CreateTransientLoopbackEndpoints(creationConfig);

    if (response.Success)
    {
        std::wcout << L"Endpoints created successfully" << std::endl << std::endl;

        std::cout
            << "Loopback Endpoint A: " << std::endl 
            << " - " << winrt::to_string(definitionA.Name) << std::endl
            << " - " << winrt::to_string(response.EndpointDeviceIdA) << std::endl << std::endl;

        std::cout 
            << "Loopback Endpoint B: "  << std::endl
            << " - " << winrt::to_string(definitionB.Name) << std::endl
            << " - " << winrt::to_string(response.EndpointDeviceIdB) << std::endl << std::endl;

        m_endpointAId = response.EndpointDeviceIdA;
        m_endpointBId = response.EndpointDeviceIdB;
    }
    else
    {
        std::cout << "Failed to create loopback endpoints." << std::endl; 
        std::cout << "This can happen if you control-C or crash out of the sample before the loopbacks are removed." << std::endl;
        std::cout << "If that's the case, restart MidiSrv, or change the unique Ids above." << std::endl;
    }

    // Success here is a boolean for success/fail
    return response.Success;
}


int main()
{
    // initialize the thread before calling the bootstrapper or any WinRT code. You may also
    // be able to leave this out and call RoInitialize() or CoInitializeEx() before creating
    // the initializer.
    //winrt::init_apartment(winrt::apartment_type::single_threaded);

    // MTA by default
    winrt::init_apartment();

    // this is the initializer in the bootstrapper hpp file
    std::shared_ptr<init::MidiDesktopAppSdkInitializer> initializer = std::make_shared<init::MidiDesktopAppSdkInitializer>();

    // you can, of course, use guard macros instead of this check
    if (initializer != nullptr)
    {
        if (!initializer->InitializeSdkRuntime())
        {
            std::cout << "Could not initialize SDK runtime" << std::endl;
            std::wcout << "Install the latest SDK runtime installer from " << initializer->LatestMidiAppSdkDownloadUrl << std::endl;
            return 1;
        }

        if (!initializer->EnsureServiceAvailable())
        {
            std::cout << "Could not demand-start the MIDI service" << std::endl;
            return 1;
        }
    }
    else
    {
        // This shouldn't happen, but good to guard
        std::cout << "Unable to create initializer" << std::endl;
        return 1;
    }


    // create the MIDI session, giving us access to Windows MIDI Services. An app may open 
    // more than one session. If so, the session name should be meaningful to the user, like
    // the name of a browser tab, or a project.

    std::cout << std::endl << "Creating session..." << std::endl;

    auto session = MidiSession::Create(L"Loopback Sample Session");

    if (CreateLoopbackEndpoints())
    {
        auto sendEndpoint = session.CreateEndpointConnection(m_endpointAId);
        std::cout << "Connected to sending endpoint: " << winrt::to_string(m_endpointAId) << std::endl;

        auto receiveEndpoint = session.CreateEndpointConnection(m_endpointBId);
        std::cout << "Connected to receiving endpoint: " << winrt::to_string(m_endpointBId) << std::endl;

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

                std::cout << std::endl;
                std::cout << "Received UMP" << std::endl;
                std::cout << "- Current Timestamp: " << std::dec << MidiClock::Now() << std::endl;
                std::cout << "- UMP Timestamp:     " << std::dec << ump.Timestamp() << std::endl;
                std::cout << "- UMP Msg Type:      0x" << std::hex << static_cast<uint32_t>(ump.MessageType()) << std::endl;
                std::cout << "- UMP Packet Type:   0x" << std::hex << static_cast<uint32_t>(ump.PacketType()) << std::endl;
                std::cout << "- Message:           " << winrt::to_string(MidiMessageHelper::GetMessageDisplayNameFromFirstWord(args.PeekFirstWord())) << std::endl;

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
            MidiClock::Now(),                           // use current timestamp
            MidiGroup(5),                               // group 6
            Midi1ChannelVoiceMessageStatus::NoteOn,     // 9
            MidiChannel(3),                             // channel 4
            120,                                        // note 120 - hex 0x78
            100);                                       // velocity 100 hex 0x64

        // here you would set other values in the UMP word(s)

        std::cout << "Sending single UMP " << std::hex << ump32.Word0() << " ..." << std::endl;
        std::cout << std::endl << " ** Wait for the sent UMP to arrive, and then press enter to cleanup. **" << std::endl;

        auto ump = ump32.as<IMidiUniversalPacket>();
        auto sendResult = sendEndpoint.SendSingleMessagePacket(ump);          // could also use the SendWords methods, etc.

        if (MidiEndpointConnection::SendMessageFailed(sendResult))
        {
            std::cout << std::endl << "Send message failed..." << std::endl;
        }

        system("pause");

        std::cout << std::endl << "Deregistering event handler..." << std::endl;

        if (eventRevokeToken)
        {
            // deregister the event by passing in the revoke token
            receiveEndpoint.MessageReceived(eventRevokeToken);
        }

        std::cout << "Disconnecting UMP Endpoint Connection..." << std::endl;


        session.DisconnectEndpointConnection(sendEndpoint.ConnectionId());
        session.DisconnectEndpointConnection(receiveEndpoint.ConnectionId());

        // close the session, detaching all Windows MIDI Services resources and closing all connections
        // You can also disconnect individual Endpoint Connections when you are done with them, as we did above
        session.Close();


        // remove the loopback endpoints
        // If you don't do this, they will stay active, and the next attempt
        // to create them will fail because the unique Ids are already in use

        MidiLoopbackEndpointRemovalConfig removalConfig(m_associationId);

        if (MidiLoopbackEndpointManager::RemoveTransientLoopbackEndpoints(removalConfig))
        {
            std::cout << "Loopback endpoints removed." << std::endl;
        }
        else
        {
            std::cout << "There was a problem removing the endpoints. You may want to restart the service." << std::endl;
        }
    }

    // ensure we release all the WinRT and COM objects before uninitializing COM
    // otherwise, you can crash when closing down the apartment. You could just put them all in 
    // a sub-scope which closes before the uninit_apartment call, or you can set them to nullptr.
    session = nullptr;

    // clean up the SDK WinRT redirection
    std::cout << "Cleaning up SDK..." << std::endl;
    if (initializer != nullptr)
    {
        initializer->ShutdownSdkRuntime();
        initializer.reset();
    }

    std::cout << "Cleaning up WinRT / COM apartment..." << std::endl;
    winrt::uninit_apartment();

}
