// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

#include <winrt/Windows.Data.Json.h>
// ConfigJson() is declared on IMidiServiceTransportPluginConfig with a deferred auto return type
#include <winrt/Windows.Devices.Midi2.ServiceConfig.h>
#include <mmsystem.h>
#include <vector>
#include <atomic>
#include <chrono>

#pragma comment(lib, "winmm.lib")


#include <io.h>
#include <fcntl.h>

// Looks up an active loopback entry by association id. Returns nullptr if not found.
static MidiLoopbackEntry FindActiveLoopbackEntry(winrt::guid const& associationId)
{
    auto entries = MidiLoopbackManager::GetActiveLoopbackEntries();

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

// Creates a transient A/B loopback with unique ids, and verifies the response.
static MidiLoopbackCreationResponse CreateTestLoopback(_In_ winrt::hstring const& namePrefix)
{
    auto uniqueId = L"ID" + winrt::to_hstring(MidiClock::Now()) + winrt::to_hstring(rand());

    MidiLoopbackEndpointDefinition definitionA(
        namePrefix + L" A",
        L"A-side loopback created by the Windows MIDI Services TAEF tests.",
        uniqueId + L"-A"
    );

    MidiLoopbackEndpointDefinition definitionB(
        namePrefix + L" B",
        L"B-side loopback created by the Windows MIDI Services TAEF tests.",
        uniqueId + L"-B"
    );

    MidiLoopbackCreationConfig creationConfig(definitionA, definitionB);

    auto response = MidiLoopbackManager::CreateTransientLoopback(creationConfig);

    VERIFY_IS_NOT_NULL(response);

    if (!response.Success())
    {
        std::wcout << L"Error Code:    " << std::hex << static_cast<uint32_t>(response.ErrorCode()) << std::dec << std::endl;
        std::wcout << L"Error Message: " << response.ErrorMessage().c_str() << std::endl;
    }

    VERIFY_IS_TRUE(response.Success());
    VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry());
    VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry().EndpointA());
    VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry().EndpointB());
    VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId().empty());
    VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId().empty());

    return response;
}

static void RemoveTestLoopback(winrt::guid const& associationId)
{
    MidiLoopbackRemovalConfig removalConfig(associationId);
    auto removalResponse = MidiLoopbackManager::RemoveTransientLoopback(removalConfig);

    VERIFY_IS_NOT_NULL(removalResponse);
    VERIFY_IS_TRUE(removalResponse.Success());
}


// Every loopback created here leaves a deactivated software device node behind for each of its
// UMP endpoints and each of their MIDI 1.0 ports, because the service never deletes one. These
// two hooks remove exactly the nodes this test method caused to be created.
bool MidiLoopbackEndpointTests::TestSetup()
{
    m_deviceNodeTracker.Start();

    return true;
}

bool MidiLoopbackEndpointTests::TestCleanup()
{
    m_deviceNodeTracker.RemoveDeviceNodesCreatedSinceStart();

    return true;
}


void MidiLoopbackEndpointTests::TestUnicodeGtbAndDeviceNames()
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
    auto nameA = L"我的虚拟设备";
    auto nameB = L"ענדפוינט ב";

    MidiLoopbackEndpointDefinition definitionA;
    definitionA.Name(nameA);
    definitionA.UniqueId(uniqueId);

    MidiLoopbackEndpointDefinition definitionB;
    definitionB.Name(nameB);
    definitionB.UniqueId(uniqueId);

    MidiLoopbackCreationConfig config(definitionA, definitionB);

    VERIFY_IS_FALSE(config.EndpointDefinitionA().Name().empty());
    VERIFY_IS_FALSE(config.EndpointDefinitionB().Name().empty());


    auto result = MidiLoopbackManager::CreateTransientLoopback(config);

    VERIFY_IS_NOT_NULL(result);
    VERIFY_IS_TRUE(result.Success());

    auto associationId = result.CreatedLoopbackEntry().AssociationId();

    // remove the loopback even if a VERIFY macro below halts the method
    auto cleanupLoopback = wil::scope_exit([&]
        {
            MidiLoopbackRemovalConfig removalConfig(associationId);
            MidiLoopbackManager::RemoveTransientLoopback(removalConfig);
        });

    auto endpointDeviceIdA = result.CreatedLoopbackEntry().EndpointA().EndpointDeviceId();
    auto endpointDeviceIdB = result.CreatedLoopbackEntry().EndpointB().EndpointDeviceId();

    VERIFY_IS_FALSE(config.EndpointDefinitionA().Name().empty());
    VERIFY_IS_FALSE(config.EndpointDefinitionB().Name().empty());

    auto endpointInformationA = MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(endpointDeviceIdA);
    auto endpointInformationB = MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(endpointDeviceIdB);

    VERIFY_IS_NOT_NULL(endpointInformationA);
    VERIFY_IS_NOT_NULL(endpointInformationB);

    std::wcout << L"Endpoint A Name: " << endpointInformationA.Name().c_str() << std::endl;
    std::wcout << L"Endpoint B Name: " << endpointInformationB.Name().c_str() << std::endl;

    // Check names

    std::wcout << L"Sent Device Name A Char Codes: " << std::endl;
    for (wchar_t ch : definitionA.Name())
    {
        std::wcout << std::hex << std::setw(4) << (int)ch << ", ";
    }
    std::wcout << std::endl;

    std::wcout << L"Sent Device Name B Char Codes: " << std::endl;
    for (wchar_t ch : definitionB.Name())
    {
        std::wcout << std::hex << std::setw(4) << (int)ch << ", ";
    }
    std::wcout << std::endl;



    std::wcout << L"Received Device Name A Char Codes: " << std::endl;
    for (wchar_t ch : endpointInformationA.Name())
    {
        std::wcout << std::hex << std::setw(4) << (int)ch << ", ";
    }
    std::wcout << std::endl;

    std::wcout << L"Received Device Name B Char Codes: " << std::endl;
    for (wchar_t ch : endpointInformationB.Name())
    {
        std::wcout << std::hex << std::setw(4) << (int)ch << ", ";
    }
    std::wcout << std::endl;



    auto nameAResult = wcscmp(endpointInformationA.Name().c_str(), definitionA.Name().c_str());
    VERIFY_IS_TRUE(nameAResult == 0);

    auto nameBResult = wcscmp(endpointInformationB.Name().c_str(), definitionB.Name().c_str());
    VERIFY_IS_TRUE(nameBResult == 0);


    // Check group terminal blocks.

    std::wcout << L"Received GTB A Char Codes for first block: " << std::endl;
    for (wchar_t ch : endpointInformationA.GetGroupTerminalBlocks().GetAt(0).Name())
    {
        std::wcout << std::hex << std::setw(4) << (int)ch << ", ";
    }
    std::wcout << std::endl;

    std::wcout << L"Received GTB A Char Codes for second block: " << std::endl;
    for (wchar_t ch : endpointInformationA.GetGroupTerminalBlocks().GetAt(1).Name())
    {
        std::wcout << std::hex << std::setw(4) << (int)ch << ", ";
    }
    std::wcout << std::endl;

    std::wcout << L"Received GTB B Char Codes for first block: " << std::endl;
    for (wchar_t ch : endpointInformationB.GetGroupTerminalBlocks().GetAt(0).Name())
    {
        std::wcout << std::hex << std::setw(4) << (int)ch << ", ";
    }
    std::wcout << std::endl;

    std::wcout << L"Received GTB B Char Codes for second block: " << std::endl;
    for (wchar_t ch : endpointInformationB.GetGroupTerminalBlocks().GetAt(1).Name())
    {
        std::wcout << std::hex << std::setw(4) << (int)ch << ", ";
    }
    std::wcout << std::endl;

    auto gtbNameA0Result = wcscmp(endpointInformationA.GetGroupTerminalBlocks().GetAt(0).Name().c_str(), definitionA.Name().c_str());
    VERIFY_IS_TRUE(gtbNameA0Result == 0);

    auto gtbNameA1Result = wcscmp(endpointInformationA.GetGroupTerminalBlocks().GetAt(1).Name().c_str(), definitionA.Name().c_str());
    VERIFY_IS_TRUE(gtbNameA1Result == 0);

    auto gtbNameB0Result = wcscmp(endpointInformationB.GetGroupTerminalBlocks().GetAt(0).Name().c_str(), definitionB.Name().c_str());
    VERIFY_IS_TRUE(gtbNameB0Result == 0);

    auto gtbNameB1Result = wcscmp(endpointInformationB.GetGroupTerminalBlocks().GetAt(1).Name().c_str(), definitionB.Name().c_str());
    VERIFY_IS_TRUE(gtbNameB1Result == 0);

    // test that we can find a MIDI 1 device with this name. 

    auto foundAPorts = MidiLegacyPortDeviceInformation::FindAllForName(definitionA.Name());
    VERIFY_IS_TRUE(foundAPorts.Size() > 0);
    std::wcout << L"Found Port Name: " << foundAPorts.GetAt(0).Name().c_str() << std::endl;

    auto foundBPorts = MidiLegacyPortDeviceInformation::FindAllForName(definitionB.Name());
    VERIFY_IS_TRUE(foundBPorts.Size() > 0);
    std::wcout << L"Found Port Name: " << foundBPorts.GetAt(0).Name().c_str() << std::endl;

}




// The specification's endpoint name limit is a UTF-8 byte count, not a character count, so a
// name well inside the character limit can still be over the byte limit once encoded.
void MidiLoopbackEndpointTests::TestOverlongUnicodeDeviceNameIsTruncatedOnCharacterBoundary()
{
    const size_t maxByteCount = 98;

    auto utf8ByteCount = [](std::wstring const& s) -> size_t
        {
            if (s.empty()) return 0;
            auto count = ::WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.length(), nullptr, 0, nullptr, nullptr);
            return count > 0 ? (size_t)count : 0;
        };

    // 40 CJK characters: 40 UTF-16 code units, but 120 UTF-8 bytes. 32 characters (96 bytes) is
    // the most that fits, because a 33rd would land on byte 99.
    std::wstring longName{};
    for (int i = 0; i < 40; i++)
    {
        longName += L"設";
    }

    VERIFY_ARE_EQUAL(longName.length(), (size_t)40);
    VERIFY_ARE_EQUAL(utf8ByteCount(longName), (size_t)120);

    // Both sides carry an overlong name, but they have to differ from each other because a
    // loopback endpoint name has to be unique within the transport.
    std::wstring longNameB{};
    for (int i = 0; i < 40; i++)
    {
        longNameB += L"定";
    }

    winrt::hstring uniqueId = winrt::to_hstring(winrt::Windows::Foundation::GuidHelper::CreateNewGuid());

    MidiLoopbackEndpointDefinition definitionA;
    definitionA.Name(longName);
    definitionA.UniqueId(uniqueId);

    MidiLoopbackEndpointDefinition definitionB;
    definitionB.Name(longNameB);
    definitionB.UniqueId(uniqueId);

    MidiLoopbackCreationConfig config(definitionA, definitionB);

    auto result = MidiLoopbackManager::CreateTransientLoopback(config);

    VERIFY_IS_NOT_NULL(result);
    VERIFY_IS_TRUE(result.Success());

    auto associationId = result.CreatedLoopbackEntry().AssociationId();

    auto cleanupLoopback = wil::scope_exit([&]
        {
            MidiLoopbackRemovalConfig removalConfig(associationId);
            MidiLoopbackManager::RemoveTransientLoopback(removalConfig);
        });

    auto endpointInformationA = MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(
        result.CreatedLoopbackEntry().EndpointA().EndpointDeviceId());

    VERIFY_IS_NOT_NULL(endpointInformationA);

    std::wstring returnedName{ endpointInformationA.Name() };

    std::cout << "Original: 40 chars, " << utf8ByteCount(longName) << " UTF-8 bytes" << std::endl;
    std::cout << "Returned: " << returnedName.length() << " chars, " << utf8ByteCount(returnedName) << " UTF-8 bytes" << std::endl;

    // within the limit, and a whole number of characters rather than a split one
    VERIFY_IS_LESS_THAN_OR_EQUAL(utf8ByteCount(returnedName), maxByteCount);
    VERIFY_ARE_EQUAL(returnedName.length(), (size_t)32);
    VERIFY_ARE_EQUAL(utf8ByteCount(returnedName), (size_t)96);

    // and it is a prefix of what was submitted, not a mangled string
    VERIFY_ARE_EQUAL(longName.compare(0, returnedName.length(), returnedName), 0);
}




// ============================================================================
// Repro for GH1070 support code and the A/B loopback tests
// ============================================================================


void MidiLoopbackEndpointTests::TestMuteLoopback()
{
    // Once a loopback is muted, messages sent from the A-side must no longer
    // arrive at the B-side.

    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    auto response = CreateTestLoopback(L"Test Loopback Mute");

    auto associationId = response.CreatedLoopbackEntry().AssociationId();
    auto endpointAId = response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId();
    auto endpointBId = response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId();

    // a newly created loopback must not be muted
    VERIFY_IS_FALSE(response.CreatedLoopbackEntry().IsMuted());

    auto cleanupLoopback = wil::scope_exit([&] { RemoveTestLoopback(associationId); });

    LOG_OUTPUT(L"Creating session and connections");

    auto session = MidiSession::Create(L"TestMuteLoopback");
    VERIFY_IS_NOT_NULL(session);

    auto connectionA = session.CreateEndpointConnection(endpointAId);
    VERIFY_IS_NOT_NULL(connectionA);

    auto connectionB = session.CreateEndpointConnection(endpointBId);
    VERIFY_IS_NOT_NULL(connectionB);

    wil::unique_event_nothrow messageReceived;
    messageReceived.create();

    std::atomic<uint32_t> receivedMessageCount{ 0 };

    auto eventToken = connectionB.MessageReceived([&](auto&&, MidiMessageReceivedEventArgs const& args)
        {
            VERIFY_IS_NOT_NULL(args);

            std::cout << "B received message 0x" << std::hex << args.PeekFirstWord() << std::dec << std::endl;

            receivedMessageCount++;
            messageReceived.SetEvent();
        });

    VERIFY_IS_TRUE(connectionA.Open());
    VERIFY_IS_TRUE(connectionB.Open());

    MidiMessage64 message(MidiClock::TimestampConstantSendImmediately(), 0x43001627, 0x86753090);

    // Baseline: while unmuted, the message must arrive at B
    LOG_OUTPUT(L"Sending message while unmuted");
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connectionA.SendSingleMessagePacket(message)));

    VERIFY_IS_TRUE(messageReceived.wait(5000));
    VERIFY_ARE_EQUAL(receivedMessageCount.load(), (uint32_t)1);

    // Mute the loopback
    LOG_OUTPUT(L"Muting the loopback");
    auto muteResponse = MidiLoopbackManager::MuteLoopback(associationId);
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

    // Send while muted. Nothing should arrive at B.
    messageReceived.ResetEvent();
    receivedMessageCount = 0;

    LOG_OUTPUT(L"Sending message while muted");
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connectionA.SendSingleMessagePacket(message)));

    // wait long enough that a message would have arrived had it not been muted
    VERIFY_IS_FALSE(messageReceived.wait(2000));
    VERIFY_ARE_EQUAL(receivedMessageCount.load(), (uint32_t)0);

    connectionB.MessageReceived(eventToken);
    session.DisconnectEndpointConnection(connectionA.ConnectionId());
    session.DisconnectEndpointConnection(connectionB.ConnectionId());
    session.Close();
}


void MidiLoopbackEndpointTests::TestUnmuteAfterMute()
{
    // After unmuting a previously muted loopback, messages must flow again.

    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    auto response = CreateTestLoopback(L"Test Loopback Unmute");
    VERIFY_IS_TRUE(response.Success());

    auto associationId = response.CreatedLoopbackEntry().AssociationId();
    auto endpointAId = response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId();
    auto endpointBId = response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId();

    auto cleanupLoopback = wil::scope_exit([&] { RemoveTestLoopback(associationId); });

    auto session = MidiSession::Create(L"TestUnmuteAfterMute");
    VERIFY_IS_NOT_NULL(session);

    auto connectionA = session.CreateEndpointConnection(endpointAId);
    VERIFY_IS_NOT_NULL(connectionA);

    auto connectionB = session.CreateEndpointConnection(endpointBId);
    VERIFY_IS_NOT_NULL(connectionB);

    wil::unique_event_nothrow messageReceived;
    messageReceived.create();

    std::atomic<uint32_t> receivedMessageCount{ 0 };

    auto eventToken = connectionB.MessageReceived([&](auto&&, MidiMessageReceivedEventArgs const& args)
        {
            VERIFY_IS_NOT_NULL(args);

            std::cout << "B received message 0x" << std::hex << args.PeekFirstWord() << std::dec << std::endl;

            receivedMessageCount++;
            messageReceived.SetEvent();
        });

    VERIFY_IS_TRUE(connectionA.Open());
    VERIFY_IS_TRUE(connectionB.Open());

    MidiMessage64 message(MidiClock::TimestampConstantSendImmediately(), 0x43001627, 0x86753090);

    // Mute first, and confirm the messages are actually blocked
    LOG_OUTPUT(L"Muting the loopback");
    auto muteResponse = MidiLoopbackManager::MuteLoopback(associationId);
    VERIFY_IS_NOT_NULL(muteResponse);
    VERIFY_IS_TRUE(muteResponse.Success());

    LOG_OUTPUT(L"Sending message while muted");
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connectionA.SendSingleMessagePacket(message)));

    VERIFY_IS_FALSE(messageReceived.wait(2000));
    VERIFY_ARE_EQUAL(receivedMessageCount.load(), (uint32_t)0);

    // Now unmute
    LOG_OUTPUT(L"Unmuting the loopback");
    auto unmuteResponse = MidiLoopbackManager::UnmuteLoopback(associationId);
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
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connectionA.SendSingleMessagePacket(message)));

    VERIFY_IS_TRUE(messageReceived.wait(5000));
    VERIFY_ARE_EQUAL(receivedMessageCount.load(), (uint32_t)1);

    connectionB.MessageReceived(eventToken);
    session.DisconnectEndpointConnection(connectionA.ConnectionId());
    session.DisconnectEndpointConnection(connectionB.ConnectionId());
    session.Close();
}


void MidiLoopbackEndpointTests::TestListActiveLoopbacks()
{
    // Creating multiple loopbacks must result in all of them being reported by
    // GetActiveLoopbackEntries, and removing them must take them back out of the list.

    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    auto countBefore = MidiLoopbackManager::GetActiveLoopbackEntries().Size();

    LOG_OUTPUT(L"Creating first loopback");
    auto response1 = CreateTestLoopback(L"Test Loopback List 1");
    auto associationId1 = response1.CreatedLoopbackEntry().AssociationId();
    auto endpointA1Id = response1.CreatedLoopbackEntry().EndpointA().EndpointDeviceId();
    auto endpointB1Id = response1.CreatedLoopbackEntry().EndpointB().EndpointDeviceId();

    auto cleanup1 = wil::scope_exit([&] { RemoveTestLoopback(associationId1); });

    LOG_OUTPUT(L"Creating second loopback");
    auto response2 = CreateTestLoopback(L"Test Loopback List 2");
    auto associationId2 = response2.CreatedLoopbackEntry().AssociationId();
    auto endpointA2Id = response2.CreatedLoopbackEntry().EndpointA().EndpointDeviceId();

    auto cleanup2 = wil::scope_exit([&] { RemoveTestLoopback(associationId2); });

    // the two loopbacks must be distinct, as must the A and B sides of each
    VERIFY_IS_FALSE(associationId1 == associationId2);
    VERIFY_IS_FALSE(HStringsAreCaseInsensitiveEqual(endpointA1Id, endpointB1Id));
    VERIFY_IS_FALSE(HStringsAreCaseInsensitiveEqual(endpointA1Id, endpointA2Id));

    auto entries = MidiLoopbackManager::GetActiveLoopbackEntries();
    VERIFY_IS_NOT_NULL(entries);

    std::cout << "Active loopback entries: " << entries.Size() << std::endl;

    for (auto const& entry : entries)
    {
        std::cout
            << " - A: " << winrt::to_string(entry.EndpointA().EndpointDeviceId())
            << " / B: " << winrt::to_string(entry.EndpointB().EndpointDeviceId())
            << std::endl;
    }

    // both of the loopbacks we created must be present, with both sides intact
    auto entry1 = FindActiveLoopbackEntry(associationId1);
    VERIFY_IS_NOT_NULL(entry1);
    VERIFY_IS_TRUE(HStringsAreCaseInsensitiveEqual(entry1.EndpointA().EndpointDeviceId(), endpointA1Id));
    VERIFY_IS_TRUE(HStringsAreCaseInsensitiveEqual(entry1.EndpointB().EndpointDeviceId(), endpointB1Id));

    auto entry2 = FindActiveLoopbackEntry(associationId2);
    VERIFY_IS_NOT_NULL(entry2);
    VERIFY_IS_TRUE(HStringsAreCaseInsensitiveEqual(entry2.EndpointA().EndpointDeviceId(), endpointA2Id));

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
    VERIFY_ARE_EQUAL(MidiLoopbackManager::GetActiveLoopbackEntries().Size(), countBefore);
}


void MidiLoopbackEndpointTests::TestReopenLegacyWinMMPorts()
{
    // Regression test for issue GH1070, for the A/B loopback. Unlike the basic
    // loopback, which has a single endpoint with two WinMM ports, an A/B loopback
    // has two endpoints and therefore four WinMM ports: a source and a destination
    // for each of the A and B sides. All four must be re-openable after being closed.

    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    // Start a legacy port device watcher so the ports are being observed while the
    // loopback is created, matching how a real MIDI 1.0 application behaves.
    LOG_OUTPUT(L"Creating and starting the legacy port device watcher");

    auto watcher = MidiLegacyPortDeviceWatcher::Create();
    VERIFY_IS_NOT_NULL(watcher);

    std::atomic<uint32_t> watcherAddedCount{ 0 };

    auto addedToken = watcher.Added([&](auto const& source, MidiLegacyPortDeviceInformationAddedEventArgs const& args)
        {
            VERIFY_IS_NOT_NULL(source);
            VERIFY_IS_NOT_NULL(args);

            watcherAddedCount++;
        });

    auto cleanupWatcher = wil::scope_exit([&]
        {
            if (watcher == nullptr) return;

            watcher.Stop();

            if (addedToken) watcher.Added(addedToken);
        });

    watcher.Start();

    LOG_OUTPUT(L"Creating loopback");
    auto response = CreateTestLoopback(L"Test Loopback WinMM Reopen");

    auto associationId = response.CreatedLoopbackEntry().AssociationId();
    auto endpointAId = response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId();
    auto endpointBId = response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId();

    auto cleanupLoopback = wil::scope_exit([&] { RemoveTestLoopback(associationId); });

    // Resolve the four WinMM port numbers: a source and a destination for each of
    // the A and B sides.
    //
    // These are resolved by querying the ports directly rather than by waiting on the
    // watcher's Added events. The watcher does not reliably raise Added for every port
    // when several are created at once, and the port number itself is assigned
    // asynchronously, so a direct query is the authoritative source for this test.
    uint32_t sourcePortNumberA{ 0 };
    uint32_t destinationPortNumberA{ 0 };
    uint32_t sourcePortNumberB{ 0 };
    uint32_t destinationPortNumberB{ 0 };

    LOG_OUTPUT(L"Resolving the four legacy WinMM port numbers");

    VERIFY_IS_TRUE(TryResolveWinMMPortNumber(endpointAId, Midi1PortFlow::MidiMessageSource, sourcePortNumberA));
    VERIFY_IS_TRUE(TryResolveWinMMPortNumber(endpointAId, Midi1PortFlow::MidiMessageDestination, destinationPortNumberA));
    VERIFY_IS_TRUE(TryResolveWinMMPortNumber(endpointBId, Midi1PortFlow::MidiMessageSource, sourcePortNumberB));
    VERIFY_IS_TRUE(TryResolveWinMMPortNumber(endpointBId, Midi1PortFlow::MidiMessageDestination, destinationPortNumberB));

    // the A and B sides must have distinct ports within each flow
    VERIFY_ARE_NOT_EQUAL(sourcePortNumberA, sourcePortNumberB);
    VERIFY_ARE_NOT_EQUAL(destinationPortNumberA, destinationPortNumberB);

    LOG_OUTPUT(WEX::Common::String().Format(
        L"A: source %u / destination %u,  B: source %u / destination %u",
        sourcePortNumberA, destinationPortNumberA, sourcePortNumberB, destinationPortNumberB));

    // Open and close all four WinMM ports several times in a row, verifying that
    // they can be re-opened each time.
    const int iterations = 4;

    for (int i = 0; i < iterations; i++)
    {
        LOG_OUTPUT(WEX::Common::String().Format(L"WinMM open/close iteration %d of %d", i + 1, iterations));

        HMIDIIN hMidiInA{ nullptr };
        HMIDIIN hMidiInB{ nullptr };
        HMIDIOUT hMidiOutA{ nullptr };
        HMIDIOUT hMidiOutB{ nullptr };

        auto inResultA = midiInOpen(&hMidiInA, sourcePortNumberA, 0, 0, CALLBACK_NULL);
        VERIFY_ARE_EQUAL(inResultA, static_cast<MMRESULT>(MMSYSERR_NOERROR));

        auto inResultB = midiInOpen(&hMidiInB, sourcePortNumberB, 0, 0, CALLBACK_NULL);
        VERIFY_ARE_EQUAL(inResultB, static_cast<MMRESULT>(MMSYSERR_NOERROR));

        auto outResultA = midiOutOpen(&hMidiOutA, destinationPortNumberA, 0, 0, CALLBACK_NULL);
        VERIFY_ARE_EQUAL(outResultA, static_cast<MMRESULT>(MMSYSERR_NOERROR));

        auto outResultB = midiOutOpen(&hMidiOutB, destinationPortNumberB, 0, 0, CALLBACK_NULL);
        VERIFY_ARE_EQUAL(outResultB, static_cast<MMRESULT>(MMSYSERR_NOERROR));

        // close all four ports
        if (inResultA == MMSYSERR_NOERROR)
        {
            VERIFY_ARE_EQUAL(midiInClose(hMidiInA), static_cast<MMRESULT>(MMSYSERR_NOERROR));
        }

        if (inResultB == MMSYSERR_NOERROR)
        {
            VERIFY_ARE_EQUAL(midiInClose(hMidiInB), static_cast<MMRESULT>(MMSYSERR_NOERROR));
        }

        if (outResultA == MMSYSERR_NOERROR)
        {
            VERIFY_ARE_EQUAL(midiOutClose(hMidiOutA), static_cast<MMRESULT>(MMSYSERR_NOERROR));
        }

        if (outResultB == MMSYSERR_NOERROR)
        {
            VERIFY_ARE_EQUAL(midiOutClose(hMidiOutB), static_cast<MMRESULT>(MMSYSERR_NOERROR));
        }

        // give the driver a moment before attempting to re-open
        Sleep(100);
    }
}


void MidiLoopbackEndpointTests::TestCreateLoopbackWithGarbageUniqueIds()
{
    // Unique ids containing spaces, symbols, and punctuation must still result in a
    // successfully created loopback, because MidiLoopbackManager strips the invalid
    // characters from both the A-side and B-side definitions before submitting the
    // config to the service.

    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    auto validPrefix = L"ID" + winrt::to_hstring(MidiClock::Now());

    // deliberately different garbage ids for the A and B sides
    auto garbageUniqueIdA = MakeGarbageUniqueId(validPrefix.c_str() + std::wstring(L"-A"));
    auto garbageUniqueIdB = MakeGarbageUniqueId(validPrefix.c_str() + std::wstring(L"-B"));

    auto expectedUniqueIdA = ExpectedCleanedUniqueId(garbageUniqueIdA);
    auto expectedUniqueIdB = ExpectedCleanedUniqueId(garbageUniqueIdB);

    // sanity check the test data itself: the garbage ids must actually be dirty, and
    // must still contain something valid once cleaned
    VERIFY_IS_FALSE(UniqueIdContainsOnlyValidCharacters(garbageUniqueIdA));
    VERIFY_IS_FALSE(UniqueIdContainsOnlyValidCharacters(garbageUniqueIdB));
    VERIFY_IS_FALSE(expectedUniqueIdA.empty());
    VERIFY_IS_FALSE(expectedUniqueIdB.empty());

    std::wcout << L"Supplied unique id A: " << garbageUniqueIdA << std::endl;
    std::wcout << L"Expected cleaned unique id A: " << expectedUniqueIdA << std::endl;
    std::wcout << L"Supplied unique id B: " << garbageUniqueIdB << std::endl;
    std::wcout << L"Expected cleaned unique id B: " << expectedUniqueIdB << std::endl;

    // A-side of the loopback
    MidiLoopbackEndpointDefinition definitionA(
        L"Test Loopback A Garbage Id",
        L"A-side created with a unique id which contains invalid characters.",
        winrt::hstring{ garbageUniqueIdA }
        );

    // B-side of the loopback
    MidiLoopbackEndpointDefinition definitionB(
        L"Test Loopback B Garbage Id",
        L"B-side created with a unique id which contains invalid characters.",
        winrt::hstring{ garbageUniqueIdB }
        );

    winrt::guid associationId = foundation::GuidHelper::CreateNewGuid();

    LOG_OUTPUT(L"Creating loopback endpoint creation config");

    MidiLoopbackCreationConfig creationConfig(definitionA, definitionB);

    LOG_OUTPUT(L"Creating loopbacks");

    auto response = MidiLoopbackManager::CreateTransientLoopback(creationConfig);
    VERIFY_IS_NOT_NULL(response);

    if (response.Success())
    {
        LOG_OUTPUT(L"Endpoints created successfully");

        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry());
        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry().EndpointA());
        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry().EndpointB());
        VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId().empty());
        VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId().empty());

        // the manager cleans both unique ids in the config before submitting it
        std::wstring actualUniqueIdA{ creationConfig.EndpointDefinitionA().UniqueId().c_str() };
        std::wstring actualUniqueIdB{ creationConfig.EndpointDefinitionB().UniqueId().c_str() };

        std::wcout << L"Actual unique id A after creation: " << actualUniqueIdA << std::endl;
        std::wcout << L"Actual unique id B after creation: " << actualUniqueIdB << std::endl;

        VERIFY_IS_TRUE(UniqueIdContainsOnlyValidCharacters(actualUniqueIdA));
        VERIFY_IS_TRUE(actualUniqueIdA == expectedUniqueIdA);

        VERIFY_IS_TRUE(UniqueIdContainsOnlyValidCharacters(actualUniqueIdB));
        VERIFY_IS_TRUE(actualUniqueIdB == expectedUniqueIdB);

        // Give a hoot. Don't pollute.
        MidiLoopbackRemovalConfig removalConfig(response.CreatedLoopbackEntry().AssociationId());
        auto removalResponse = MidiLoopbackManager::RemoveTransientLoopback(removalConfig);

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


void MidiLoopbackEndpointTests::TestCreateLoopbackWithoutUniqueIdsGeneratesThem()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    // no unique ids supplied, so the config has to produce them. A caller should not have to
    // invent a random string just to create a loopback.
    MidiLoopbackEndpointDefinition definitionA(
        L"Test Loopback Generated Id A",
        L"A-side created without supplying a unique id."
        );

    MidiLoopbackEndpointDefinition definitionB(
        L"Test Loopback Generated Id B",
        L"B-side created without supplying a unique id."
        );

    VERIFY_IS_TRUE(definitionA.UniqueId().empty());
    VERIFY_IS_TRUE(definitionB.UniqueId().empty());

    MidiLoopbackCreationConfig config(definitionA, definitionB);

    std::wstring generatedIdA{ config.EndpointDefinitionA().UniqueId().c_str() };
    std::wstring generatedIdB{ config.EndpointDefinitionB().UniqueId().c_str() };

    std::wcout << L"Generated unique id A: " << generatedIdA << std::endl;
    std::wcout << L"Generated unique id B: " << generatedIdB << std::endl;

    VERIFY_IS_FALSE(generatedIdA.empty());
    VERIFY_IS_FALSE(generatedIdB.empty());
    VERIFY_IS_TRUE(UniqueIdContainsOnlyValidCharacters(generatedIdA));
    VERIFY_IS_TRUE(UniqueIdContainsOnlyValidCharacters(generatedIdB));

    auto response = MidiLoopbackManager::CreateTransientLoopback(config);
    VERIFY_IS_NOT_NULL(response);
    VERIFY_IS_TRUE(response.Success());

    auto cleanupLoopback = wil::scope_exit([&]
        {
            MidiLoopbackRemovalConfig removalConfig(response.CreatedLoopbackEntry().AssociationId());
            MidiLoopbackManager::RemoveTransientLoopback(removalConfig);
        });

    VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId().empty());
    VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId().empty());

    // the two endpoints still have to be distinct even though the ids were generated
    VERIFY_ARE_NOT_EQUAL(
        response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId(),
        response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId());

    // and a second config must not collide with the first
    MidiLoopbackEndpointDefinition otherA(L"Test Loopback Generated Id A2");
    MidiLoopbackEndpointDefinition otherB(L"Test Loopback Generated Id B2");
    MidiLoopbackCreationConfig otherConfig(otherA, otherB);

    VERIFY_IS_FALSE(otherConfig.EndpointDefinitionA().UniqueId().empty());
    VERIFY_ARE_NOT_EQUAL(otherConfig.EndpointDefinitionA().UniqueId(), config.EndpointDefinitionA().UniqueId());
    VERIFY_ARE_NOT_EQUAL(otherConfig.AssociationId(), config.AssociationId());
}


void MidiLoopbackEndpointTests::TestCreateLoopback()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    winrt::hstring endpointAId{};
    winrt::hstring endpointBId{};

    auto uniqueId = L"ID" + winrt::to_hstring(MidiClock::Now());

    // A-side of the loopback
    MidiLoopbackEndpointDefinition definitionA(
        L"Test Loopback A", // name
        L"The first description is optional, but is displayed to users. This becomes the transport-defined description.", // description
        uniqueId // unique Id that identifies the loopback
        );

    // B-side of the loopback
    MidiLoopbackEndpointDefinition definitionB(
        L"Test Loopback B",
        L"The second description is optional, but is displayed to users. This becomes the transport-defined description.",
        uniqueId // can be the same as the first one, but doesn't need to be.
        );

    LOG_OUTPUT(L"Creating loopback endpoint creation config");

    MidiLoopbackCreationConfig creationConfig(definitionA, definitionB);

    LOG_OUTPUT(L"Creating loopbacks");

    auto response = MidiLoopbackManager::CreateTransientLoopback(creationConfig);
    VERIFY_IS_NOT_NULL(response);

    if (response.Success())
    {
        LOG_OUTPUT(L"Endpoints created successfully");

        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry());
        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry().EndpointA());
        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry().EndpointB());
        VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId().empty());
        VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId().empty());

        endpointAId = response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId();
        endpointBId = response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId();


        std::cout
            << "Loopback Endpoint A: " << std::endl
            << " - " << winrt::to_string(endpointAId)
            << " - " << winrt::to_string(response.CreatedLoopbackEntry().EndpointA().Name())
            << std::endl << std::endl;

        std::cout
            << "Loopback Endpoint B: " << std::endl
            << " - " << winrt::to_string(endpointBId)
            << " - " << winrt::to_string(response.CreatedLoopbackEntry().EndpointB().Name())
            << std::endl << std::endl;


        // Give a hoot. Don't pollute.
        MidiLoopbackRemovalConfig removalConfig(response.CreatedLoopbackEntry().AssociationId());
        auto removalResponse = MidiLoopbackManager::RemoveTransientLoopback(removalConfig);

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

// The service reads umpOnly from each endpoint object and defaults it to false. If the SDK
// omits the key, the caller's choice is silently replaced by that default, so verify the
// key is always written and that the two sides are independent.
void MidiLoopbackEndpointTests::TestCreationConfigJsonCarriesUmpOnly()
{
    MidiLoopbackEndpointDefinition definitionA(L"UMP Only Test A");
    MidiLoopbackEndpointDefinition definitionB(L"UMP Only Test B");

    VERIFY_IS_FALSE(definitionA.CreateOnlyUmpEndpoint());
    VERIFY_IS_FALSE(definitionB.CreateOnlyUmpEndpoint());

    definitionA.CreateOnlyUmpEndpoint(true);

    MidiLoopbackCreationConfig creationConfig(definitionA, definitionB);

    auto configJson = creationConfig.ConfigJson();
    VERIFY_IS_NOT_NULL(configJson);

    std::wcout << L"Config json: " << configJson.Stringify().c_str() << std::endl;

    // literal key names rather than the json_defs.h macros, because what matters here is the
    // on-disk format the service parses
    auto transports = configJson.GetNamedObject(L"endpointTransportPluginSettings");
    auto transport = transports.First().Current().Value().GetObject();
    auto createObject = transport.GetNamedObject(L"create");
    auto association = createObject.First().Current().Value().GetObject();

    auto endpointA = association.GetNamedObject(L"endpointA");
    auto endpointB = association.GetNamedObject(L"endpointB");

    VERIFY_IS_TRUE(endpointA.HasKey(L"umpOnly"));
    VERIFY_IS_TRUE(endpointB.HasKey(L"umpOnly"));

    VERIFY_IS_TRUE(endpointA.GetNamedBoolean(L"umpOnly"));
    VERIFY_IS_FALSE(endpointB.GetNamedBoolean(L"umpOnly"));
}

// The mirror of TestCreateLegacyPorts: with CreateOnlyUmpEndpoint set, the MIDI 1.0 ports must
// not appear. An ordinary loopback is created alongside as a positive control, because the ports
// are created asynchronously and an absence check on its own would pass just as happily if we
// simply looked too early.
void MidiLoopbackEndpointTests::TestCreateOnlyUmpEndpointSuppressesLegacyPorts()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    auto uniqueId = L"ID" + winrt::to_hstring(MidiClock::Now());

    MidiLoopbackEndpointDefinition definitionA(L"Test Ump Only A", L"", uniqueId + L"UA");
    MidiLoopbackEndpointDefinition definitionB(L"Test Ump Only B", L"", uniqueId + L"UB");

    definitionA.CreateOnlyUmpEndpoint(true);
    definitionB.CreateOnlyUmpEndpoint(true);

    MidiLoopbackCreationConfig creationConfig(definitionA, definitionB);

    auto response = MidiLoopbackManager::CreateTransientLoopback(creationConfig);
    VERIFY_IS_NOT_NULL(response);

    if (!response.Success())
    {
        std::wcout << L"Error Message: " << response.ErrorMessage().c_str() << std::endl;
        VERIFY_FAIL();
    }

    auto controlResponse = CreateTestLoopback(L"Test Ump Only Control");

    auto cleanup = wil::scope_exit([&]
        {
            RemoveTestLoopback(response.CreatedLoopbackEntry().AssociationId());
            RemoveTestLoopback(controlResponse.CreatedLoopbackEntry().AssociationId());
        });

    auto countPortsFor = [](winrt::hstring const& endpointDeviceId) -> uint32_t
        {
            auto ports = MidiLegacyPortDeviceInformation::FindAllForAssociatedEndpoint(endpointDeviceId);
            return ports == nullptr ? 0 : ports.Size();
        };

    // wait for the control's ports to show up, which tells us the service has had long enough
    bool controlHasPorts = false;

    for (int i = 0; i < 100 && !controlHasPorts; i++)
    {
        controlHasPorts = countPortsFor(controlResponse.CreatedLoopbackEntry().EndpointA().EndpointDeviceId()) > 0 &&
                          countPortsFor(controlResponse.CreatedLoopbackEntry().EndpointB().EndpointDeviceId()) > 0;

        if (!controlHasPorts) Sleep(100);
    }

    VERIFY_IS_TRUE(controlHasPorts);

    auto umpOnlyPortCountA = countPortsFor(response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId());
    auto umpOnlyPortCountB = countPortsFor(response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId());

    std::wcout << L"MIDI 1.0 port count, UMP-only A: " << umpOnlyPortCountA << std::endl;
    std::wcout << L"MIDI 1.0 port count, UMP-only B: " << umpOnlyPortCountB << std::endl;

    VERIFY_ARE_EQUAL(0u, umpOnlyPortCountA);
    VERIFY_ARE_EQUAL(0u, umpOnlyPortCountB);
}

void MidiLoopbackEndpointTests::TestCreateLegacyPorts()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    winrt::hstring endpointAId{};
    winrt::hstring endpointBId{};

    auto uniqueId = L"ID" + winrt::to_hstring(MidiClock::Now());

    // A-side of the loopback
    MidiLoopbackEndpointDefinition definitionA(
        L"Test Loopback A", // name
        L"The first description is optional, but is displayed to users. This becomes the transport-defined description.", // description
        uniqueId // unique Id that identifies the loopback
        );

    // B-side of the loopback
    MidiLoopbackEndpointDefinition definitionB(
        L"Test Loopback B",
        L"The second description is optional, but is displayed to users. This becomes the transport-defined description.",
        uniqueId // can be the same as the first one, but doesn't need to be.
        );

    LOG_OUTPUT(L"Creating loopback endpoint creation config");

    MidiLoopbackCreationConfig creationConfig(definitionA, definitionB);

    LOG_OUTPUT(L"Creating loopbacks");

    auto response = MidiLoopbackManager::CreateTransientLoopback(creationConfig);
    VERIFY_IS_NOT_NULL(response);

    if (response.Success())
    {
        LOG_OUTPUT(L"Endpoints created successfully");

        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry());
        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry().EndpointA());
        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry().EndpointB());
        VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId().empty());
        VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId().empty());

        endpointAId = response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId();
        endpointBId = response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId();

        auto endpointAPorts = MidiLegacyPortDeviceInformation::FindAllForAssociatedEndpoint(endpointAId);
        VERIFY_IS_NOT_NULL(endpointAPorts);
        VERIFY_IS_TRUE(endpointAPorts.Size() > 0);

        std::cout << std::endl;
        for (auto const& portInfo : endpointAPorts)
        {
            if (portInfo.Flow() == Midi1PortFlow::MidiMessageDestination)
            {
                std::cout << "Destination Port for Endpoint A: ";
            }
            else
            {
                std::cout << "Source Port for Endpoint A: ";
            }

            std::cout
                << " - " << winrt::to_string(portInfo.PortDeviceInstanceId())
                << " - " << winrt::to_string(portInfo.Name())
                << std::endl;
        }

        auto endpointBPorts = MidiLegacyPortDeviceInformation::FindAllForAssociatedEndpoint(endpointBId);
        VERIFY_IS_NOT_NULL(endpointBPorts);
        VERIFY_IS_TRUE(endpointBPorts.Size() > 0);

        std::cout << std::endl;
        for (auto const& portInfo : endpointBPorts)
        {
            if (portInfo.Flow() == Midi1PortFlow::MidiMessageDestination)
            {
                std::cout << "Destination Port for Endpoint B: ";
            }
            else
            {
                std::cout << "Source Port for Endpoint B: ";
            }

            std::cout
                << " - " << winrt::to_string(portInfo.PortDeviceInstanceId())
                << " - " << winrt::to_string(portInfo.Name())
                << std::endl;
        }



        // Give a hoot. Don't pollute.
        MidiLoopbackRemovalConfig removalConfig(response.CreatedLoopbackEntry().AssociationId());
        auto removalResponse = MidiLoopbackManager::RemoveTransientLoopback(removalConfig);

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



void MidiLoopbackEndpointTests::TestUmpSendReceive()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    winrt::hstring endpointAId{};
    winrt::hstring endpointBId{};

    auto uniqueId = L"ID" + winrt::to_hstring(MidiClock::Now());

    // A-side of the loopback
    MidiLoopbackEndpointDefinition definitionA(
        L"Test Loopback A", // name
        L"The first description is optional, but is displayed to users. This becomes the transport-defined description.", // description
        uniqueId // unique Id that identifies the loopback
        );

    // B-side of the loopback
    MidiLoopbackEndpointDefinition definitionB(
        L"Test Loopback B",
        L"The second description is optional, but is displayed to users. This becomes the transport-defined description.",
        uniqueId // can be the same as the first one, but doesn't need to be.
        );

    LOG_OUTPUT(L"Creating loopback endpoint creation config");

    MidiLoopbackCreationConfig creationConfig(definitionA, definitionB);

    LOG_OUTPUT(L"Creating loopbacks");

    auto response = MidiLoopbackManager::CreateTransientLoopback(creationConfig);
    VERIFY_IS_NOT_NULL(response);

    if (response.Success())
    {
        LOG_OUTPUT(L"Endpoints created successfully");

        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry());
        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry().EndpointA());
        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry().EndpointB());
        VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId().empty());
        VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId().empty());

        endpointAId = response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId();
        endpointBId = response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId();


        std::cout << "Creating messages" << std::endl;

        MidiMessage32 messageAToB;
        messageAToB.Timestamp(MidiClock::TimestampConstantSendImmediately());
        messageAToB.Word0(0x23001627);

        MidiMessage64 messageBToA;
        messageBToA.Timestamp(MidiClock::TimestampConstantSendImmediately());
        messageBToA.Word0(0x43001627);
        messageBToA.Word1(0x86753090);

        bool messageAToBReceived{ false };
        bool messageBToAReceived{ false };

        std::cout << "Setting up events" << std::endl;
        wil::unique_event_nothrow allMessagesReceived;
        allMessagesReceived.create();


        std::cout << "Creating session" << std::endl;

        auto session = MidiSession::Create(L"TAEF TestLoopbackEndpointConnections Session");
        VERIFY_IS_NOT_NULL(session);


        std::cout << "Creating connections" << std::endl;

        auto connectionA = session.CreateEndpointConnection(endpointAId);
        VERIFY_IS_NOT_NULL(connectionA);

        auto connectionB = session.CreateEndpointConnection(endpointBId);
        VERIFY_IS_NOT_NULL(connectionB);

        connectionA.MessageReceived([&](auto&&, MidiMessageReceivedEventArgs const& args)
            {
                VERIFY_IS_NOT_NULL(args);
                VERIFY_IS_TRUE(args.PacketType() == MidiPacketType::UniversalMidiPacket64);

                auto message = args.GetMessagePacket().as<MidiMessage64>();

                VERIFY_ARE_EQUAL(message.Word0(), messageBToA.Word0());
                VERIFY_ARE_EQUAL(message.Word1(), messageBToA.Word1());

                messageBToAReceived = true;
                if (messageAToBReceived && messageBToAReceived)
                {
                    allMessagesReceived.SetEvent();
                }
            });

        connectionB.MessageReceived([&](auto&&, MidiMessageReceivedEventArgs const& args)
            {
                VERIFY_IS_NOT_NULL(args);
                VERIFY_IS_TRUE(args.PacketType() == MidiPacketType::UniversalMidiPacket32);

                auto message = args.GetMessagePacket().as<MidiMessage32>();

                VERIFY_ARE_EQUAL(message.Word0(), messageAToB.Word0());

                messageAToBReceived = true;
                if (messageAToBReceived && messageBToAReceived)
                {
                    allMessagesReceived.SetEvent();
                }
            });

        std::cout << "Opening connections" << std::endl;

        VERIFY_IS_TRUE(connectionA.Open());
        VERIFY_IS_TRUE(connectionB.Open());

        // send messages
        std::cout << "Sending messages" << std::endl;
        connectionA.SendSingleMessagePacket(messageAToB);
        connectionB.SendSingleMessagePacket(messageBToA);

        std::cout << "Waiting..." << std::endl;
        allMessagesReceived.wait(5000);

        VERIFY_IS_TRUE(messageAToBReceived);
        VERIFY_IS_TRUE(messageBToAReceived);





        // Give a hoot. Don't pollute.
        MidiLoopbackRemovalConfig removalConfig(response.CreatedLoopbackEntry().AssociationId());
        auto removalResponse = MidiLoopbackManager::RemoveTransientLoopback(removalConfig);

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
    class VerifyOutputTraits<MidiLoopbackFeedbackProtection>
    {
    public:
        static WEX::Common::NoThrowString ToString(MidiLoopbackFeedbackProtection const& value)
        {
            return WEX::Common::NoThrowString().Format(L"%d", static_cast<int32_t>(value));
        }
    };

    template <>
    class VerifyOutputTraits<MidiLoopbackErrorCode>
    {
    public:
        static WEX::Common::NoThrowString ToString(MidiLoopbackErrorCode const& value)
        {
            return WEX::Common::NoThrowString().Format(L"0x%08x", static_cast<uint32_t>(value));
        }
    };
}

namespace
{
    // Where the loopback transport cannot watch for feedback, the typed properties report Off and
    // there is nothing to test, so these skip rather than fail.
    bool FeedbackProtectionAvailableOrSkip()
    {
        if (!MidiLoopbackManager::IsFeedbackProtectionAvailable())
        {
            WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"The loopback transport on this PC cannot watch for feedback.");
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

    // The loop a customer makes by accident: whatever arrives at either end of the pair is sent
    // straight back out of that same end, so it crosses to the other side and comes back again. A
    // fixed set of different messages keeps going around, so the loop runs as fast as the service
    // can carry it without ever filling a buffer.
    class PairFeedbackLoop
    {
    public:
        PairFeedbackLoop(_In_ MidiLoopbackEntry const& loopback, _In_ winrt::hstring const& sessionName)
        {
            m_session = MidiSession::Create(sessionName);
            VERIFY_IS_NOT_NULL(m_session);

            m_connectionA = m_session.CreateEndpointConnection(loopback.EndpointA().EndpointDeviceId());
            VERIFY_IS_NOT_NULL(m_connectionA);

            m_connectionB = m_session.CreateEndpointConnection(loopback.EndpointB().EndpointDeviceId());
            VERIFY_IS_NOT_NULL(m_connectionB);

            // Captured by value so a callback still running after Stop() touches nothing freed.
            auto state = m_state;
            auto connectionA = m_connectionA;
            auto connectionB = m_connectionB;

            m_tokenA = m_connectionA.MessageReceived([state, connectionA](auto&&, MidiMessageReceivedEventArgs const& args)
                {
                    Echo(*state, connectionA, args);
                });

            m_tokenB = m_connectionB.MessageReceived([state, connectionB](auto&&, MidiMessageReceivedEventArgs const& args)
                {
                    Echo(*state, connectionB, args);
                });

            VERIFY_IS_TRUE(m_connectionA.Open());
            VERIFY_IS_TRUE(m_connectionB.Open());
        }

        ~PairFeedbackLoop()
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
                    m_connectionA.SendSingleMessageWords(MidiClock::TimestampConstantSendImmediately(), word0, word1)));
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

                m_connectionA.MessageReceived(m_tokenA);
                m_connectionB.MessageReceived(m_tokenB);

                m_session.DisconnectEndpointConnection(m_connectionA.ConnectionId());
                m_session.DisconnectEndpointConnection(m_connectionB.ConnectionId());
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

        static void Echo(
            _In_ State& state,
            _In_ MidiEndpointConnection const& connection,
            _In_ MidiMessageReceivedEventArgs const& args) noexcept
        {
            if (!state.Echoing)
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

            state.EchoCount++;

            connection.SendSingleMessageWords(MidiClock::TimestampConstantSendImmediately(), word0, word1);
        }

        std::shared_ptr<State> m_state{ std::make_shared<State>() };

        MidiSession m_session{ nullptr };
        MidiEndpointConnection m_connectionA{ nullptr };
        MidiEndpointConnection m_connectionB{ nullptr };
        winrt::event_token m_tokenA{};
        winrt::event_token m_tokenB{};
    };

    constexpr uint32_t FeedbackLoopMessageCount{ 32 };
}


void MidiLoopbackEndpointTests::TestFeedbackProtectionDefaultsToMute()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    if (!FeedbackProtectionAvailableOrSkip())
    {
        return;
    }

    // what a loopback gets when the app says nothing about feedback
    VERIFY_ARE_EQUAL(MidiLoopbackCreationConfig{}.FeedbackProtection(), MidiLoopbackFeedbackProtection::Mute);

    auto response = CreateTestLoopback(L"Test Loopback Feedback Default");
    auto associationId = response.CreatedLoopbackEntry().AssociationId();

    auto cleanupLoopback = wil::scope_exit([&] { RemoveTestLoopback(associationId); });

    VERIFY_ARE_EQUAL(response.CreatedLoopbackEntry().FeedbackProtection(), MidiLoopbackFeedbackProtection::Mute);
    VERIFY_IS_FALSE(response.CreatedLoopbackEntry().IsMutedForFeedback());

    auto const entry = FindActiveLoopbackEntry(associationId);
    VERIFY_IS_NOT_NULL(entry);

    VERIFY_ARE_EQUAL(entry.FeedbackProtection(), MidiLoopbackFeedbackProtection::Mute);
    VERIFY_IS_FALSE(entry.IsMutedForFeedback());
    VERIFY_ARE_EQUAL(entry.FeedbackDetectedTime().time_since_epoch().count(), (int64_t)0);
}


void MidiLoopbackEndpointTests::TestCreateWithFeedbackProtectionOff()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    if (!FeedbackProtectionAvailableOrSkip())
    {
        return;
    }

    auto uniqueId = L"ID" + winrt::to_hstring(MidiClock::Now()) + winrt::to_hstring(rand());

    MidiLoopbackEndpointDefinition definitionA(L"Test Loopback Feedback Off A", L"A-side loopback created by the Windows MIDI Services TAEF tests.", uniqueId + L"-A");
    MidiLoopbackEndpointDefinition definitionB(L"Test Loopback Feedback Off B", L"B-side loopback created by the Windows MIDI Services TAEF tests.", uniqueId + L"-B");

    MidiLoopbackCreationConfig creationConfig(definitionA, definitionB);
    creationConfig.FeedbackProtection(MidiLoopbackFeedbackProtection::Off);

    auto response = MidiLoopbackManager::CreateTransientLoopback(creationConfig);
    VERIFY_IS_NOT_NULL(response);
    VERIFY_IS_TRUE(response.Success());

    auto associationId = response.CreatedLoopbackEntry().AssociationId();

    auto cleanupLoopback = wil::scope_exit([&] { RemoveTestLoopback(associationId); });

    VERIFY_ARE_EQUAL(response.CreatedLoopbackEntry().FeedbackProtection(), MidiLoopbackFeedbackProtection::Off);

    // and the service agrees, rather than only the SDK echoing back what it was given
    auto const entry = FindActiveLoopbackEntry(associationId);
    VERIFY_IS_NOT_NULL(entry);
    VERIFY_ARE_EQUAL(entry.FeedbackProtection(), MidiLoopbackFeedbackProtection::Off);
}


void MidiLoopbackEndpointTests::TestSetFeedbackProtection()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    if (!FeedbackProtectionAvailableOrSkip())
    {
        return;
    }

    auto response = CreateTestLoopback(L"Test Loopback Feedback Set");
    auto associationId = response.CreatedLoopbackEntry().AssociationId();

    auto cleanupLoopback = wil::scope_exit([&] { RemoveTestLoopback(associationId); });

    auto offResponse = MidiLoopbackManager::SetFeedbackProtection(associationId, MidiLoopbackFeedbackProtection::Off);
    VERIFY_IS_NOT_NULL(offResponse);

    if (!offResponse.Success())
    {
        std::wcout << L"Error Message: " << offResponse.ErrorMessage().c_str() << std::endl;
    }

    VERIFY_IS_TRUE(offResponse.Success());

    auto entry = FindActiveLoopbackEntry(associationId);
    VERIFY_IS_NOT_NULL(entry);
    VERIFY_ARE_EQUAL(entry.FeedbackProtection(), MidiLoopbackFeedbackProtection::Off);

    // the muted state is a separate thing, and changing protection must not touch it
    VERIFY_IS_FALSE(entry.IsMuted());

    auto muteResponse = MidiLoopbackManager::SetFeedbackProtection(associationId, MidiLoopbackFeedbackProtection::Mute);
    VERIFY_IS_NOT_NULL(muteResponse);
    VERIFY_IS_TRUE(muteResponse.Success());

    entry = FindActiveLoopbackEntry(associationId);
    VERIFY_IS_NOT_NULL(entry);
    VERIFY_ARE_EQUAL(entry.FeedbackProtection(), MidiLoopbackFeedbackProtection::Mute);

    // a loopback that is not there is reported as such
    auto missingResponse = MidiLoopbackManager::SetFeedbackProtection(
        winrt::Windows::Foundation::GuidHelper::CreateNewGuid(),
        MidiLoopbackFeedbackProtection::Off);

    VERIFY_IS_NOT_NULL(missingResponse);
    VERIFY_IS_FALSE(missingResponse.Success());
}


void MidiLoopbackEndpointTests::TestSetFeedbackProtectionRejectsUnknownValue()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    if (!FeedbackProtectionAvailableOrSkip())
    {
        return;
    }

    auto response = CreateTestLoopback(L"Test Loopback Feedback Unknown");
    auto associationId = response.CreatedLoopbackEntry().AssociationId();

    auto cleanupLoopback = wil::scope_exit([&] { RemoveTestLoopback(associationId); });

    // an enum from a newer SDK, or a cast, is refused rather than guessed at
    auto unknownResponse = MidiLoopbackManager::SetFeedbackProtection(
        associationId,
        static_cast<MidiLoopbackFeedbackProtection>(99));

    VERIFY_IS_NOT_NULL(unknownResponse);
    VERIFY_IS_FALSE(unknownResponse.Success());
    VERIFY_ARE_EQUAL(unknownResponse.ErrorCode(), MidiLoopbackErrorCode::InvalidArgument);

    // and nothing changed
    auto const entry = FindActiveLoopbackEntry(associationId);
    VERIFY_IS_NOT_NULL(entry);
    VERIFY_ARE_EQUAL(entry.FeedbackProtection(), MidiLoopbackFeedbackProtection::Mute);
}


void MidiLoopbackEndpointTests::TestFeedbackLoopMutesLoopback()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    if (!FeedbackProtectionAvailableOrSkip())
    {
        return;
    }

    auto response = CreateTestLoopback(L"Test Loopback Feedback Loop");
    auto associationId = response.CreatedLoopbackEntry().AssociationId();

    auto cleanupLoopback = wil::scope_exit([&] { RemoveTestLoopback(associationId); });

    PairFeedbackLoop loop(response.CreatedLoopbackEntry(), L"TestFeedbackLoopMutesLoopback");

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
    auto unmuteResponse = MidiLoopbackManager::UnmuteLoopback(associationId);
    VERIFY_IS_NOT_NULL(unmuteResponse);
    VERIFY_IS_TRUE(unmuteResponse.Success());

    auto const unmutedEntry = FindActiveLoopbackEntry(associationId);
    VERIFY_IS_NOT_NULL(unmutedEntry);
    VERIFY_IS_FALSE(unmutedEntry.IsMuted());
    VERIFY_IS_FALSE(unmutedEntry.IsMutedForFeedback());
    VERIFY_ARE_EQUAL(unmutedEntry.FeedbackProtection(), MidiLoopbackFeedbackProtection::Mute);
}


void MidiLoopbackEndpointTests::TestFeedbackLoopWithProtectionOffIsNotMuted()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    if (!FeedbackProtectionAvailableOrSkip())
    {
        return;
    }

    auto response = CreateTestLoopback(L"Test Loopback Feedback Loop Off");
    auto associationId = response.CreatedLoopbackEntry().AssociationId();

    auto cleanupLoopback = wil::scope_exit([&] { RemoveTestLoopback(associationId); });

    auto offResponse = MidiLoopbackManager::SetFeedbackProtection(associationId, MidiLoopbackFeedbackProtection::Off);
    VERIFY_IS_NOT_NULL(offResponse);
    VERIFY_IS_TRUE(offResponse.Success());

    PairFeedbackLoop loop(response.CreatedLoopbackEntry(), L"TestFeedbackLoopWithProtectionOffIsNotMuted");

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
void MidiLoopbackEndpointTests::TestSteadyTrafficIsNotMuted()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    if (!FeedbackProtectionAvailableOrSkip())
    {
        return;
    }

    auto response = CreateTestLoopback(L"Test Loopback Steady Traffic");
    auto associationId = response.CreatedLoopbackEntry().AssociationId();

    auto cleanupLoopback = wil::scope_exit([&] { RemoveTestLoopback(associationId); });

    auto session = MidiSession::Create(L"TestSteadyTrafficIsNotMuted");
    VERIFY_IS_NOT_NULL(session);

    auto connectionA = session.CreateEndpointConnection(response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId());
    VERIFY_IS_NOT_NULL(connectionA);

    auto connectionB = session.CreateEndpointConnection(response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId());
    VERIFY_IS_NOT_NULL(connectionB);

    std::atomic<uint32_t> receivedCount{ 0 };
    std::atomic<uint32_t> outOfOrderCount{ 0 };

    wil::unique_event_nothrow allReceived;
    allReceived.create();

    constexpr uint32_t messageCount{ 30'000 };

    // Sixteen notes over and over, identical each time around. That is what makes it look like a
    // loop, and it also means the order can be checked: message n is always note n % 16.
    auto token = connectionB.MessageReceived([&](auto&&, MidiMessageReceivedEventArgs const& args)
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

    VERIFY_IS_TRUE(connectionA.Open());
    VERIFY_IS_TRUE(connectionB.Open());

    // several thousand a second for a few seconds: well past the point where the checks start
    for (uint32_t i = 0; i < messageCount; i++)
    {
        uint32_t const word0{ 0x40900000 | ((i & 0x0F) << 8) };
        uint32_t const word1{ 0x80000000 };

        VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(
            connectionA.SendSingleMessageWords(MidiClock::TimestampConstantSendImmediately(), word0, word1)));

        if ((i % 100) == 99)
        {
            ::Sleep(10);
        }
    }

    auto const gotEverything = allReceived.wait(15000);

    connectionB.MessageReceived(token);
    session.DisconnectEndpointConnection(connectionA.ConnectionId());
    session.DisconnectEndpointConnection(connectionB.ConnectionId());
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

    MidiLoopbackSavedEntry FindSavedLoopbackEntry(_In_ winrt::guid const& associationId)
    {
        for (auto const& entry : MidiLoopbackManager::GetSavedLoopbackEntries())
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
            svc::MidiServiceTransportPluginConfigManager::SaveUpdate(MidiLoopbackRemovalConfig(associationId));
        }
        catch (...)
        {
        }
    }
}


// Saved without being created, so the running service never sees it
void MidiLoopbackEndpointTests::TestSavedLoopbackFollowsSavedChanges()
{
    if (!ConfigFileRegisteredOrSkip())
    {
        return;
    }

    auto const suffix = winrt::to_hstring(MidiClock::Now());

    MidiLoopbackEndpointDefinition definitionA(L"Test Saved Loopback A " + suffix, L"Saved A side", L"SAVEDA" + suffix);
    MidiLoopbackEndpointDefinition definitionB(L"Test Saved Loopback B " + suffix, L"Saved B side", L"SAVEDB" + suffix);

    MidiLoopbackCreationConfig creationConfig(definitionA, definitionB);

    auto const associationId = creationConfig.AssociationId();

    VERIFY_IS_TRUE(FindSavedLoopbackEntry(associationId) == nullptr, L"not saved to begin with");

    auto removeEntry = wil::scope_exit([&] { RemoveSavedLoopback(associationId); });

    VerifySaved(svc::MidiServiceTransportPluginConfigManager::SaveUpdate(creationConfig), L"saving the loopback works");

    auto saved = FindSavedLoopbackEntry(associationId);

    VERIFY_IS_TRUE(saved != nullptr, L"it is listed once saved");
    VERIFY_IS_TRUE(saved.EndpointDefinitionA().Name() == definitionA.Name(), L"with the A name");
    VERIFY_IS_TRUE(saved.EndpointDefinitionA().Description() == definitionA.Description(), L"the A description");
    VERIFY_IS_TRUE(saved.EndpointDefinitionA().UniqueId() == definitionA.UniqueId(), L"the A unique id");
    VERIFY_IS_TRUE(saved.EndpointDefinitionB().Name() == definitionB.Name(), L"and the B name");
    VERIFY_IS_FALSE(saved.IsMuted());
    VERIFY_ARE_EQUAL(saved.FeedbackProtection(), MidiLoopbackFeedbackProtection::Mute);

    MidiLoopbackUpdateConfig update(associationId);
    update.EndpointAName(L"Test Saved Loopback A Renamed " + suffix);
    update.EndpointBDescription(L"");
    update.IsMuted(true);
    update.FeedbackProtection(MidiLoopbackFeedbackProtection::Off);

    VerifySaved(svc::MidiServiceTransportPluginConfigManager::SaveUpdate(update), L"saving a change to it works");

    saved = FindSavedLoopbackEntry(associationId);

    VERIFY_IS_TRUE(saved != nullptr);
    VERIFY_IS_TRUE(saved.EndpointDefinitionA().Name() == update.EndpointAName(), L"the new name is saved");
    VERIFY_IS_TRUE(saved.EndpointDefinitionA().Description() == definitionA.Description(), L"what was not set is left alone");
    VERIFY_IS_TRUE(saved.EndpointDefinitionA().UniqueId() == definitionA.UniqueId(), L"including the unique id");
    VERIFY_IS_TRUE(saved.EndpointDefinitionB().Name() == definitionB.Name(), L"and the other side's name");
    VERIFY_IS_TRUE(saved.EndpointDefinitionB().Description().empty(), L"a description set to empty is saved empty");
    VERIFY_IS_TRUE(saved.IsMuted());
    VERIFY_ARE_EQUAL(saved.FeedbackProtection(), MidiLoopbackFeedbackProtection::Off);

    VerifySaved(svc::MidiServiceTransportPluginConfigManager::SaveUpdate(MidiLoopbackRemovalConfig(associationId)), L"removing it works");

    VERIFY_IS_TRUE(FindSavedLoopbackEntry(associationId) == nullptr, L"a removed loopback is no longer listed");
}


// Saving a change for an entry that is not there would leave half an entry in the file
void MidiLoopbackEndpointTests::TestSavingUpdateForUnsavedLoopbackIsRefused()
{
    if (!ConfigFileRegisteredOrSkip())
    {
        return;
    }

    auto const associationId = winrt::Windows::Foundation::GuidHelper::CreateNewGuid();

    auto removeEntry = wil::scope_exit([&] { RemoveSavedLoopback(associationId); });

    MidiLoopbackUpdateConfig update(associationId);
    update.EndpointAName(L"Test Unsaved Loopback");
    update.IsMuted(true);

    auto const response = svc::MidiServiceTransportPluginConfigManager::SaveUpdate(update);

    VERIFY_IS_TRUE(response != nullptr);
    VERIFY_IS_FALSE(response.Success(), L"a change to a loopback which is not saved is not saved");
    VERIFY_IS_TRUE(response.Result() == svc::MidiServiceConfigSaveResult::ErrorEntryNotSaved, L"and says why");
    VERIFY_IS_FALSE(response.ErrorMessage().empty(), L"in words as well");

    VERIFY_IS_TRUE(FindSavedLoopbackEntry(associationId) == nullptr, L"and nothing is left in the file");
}


void MidiLoopbackEndpointTests::TestUpdateRunningLoopback()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    auto response = CreateTestLoopback(L"Test Loopback Update");
    auto const created = response.CreatedLoopbackEntry();
    auto const associationId = created.AssociationId();

    auto cleanupLoopback = wil::scope_exit([&] { RemoveTestLoopback(associationId); });

    // a transport build without the customization handler can still mute
    bool const canRename = svc::MidiServiceTransportPluginConfigManager::QueryCapability(
        MidiLoopbackManager::TransportId(),
        L"customizeEndpoint");

    auto const newNameA = created.EndpointA().Name() + L" Renamed";

    MidiLoopbackUpdateConfig update(associationId);
    update.IsMuted(true);

    if (canRename)
    {
        update.EndpointAName(newNameA);
    }
    else
    {
        WEX::Logging::Log::Comment(L"This loopback transport cannot rename an endpoint, so only muting is checked.");
    }

    auto const updateResponse = MidiLoopbackManager::UpdateLoopback(update);

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
        VERIFY_IS_TRUE(entry.EndpointA().Name() == newNameA, L"side A has its new name");
        VERIFY_IS_TRUE(entry.EndpointA().Description() == created.EndpointA().Description(), L"and keeps its description");
        VERIFY_IS_TRUE(entry.EndpointB().Name() == created.EndpointB().Name(), L"side B is left alone");
    }

    // the same kind of change through the generic path goes to the manager, not the transport
    MidiLoopbackUpdateConfig unmute(associationId);
    unmute.IsMuted(false);

    auto const sendResponse = svc::MidiServiceTransportPluginConfigManager::SendUpdate(unmute);

    VERIFY_IS_NOT_NULL(sendResponse);
    VERIFY_IS_TRUE(sendResponse.Status() == svc::MidiServiceConfigResponseStatus::Success, L"SendUpdate applies an update config");

    entry = FindActiveLoopbackEntry(associationId);

    VERIFY_IS_NOT_NULL(entry);
    VERIFY_IS_FALSE(entry.IsMuted(), L"the loopback is unmuted");

    // a loopback which is not running is reported, rather than the change being lost quietly
    MidiLoopbackUpdateConfig missing(winrt::Windows::Foundation::GuidHelper::CreateNewGuid());
    missing.EndpointAName(L"Test Loopback Nobody");

    auto const missingResponse = MidiLoopbackManager::UpdateLoopback(missing);

    VERIFY_IS_NOT_NULL(missingResponse);
    VERIFY_IS_FALSE(missingResponse.Success());
    VERIFY_ARE_EQUAL(missingResponse.ErrorCode(), MidiLoopbackErrorCode::EndpointNotFound);
}