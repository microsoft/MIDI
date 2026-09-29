// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Devices.Midi2.ServiceConfig.h>
#include <mmsystem.h>
#include <vector>
#include <atomic>
#include <chrono>

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


void MidiBasicLoopbackTests::TestCreateLoopbackWithANameAlreadyInUseIsRejected()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());

    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    MidiBasicLoopbackEndpointDefinition firstDefinition(
        L"Test Basic Loopback Name Domain",
        L"ID" + winrt::to_hstring(MidiClock::Now()),
        L"The loopback which takes the name.");

    MidiBasicLoopbackCreationConfig firstConfig(firstDefinition);

    auto firstResponse = MidiBasicLoopbackManager::CreateTransientLoopback(firstConfig);

    VERIFY_IS_NOT_NULL(firstResponse);
    VERIFY_IS_TRUE(firstResponse.Success());

    auto cleanupLoopback = wil::scope_exit([&]
        {
            MidiBasicLoopbackRemovalConfig removalConfig(firstResponse.CreatedLoopbackEntry().AssociationId());
            MidiBasicLoopbackManager::RemoveTransientLoopback(removalConfig);
        });

    // a completely separate loopback, with its own unique id, differing only by case
    MidiBasicLoopbackEndpointDefinition secondDefinition(
        L"TEST BASIC LOOPBACK NAME DOMAIN",
        L"ID" + winrt::to_hstring(MidiClock::Now()),
        L"The loopback which should be refused.");

    MidiBasicLoopbackCreationConfig secondConfig(secondDefinition);

    auto secondResponse = MidiBasicLoopbackManager::CreateTransientLoopback(secondConfig);

    VERIFY_IS_NOT_NULL(secondResponse);

    auto cleanupSecond = wil::scope_exit([&]
        {
            if (secondResponse.Success())
            {
                MidiBasicLoopbackRemovalConfig removalConfig(secondResponse.CreatedLoopbackEntry().AssociationId());
                MidiBasicLoopbackManager::RemoveTransientLoopback(removalConfig);
            }
        });

    std::wcout << L"Error Message: " << secondResponse.ErrorMessage().c_str() << std::endl;

    VERIFY_IS_FALSE(secondResponse.Success());
}


void MidiBasicLoopbackTests::TestCreateLoopback()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());

    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    winrt::hstring endpointId{};

    auto uniqueId = L"ID" + winrt::to_hstring(MidiClock::Now());

    MidiBasicLoopbackEndpointDefinition definition(
        L"Test Basic Loopback Create", // name
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
        L"Test Basic Loopback Legacy Ports", // name
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


// The setup tool draws its traffic graph from this, so what matters is that it counts MESSAGES.
// A count of buffers would make a burst of notes look the same as one long system exclusive.
void MidiBasicLoopbackTests::TestMessageCountCountsMessagesNotBuffers()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    auto response = CreateTestLoopback(L"Test Basic Loopback Message Count");

    auto associationId = response.CreatedLoopbackEntry().AssociationId();
    auto endpointId = response.CreatedLoopbackEntry().EndpointDeviceId();

    // nothing has passed through a loopback which was created a moment ago
    VERIFY_ARE_EQUAL(response.CreatedLoopbackEntry().MessageCount(), (uint64_t)0);

    auto cleanupLoopback = wil::scope_exit([&] { RemoveTestLoopback(associationId); });

    auto session = MidiSession::Create(L"TestMessageCountCountsMessagesNotBuffers");
    VERIFY_IS_NOT_NULL(session);

    auto connection = session.CreateEndpointConnection(endpointId);
    VERIFY_IS_NOT_NULL(connection);

    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();

    const uint32_t expectedMessageCount{ 3 };

    std::atomic<uint32_t> receivedMessageCount{ 0 };

    auto eventToken = connection.MessageReceived([&](auto&&, MidiMessageReceivedEventArgs const& args)
        {
            VERIFY_IS_NOT_NULL(args);

            if (++receivedMessageCount >= expectedMessageCount)
            {
                allMessagesReceived.SetEvent();
            }
        });

    VERIFY_IS_TRUE(connection.Open());

    // Three 64-bit messages in one send. The count has to be 3, not 1.
    std::vector<uint32_t> words{ 0x43001627, 0x86753090, 0x43001628, 0x86753091, 0x43001629, 0x86753092 };

    winrt::array_view<uint32_t> wordArray(words);

    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(
        connection.SendMultipleMessagesWordArray(
            MidiClock::TimestampConstantSendImmediately(),
            0,
            static_cast<uint32_t>(words.size()),
            wordArray)));

    VERIFY_IS_TRUE(allMessagesReceived.wait(5000));
    VERIFY_ARE_EQUAL(receivedMessageCount.load(), expectedMessageCount);

    connection.MessageReceived(eventToken);

    auto const entry = FindActiveLoopbackEntry(associationId);
    VERIFY_IS_NOT_NULL(entry);

    std::wcout << L"Reported message count: " << entry.MessageCount() << std::endl;

    VERIFY_ARE_EQUAL(entry.MessageCount(), (uint64_t)expectedMessageCount);
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
        L"Test Basic Loopback Send Receive", // name
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



// ============================================================================
// Feedback protection
// ============================================================================

// TAEF only knows how to print its own types, so a WinRT enum needs telling.
namespace WEX::TestExecution
{
    template <>
    class VerifyOutputTraits<MidiBasicLoopbackFeedbackProtection>
    {
    public:
        static WEX::Common::NoThrowString ToString(MidiBasicLoopbackFeedbackProtection const& value)
        {
            return WEX::Common::NoThrowString().Format(L"%d", static_cast<int32_t>(value));
        }
    };

    template <>
    class VerifyOutputTraits<MidiBasicLoopbackErrorCode>
    {
    public:
        static WEX::Common::NoThrowString ToString(MidiBasicLoopbackErrorCode const& value)
        {
            return WEX::Common::NoThrowString().Format(L"0x%08x", static_cast<uint32_t>(value));
        }
    };
}

namespace
{
    // Where the basic loopback transport cannot watch for feedback, the typed properties report
    // Off and there is nothing to test, so these skip rather than fail.
    bool FeedbackProtectionAvailableOrSkip()
    {
        if (!MidiBasicLoopbackManager::IsFeedbackProtectionAvailable())
        {
            WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"The basic loopback transport on this PC cannot watch for feedback.");
            return false;
        }

        return true;
    }

    // The detector needs about a second of sustained feedback, and a work item applies the mute
    // after that, so this polls.
    bool WaitForFeedbackMute(_In_ winrt::guid const& associationId, _In_ std::chrono::milliseconds const timeout)
    {
        auto const deadline = std::chrono::steady_clock::now() + timeout;

        do
        {
            auto const entry = FindActiveLoopbackEntry(associationId);

            if (entry != nullptr && entry.IsMutedForFeedback())
            {
                return true;
            }

            ::Sleep(100);
        } while (std::chrono::steady_clock::now() < deadline);

        return false;
    }

    // The loop a customer makes by accident with a basic loopback: an app with MIDI thru on,
    // listening to the loopback and sending to the same loopback. Whatever comes in goes straight
    // back out, and comes in again. A fixed set of different messages keeps going around, so the
    // loop runs as fast as the service can carry it without ever filling a buffer.
    class ThruFeedbackLoop
    {
    public:
        ThruFeedbackLoop(_In_ MidiBasicLoopbackEntry const& loopback, _In_ winrt::hstring const& sessionName)
        {
            m_session = MidiSession::Create(sessionName);
            VERIFY_IS_NOT_NULL(m_session);

            m_connection = m_session.CreateEndpointConnection(loopback.EndpointDeviceId());
            VERIFY_IS_NOT_NULL(m_connection);

            // Captured by value so a callback still running after Stop() touches nothing freed.
            auto state = m_state;
            auto connection = m_connection;

            m_token = m_connection.MessageReceived([state, connection](auto&&, MidiMessageReceivedEventArgs const& args)
                {
                    if (!state->Echoing)
                    {
                        return;
                    }

                    uint32_t word0{ 0 };
                    uint32_t word1{ 0 };
                    uint32_t word2{ 0 };
                    uint32_t word3{ 0 };

                    if (args.FillWords(word0, word1, word2, word3) != 2)
                    {
                        return;
                    }

                    state->EchoCount++;

                    connection.SendSingleMessageWords(MidiClock::TimestampConstantSendImmediately(), word0, word1);
                });

            VERIFY_IS_TRUE(m_connection.Open());
        }

        ~ThruFeedbackLoop()
        {
            Stop();
        }

        // Different notes, as a chord would have, and every one of them keeps going around.
        void Start(_In_ uint32_t const distinctMessageCount)
        {
            for (uint32_t i = 0; i < distinctMessageCount; i++)
            {
                uint32_t const word0{ 0x40900000 | ((i & 0x7F) << 8) };
                uint32_t const word1{ 0x80000000 };

                VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(
                    m_connection.SendSingleMessageWords(MidiClock::TimestampConstantSendImmediately(), word0, word1)));
            }
        }

        void Stop() noexcept
        {
            m_state->Echoing = false;

            try
            {
                if (m_session == nullptr)
                {
                    return;
                }

                m_connection.MessageReceived(m_token);

                m_session.DisconnectEndpointConnection(m_connection.ConnectionId());
                m_session.Close();

                m_session = nullptr;
            }
            catch (...)
            {
            }
        }

        uint64_t EchoCount() const noexcept
        {
            return m_state->EchoCount;
        }

    private:
        struct State
        {
            std::atomic<bool> Echoing{ true };
            std::atomic<uint64_t> EchoCount{ 0 };
        };

        std::shared_ptr<State> m_state{ std::make_shared<State>() };

        MidiSession m_session{ nullptr };
        MidiEndpointConnection m_connection{ nullptr };
        winrt::event_token m_token{};
    };

    constexpr uint32_t FeedbackLoopMessageCount{ 32 };
}


void MidiBasicLoopbackTests::TestFeedbackProtectionDefaultsToMute()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    if (!FeedbackProtectionAvailableOrSkip())
    {
        return;
    }

    // what a loopback gets when the app says nothing about feedback
    VERIFY_ARE_EQUAL(MidiBasicLoopbackCreationConfig{}.FeedbackProtection(), MidiBasicLoopbackFeedbackProtection::Mute);

    auto response = CreateTestLoopback(L"Test Basic Loopback Feedback Default");
    auto associationId = response.CreatedLoopbackEntry().AssociationId();

    auto cleanupLoopback = wil::scope_exit([&] { RemoveTestLoopback(associationId); });

    VERIFY_ARE_EQUAL(response.CreatedLoopbackEntry().FeedbackProtection(), MidiBasicLoopbackFeedbackProtection::Mute);
    VERIFY_IS_FALSE(response.CreatedLoopbackEntry().IsMutedForFeedback());

    auto const entry = FindActiveLoopbackEntry(associationId);
    VERIFY_IS_NOT_NULL(entry);

    VERIFY_ARE_EQUAL(entry.FeedbackProtection(), MidiBasicLoopbackFeedbackProtection::Mute);
    VERIFY_IS_FALSE(entry.IsMutedForFeedback());
    VERIFY_ARE_EQUAL(entry.FeedbackDetectedTime().time_since_epoch().count(), (int64_t)0);
}


void MidiBasicLoopbackTests::TestCreateWithFeedbackProtectionOff()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    if (!FeedbackProtectionAvailableOrSkip())
    {
        return;
    }

    auto uniqueId = L"ID" + winrt::to_hstring(MidiClock::Now()) + winrt::to_hstring(rand());

    MidiBasicLoopbackEndpointDefinition definition;
    definition.Name(L"Test Basic Loopback Feedback Off");
    definition.Description(L"Basic loopback created by the Windows MIDI Services TAEF tests.");
    definition.UniqueId(uniqueId);

    MidiBasicLoopbackCreationConfig creationConfig(definition);
    creationConfig.FeedbackProtection(MidiBasicLoopbackFeedbackProtection::Off);

    auto response = MidiBasicLoopbackManager::CreateTransientLoopback(creationConfig);
    VERIFY_IS_NOT_NULL(response);

    if (!response.Success())
    {
        std::wcout << L"Error Message: " << response.ErrorMessage().c_str() << std::endl;
    }

    VERIFY_IS_TRUE(response.Success());

    auto associationId = response.CreatedLoopbackEntry().AssociationId();

    auto cleanupLoopback = wil::scope_exit([&] { RemoveTestLoopback(associationId); });

    VERIFY_ARE_EQUAL(response.CreatedLoopbackEntry().FeedbackProtection(), MidiBasicLoopbackFeedbackProtection::Off);

    // and the service agrees, rather than only the SDK echoing back what it was given
    auto const entry = FindActiveLoopbackEntry(associationId);
    VERIFY_IS_NOT_NULL(entry);
    VERIFY_ARE_EQUAL(entry.FeedbackProtection(), MidiBasicLoopbackFeedbackProtection::Off);
}


void MidiBasicLoopbackTests::TestSetFeedbackProtection()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    if (!FeedbackProtectionAvailableOrSkip())
    {
        return;
    }

    auto response = CreateTestLoopback(L"Test Basic Loopback Feedback Set");
    auto associationId = response.CreatedLoopbackEntry().AssociationId();

    auto cleanupLoopback = wil::scope_exit([&] { RemoveTestLoopback(associationId); });

    auto offResponse = MidiBasicLoopbackManager::SetFeedbackProtection(associationId, MidiBasicLoopbackFeedbackProtection::Off);
    VERIFY_IS_NOT_NULL(offResponse);

    if (!offResponse.Success())
    {
        std::wcout << L"Error Message: " << offResponse.ErrorMessage().c_str() << std::endl;
    }

    VERIFY_IS_TRUE(offResponse.Success());

    auto entry = FindActiveLoopbackEntry(associationId);
    VERIFY_IS_NOT_NULL(entry);
    VERIFY_ARE_EQUAL(entry.FeedbackProtection(), MidiBasicLoopbackFeedbackProtection::Off);

    // the muted state is a separate thing, and changing protection must not touch it
    VERIFY_IS_FALSE(entry.IsMuted());

    auto muteResponse = MidiBasicLoopbackManager::SetFeedbackProtection(associationId, MidiBasicLoopbackFeedbackProtection::Mute);
    VERIFY_IS_NOT_NULL(muteResponse);
    VERIFY_IS_TRUE(muteResponse.Success());

    entry = FindActiveLoopbackEntry(associationId);
    VERIFY_IS_NOT_NULL(entry);
    VERIFY_ARE_EQUAL(entry.FeedbackProtection(), MidiBasicLoopbackFeedbackProtection::Mute);

    // a loopback that is not there is reported as such
    auto missingResponse = MidiBasicLoopbackManager::SetFeedbackProtection(
        winrt::Windows::Foundation::GuidHelper::CreateNewGuid(),
        MidiBasicLoopbackFeedbackProtection::Off);

    VERIFY_IS_NOT_NULL(missingResponse);
    VERIFY_IS_FALSE(missingResponse.Success());
}


void MidiBasicLoopbackTests::TestSetFeedbackProtectionRejectsUnknownValue()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    if (!FeedbackProtectionAvailableOrSkip())
    {
        return;
    }

    auto response = CreateTestLoopback(L"Test Basic Loopback Feedback Unknown");
    auto associationId = response.CreatedLoopbackEntry().AssociationId();

    auto cleanupLoopback = wil::scope_exit([&] { RemoveTestLoopback(associationId); });

    // an enum from a newer SDK, or a cast, is refused rather than guessed at
    auto unknownResponse = MidiBasicLoopbackManager::SetFeedbackProtection(
        associationId,
        static_cast<MidiBasicLoopbackFeedbackProtection>(99));

    VERIFY_IS_NOT_NULL(unknownResponse);
    VERIFY_IS_FALSE(unknownResponse.Success());
    VERIFY_ARE_EQUAL(unknownResponse.ErrorCode(), MidiBasicLoopbackErrorCode::InvalidArgument);

    // and nothing changed
    auto const entry = FindActiveLoopbackEntry(associationId);
    VERIFY_IS_NOT_NULL(entry);
    VERIFY_ARE_EQUAL(entry.FeedbackProtection(), MidiBasicLoopbackFeedbackProtection::Mute);
}


void MidiBasicLoopbackTests::TestFeedbackLoopMutesLoopback()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    if (!FeedbackProtectionAvailableOrSkip())
    {
        return;
    }

    auto response = CreateTestLoopback(L"Test Basic Loopback Feedback Loop");
    auto associationId = response.CreatedLoopbackEntry().AssociationId();

    auto cleanupLoopback = wil::scope_exit([&] { RemoveTestLoopback(associationId); });

    ThruFeedbackLoop loop(response.CreatedLoopbackEntry(), L"TestFeedbackLoopMutesLoopback");

    auto const beforeLoop = winrt::clock::now();

    LOG_OUTPUT(L"Starting a feedback loop");
    loop.Start(FeedbackLoopMessageCount);

    auto const muted = WaitForFeedbackMute(associationId, std::chrono::seconds(15));

    std::cout << "Messages echoed: " << loop.EchoCount() << std::endl;

    loop.Stop();

    VERIFY_IS_TRUE(muted, L"the loopback should have muted itself");

    auto const entry = FindActiveLoopbackEntry(associationId);
    VERIFY_IS_NOT_NULL(entry);

    // the same mute a customer applies by hand, so every app sees it the same way
    VERIFY_IS_TRUE(entry.IsMuted());
    VERIFY_IS_TRUE(entry.IsMutedForFeedback());
    VERIFY_IS_TRUE(entry.FeedbackDetectedTime() >= beforeLoop - std::chrono::seconds(5));

    // Unmuting is how the customer says the loop is fixed, and it clears the report.
    auto unmuteResponse = MidiBasicLoopbackManager::UnmuteLoopback(associationId);
    VERIFY_IS_NOT_NULL(unmuteResponse);
    VERIFY_IS_TRUE(unmuteResponse.Success());

    auto const unmutedEntry = FindActiveLoopbackEntry(associationId);
    VERIFY_IS_NOT_NULL(unmutedEntry);
    VERIFY_IS_FALSE(unmutedEntry.IsMuted());
    VERIFY_IS_FALSE(unmutedEntry.IsMutedForFeedback());
    VERIFY_ARE_EQUAL(unmutedEntry.FeedbackProtection(), MidiBasicLoopbackFeedbackProtection::Mute);
}


void MidiBasicLoopbackTests::TestFeedbackLoopWithProtectionOffIsNotMuted()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    if (!FeedbackProtectionAvailableOrSkip())
    {
        return;
    }

    auto response = CreateTestLoopback(L"Test Basic Loopback Feedback Loop Off");
    auto associationId = response.CreatedLoopbackEntry().AssociationId();

    auto cleanupLoopback = wil::scope_exit([&] { RemoveTestLoopback(associationId); });

    auto offResponse = MidiBasicLoopbackManager::SetFeedbackProtection(associationId, MidiBasicLoopbackFeedbackProtection::Off);
    VERIFY_IS_NOT_NULL(offResponse);
    VERIFY_IS_TRUE(offResponse.Success());

    ThruFeedbackLoop loop(response.CreatedLoopbackEntry(), L"TestFeedbackLoopWithProtectionOffIsNotMuted");

    LOG_OUTPUT(L"Starting a feedback loop on a loopback which is not watched");
    loop.Start(FeedbackLoopMessageCount);

    // several times longer than the protection needs to act
    auto const muted = WaitForFeedbackMute(associationId, std::chrono::seconds(5));

    auto const echoed = loop.EchoCount();
    std::cout << "Messages echoed: " << echoed << std::endl;

    loop.Stop();

    VERIFY_IS_FALSE(muted, L"Do nothing means do nothing");

    // and the loop really was running, or not muting it proves nothing
    VERIFY_IS_GREATER_THAN(echoed, (uint64_t)10'000);

    auto const entry = FindActiveLoopbackEntry(associationId);
    VERIFY_IS_NOT_NULL(entry);
    VERIFY_IS_FALSE(entry.IsMuted());
}


// A sender that keeps sending, fast and repetitive, looks like a loop until the pause shows it is
// not. It must never be muted, and the check must not lose or reorder anything.
void MidiBasicLoopbackTests::TestSteadyTrafficIsNotMuted()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    if (!FeedbackProtectionAvailableOrSkip())
    {
        return;
    }

    auto response = CreateTestLoopback(L"Test Basic Loopback Steady Traffic");
    auto associationId = response.CreatedLoopbackEntry().AssociationId();

    auto cleanupLoopback = wil::scope_exit([&] { RemoveTestLoopback(associationId); });

    auto session = MidiSession::Create(L"TestSteadyTrafficIsNotMuted");
    VERIFY_IS_NOT_NULL(session);

    auto connection = session.CreateEndpointConnection(response.CreatedLoopbackEntry().EndpointDeviceId());
    VERIFY_IS_NOT_NULL(connection);

    std::atomic<uint32_t> receivedCount{ 0 };
    std::atomic<uint32_t> outOfOrderCount{ 0 };

    wil::unique_event_nothrow allReceived;
    allReceived.create();

    constexpr uint32_t messageCount{ 30'000 };

    // Sixteen notes over and over, identical each time around. That is what makes it look like a
    // loop, and it also means the order can be checked: message n is always note n % 16.
    auto token = connection.MessageReceived([&](auto&&, MidiMessageReceivedEventArgs const& args)
        {
            uint32_t word0{ 0 };
            uint32_t word1{ 0 };
            uint32_t word2{ 0 };
            uint32_t word3{ 0 };

            if (args.FillWords(word0, word1, word2, word3) != 2)
            {
                return;
            }

            auto const index = receivedCount.fetch_add(1);

            if (((word0 >> 8) & 0x7F) != (index & 0x0F))
            {
                outOfOrderCount++;
            }

            if (index + 1 == messageCount)
            {
                allReceived.SetEvent();
            }
        });

    VERIFY_IS_TRUE(connection.Open());

    // several thousand a second for a few seconds: well past the point where the checks start
    for (uint32_t i = 0; i < messageCount; i++)
    {
        uint32_t const word0{ 0x40900000 | ((i & 0x0F) << 8) };
        uint32_t const word1{ 0x80000000 };

        VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(
            connection.SendSingleMessageWords(MidiClock::TimestampConstantSendImmediately(), word0, word1)));

        if ((i % 100) == 99)
        {
            ::Sleep(10);
        }
    }

    auto const gotEverything = allReceived.wait(15000);

    connection.MessageReceived(token);
    session.DisconnectEndpointConnection(connection.ConnectionId());
    session.Close();

    std::cout << "Received " << receivedCount << " of " << messageCount << ", " << outOfOrderCount << " out of order" << std::endl;

    VERIFY_IS_TRUE(gotEverything, L"a pause to check for feedback must not lose anything");
    VERIFY_ARE_EQUAL(outOfOrderCount.load(), (uint32_t)0);

    auto const entry = FindActiveLoopbackEntry(associationId);
    VERIFY_IS_NOT_NULL(entry);
    VERIFY_IS_FALSE(entry.IsMuted());
    VERIFY_IS_FALSE(entry.IsMutedForFeedback());
}


// ------------------------------------------------------------------------------------
// What is saved in the configuration file, and changing a loopback which already exists
// ------------------------------------------------------------------------------------

namespace
{
    namespace svc = winrt::Windows::Devices::Midi2::ServiceConfig;

    bool ConfigFileRegisteredOrSkip()
    {
        if (svc::MidiServiceTransportPluginConfigManager::ConfigFilePath().empty())
        {
            WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"No configuration file is registered on this PC, so nothing can be saved.");
            return false;
        }

        return true;
    }

    MidiBasicLoopbackSavedEntry FindSavedLoopbackEntry(_In_ winrt::guid const& associationId)
    {
        for (auto const& entry : MidiBasicLoopbackManager::GetSavedLoopbackEntries())
        {
            if (entry != nullptr && entry.AssociationId() == associationId)
            {
                return entry;
            }
        }

        return nullptr;
    }

    void VerifySaved(_In_ svc::MidiServiceConfigSaveResponse const& response, _In_ PCWSTR description)
    {
        VERIFY_IS_TRUE(response != nullptr);

        if (!response.Success())
        {
            std::wcout << L"Save failed: " << static_cast<int>(response.Result()) << L" " << response.ErrorMessage().c_str() << std::endl;
        }

        VERIFY_IS_TRUE(response.Success(), description);
    }

    // for cleanup, so it never throws
    void RemoveSavedLoopback(_In_ winrt::guid const& associationId) noexcept
    {
        try
        {
            svc::MidiServiceTransportPluginConfigManager::SaveUpdate(MidiBasicLoopbackRemovalConfig(associationId));
        }
        catch (...)
        {
        }
    }
}


// Saved without being created, so the running service never sees it
void MidiBasicLoopbackTests::TestSavedLoopbackFollowsSavedChanges()
{
    if (!ConfigFileRegisteredOrSkip())
    {
        return;
    }

    auto const suffix = winrt::to_hstring(MidiClock::Now());

    MidiBasicLoopbackEndpointDefinition definition(L"Test Saved Basic Loopback " + suffix, L"Saved basic loopback", L"SAVEDBASIC" + suffix);

    MidiBasicLoopbackCreationConfig creationConfig(definition);

    auto const associationId = creationConfig.AssociationId();

    VERIFY_IS_TRUE(FindSavedLoopbackEntry(associationId) == nullptr, L"not saved to begin with");

    auto removeEntry = wil::scope_exit([&] { RemoveSavedLoopback(associationId); });

    VerifySaved(svc::MidiServiceTransportPluginConfigManager::SaveUpdate(creationConfig), L"saving the loopback works");

    auto saved = FindSavedLoopbackEntry(associationId);

    VERIFY_IS_TRUE(saved != nullptr, L"it is listed once saved");
    VERIFY_IS_TRUE(saved.EndpointDefinition().Name() == definition.Name(), L"with its name");
    VERIFY_IS_TRUE(saved.EndpointDefinition().Description() == definition.Description(), L"its description");
    VERIFY_IS_TRUE(saved.EndpointDefinition().UniqueId() == definition.UniqueId(), L"and its unique id");
    VERIFY_IS_FALSE(saved.IsMuted());
    VERIFY_ARE_EQUAL(saved.FeedbackProtection(), MidiBasicLoopbackFeedbackProtection::Mute);

    MidiBasicLoopbackUpdateConfig update(associationId);
    update.Name(L"Test Saved Basic Loopback Renamed " + suffix);
    update.IsMuted(true);
    update.FeedbackProtection(MidiBasicLoopbackFeedbackProtection::Off);

    VerifySaved(svc::MidiServiceTransportPluginConfigManager::SaveUpdate(update), L"saving a change to it works");

    saved = FindSavedLoopbackEntry(associationId);

    VERIFY_IS_TRUE(saved != nullptr);
    VERIFY_IS_TRUE(saved.EndpointDefinition().Name() == update.Name(), L"the new name is saved");
    VERIFY_IS_TRUE(saved.EndpointDefinition().Description() == definition.Description(), L"what was not set is left alone");
    VERIFY_IS_TRUE(saved.EndpointDefinition().UniqueId() == definition.UniqueId(), L"including the unique id");
    VERIFY_IS_TRUE(saved.IsMuted());
    VERIFY_ARE_EQUAL(saved.FeedbackProtection(), MidiBasicLoopbackFeedbackProtection::Off);

    VerifySaved(svc::MidiServiceTransportPluginConfigManager::SaveUpdate(MidiBasicLoopbackRemovalConfig(associationId)), L"removing it works");

    VERIFY_IS_TRUE(FindSavedLoopbackEntry(associationId) == nullptr, L"a removed loopback is no longer listed");
}


// Saving a change for an entry that is not there would leave half an entry in the file
void MidiBasicLoopbackTests::TestSavingUpdateForUnsavedLoopbackIsRefused()
{
    if (!ConfigFileRegisteredOrSkip())
    {
        return;
    }

    auto const associationId = winrt::Windows::Foundation::GuidHelper::CreateNewGuid();

    auto removeEntry = wil::scope_exit([&] { RemoveSavedLoopback(associationId); });

    MidiBasicLoopbackUpdateConfig update(associationId);
    update.Name(L"Test Unsaved Basic Loopback");
    update.IsMuted(true);

    auto const response = svc::MidiServiceTransportPluginConfigManager::SaveUpdate(update);

    VERIFY_IS_TRUE(response != nullptr);
    VERIFY_IS_FALSE(response.Success(), L"a change to a loopback which is not saved is not saved");
    VERIFY_IS_TRUE(response.Result() == svc::MidiServiceConfigSaveResult::ErrorEntryNotSaved, L"and says why");

    VERIFY_IS_TRUE(FindSavedLoopbackEntry(associationId) == nullptr, L"and nothing is left in the file");
}


void MidiBasicLoopbackTests::TestUpdateRunningLoopback()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    auto response = CreateTestLoopback(L"Test Basic Loopback Update");
    auto const created = response.CreatedLoopbackEntry();
    auto const associationId = created.AssociationId();

    auto cleanupLoopback = wil::scope_exit([&] { RemoveTestLoopback(associationId); });

    // a transport build without the customization handler can still mute
    bool const canRename = svc::MidiServiceTransportPluginConfigManager::QueryCapability(
        MidiBasicLoopbackManager::TransportId(),
        L"customizeEndpoint");

    auto const newName = created.Name() + L" Renamed";

    MidiBasicLoopbackUpdateConfig update(associationId);
    update.IsMuted(true);

    if (canRename)
    {
        update.Name(newName);
    }
    else
    {
        WEX::Logging::Log::Comment(L"This basic loopback transport cannot rename an endpoint, so only muting is checked.");
    }

    auto const updateResponse = MidiBasicLoopbackManager::UpdateLoopback(update);

    VERIFY_IS_NOT_NULL(updateResponse);

    if (!updateResponse.Success())
    {
        std::wcout << L"Error Message: " << updateResponse.ErrorMessage().c_str() << std::endl;
    }

    VERIFY_IS_TRUE(updateResponse.Success());

    auto entry = FindActiveLoopbackEntry(associationId);

    VERIFY_IS_NOT_NULL(entry);
    VERIFY_IS_TRUE(entry.IsMuted(), L"the loopback is muted");

    if (canRename)
    {
        VERIFY_IS_TRUE(entry.Name() == newName, L"it has its new name");
        VERIFY_IS_TRUE(entry.Description() == created.Description(), L"and keeps its description");
    }

    // the same kind of change through the generic path goes to the manager, not the transport
    MidiBasicLoopbackUpdateConfig unmute(associationId);
    unmute.IsMuted(false);

    auto const sendResponse = svc::MidiServiceTransportPluginConfigManager::SendUpdate(unmute);

    VERIFY_IS_NOT_NULL(sendResponse);
    VERIFY_IS_TRUE(sendResponse.Status() == svc::MidiServiceConfigResponseStatus::Success, L"SendUpdate applies an update config");

    entry = FindActiveLoopbackEntry(associationId);

    VERIFY_IS_NOT_NULL(entry);
    VERIFY_IS_FALSE(entry.IsMuted(), L"the loopback is unmuted");

    // a loopback which is not running is reported, rather than the change being lost quietly
    MidiBasicLoopbackUpdateConfig missing(winrt::Windows::Foundation::GuidHelper::CreateNewGuid());
    missing.Name(L"Test Basic Loopback Nobody");

    auto const missingResponse = MidiBasicLoopbackManager::UpdateLoopback(missing);

    VERIFY_IS_NOT_NULL(missingResponse);
    VERIFY_IS_FALSE(missingResponse.Success());
    VERIFY_ARE_EQUAL(missingResponse.ErrorCode(), MidiBasicLoopbackErrorCode::EndpointNotFound);
}



