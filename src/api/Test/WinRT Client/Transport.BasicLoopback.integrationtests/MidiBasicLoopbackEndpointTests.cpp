// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

#include <mmsystem.h>
#include <vector>
#include <atomic>

#pragma comment(lib, "winmm.lib")


void MidiBasicLoopbackTests::TestReopenLegacyWinMMPorts()
{
    // Regression test for issue 1070: after opening and closing the WinMM
    // ports created for a basic loopback, we must be able to re-open them.

    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    // Start the legacy port device watcher *before* creating the loopback so
    // we receive the Added events for the newly created ports.
    LOG_OUTPUT(L"Creating and starting the legacy port device watcher");

    auto watcher = MidiLegacyPortDeviceWatcher::Create();
    VERIFY_IS_NOT_NULL(watcher);

    wil::critical_section portListLock;
    wil::unique_event_nothrow bothPortsAvailable;
    bothPortsAvailable.create();

    winrt::hstring endpointId{};

    uint32_t sourcePortNumber{ 0 };
    uint32_t destinationPortNumber{ 0 };
    bool haveSourcePort{ false };
    bool haveDestinationPort{ false };

    std::vector<MidiLegacyPortDeviceInformation> addedDevices;

    auto addedToken = watcher.Added([&](auto const& /*source*/, MidiLegacyPortDeviceInformationAddedEventArgs const& args)
        {
            auto lock = portListLock.lock();

            auto port = args.AddedDevice();

            addedDevices.push_back(port);
            
            if (port.AssociatedEndpointDeviceId() == endpointId)
            {
                std::wcout << L"Added device assoc id: " << port.AssociatedEndpointDeviceId().c_str() << std::endl;

                if (port.Flow() == Midi1PortFlow::MidiMessageSource && !haveSourcePort)
                {
                    sourcePortNumber = port.Number();
                    haveSourcePort = true;
                }
                else if (port.Flow() == Midi1PortFlow::MidiMessageDestination && !haveDestinationPort)
                {
                    destinationPortNumber = port.Number();
                    haveDestinationPort = true;
                }

                if (haveSourcePort && haveDestinationPort)
                {
                    bothPortsAvailable.SetEvent();
                }
            }

        });

    // Create the basic loopback

    auto uniqueId = winrt::to_hstring(foundation::GuidHelper::CreateNewGuid());

    MidiBasicLoopbackEndpointDefinition definition(
        L"Test Basic Loopback GH1070",
        uniqueId,
        L"Regression test loopback for issue GH1070."
    );

    winrt::guid associationId = foundation::GuidHelper::CreateNewGuid();

    LOG_OUTPUT(L"Creating loopback endpoint creation config");
    MidiBasicLoopbackCreationConfig creationConfig(associationId, definition);

    LOG_OUTPUT(L"Creating loopback");
    auto response = MidiBasicLoopbackManager::CreateTransientLoopback(creationConfig);
    VERIFY_IS_NOT_NULL(response);
    VERIFY_IS_TRUE(response.Success());
    VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry());
    VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointDeviceId().empty());

    auto removalAssociationId = response.CreatedLoopbackEntry().AssociationId();

    // Ensure we always remove the loopback we created.
    auto cleanupLoopback = wil::scope_exit([&]
        {
            LOG_OUTPUT(L"Removing loopback");
            MidiBasicLoopbackRemovalConfig removalConfig(removalAssociationId);
            auto removalResponse = MidiBasicLoopbackManager::RemoveTransientLoopback(removalConfig);

            VERIFY_IS_NOT_NULL(removalResponse);
            VERIFY_IS_TRUE(removalResponse.Success());
        });

    {
        auto lock = portListLock.lock();
        endpointId = response.CreatedLoopbackEntry().EndpointDeviceId();
    }


    watcher.Start();

    // Resolve the WinMM port numbers for the loopback.
    //
    // These are resolved by querying the ports directly rather than by waiting on the
    // watcher's Added events. The watcher does not reliably raise Added for every port
    // when several are created at once, and the port number itself is assigned
    // asynchronously, so a direct query is the authoritative source for this test.
    LOG_OUTPUT(L"Resolving the source and destination legacy WinMM port numbers");

    VERIFY_IS_TRUE(TryResolveWinMMPortNumber(endpointId, Midi1PortFlow::MidiMessageSource, sourcePortNumber));
    VERIFY_IS_TRUE(TryResolveWinMMPortNumber(endpointId, Midi1PortFlow::MidiMessageDestination, destinationPortNumber));

    // stop watching before we do anything else
    if (watcher != nullptr)
    {
        if (addedToken) watcher.Added(addedToken);

        watcher.Stop();
    }

    LOG_OUTPUT(WEX::Common::String().Format(
        L"Source WinMM port number: %u, Destination WinMM port number: %u",
        sourcePortNumber, destinationPortNumber));

    // Open and close the WinMM ports several times in a row, verifying that the
    // ports can be re-opened each time (this is the core of issue 1070).
    const int iterations = 4;

    midiInGetNumDevs(); // no longer required after June 2026 CFR
    midiOutGetNumDevs(); // no longer required after June 2026 CFR

    for (int i = 0; i < iterations; i++)
    {
        LOG_OUTPUT(WEX::Common::String().Format(L"WinMM open/close iteration %d of %d", i + 1, iterations));

        // A MIDI message "source" is a WinMM MIDI input port.
        HMIDIIN hMidiIn{ nullptr };
        auto inResult = midiInOpen(&hMidiIn, sourcePortNumber, 0, 0, CALLBACK_NULL);
        VERIFY_ARE_EQUAL(inResult, static_cast<MMRESULT>(MMSYSERR_NOERROR));
        VERIFY_IS_NOT_NULL(hMidiIn);

        // A MIDI message "destination" is a WinMM MIDI output port.
        HMIDIOUT hMidiOut{ nullptr };
        auto outResult = midiOutOpen(&hMidiOut, destinationPortNumber, 0, 0, CALLBACK_NULL);
        VERIFY_ARE_EQUAL(outResult, static_cast<MMRESULT>(MMSYSERR_NOERROR));
        VERIFY_IS_NOT_NULL(hMidiOut);

        // Close the WinMM ports
        if (inResult == MMSYSERR_NOERROR)
        {
            VERIFY_ARE_EQUAL(midiInClose(hMidiIn), static_cast<MMRESULT>(MMSYSERR_NOERROR));
        }

        if (outResult == MMSYSERR_NOERROR)
        {
            VERIFY_ARE_EQUAL(midiOutClose(hMidiOut), static_cast<MMRESULT>(MMSYSERR_NOERROR));
        }

        // Give a moment before attempting to re-open
        Sleep(100);
    }

    // cleanupLoopback scope_exit handler removes the loopback
}


void MidiBasicLoopbackTests::TestCreateLoopbackWithGarbageUniqueId()
{
    // A unique id containing spaces, symbols, and punctuation must still result in a
    // successfully created loopback, because MidiBasicLoopbackManager strips the
    // invalid characters before submitting the config to the service.

    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    auto validPrefix = L"ID" + winrt::to_hstring(MidiClock::Now());
    auto garbageUniqueId = MakeGarbageUniqueId(validPrefix.c_str());

    auto expectedUniqueId = ExpectedCleanedUniqueId(garbageUniqueId);

    // sanity check the test data itself: the garbage id must actually be dirty, and
    // must still contain something valid once cleaned
    VERIFY_IS_FALSE(UniqueIdContainsOnlyValidCharacters(garbageUniqueId));
    VERIFY_IS_FALSE(expectedUniqueId.empty());

    std::wcout << L"Supplied unique id: " << garbageUniqueId << std::endl;
    std::wcout << L"Expected cleaned unique id: " << expectedUniqueId << std::endl;

    MidiBasicLoopbackEndpointDefinition definition(
        L"Test Basic Loopback Garbage Id",
        winrt::hstring{ garbageUniqueId },
        L"Loopback created with a unique id which contains invalid characters."
    );

    winrt::guid associationId = foundation::GuidHelper::CreateNewGuid();

    LOG_OUTPUT(L"Creating loopback endpoint creation config");
    MidiBasicLoopbackCreationConfig creationConfig(associationId, definition);

    LOG_OUTPUT(L"Creating loopback");
    auto response = MidiBasicLoopbackManager::CreateTransientLoopback(creationConfig);
    VERIFY_IS_NOT_NULL(response);

    if (response.Success())
    {
        LOG_OUTPUT(L"Endpoint created successfully");

        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry());
        VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointDeviceId().empty());

        // the manager cleans the unique id in the config before submitting it
        std::wstring actualUniqueId{ creationConfig.EndpointDefinition().UniqueId().c_str() };

        std::wcout << L"Actual unique id after creation: " << actualUniqueId << std::endl;

        VERIFY_IS_TRUE(UniqueIdContainsOnlyValidCharacters(actualUniqueId));
        VERIFY_IS_TRUE(actualUniqueId == expectedUniqueId);

        // Give a hoot. Don't pollute.
        MidiBasicLoopbackRemovalConfig removalConfig(response.CreatedLoopbackEntry().AssociationId());
        auto removalResponse = MidiBasicLoopbackManager::RemoveTransientLoopback(removalConfig);

        VERIFY_IS_NOT_NULL(removalResponse);
        VERIFY_IS_TRUE(removalResponse.Success());
    }
    else
    {
        LOG_OUTPUT(L"Return result indicates failure");

        std::wcout << L"Success:       " << response.Success() << std::endl;
        std::wcout << L"Error Code:    " << std::hex << static_cast<uint32_t>(response.ErrorCode()) << std::dec << std::endl;
        std::wcout << L"Error Message: " << response.ErrorMessage().c_str() << std::endl;

        VERIFY_FAIL();
    }
}


void MidiBasicLoopbackTests::TestCreateLoopback()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());

    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    winrt::hstring endpointId{};

    auto uniqueId = L"ID" + winrt::to_hstring(MidiClock::Now());

    MidiBasicLoopbackEndpointDefinition definition(
        L"Test Basic Loopback", // name
        uniqueId, // unique Id that identifies the loopback
        L"The first description is optional, but is displayed to users. This becomes the transport-defined description." // description
    );

    winrt::guid associationId = foundation::GuidHelper::CreateNewGuid();

    LOG_OUTPUT(L"Creating loopback endpoint creation config");

    MidiBasicLoopbackCreationConfig creationConfig(associationId, definition);

    LOG_OUTPUT(L"Creating loopbacks");

    auto response = MidiBasicLoopbackManager::CreateTransientLoopback(creationConfig);
    VERIFY_IS_NOT_NULL(response);

    if (response.Success())
    {
        LOG_OUTPUT(L"Endpoints created successfully");

        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry());
        VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointDeviceId().empty());

        endpointId = response.CreatedLoopbackEntry().EndpointDeviceId();

        std::cout
            << "Loopback Endpoint: " << std::endl
            << " - " << winrt::to_string(endpointId)
            << " - " << winrt::to_string(response.CreatedLoopbackEntry().Name())
            << std::endl << std::endl;


        // Call GetActiveLoopbackEntries and validate that the new entry is present in the list

        auto entries = MidiBasicLoopbackManager::GetActiveLoopbackEntries();
        bool thisLoopbackEntryFound = false;

        for (auto const& entry : entries)
        {
            if (entry.AssociationId() == response.CreatedLoopbackEntry().AssociationId())
            {
                thisLoopbackEntryFound = true;
                break;
            }
        }

        VERIFY_IS_TRUE(thisLoopbackEntryFound);

        // Give a hoot. Don't pollute.
        MidiBasicLoopbackRemovalConfig removalConfig(response.CreatedLoopbackEntry().AssociationId());
        auto removalResponse = MidiBasicLoopbackManager::RemoveTransientLoopback(removalConfig);

        VERIFY_IS_NOT_NULL(removalResponse);
        VERIFY_IS_TRUE(removalResponse.Success());

    }
    else
    {
        LOG_OUTPUT(L"Return result indicates failure");

        std::wcout << L"Success:       " << response.Success() << std::endl;
        std::wcout << L"Error Code:    " << std::hex << static_cast<uint32_t>(response.ErrorCode()) << std::dec << std::endl;
        std::wcout << L"Error Message: " << response.ErrorMessage().c_str() << std::endl;

        VERIFY_FAIL();
    }
}

void MidiBasicLoopbackTests::TestCreateLegacyPorts()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());

    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    winrt::hstring endpointId{};

    auto uniqueId = L"ID" + winrt::to_hstring(MidiClock::Now());

    MidiBasicLoopbackEndpointDefinition definition(
        L"Test Basic Loopback", // name
        uniqueId, // unique Id that identifies the loopback
        L"The description is optional, but is displayed to users. This becomes the transport-defined description." // description
    );

    winrt::guid associationId = foundation::GuidHelper::CreateNewGuid();

    LOG_OUTPUT(L"Creating loopback endpoint creation config");

    MidiBasicLoopbackCreationConfig creationConfig(associationId, definition);

    LOG_OUTPUT(L"Creating loopbacks");

    auto response = MidiBasicLoopbackManager::CreateTransientLoopback(creationConfig);
    VERIFY_IS_NOT_NULL(response);

    if (response.Success())
    {
        LOG_OUTPUT(L"Endpoints created successfully");

        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry());
        VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointDeviceId().empty());

        endpointId = response.CreatedLoopbackEntry().EndpointDeviceId();

        std::cout
            << "Loopback Endpoint: " << std::endl
            << " - " << winrt::to_string(endpointId)
            << " - " << winrt::to_string(response.CreatedLoopbackEntry().Name())
            << std::endl << std::endl;

        auto endpointPorts = MidiLegacyPortDeviceInformation::FindAllForAssociatedEndpoint(endpointId);
        VERIFY_IS_NOT_NULL(endpointPorts);
        VERIFY_IS_TRUE(endpointPorts.Size() > 0);

        std::cout << std::endl;
        for (auto const& portInfo : endpointPorts)
        {
            if (portInfo.Flow() == Midi1PortFlow::MidiMessageDestination)
            {
                std::cout << "Destination Port for Endpoint: ";
            }
            else
            {
                std::cout << "Source Port for Endpoint: ";
            }

            std::cout
                << " - " << winrt::to_string(portInfo.AssociatedEndpointDeviceId())
                << " - " << winrt::to_string(portInfo.Name())
                << std::endl;
        }


        // Give a hoot. Don't pollute.
        MidiBasicLoopbackRemovalConfig removalConfig(response.CreatedLoopbackEntry().AssociationId());
        auto removalResponse = MidiBasicLoopbackManager::RemoveTransientLoopback(removalConfig);

        VERIFY_IS_NOT_NULL(removalResponse);
        VERIFY_IS_TRUE(removalResponse.Success());

    }
    else
    {
        LOG_OUTPUT(L"Return result indicates failure");

        std::wcout << L"Success:       " << response.Success() << std::endl;
        std::wcout << L"Error Code:    " << std::hex << static_cast<uint32_t>(response.ErrorCode()) << std::dec << std::endl;
        std::wcout << L"Error Message: " << response.ErrorMessage().c_str() << std::endl;

        VERIFY_FAIL();
    }
}



// Looks up an active loopback entry by association id. Returns nullptr if not found.
static MidiBasicLoopbackEntry FindActiveLoopbackEntry(winrt::guid const& associationId)
{
    auto entries = MidiBasicLoopbackManager::GetActiveLoopbackEntries();

    if (entries == nullptr) return nullptr;

    for (auto const& entry : entries)
    {
        if (entry.AssociationId() == associationId)
        {
            return entry;
        }
    }

    return nullptr;
}

// Creates a transient basic loopback with a unique name/id, and verifies the response.
static MidiBasicLoopbackCreationResponse CreateTestLoopback(_In_ winrt::hstring const& namePrefix)
{
    auto uniqueId = L"ID" + winrt::to_hstring(MidiClock::Now()) + winrt::to_hstring(rand());

    MidiBasicLoopbackEndpointDefinition definition(
        namePrefix,
        uniqueId,
        L"Loopback created by the Windows MIDI Services TAEF tests."
    );

    MidiBasicLoopbackCreationConfig creationConfig(foundation::GuidHelper::CreateNewGuid(), definition);

    auto response = MidiBasicLoopbackManager::CreateTransientLoopback(creationConfig);

    VERIFY_IS_NOT_NULL(response);

    if (!response.Success())
    {
        std::wcout << L"Error Code:    " << std::hex << static_cast<uint32_t>(response.ErrorCode()) << std::dec << std::endl;
        std::wcout << L"Error Message: " << response.ErrorMessage().c_str() << std::endl;
    }

    VERIFY_IS_TRUE(response.Success());
    VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry());
    VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointDeviceId().empty());

    return response;
}

static void RemoveTestLoopback(winrt::guid const& associationId)
{
    MidiBasicLoopbackRemovalConfig removalConfig(associationId);
    auto removalResponse = MidiBasicLoopbackManager::RemoveTransientLoopback(removalConfig);

    VERIFY_IS_NOT_NULL(removalResponse);
    VERIFY_IS_TRUE(removalResponse.Success());
}


void MidiBasicLoopbackTests::TestMuteLoopback()
{
    // Once a loopback is muted, messages sent to it must no longer be looped back.

    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    auto response = CreateTestLoopback(L"Test Basic Loopback Mute");

    auto associationId = response.CreatedLoopbackEntry().AssociationId();
    auto endpointId = response.CreatedLoopbackEntry().EndpointDeviceId();

    // a newly created loopback must not be muted
    VERIFY_IS_FALSE(response.CreatedLoopbackEntry().IsMuted());

    auto cleanupLoopback = wil::scope_exit([&] { RemoveTestLoopback(associationId); });

    LOG_OUTPUT(L"Creating session and connection");

    auto session = MidiSession::Create(L"TestMuteLoopback");
    VERIFY_IS_NOT_NULL(session);

    auto connection = session.CreateEndpointConnection(endpointId);
    VERIFY_IS_NOT_NULL(connection);

    wil::unique_event_nothrow messageReceived;
    messageReceived.create();

    std::atomic<uint32_t> receivedMessageCount{ 0 };

    auto eventToken = connection.MessageReceived([&](auto&&, MidiMessageReceivedEventArgs const& args)
        {
            VERIFY_IS_NOT_NULL(args);

            std::cout << "Received message 0x" << std::hex << args.PeekFirstWord() << std::dec << std::endl;

            receivedMessageCount++;
            messageReceived.SetEvent();
        });

    VERIFY_IS_TRUE(connection.Open());

    MidiMessage64 message(MidiClock::TimestampConstantSendImmediately(), 0x43001627, 0x86753090);

    // Baseline: while unmuted, the message must loop back
    LOG_OUTPUT(L"Sending message while unmuted");
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connection.SendSingleMessagePacket(message)));

    VERIFY_IS_TRUE(messageReceived.wait(5000));
    VERIFY_ARE_EQUAL(receivedMessageCount.load(), (uint32_t)1);

    // Mute the loopback
    LOG_OUTPUT(L"Muting the loopback");
    auto muteResponse = MidiBasicLoopbackManager::MuteLoopback(associationId);
    VERIFY_IS_NOT_NULL(muteResponse);

    if (!muteResponse.Success())
    {
        std::wcout << L"Mute Error Message: " << muteResponse.ErrorMessage().c_str() << std::endl;
    }

    VERIFY_IS_TRUE(muteResponse.Success());

    // the active loopback entry must now report that it is muted
    auto mutedEntry = FindActiveLoopbackEntry(associationId);
    VERIFY_IS_NOT_NULL(mutedEntry);
    VERIFY_IS_TRUE(mutedEntry.IsMuted());

    // Send while muted. Nothing should come back.
    messageReceived.ResetEvent();
    receivedMessageCount = 0;

    LOG_OUTPUT(L"Sending message while muted");
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connection.SendSingleMessagePacket(message)));

    // wait long enough that a message would have arrived had it not been muted
    VERIFY_IS_FALSE(messageReceived.wait(2000));
    VERIFY_ARE_EQUAL(receivedMessageCount.load(), (uint32_t)0);

    connection.MessageReceived(eventToken);
    session.DisconnectEndpointConnection(connection.ConnectionId());
    session.Close();
}


void MidiBasicLoopbackTests::TestUnmuteAfterMute()
{
    // After unmuting a previously muted loopback, messages must flow again.

    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    auto response = CreateTestLoopback(L"Test Basic Loopback Unmute");

    auto associationId = response.CreatedLoopbackEntry().AssociationId();
    auto endpointId = response.CreatedLoopbackEntry().EndpointDeviceId();

    auto cleanupLoopback = wil::scope_exit([&] { RemoveTestLoopback(associationId); });

    auto session = MidiSession::Create(L"TestUnmuteAfterMute");
    VERIFY_IS_NOT_NULL(session);

    auto connection = session.CreateEndpointConnection(endpointId);
    VERIFY_IS_NOT_NULL(connection);

    wil::unique_event_nothrow messageReceived;
    messageReceived.create();

    std::atomic<uint32_t> receivedMessageCount{ 0 };

    auto eventToken = connection.MessageReceived([&](auto&&, MidiMessageReceivedEventArgs const& args)
        {
            VERIFY_IS_NOT_NULL(args);

            std::cout << "Received message 0x" << std::hex << args.PeekFirstWord() << std::dec << std::endl;

            receivedMessageCount++;
            messageReceived.SetEvent();
        });

    VERIFY_IS_TRUE(connection.Open());

    MidiMessage64 message(MidiClock::TimestampConstantSendImmediately(), 0x43001627, 0x86753090);

    // Mute first, and confirm the messages are actually blocked
    LOG_OUTPUT(L"Muting the loopback");
    auto muteResponse = MidiBasicLoopbackManager::MuteLoopback(associationId);
    VERIFY_IS_NOT_NULL(muteResponse);
    VERIFY_IS_TRUE(muteResponse.Success());

    LOG_OUTPUT(L"Sending message while muted");
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connection.SendSingleMessagePacket(message)));

    VERIFY_IS_FALSE(messageReceived.wait(2000));
    VERIFY_ARE_EQUAL(receivedMessageCount.load(), (uint32_t)0);

    // Now unmute
    LOG_OUTPUT(L"Unmuting the loopback");
    auto unmuteResponse = MidiBasicLoopbackManager::UnmuteLoopback(associationId);
    VERIFY_IS_NOT_NULL(unmuteResponse);

    if (!unmuteResponse.Success())
    {
        std::wcout << L"Unmute Error Message: " << unmuteResponse.ErrorMessage().c_str() << std::endl;
    }

    VERIFY_IS_TRUE(unmuteResponse.Success());

    // the active loopback entry must no longer report that it is muted
    auto unmutedEntry = FindActiveLoopbackEntry(associationId);
    VERIFY_IS_NOT_NULL(unmutedEntry);
    VERIFY_IS_FALSE(unmutedEntry.IsMuted());

    // messages must flow again
    messageReceived.ResetEvent();
    receivedMessageCount = 0;

    LOG_OUTPUT(L"Sending message after unmuting");
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connection.SendSingleMessagePacket(message)));

    VERIFY_IS_TRUE(messageReceived.wait(5000));
    VERIFY_ARE_EQUAL(receivedMessageCount.load(), (uint32_t)1);

    connection.MessageReceived(eventToken);
    session.DisconnectEndpointConnection(connection.ConnectionId());
    session.Close();
}


void MidiBasicLoopbackTests::TestListActiveLoopbacks()
{
    // Creating multiple loopbacks must result in all of them being reported by
    // GetActiveLoopbackEntries, and removing them must take them back out of the list.

    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    auto countBefore = MidiBasicLoopbackManager::GetActiveLoopbackEntries().Size();

    LOG_OUTPUT(L"Creating first loopback");
    auto response1 = CreateTestLoopback(L"Test Basic Loopback List 1");
    auto associationId1 = response1.CreatedLoopbackEntry().AssociationId();
    auto endpointId1 = response1.CreatedLoopbackEntry().EndpointDeviceId();

    auto cleanup1 = wil::scope_exit([&] { RemoveTestLoopback(associationId1); });

    LOG_OUTPUT(L"Creating second loopback");
    auto response2 = CreateTestLoopback(L"Test Basic Loopback List 2");
    auto associationId2 = response2.CreatedLoopbackEntry().AssociationId();
    auto endpointId2 = response2.CreatedLoopbackEntry().EndpointDeviceId();

    auto cleanup2 = wil::scope_exit([&] { RemoveTestLoopback(associationId2); });

    // the two loopbacks must be distinct
    VERIFY_IS_FALSE(associationId1 == associationId2);
    VERIFY_IS_FALSE(HStringsAreCaseInsensitiveEqual(endpointId1, endpointId2));

    auto entries = MidiBasicLoopbackManager::GetActiveLoopbackEntries();
    VERIFY_IS_NOT_NULL(entries);

    std::cout << "Active loopback entries: " << entries.Size() << std::endl;

    for (auto const& entry : entries)
    {
        std::cout
            << " - " << winrt::to_string(entry.Name())
            << " : " << winrt::to_string(entry.EndpointDeviceId())
            << std::endl;
    }

    // both of the loopbacks we created must be present
    auto entry1 = FindActiveLoopbackEntry(associationId1);
    VERIFY_IS_NOT_NULL(entry1);
    VERIFY_IS_TRUE(HStringsAreCaseInsensitiveEqual(entry1.EndpointDeviceId(), endpointId1));

    auto entry2 = FindActiveLoopbackEntry(associationId2);
    VERIFY_IS_NOT_NULL(entry2);
    VERIFY_IS_TRUE(HStringsAreCaseInsensitiveEqual(entry2.EndpointDeviceId(), endpointId2));

    // and the count must have grown by exactly the two we added
    VERIFY_ARE_EQUAL(entries.Size(), countBefore + 2);

    // now remove the first one and verify it drops out of the list while the other remains
    LOG_OUTPUT(L"Removing the first loopback");
    RemoveTestLoopback(associationId1);
    cleanup1.release();

    VERIFY_IS_NULL(FindActiveLoopbackEntry(associationId1));
    VERIFY_IS_NOT_NULL(FindActiveLoopbackEntry(associationId2));

    LOG_OUTPUT(L"Removing the second loopback");
    RemoveTestLoopback(associationId2);
    cleanup2.release();

    VERIFY_IS_NULL(FindActiveLoopbackEntry(associationId2));

    // we should be back where we started
    VERIFY_ARE_EQUAL(MidiBasicLoopbackManager::GetActiveLoopbackEntries().Size(), countBefore);
}


void MidiBasicLoopbackTests::TestUmpSendReceive()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());

    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    winrt::hstring endpointId{};

    auto uniqueId = L"ID" + winrt::to_hstring(MidiClock::Now());

    MidiBasicLoopbackEndpointDefinition definition(
        L"Test Basic Loopback", // name
        uniqueId, // unique Id that identifies the loopback
        L"The description is optional, but is displayed to users. This becomes the transport-defined description." // description
    );

    winrt::guid associationId = foundation::GuidHelper::CreateNewGuid();

    LOG_OUTPUT(L"Creating loopback endpoint creation config");

    MidiBasicLoopbackCreationConfig creationConfig(associationId, definition);

    LOG_OUTPUT(L"Creating loopbacks");

    auto response = MidiBasicLoopbackManager::CreateTransientLoopback(creationConfig);
    VERIFY_IS_NOT_NULL(response);

    if (response.Success())
    {
        LOG_OUTPUT(L"Endpoints created successfully");

        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry());
        VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointDeviceId().empty());

        endpointId = response.CreatedLoopbackEntry().EndpointDeviceId();

        std::cout
            << "Loopback Endpoint: " << std::endl
            << " - " << winrt::to_string(endpointId)
            << " - " << winrt::to_string(response.CreatedLoopbackEntry().Name())
            << std::endl << std::endl;


        std::cout << "Setting up events" << std::endl;
        wil::unique_event_nothrow allMessagesReceived;
        allMessagesReceived.create();

        bool messageReceived { false };

        std::cout << "Creating messages" << std::endl;

        MidiMessage64 message;
        message.Timestamp(MidiClock::TimestampConstantSendImmediately());
        message.Word0(0x43001627);
        message.Word1(0x86753090);

        std::cout << "Creating session" << std::endl;

        auto session = MidiSession::Create(L"TAEF TestBasicLoopbackEndpointConnections Session");
        VERIFY_IS_NOT_NULL(session);

        std::cout << "Creating connections" << std::endl;

        auto connection = session.CreateEndpointConnection(endpointId);
        VERIFY_IS_NOT_NULL(connection);

        connection.MessageReceived([&](auto&&, MidiMessageReceivedEventArgs const& args)
            {
                VERIFY_IS_NOT_NULL(args);
                VERIFY_IS_TRUE(args.PacketType() == MidiPacketType::UniversalMidiPacket64);

                auto message = args.GetMessagePacket().as<MidiMessage64>();

                VERIFY_ARE_EQUAL(message.Word0(), message.Word0());

                messageReceived = true;
                allMessagesReceived.SetEvent();
            });

        std::cout << "Opening connections" << std::endl;

        VERIFY_IS_TRUE(connection.Open());

        // send messages
        std::cout << "Sending messages" << std::endl;
        connection.SendSingleMessagePacket(message);

        std::cout << "Waiting..." << std::endl;
        allMessagesReceived.wait(5000);

        VERIFY_IS_TRUE(messageReceived);




        // Give a hoot. Don't pollute.
        MidiBasicLoopbackRemovalConfig removalConfig(response.CreatedLoopbackEntry().AssociationId());
        auto removalResponse = MidiBasicLoopbackManager::RemoveTransientLoopback(removalConfig);

        VERIFY_IS_NOT_NULL(removalResponse);
        VERIFY_IS_TRUE(removalResponse.Success());

    }
    else
    {
        LOG_OUTPUT(L"Return result indicates failure");

        std::wcout << L"Success:       " << response.Success() << std::endl;
        std::wcout << L"Error Code:    " << std::hex << static_cast<uint32_t>(response.ErrorCode()) << std::dec << std::endl;
        std::wcout << L"Error Message: " << response.ErrorMessage().c_str() << std::endl;

        VERIFY_FAIL();
    }



    
    ////    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());

    //VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    //winrt::hstring endpointAId{};
    //winrt::hstring endpointBId{};

    //auto uniqueId = L"ID" + winrt::to_hstring(MidiClock::Now());

    //// A-side of the loopback
    //MidiLoopbackEndpointDefinition definitionA(
    //    L"Test Loopback A", // name
    //    uniqueId, // unique Id that identifies the loopback
    //    L"The first description is optional, but is displayed to users. This becomes the transport-defined description." // description
    //);

    //// B-side of the loopback
    //MidiLoopbackEndpointDefinition definitionB(
    //    L"Test Loopback B",
    //    uniqueId, // can be the same as the first one, but doesn't need to be.
    //    L"The second description is optional, but is displayed to users. This becomes the transport-defined description."
    //);

    //winrt::guid associationId = foundation::GuidHelper::CreateNewGuid();

    //LOG_OUTPUT(L"Creating loopback endpoint creation config");

    //MidiLoopbackCreationConfig creationConfig(associationId, definitionA, definitionB);

    //LOG_OUTPUT(L"Creating loopbacks");

    //auto response = MidiLoopbackManager::CreateTransientLoopback(creationConfig);
    //VERIFY_IS_NOT_NULL(response);

    //if (response.Success())
    //{
    //    LOG_OUTPUT(L"Endpoints created successfully");

    //    VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry());
    //    VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry().EndpointA());
    //    VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry().EndpointB());
    //    VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId().empty());
    //    VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId().empty());

    //    endpointAId = response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId();
    //    endpointBId = response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId();


    //    std::cout << "Creating messages" << std::endl;

    //    MidiMessage32 messageAToB;
    //    messageAToB.Timestamp(MidiClock::TimestampConstantSendImmediately());
    //    messageAToB.Word0(0x23001627);

    //    MidiMessage64 messageBToA;
    //    messageBToA.Timestamp(MidiClock::TimestampConstantSendImmediately());
    //    messageBToA.Word0(0x43001627);
    //    messageBToA.Word1(0x86753090);

    //    bool messageAToBReceived{ false };
    //    bool messageBToAReceived{ false };

    //    std::cout << "Setting up events" << std::endl;
    //    wil::unique_event_nothrow allMessagesReceived;
    //    allMessagesReceived.create();


    //    std::cout << "Creating session" << std::endl;

    //    auto session = MidiSession::Create(L"TAEF TestLoopbackEndpointConnections Session");
    //    VERIFY_IS_NOT_NULL(session);


    //    std::cout << "Creating connections" << std::endl;

    //    auto connectionA = session.CreateEndpointConnection(endpointAId);
    //    VERIFY_IS_NOT_NULL(connectionA);

    //    auto connectionB = session.CreateEndpointConnection(endpointBId);
    //    VERIFY_IS_NOT_NULL(connectionB);

    //    connectionA.MessageReceived([&](auto&&, MidiMessageReceivedEventArgs const& args)
    //        {
    //            VERIFY_IS_NOT_NULL(args);
    //            VERIFY_IS_TRUE(args.PacketType() == MidiPacketType::UniversalMidiPacket64);

    //            auto message = args.GetMessagePacket().as<MidiMessage64>();

    //            VERIFY_ARE_EQUAL(message.Word0(), messageBToA.Word0());
    //            VERIFY_ARE_EQUAL(message.Word1(), messageBToA.Word1());

    //            messageBToAReceived = true;
    //            if (messageAToBReceived && messageBToAReceived)
    //            {
    //                allMessagesReceived.SetEvent();
    //            }
    //        });

    //    connectionB.MessageReceived([&](auto&&, MidiMessageReceivedEventArgs const& args)
    //        {
    //            VERIFY_IS_NOT_NULL(args);
    //            VERIFY_IS_TRUE(args.PacketType() == MidiPacketType::UniversalMidiPacket32);

    //            auto message = args.GetMessagePacket().as<MidiMessage32>();

    //            VERIFY_ARE_EQUAL(message.Word0(), messageAToB.Word0());

    //            messageAToBReceived = true;
    //            if (messageAToBReceived && messageBToAReceived)
    //            {
    //                allMessagesReceived.SetEvent();
    //            }
    //        });

    //    std::cout << "Opening connections" << std::endl;

    //    VERIFY_IS_TRUE(connectionA.Open());
    //    VERIFY_IS_TRUE(connectionB.Open());

    //    // send messages
    //    std::cout << "Sending messages" << std::endl;
    //    connectionA.SendSingleMessagePacket(messageAToB);
    //    connectionB.SendSingleMessagePacket(messageBToA);

    //    std::cout << "Waiting..." << std::endl;
    //    allMessagesReceived.wait(5000);

    //    VERIFY_IS_TRUE(messageAToBReceived);
    //    VERIFY_IS_TRUE(messageBToAReceived);





    //    // Give a hoot. Don't pollute.
    //    MidiLoopbackRemovalConfig removalConfig(response.CreatedLoopbackEntry().AssociationId());
    //    auto removalResponse = MidiLoopbackManager::RemoveTransientLoopback(removalConfig);

    //    VERIFY_IS_NOT_NULL(removalResponse);
    //    VERIFY_IS_TRUE(removalResponse.Success());

    //}
    //else
    //{
    //    LOG_OUTPUT(L"Return result indicates failure");

    //    std::wcout << L"Success:       " << response.Success() << std::endl;
    //    std::wcout << L"Error Code:    " << std::hex << static_cast<uint32_t>(response.ErrorCode()) << std::dec << std::endl;
    //    std::wcout << L"Error Message: " << response.ErrorMessage().c_str() << std::endl;

    //    VERIFY_FAIL();
    //}

}



