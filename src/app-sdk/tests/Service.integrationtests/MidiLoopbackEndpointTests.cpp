// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"


void MidiLoopbackEndpointTests::TestCreateLoopbackEndpoints()
{
    auto initializer = InitWinRTAndSDK_MTA();

    auto cleanup = wil::scope_exit([&]
        {
            ShutdownSDKAndWinRT(initializer);
        });


    {

        winrt::hstring endpointAId{};
        winrt::hstring endpointBId{};

        MidiLoopbackEndpointDefinition definitionA;
        MidiLoopbackEndpointDefinition definitionB;

        auto uniqueId = L"ID" + winrt::to_hstring(MidiClock::Now());

        // A-side of the loopback
        definitionA.Name = L"Test Loopback A";
        definitionA.Description = L"The first description is optional, but is displayed to users. This becomes the transport-defined description.";
        definitionA.UniqueId = uniqueId;

        // B-side of the loopback
        definitionB.Name = L"Test Loopback B";
        definitionB.Description = L"The second description is optional, but is displayed to users. This becomes the transport-defined description.";
        definitionB.UniqueId = uniqueId; // can be the same as the first one, but doesn't need to be.

        winrt::guid associationId = foundation::GuidHelper::CreateNewGuid();

        LOG_OUTPUT(L"Creating loopback endpoint creation config");

        MidiLoopbackEndpointCreationConfig creationConfig(associationId, definitionA, definitionB);

        LOG_OUTPUT(L"Creating loopbacks");

        auto response = MidiLoopbackEndpointManager::CreateTransientLoopbackEndpoints(creationConfig);

        if (response.Success)
        {
            LOG_OUTPUT(L"Endpoints created successfully");

            std::cout
                << "Loopback Endpoint A: " << std::endl
                << " - " << winrt::to_string(definitionA.Name) << std::endl
                << " - " << winrt::to_string(response.EndpointDeviceIdA) << std::endl << std::endl;

            std::cout
                << "Loopback Endpoint B: " << std::endl
                << " - " << winrt::to_string(definitionB.Name) << std::endl
                << " - " << winrt::to_string(response.EndpointDeviceIdB) << std::endl << std::endl;

            endpointAId = response.EndpointDeviceIdA;
            endpointBId = response.EndpointDeviceIdB;


            // TODO: enumerate endpoints and verify that the endpoints are present


            // Give a hoot. Don't pollute.
            MidiLoopbackEndpointRemovalConfig removalConfig(response.AssociationId);
            VERIFY_IS_TRUE(MidiLoopbackEndpointManager::RemoveTransientLoopbackEndpoints(removalConfig));

        }
        else
        {
            LOG_OUTPUT(L"Return result indicates failure");

            // failed to create the loopback pair. It may be that the unique
            // Ids are already in use.

            VERIFY_FAIL();
        }

    }
}

