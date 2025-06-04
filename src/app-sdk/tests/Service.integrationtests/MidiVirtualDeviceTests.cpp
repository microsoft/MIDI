// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"


void MidiVirtualDeviceTests::TestCreateVirtualDevice()
{
    auto initializer = InitWinRTAndSDK_MTA();
    {

        LOG_OUTPUT(L"TestCreateVirtualDevice **********************************************************************");

        //   VERIFY_IS_TRUE(MidiServicesInitializer::EnsureServiceAvailable());


        winrt::hstring endpointSuppliedName = L"Endpoint From Test";

        winrt::hstring userSuppliedName = L"User Supplied Endpoint From Test";
        winrt::hstring userSuppliedDescription = L"This is the user-supplied description";

        winrt::hstring transportSuppliedName = L"Transport Supplied Endpoint From Test";
        winrt::hstring transportSuppliedDescription = L"This is the transport-supplied description";
        winrt::hstring transportSuppliedManufacturerName = L"Manufacturer Name";


        // endpoint information returned from endpoint discovery
        MidiDeclaredEndpointInfo declaredEndpointInfo{ };
        declaredEndpointInfo.Name = endpointSuppliedName;
        declaredEndpointInfo.ProductInstanceId = L"TAEF_TEST_3263827";   // must be unique
        declaredEndpointInfo.SpecificationVersionMajor = 1; // see latest MIDI 2 UMP spec
        declaredEndpointInfo.SpecificationVersionMinor = 1; // see latest MIDI 2 UMP spec
        declaredEndpointInfo.SupportsMidi10Protocol = true;
        declaredEndpointInfo.SupportsMidi20Protocol = true;
        declaredEndpointInfo.SupportsReceivingJitterReductionTimestamps = false;
        declaredEndpointInfo.SupportsSendingJitterReductionTimestamps = false;
        declaredEndpointInfo.HasStaticFunctionBlocks = true;

        MidiDeclaredDeviceIdentity declaredDeviceIdentity{ };
        // todo: set any device identity values if you want. This is optional

        MidiEndpointUserSuppliedInfo userSuppliedInfo{ };
        userSuppliedInfo.Name = userSuppliedName;           // for names, this will bubble to the top in priority
        userSuppliedInfo.Description = userSuppliedDescription;

        // create the config type to aggregate all this info
        MidiVirtualDeviceCreationConfig config(
            transportSuppliedName,                          // this could be a different "transport-supplied" name value here
            transportSuppliedDescription,                   // transport-supplied description
            transportSuppliedManufacturerName,              // transport-supplied company name
            declaredEndpointInfo,                           // for endpoint discovery
            declaredDeviceIdentity,                         // for endpoint discovery
            userSuppliedInfo
        );

        // Function blocks.The MIDI 2 UMP specification covers the meanings of these values
        MidiFunctionBlock block1{ };
        block1.Number(0);
        block1.Name(L"Test Output");
        block1.IsActive(true);
        block1.UIHint(MidiFunctionBlockUIHint::Sender);
        block1.FirstGroup(MidiGroup((uint8_t)0));
        block1.GroupCount(1);
        block1.Direction(MidiFunctionBlockDirection::Bidirectional);
        block1.RepresentsMidi10Connection(MidiFunctionBlockRepresentsMidi10Connection::Not10);
        block1.MaxSystemExclusive8Streams(0);
        block1.MidiCIMessageVersionFormat(0);

        config.FunctionBlocks().Append(block1);

        MidiFunctionBlock block2{ };
        block2.Number(1);
        block2.Name(L"A Test Function Block");
        block2.IsActive(true);
        block2.UIHint(MidiFunctionBlockUIHint::Sender);
        block2.FirstGroup(MidiGroup((uint8_t)1));
        block2.GroupCount(2);
        block2.Direction(MidiFunctionBlockDirection::Bidirectional);
        block2.RepresentsMidi10Connection(MidiFunctionBlockRepresentsMidi10Connection::Not10);
        block2.MaxSystemExclusive8Streams(0);
        block2.MidiCIMessageVersionFormat(0);

        config.FunctionBlocks().Append(block2);



        // create the session. The name here is just convenience.
        auto session = MidiSession::Create(config.Name());
        VERIFY_IS_NOT_NULL(session);

        // create the virtual device, so we can get the endpoint device id to connect to
        auto virtualDevice = MidiVirtualDeviceManager::CreateVirtualDevice(config);
        VERIFY_IS_NOT_NULL(virtualDevice);
        VERIFY_IS_FALSE(virtualDevice.DeviceEndpointDeviceId().empty());
        LOG_OUTPUT(L"Created virtual device");
        LOG_OUTPUT(virtualDevice.DeviceEndpointDeviceId().c_str());


        // create the endpoint connection to the device-side endpoint
        // to prevent confusion, this endpoint is not enumerated to 
        // apps when using the standard set of enumeration filters
        auto connection = session.CreateEndpointConnection(virtualDevice.DeviceEndpointDeviceId());
        VERIFY_IS_NOT_NULL(connection);

        LOG_OUTPUT(L"Checking device properties");
        auto info = MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(virtualDevice.DeviceEndpointDeviceId());
        VERIFY_IS_NOT_NULL(info);

        LOG_OUTPUT(info.Name().c_str());
        LOG_OUTPUT(info.EndpointDeviceId().c_str());


        // add the virtual device as a message processing plugin so it receives the messages
        connection.AddMessageProcessingPlugin(virtualDevice);

        // wire up the stream configuration request received handler
        //auto streamEventToken = virtualDevice.StreamConfigRequestReceived(
        //    { this, &MidiVirtualDeviceTests::OnStreamConfigurationRequestReceived });

        //// wire up the message received handler on the connection itself
        //auto messageEventToken = connection.MessageReceived(
        //    { this, &MidiVirtualDeviceTests::OnMidiMessageReceived });

        // the client-side endpoint will become visible to other apps once Open() completes
        VERIFY_IS_TRUE(connection.Open());

    }

    // in this example, we put all the allocations in a sub-scope so no need to set them
    // all to nullptr before shutting down WinRT
    ShutdownSDKAndWinRT(initializer);

}


