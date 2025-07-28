// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code

#include <iostream>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <winrt/Microsoft.Windows.Devices.Midi2.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.Endpoints.Virtual.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.Messages.h>

using namespace winrt::Microsoft::Windows::Devices::Midi2;                          // Core SDK
using namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Virtual;      // For creating the app-to-app midi device
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
winrt::guid m_associationId = winrt::Windows::Foundation::GuidHelper::CreateNewGuid();

MidiVirtualDevice m_virtualDevice{ nullptr };

bool CreateVirtualDevice()
{
    //uint8_t functionBlockCount{ 4 };

    std::cout << "Creating virtual device." << std::endl;

    MidiDeclaredEndpointInfo endpointInfo;
    endpointInfo.HasStaticFunctionBlocks = false;   // this allows us to change them after the fact. The count cannot change, however, per spec.
    endpointInfo.Name = L"MyApp";
    endpointInfo.ProductInstanceId = L"3263827-8675309-5150";
    endpointInfo.SupportsMidi10Protocol = true;
    endpointInfo.SupportsMidi20Protocol = true;
    endpointInfo.SupportsReceivingJitterReductionTimestamps = false;
    endpointInfo.SupportsSendingJitterReductionTimestamps = false;
    endpointInfo.SpecificationVersionMajor = 1;
    endpointInfo.SpecificationVersionMinor = 1;

    // there are constructor overloads which also take the device identity and user-supplied info
    // Examples may be found in the C# app-to-app MIDI sample
    MidiVirtualDeviceCreationConfig creationConfig(
        endpointInfo.Name,
        L"Virtual Device created from the C++ sample code",
        L"Samples R Us",
        endpointInfo);

    // NOTE: If you want the function block names to also work as MIDI 1 port names
    // then you need to keep the names short. We normally add the device name plus
    // the FB name with a space in between, and then possibly a differentiator if 
    // needed. In total, including null terminator, that needs to be 32 characters 
    // or fewer.
    MidiFunctionBlock block0;
    block0.Number(0);
    block0.IsActive(true);
    block0.Name(L"BiDi Function Block");
    block0.FirstGroup(MidiGroup(static_cast<uint8_t>(0)));
    block0.GroupCount(3); 
    block0.Direction(MidiFunctionBlockDirection::Bidirectional); // both an input and an output
    creationConfig.FunctionBlocks().Append(block0);

    MidiFunctionBlock block1;
    block1.Number(1);
    block1.IsActive(false);
    block1.Name(L"Inactive Block 1");
    block1.FirstGroup(MidiGroup(static_cast<uint8_t>(3)));
    block1.GroupCount(3); 
    block1.Direction(MidiFunctionBlockDirection::BlockInput);   // a midi message destination
    creationConfig.FunctionBlocks().Append(block1);

    MidiFunctionBlock block2;
    block2.Number(2);
    block2.IsActive(true);
    block2.Name(L"This is MIDI In Function Block 2 with a long WinMM-unfriendly name");
    block2.FirstGroup(MidiGroup(static_cast<uint8_t>(5)));  // this will overlap with block1, which is allowed per-spec
    block2.GroupCount(1);
    block2.Direction(MidiFunctionBlockDirection::BlockOutput);  // a midi message source
    creationConfig.FunctionBlocks().Append(block2);

    MidiFunctionBlock block3;
    block3.Number(3);
    block3.IsActive(true);
    block3.Name(L"Block 3 2 1 Contact");
    block3.FirstGroup(MidiGroup(static_cast<uint8_t>(11)));
    block3.GroupCount(5);                                       // this gets us to index 15, which is max index
    block3.Direction(MidiFunctionBlockDirection::BlockInput);   // a midi message destination
    creationConfig.FunctionBlocks().Append(block3);

    // creates the device using the endpoint info provided above
    m_virtualDevice = MidiVirtualDeviceManager::CreateVirtualDevice(creationConfig);
    if (m_virtualDevice == nullptr)
    {
        std::cout << "Virtual device creation failed." << std::endl;

        return false;
    }

    std::wcout << L"Virtual Device created successfully" << std::endl << std::endl;

    std::cout
        << "Device-side Connection Info: " << std::endl 
        << " " << winrt::to_string(m_virtualDevice.DeviceEndpointDeviceId()) << std::endl << std::endl;

    return true;
}

// modify this as needed to test any function block updating
void UpdateFunctionBlocks()
{
    // updating function blocks after initial creation requires using the device's UpdateFunctionBlock method
    // Per MIDI 2 spec, the number of function blocks cannot change after discovery has completed.
    // Additionally, if the function blocks are declared as static, they cannot change at all after
    // initial discovery has completed

    MidiFunctionBlock block;
    block.Number(2);            // this must be a number we're already using
    block.IsActive(true);
    block.Name(L"Updated FB2");
    block.FirstGroup(MidiGroup((uint8_t)0));
    block.GroupCount(3);
    block.Direction(MidiFunctionBlockDirection::BlockOutput);  // a midi message source

    if (m_virtualDevice.UpdateFunctionBlock(block))
    {
        std::cout << "Function Block updated" << std::endl;
    }
    else
    {
        std::cout << "Function Block failed to update" << std::endl;
    }
}

void UpdateEndpointName()
{
    if (m_virtualDevice.UpdateEndpointName(L"New Endpoint Name"))
    {
        std::cout << "Name updated" << std::endl;
    }
    else
    {
        std::cout << "Name failed to update" << std::endl;
    }
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
    if (initializer == nullptr)
    {
        // This shouldn't happen, but good to guard
        std::cout << "Unable to create initializer" << std::endl;
        return 1;
    }

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


    // create the MIDI session, giving us access to Windows MIDI Services. An app may open 
    // more than one session. If so, the session name should be meaningful to the user, like
    // the name of a browser tab, or a project.

    std::cout << std::endl << "Creating session..." << std::endl;

    auto session = MidiSession::Create(L"Virtual Device C++ Session");

    if (session == nullptr)
    {
        std::cout << "Unable to create session" << std::endl;
        return 1;
    }

    if (CreateVirtualDevice())
    {
        auto deviceEndpoint = session.CreateEndpointConnection(m_virtualDevice.DeviceEndpointDeviceId());

        if (deviceEndpoint == nullptr)
        {
            std::cout << "Unable to create device connection" << std::endl;
            return 1;
        }

        std::cout << "Connected to device endpoint: " << winrt::to_string(m_virtualDevice.DeviceEndpointDeviceId()) << std::endl;

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
                std::cout << "- Data               " << winrt::to_string(args.GetMessagePacket().as<foundation::IStringable>().ToString()) << std::endl;
                std::cout << "- Message:           " << winrt::to_string(MidiMessageHelper::GetMessageDisplayNameFromFirstWord(args.PeekFirstWord())) << std::endl;
                std::cout << std::endl;
            };

        // the returned token is used to deregister the event later.
        auto eventRevokeToken = deviceEndpoint.MessageReceived(MessageReceivedHandler);

        // this is what associates the virtual device client object with this connection
        deviceEndpoint.AddMessageProcessingPlugin(m_virtualDevice);

        std::cout << std::endl << "Opening device endpoint connection. This will create the client-visible endpoint" << std::endl;
        deviceEndpoint.Open();

        std::cout << "Press any key to send an endpoint name update" << std::endl;
        system("pause");
        UpdateEndpointName();
        std::cout << std::endl;

        std::cout << "Press any key to send a function block update" << std::endl;
        system("pause");
        UpdateFunctionBlocks();
        std::cout << std::endl;

        std::cout << "Press any key to cleanup and exit" << std::endl;
        system("pause");


        std::cout << std::endl << "Deregistering event handler..." << std::endl;

        if (eventRevokeToken)
        {
            // deregister the event by passing in the revoke token
            deviceEndpoint.MessageReceived(eventRevokeToken);
        }

        std::cout << "Disconnecting UMP Endpoint Connection..." << std::endl;
        session.DisconnectEndpointConnection(deviceEndpoint.ConnectionId());

        // close the session, detaching all Windows MIDI Services resources and closing all connections
        // You can also disconnect individual Endpoint Connections when you are done with them, as we did above
        session.Close();
    }


    // clean up the SDK WinRT redirection
    if (initializer != nullptr)
    {
        initializer->ShutdownSdkRuntime();
        initializer.reset();
    }
}
