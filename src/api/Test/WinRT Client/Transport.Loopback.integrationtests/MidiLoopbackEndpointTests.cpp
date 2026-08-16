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
        uniqueId + L"-A",
        L"A-side loopback created by the Windows MIDI Services TAEF tests."
    );

    MidiLoopbackEndpointDefinition definitionB(
        namePrefix + L" B",
        uniqueId + L"-B",
        L"B-side loopback created by the Windows MIDI Services TAEF tests."
    );

    MidiLoopbackCreationConfig creationConfig(
        foundation::GuidHelper::CreateNewGuid(), definitionA, definitionB);

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
    auto associationId = winrt::Windows::Foundation::GuidHelper::CreateNewGuid();
    auto nameA = L"我的虚拟设备";
    auto nameB = L"ענדפוינט ב";

    MidiLoopbackEndpointDefinition definitionA;
    definitionA.Name(nameA);
    definitionA.UniqueId(uniqueId);

    MidiLoopbackEndpointDefinition definitionB;
    definitionB.Name(nameB);
    definitionB.UniqueId(uniqueId);

    MidiLoopbackCreationConfig config(associationId, definitionA, definitionB);

    VERIFY_IS_FALSE(config.EndpointDefinitionA().Name().empty());
    VERIFY_IS_FALSE(config.EndpointDefinitionB().Name().empty());


    auto result = MidiLoopbackManager::CreateTransientLoopback(config);

    VERIFY_IS_NOT_NULL(result);
    VERIFY_IS_TRUE(result.Success());

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

    winrt::hstring uniqueId = winrt::to_hstring(winrt::Windows::Foundation::GuidHelper::CreateNewGuid());
    auto associationId = winrt::Windows::Foundation::GuidHelper::CreateNewGuid();

    MidiLoopbackEndpointDefinition definitionA;
    definitionA.Name(longName);
    definitionA.UniqueId(uniqueId);

    MidiLoopbackEndpointDefinition definitionB;
    definitionB.Name(longName);
    definitionB.UniqueId(uniqueId);

    MidiLoopbackCreationConfig config(associationId, definitionA, definitionB);

    auto result = MidiLoopbackManager::CreateTransientLoopback(config);

    VERIFY_IS_NOT_NULL(result);
    VERIFY_IS_TRUE(result.Success());

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
        winrt::hstring{ garbageUniqueIdA },
        L"A-side created with a unique id which contains invalid characters."
    );

    // B-side of the loopback
    MidiLoopbackEndpointDefinition definitionB(
        L"Test Loopback B Garbage Id",
        winrt::hstring{ garbageUniqueIdB },
        L"B-side created with a unique id which contains invalid characters."
    );

    winrt::guid associationId = foundation::GuidHelper::CreateNewGuid();

    LOG_OUTPUT(L"Creating loopback endpoint creation config");

    MidiLoopbackCreationConfig creationConfig(associationId, definitionA, definitionB);

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
        uniqueId, // unique Id that identifies the loopback
        L"The first description is optional, but is displayed to users. This becomes the transport-defined description." // description
    );

    // B-side of the loopback
    MidiLoopbackEndpointDefinition definitionB(
        L"Test Loopback B",
        uniqueId, // can be the same as the first one, but doesn't need to be.
        L"The second description is optional, but is displayed to users. This becomes the transport-defined description."
    );

    winrt::guid associationId = foundation::GuidHelper::CreateNewGuid();

    LOG_OUTPUT(L"Creating loopback endpoint creation config");

    MidiLoopbackCreationConfig creationConfig(associationId, definitionA, definitionB);

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
        uniqueId, // unique Id that identifies the loopback
        L"The first description is optional, but is displayed to users. This becomes the transport-defined description." // description
    );

    // B-side of the loopback
    MidiLoopbackEndpointDefinition definitionB(
        L"Test Loopback B",
        uniqueId, // can be the same as the first one, but doesn't need to be.
        L"The second description is optional, but is displayed to users. This becomes the transport-defined description."
    );

    winrt::guid associationId = foundation::GuidHelper::CreateNewGuid();

    LOG_OUTPUT(L"Creating loopback endpoint creation config");

    MidiLoopbackCreationConfig creationConfig(associationId, definitionA, definitionB);

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
        uniqueId, // unique Id that identifies the loopback
        L"The first description is optional, but is displayed to users. This becomes the transport-defined description." // description
    );

    // B-side of the loopback
    MidiLoopbackEndpointDefinition definitionB(
        L"Test Loopback B",
        uniqueId, // can be the same as the first one, but doesn't need to be.
        L"The second description is optional, but is displayed to users. This becomes the transport-defined description."
    );

    winrt::guid associationId = foundation::GuidHelper::CreateNewGuid();

    LOG_OUTPUT(L"Creating loopback endpoint creation config");

    MidiLoopbackCreationConfig creationConfig(associationId, definitionA, definitionB);

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