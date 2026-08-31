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

#include <io.h>
#include <fcntl.h>

#pragma comment(lib, "winmm.lib")

// Every loopback created here leaves a deactivated software device node behind for its UMP
// endpoint and each of that endpoint's MIDI 1.0 ports, because the service never deletes one.
// These two hooks remove exactly the nodes this test method caused to be created.
bool MidiBasicLoopbackTests::TestSetup()
{
    m_deviceNodeTracker.Start();

    return true;
}

bool MidiBasicLoopbackTests::TestCleanup()
{
    m_deviceNodeTracker.RemoveDeviceNodesCreatedSinceStart();

    return true;
}

void MidiBasicLoopbackTests::TestUnicodeGtbAndDeviceNames()
{
    auto previousStdoutMode = _setmode(_fileno(stdout), _O_U16TEXT);  // _O_WTEXT

    if (previousStdoutMode == -1)
    {
        perror("Unable to set stdout to UTF-16 mode. ");
    }

    // The stdout translation mode is process-wide and the TAEF host process is
    // shared by every test. If we leave stdout in UTF-16 mode, the next test that
    // writes narrow characters to std::cout trips the CRT invalid parameter
    // handler, which fast-fails the host process with 0xC0000409.
    auto restoreStdoutMode = wil::scope_exit([&]
        {
            std::wcout.flush();

            if (previousStdoutMode != -1)
            {
                _setmode(_fileno(stdout), previousStdoutMode);
            }
        });


    winrt::hstring uniqueId = winrt::to_hstring(winrt::Windows::Foundation::GuidHelper::CreateNewGuid());
    auto name = L"我的虚拟设备";

    MidiBasicLoopbackEndpointDefinition definition;
    definition.Name(name);
    definition.UniqueId(uniqueId);

    MidiBasicLoopbackCreationConfig config(definition);

    auto result = MidiBasicLoopbackManager::CreateTransientLoopback(config);

    VERIFY_IS_NOT_NULL(result);
    VERIFY_IS_TRUE(result.Success());

    auto associationId = result.CreatedLoopbackEntry().AssociationId();

    // remove the loopback even if a VERIFY macro below halts the method
    auto cleanupLoopback = wil::scope_exit([&]
        {
            MidiBasicLoopbackRemovalConfig removalConfig(associationId);
            MidiBasicLoopbackManager::RemoveTransientLoopback(removalConfig);
        });

    auto endpointDeviceId = result.CreatedLoopbackEntry().EndpointDeviceId();
    auto endpointInformation = MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(endpointDeviceId);
    VERIFY_IS_NOT_NULL(endpointInformation);

    std::wcout << L"Endpoint Name: " << endpointInformation.Name().c_str() << std::endl;

    // Check name

    std::wcout << L"Sent Device Name Char Codes: " << std::endl;
    for (wchar_t ch : definition.Name())
    {
        std::wcout << std::hex << std::setw(4) << (int)ch << ", ";
    }
    std::wcout << std::endl;

    std::wcout << L"Received Device Name Char Codes: " << std::endl;
    for (wchar_t ch : endpointInformation.Name())
    {
        std::wcout << std::hex << std::setw(4) << (int)ch << ", ";
    }
    std::wcout << std::endl;

    auto nameResult = wcscmp(endpointInformation.Name().c_str(), definition.Name().c_str());
    VERIFY_IS_TRUE(nameResult == 0);

    // Check group terminal blocks

    std::wcout << L"Sent GTB Char Codes: " << std::endl;
    for (wchar_t ch : definition.Name())
    {
        std::wcout << std::hex << std::setw(4) << (int)ch << ", ";
    }
    std::wcout << std::endl;

    std::wcout << L"Received GTB Char Codes: " << std::endl;
    for (wchar_t ch : endpointInformation.GetGroupTerminalBlocks().GetAt(0).Name())
    {
        std::wcout << std::hex << std::setw(4) << (int)ch << ", ";
    }
    std::wcout << std::endl;

    auto gtbNameResult = wcscmp(endpointInformation.GetGroupTerminalBlocks().GetAt(0).Name().c_str(), definition.Name().c_str());
    VERIFY_IS_TRUE(gtbNameResult == 0);


    // test that we can find a device with this name

    auto foundPorts = MidiLegacyPortDeviceInformation::FindAllForName(definition.Name());
    VERIFY_IS_TRUE(foundPorts.Size() > 0);
    std::wcout << L"Found Port Name: " << foundPorts.GetAt(0).Name().c_str() << std::endl;

}


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

    LOG_OUTPUT(L"Creating loopback endpoint creation config");
    MidiBasicLoopbackCreationConfig creationConfig(definition);

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
        L"Loopback created with a unique id which contains invalid characters.",
        winrt::hstring{ garbageUniqueId }
    );

    LOG_OUTPUT(L"Creating loopback endpoint creation config");
    MidiBasicLoopbackCreationConfig creationConfig(definition);

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


void MidiBasicLoopbackTests::TestCreateLoopbackWithoutUniqueIdGeneratesOne()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());

    // no unique id supplied, so the config has to produce one. A caller should not have to
    // invent a random string just to create a loopback.
    MidiBasicLoopbackEndpointDefinition definition(
        L"Test Basic Loopback Generated Id",
        L"Loopback created without supplying a unique id."
    );

    VERIFY_IS_TRUE(definition.UniqueId().empty());

    MidiBasicLoopbackCreationConfig creationConfig(definition);

    std::wstring generatedUniqueId{ creationConfig.EndpointDefinition().UniqueId().c_str() };

    std::wcout << L"Generated unique id: " << generatedUniqueId << std::endl;

    VERIFY_IS_FALSE(generatedUniqueId.empty());
    VERIFY_IS_TRUE(UniqueIdContainsOnlyValidCharacters(generatedUniqueId));

    auto response = MidiBasicLoopbackManager::CreateTransientLoopback(creationConfig);
    VERIFY_IS_NOT_NULL(response);
    VERIFY_IS_TRUE(response.Success());

    auto cleanupLoopback = wil::scope_exit([&]
        {
            MidiBasicLoopbackRemovalConfig removalConfig(response.CreatedLoopbackEntry().AssociationId());
            MidiBasicLoopbackManager::RemoveTransientLoopback(removalConfig);
        });

    VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointDeviceId().empty());

    std::wcout << L"Endpoint device id: " << response.CreatedLoopbackEntry().EndpointDeviceId().c_str() << std::endl;

    // and two configs must not collide
    MidiBasicLoopbackEndpointDefinition otherDefinition(L"Test Basic Loopback Generated Id 2");
    MidiBasicLoopbackCreationConfig otherConfig(otherDefinition);

    VERIFY_IS_FALSE(otherConfig.EndpointDefinition().UniqueId().empty());
    VERIFY_ARE_NOT_EQUAL(otherConfig.EndpointDefinition().UniqueId(), creationConfig.EndpointDefinition().UniqueId());
    VERIFY_ARE_NOT_EQUAL(otherConfig.AssociationId(), creationConfig.AssociationId());
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

    LOG_OUTPUT(L"Creating loopback endpoint creation config");

    MidiBasicLoopbackCreationConfig creationConfig(definition);

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

    LOG_OUTPUT(L"Creating loopback endpoint creation config");

    MidiBasicLoopbackCreationConfig creationConfig(definition);

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
static MidiBasicLoopbackEntry FindActiveLoopbackEntry(_In_ winrt::guid const& associationId)
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
        L"Loopback created by the Windows MIDI Services TAEF tests.",
        uniqueId
    );

    MidiBasicLoopbackCreationConfig creationConfig(definition);

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
    VERIFY_IS_TRUE(response1.Success());
    auto associationId1 = response1.CreatedLoopbackEntry().AssociationId();
    auto endpointId1 = response1.CreatedLoopbackEntry().EndpointDeviceId();

    auto cleanup1 = wil::scope_exit([&] { RemoveTestLoopback(associationId1); });

    LOG_OUTPUT(L"Creating second loopback");
    auto response2 = CreateTestLoopback(L"Test Basic Loopback List 2");
    VERIFY_IS_TRUE(response2.Success());
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

    LOG_OUTPUT(L"Creating loopback endpoint creation config");

    MidiBasicLoopbackCreationConfig creationConfig(definition);

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
}



