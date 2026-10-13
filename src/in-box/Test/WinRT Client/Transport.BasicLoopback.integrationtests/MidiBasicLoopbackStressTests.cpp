// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

#include <winrt/Windows.Devices.Midi2.Transports.Loopback.h>

#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "..\LoopbackStressHelpers.h"
#include "MidiBasicLoopbackStressTests.h"

#pragma comment(lib, "winmm.lib")

namespace loopback2 = winrt::Windows::Devices::Midi2::Transports::Loopback;

using namespace LoopbackStress;

namespace
{
    bool LoopbackExists(_In_ std::wstring const& uniqueId)
    {
        return MidiBasicLoopbackManager::DoesLoopbackExist(winrt::hstring{ uniqueId });
    }

    MidiBasicLoopbackCreationResponse TryCreateLoopback(_In_ std::wstring const& name, _In_ std::wstring const& uniqueId)
    {
        MidiBasicLoopbackEndpointDefinition definition(
            winrt::hstring{ name },
            L"Created by the basic loopback stress tests.",
            winrt::hstring{ uniqueId });

        MidiBasicLoopbackCreationConfig config(definition);

        return MidiBasicLoopbackManager::CreateTransientLoopback(config);
    }

    MidiBasicLoopbackCreationResponse CreateLoopback(_In_ std::wstring const& name, _In_ std::wstring const& uniqueId)
    {
        auto response = TryCreateLoopback(name, uniqueId);

        VERIFY_IS_NOT_NULL(response);

        if (!response.Success())
        {
            LOG_OUTPUT(L"Create failed. Error code 0x%08x: %s", static_cast<uint32_t>(response.ErrorCode()), response.ErrorMessage().c_str());
        }

        VERIFY_IS_TRUE(response.Success());
        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry());
        VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointDeviceId().empty());

        return response;
    }

    void RemoveLoopback(_In_ winrt::guid const& associationId)
    {
        auto const response = MidiBasicLoopbackManager::RemoveTransientLoopback(MidiBasicLoopbackRemovalConfig(associationId));

        VERIFY_IS_NOT_NULL(response);

        if (!response.Success())
        {
            LOG_OUTPUT(L"Remove failed. Error code 0x%08x: %s", static_cast<uint32_t>(response.ErrorCode()), response.ErrorMessage().c_str());
        }

        VERIFY_IS_TRUE(response.Success());
    }

    // For cleanup, which must never throw. Removing one which is already gone just fails.
    void RemoveLoopbackQuietly(_In_ winrt::guid const& associationId) noexcept
    {
        try
        {
            MidiBasicLoopbackManager::RemoveTransientLoopback(MidiBasicLoopbackRemovalConfig(associationId));
        }
        catch (...)
        {
        }
    }

    // A fresh connection which sends one message and hears it come back
    bool RoundTrip(_In_ winrt::hstring const& endpointDeviceId, _In_ uint32_t const word)
    {
        Receiver receiver{};

        return receiver.Open(endpointDeviceId, L"Basic loopback stress round trip") && receiver.SendAndHearBack(word, 5000);
    }

    // Proves MIDI still works for everyone else, on a loopback which has nothing to do with the test
    void VerifyServiceStillLoopsMessages()
    {
        VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());

        auto const response = CreateLoopback(L"Stress Basic Loopback Health Check", MakeUniqueId(L"SHEALTH"));
        auto const associationId = response.CreatedLoopbackEntry().AssociationId();

        auto cleanup = wil::scope_exit([&] { RemoveLoopbackQuietly(associationId); });

        VERIFY_IS_TRUE(RoundTrip(response.CreatedLoopbackEntry().EndpointDeviceId(), NoteOnWord(99)));

        RemoveLoopback(associationId);
        cleanup.release();
    }

    // The overlap and reserved identifier checks arrived together, so this tells an older transport apart
    bool TransportRefusesOverlappingUniqueIds()
    {
        auto const uniqueId = MakeUniqueId(L"SGUARD");
        auto const first = CreateLoopback(L"Stress Basic Loopback Guard", uniqueId);
        auto const firstAssociationId = first.CreatedLoopbackEntry().AssociationId();

        auto cleanupFirst = wil::scope_exit([&] { RemoveLoopbackQuietly(firstAssociationId); });

        auto const second = TryCreateLoopback(L"Stress Basic Loopback Guard Longer", uniqueId + L"X");

        if (second != nullptr && second.Success())
        {
            // the longer one first, so the shorter one's removal can't take it along
            RemoveLoopbackQuietly(second.CreatedLoopbackEntry().AssociationId());
            return false;
        }

        return true;
    }

}


bool MidiBasicLoopbackStressTests::TestSetup()
{
    m_deviceNodeTracker.Start();

    return true;
}

bool MidiBasicLoopbackStressTests::TestCleanup()
{
    m_deviceNodeTracker.RemoveDeviceNodesCreatedSinceStart();

    return true;
}


void MidiBasicLoopbackStressTests::TestCreateAndRemoveSameLoopbackRepeatedly()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    auto const uniqueId = MakeUniqueId(L"SREPEAT");
    constexpr uint32_t iterations{ 25 };

    winrt::hstring firstEndpointDeviceId{};

    for (uint32_t i = 0; i < iterations; i++)
    {
        auto const response = CreateLoopback(L"Stress Basic Loopback Repeat", uniqueId);
        auto const associationId = response.CreatedLoopbackEntry().AssociationId();
        auto const endpointDeviceId = response.CreatedLoopbackEntry().EndpointDeviceId();

        auto cleanup = wil::scope_exit([&] { RemoveLoopbackQuietly(associationId); });

        if (i == 0)
        {
            firstEndpointDeviceId = endpointDeviceId;
        }

        // the same unique identifier must give the same endpoint every time
        VERIFY_IS_TRUE(HStringsAreCaseInsensitiveEqual(endpointDeviceId, firstEndpointDeviceId));

        VERIFY_IS_TRUE(WaitFor([&] { return LoopbackExists(uniqueId); }, 5000));
        VERIFY_IS_TRUE(RoundTrip(endpointDeviceId, NoteOnWord(i)));

        RemoveLoopback(associationId);
        cleanup.release();

        VERIFY_IS_TRUE(WaitFor([&] { return !LoopbackExists(uniqueId); }, 5000));
    }

    LOG_OUTPUT(L"Created, used and removed the same loopback %u times", iterations);
}


void MidiBasicLoopbackStressTests::TestRemoveWhileReceiversConnectedAndSending()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    auto const uniqueId = MakeUniqueId(L"SLOAD");
    auto const response = CreateLoopback(L"Stress Basic Loopback Load", uniqueId);
    auto const associationId = response.CreatedLoopbackEntry().AssociationId();
    auto const endpointDeviceId = response.CreatedLoopbackEntry().EndpointDeviceId();

    bool removed{ false };
    auto cleanup = wil::scope_exit([&] { if (!removed) RemoveLoopbackQuietly(associationId); });

    constexpr uint32_t receiverCount{ 4 };
    std::vector<std::unique_ptr<Receiver>> receivers{};

    for (uint32_t i = 0; i < receiverCount; i++)
    {
        receivers.push_back(std::make_unique<Receiver>());
        VERIFY_IS_TRUE(receivers.back()->Open(endpointDeviceId, L"Basic loopback stress receiver"));
    }

    Receiver sender{};
    VERIFY_IS_TRUE(sender.Open(endpointDeviceId, L"Basic loopback stress sender"));

    // every receiver hears the first message before the flood starts
    VERIFY_IS_TRUE(sender.SendAndHearBack(NoteOnWord(1), 5000));

    for (auto const& receiver : receivers)
    {
        VERIFY_IS_TRUE(WaitFor([&] { return receiver->Count.load() > 0; }, 5000));
    }

    ConnectionFlood flood{};
    flood.Start(sender.Connection);

    // long enough, at this rate, for feedback protection to start holding messages to test them
    Sleep(1500);

    auto const countsBeforeRemove = SnapshotCounts(receivers);

    for (auto const count : countsBeforeRemove)
    {
        VERIFY_IS_GREATER_THAN(count, static_cast<uint64_t>(1));
    }

    LOG_OUTPUT(L"Flooded with %llu messages in 1.5 s. The first receiver heard %llu.", flood.MessagesSent(), countsBeforeRemove.front());

    auto const removeStart = StressClock::now();
    RemoveLoopback(associationId);
    removed = true;
    auto const removeMs = MillisecondsSince(removeStart);

    LOG_OUTPUT(L"Removed the loopback in %lld ms with %u receivers connected and the sender still sending", removeMs, receiverCount);
    VERIFY_IS_LESS_THAN(removeMs, 10'000LL);

    VERIFY_IS_TRUE(WaitFor([&] { return !LoopbackExists(uniqueId); }, 5000));

    // messages already on their way may still land, so the comparison starts once they have
    Sleep(500);
    auto const countsAfterRemove = SnapshotCounts(receivers);
    Sleep(1000);
    auto const countsLater = SnapshotCounts(receivers);

    for (size_t i = 0; i < receivers.size(); i++)
    {
        VERIFY_ARE_EQUAL(countsLater[i], countsAfterRemove[i]);
    }

    flood.Stop();

    LOG_OUTPUT(L"Sender: %llu messages accepted, %llu sends refused", flood.MessagesSent(), flood.FailedSends());
    VERIFY_IS_FALSE(flood.Threw());

    VerifyServiceStillLoopsMessages();

    auto const closeStart = StressClock::now();

    sender.Close();

    for (auto& receiver : receivers)
    {
        receiver->Close();
    }

    auto const closeMs = MillisecondsSince(closeStart);
    LOG_OUTPUT(L"Closed %u connections to the removed loopback in %lld ms", receiverCount + 1, closeMs);
    VERIFY_IS_LESS_THAN(closeMs, 10'000LL);
}


void MidiBasicLoopbackStressTests::TestRecreateWhileOldConnectionsStillOpen()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    auto const uniqueId = MakeUniqueId(L"SRECREATE");

    auto const first = CreateLoopback(L"Stress Basic Loopback Recreate", uniqueId);
    auto const firstAssociationId = first.CreatedLoopbackEntry().AssociationId();
    auto const endpointDeviceId = first.CreatedLoopbackEntry().EndpointDeviceId();

    bool firstRemoved{ false };
    auto cleanupFirst = wil::scope_exit([&] { if (!firstRemoved) RemoveLoopbackQuietly(firstAssociationId); });

    std::vector<std::unique_ptr<Receiver>> oldReceivers{};

    for (uint32_t i = 0; i < 2; i++)
    {
        oldReceivers.push_back(std::make_unique<Receiver>());
        VERIFY_IS_TRUE(oldReceivers.back()->Open(endpointDeviceId, L"Basic loopback stress old connection"));
    }

    VERIFY_IS_TRUE(oldReceivers.front()->SendAndHearBack(NoteOnWord(1), 5000));
    VERIFY_IS_TRUE(WaitFor([&] { return oldReceivers.back()->Count.load() > 0; }, 5000));

    LOG_OUTPUT(L"Removing the loopback with both connections still open");
    RemoveLoopback(firstAssociationId);
    firstRemoved = true;

    VERIFY_IS_TRUE(WaitFor([&] { return !LoopbackExists(uniqueId); }, 5000));

    LOG_OUTPUT(L"Creating it again, same unique identifier");
    auto const second = CreateLoopback(L"Stress Basic Loopback Recreate", uniqueId);
    auto const secondAssociationId = second.CreatedLoopbackEntry().AssociationId();

    auto cleanupSecond = wil::scope_exit([&] { RemoveLoopbackQuietly(secondAssociationId); });

    VERIFY_IS_TRUE(HStringsAreCaseInsensitiveEqual(second.CreatedLoopbackEntry().EndpointDeviceId(), endpointDeviceId));
    VERIFY_IS_TRUE(WaitFor([&] { return LoopbackExists(uniqueId); }, 5000));

    Receiver fresh{};
    VERIFY_IS_TRUE(fresh.Open(endpointDeviceId, L"Basic loopback stress new connection"));

    auto const oldCounts = SnapshotCounts(oldReceivers);

    VERIFY_IS_TRUE(fresh.SendAndHearBack(NoteOnWord(2), 5000));

    // the old connections belong to the loopback which was removed, so they hear nothing from the new one
    Sleep(500);
    VERIFY_IS_TRUE(SnapshotCounts(oldReceivers) == oldCounts);

    // and nothing sent on an old connection reaches the new loopback
    auto const freshCount = fresh.Count.load();
    oldReceivers.front()->Connection.SendSingleMessageWords(MidiClock::TimestampConstantSendImmediately(), NoteOnWord(3));
    Sleep(1000);
    VERIFY_ARE_EQUAL(fresh.Count.load(), freshCount);
    VERIFY_IS_TRUE(SnapshotCounts(oldReceivers) == oldCounts);

    LOG_OUTPUT(L"Closing the old connections");

    for (auto& receiver : oldReceivers)
    {
        receiver->Close();
    }

    // closing what was left of the old loopback must not silence the new one
    Sleep(500);
    VERIFY_IS_TRUE(fresh.SendAndHearBack(NoteOnWord(4), 5000));
    VERIFY_IS_TRUE(RoundTrip(endpointDeviceId, NoteOnWord(5)));

    fresh.Close();
}


void MidiBasicLoopbackStressTests::TestRemoveWhileWinMMPortsOpenAndSending()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    auto const uniqueId = MakeUniqueId(L"SWINMM");
    auto const response = CreateLoopback(L"Stress Basic Loopback WinMM", uniqueId);
    auto const associationId = response.CreatedLoopbackEntry().AssociationId();
    auto const endpointDeviceId = response.CreatedLoopbackEntry().EndpointDeviceId();

    bool removed{ false };
    auto cleanup = wil::scope_exit([&] { if (!removed) RemoveLoopbackQuietly(associationId); });

    uint32_t inputPortNumber{ 0 };
    uint32_t outputPortNumber{ 0 };

    VERIFY_IS_TRUE(TryResolveWinMMPortNumber(endpointDeviceId, Midi1PortFlow::MidiMessageSource, inputPortNumber));
    VERIFY_IS_TRUE(TryResolveWinMMPortNumber(endpointDeviceId, Midi1PortFlow::MidiMessageDestination, outputPortNumber));

    LOG_OUTPUT(L"WinMM input %u, output %u", inputPortNumber, outputPortNumber);

    // two applications listening, one sending
    std::array<std::atomic<uint64_t>, 2> received{};
    std::array<HMIDIIN, 2> inputs{};

    auto closeInputs = wil::scope_exit([&]
        {
            for (auto& input : inputs)
            {
                if (input != nullptr)
                {
                    midiInStop(input);
                    midiInReset(input);
                    midiInClose(input);
                    input = nullptr;
                }
            }
        });

    for (size_t i = 0; i < inputs.size(); i++)
    {
        VERIFY_ARE_EQUAL(midiInOpen(&inputs[i], inputPortNumber, reinterpret_cast<DWORD_PTR>(&CountingMidiInProc), reinterpret_cast<DWORD_PTR>(&received[i]), CALLBACK_FUNCTION),
            static_cast<MMRESULT>(MMSYSERR_NOERROR));
        VERIFY_ARE_EQUAL(midiInStart(inputs[i]), static_cast<MMRESULT>(MMSYSERR_NOERROR));
    }

    HMIDIOUT output{ nullptr };

    auto closeOutput = wil::scope_exit([&] { if (output != nullptr) midiOutClose(output); });

    VERIFY_ARE_EQUAL(midiOutOpen(&output, outputPortNumber, 0, 0, CALLBACK_NULL), static_cast<MMRESULT>(MMSYSERR_NOERROR));

    for (uint32_t i = 0; i < 10; i++)
    {
        VERIFY_ARE_EQUAL(midiOutShortMsg(output, NoteOnShortMessage(i)), static_cast<MMRESULT>(MMSYSERR_NOERROR));
    }

    VERIFY_IS_TRUE(WaitFor([&] { return received[0].load() >= 10 && received[1].load() >= 10; }, 5000));

    WinMMFlood flood{};
    flood.Start(output);

    Sleep(1500);

    LOG_OUTPUT(L"Flooded with %llu short messages in 1.5 s. The first input heard %llu.", flood.Succeeded(), received[0].load());

    auto const removeStart = StressClock::now();
    RemoveLoopback(associationId);
    removed = true;
    auto const removeMs = MillisecondsSince(removeStart);

    LOG_OUTPUT(L"Removed the loopback in %lld ms with two WinMM inputs and one output open and sending", removeMs);
    VERIFY_IS_LESS_THAN(removeMs, 10'000LL);

    VERIFY_IS_TRUE(WaitFor([&] { return !LoopbackExists(uniqueId); }, 5000));

    Sleep(500);
    auto const firstAfter = received[0].load();
    auto const secondAfter = received[1].load();
    Sleep(1000);

    VERIFY_ARE_EQUAL(received[0].load(), firstAfter);
    VERIFY_ARE_EQUAL(received[1].load(), secondAfter);

    flood.Stop();

    LOG_OUTPUT(L"midiOutShortMsg: %llu succeeded, %llu MMSYSERR_NODRIVER, %llu other failures", flood.Succeeded(), flood.NoDriver(), flood.OtherFailures());

    VerifyServiceStillLoopsMessages();

    // a removed port still closes cleanly
    for (auto& input : inputs)
    {
        midiInStop(input);
        midiInReset(input);
        VERIFY_ARE_EQUAL(midiInClose(input), static_cast<MMRESULT>(MMSYSERR_NOERROR));
        input = nullptr;
    }

    midiOutReset(output);
    VERIFY_ARE_EQUAL(midiOutClose(output), static_cast<MMRESULT>(MMSYSERR_NOERROR));
    output = nullptr;
}


void MidiBasicLoopbackStressTests::TestRemoveWithAssociationIdItDoesNotOwnIsRejected()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());
    VERIFY_IS_TRUE(loopback2::MidiLoopbackManager::IsTransportAvailable());

    auto const basicUniqueId = MakeUniqueId(L"SOWNBASIC");
    auto const basic = CreateLoopback(L"Stress Basic Loopback Ownership", basicUniqueId);
    auto const basicAssociationId = basic.CreatedLoopbackEntry().AssociationId();
    auto const basicEndpointDeviceId = basic.CreatedLoopbackEntry().EndpointDeviceId();

    bool basicRemoved{ false };
    auto cleanupBasic = wil::scope_exit([&] { if (!basicRemoved) RemoveLoopbackQuietly(basicAssociationId); });

    loopback2::MidiLoopbackEndpointDefinition definitionA(L"Stress MIDI 2.0 Loopback Ownership A", L"Created by the basic loopback stress tests.", winrt::hstring{ MakeUniqueId(L"SOWNA") });
    loopback2::MidiLoopbackEndpointDefinition definitionB(L"Stress MIDI 2.0 Loopback Ownership B", L"Created by the basic loopback stress tests.", winrt::hstring{ MakeUniqueId(L"SOWNB") });

    auto const pair = loopback2::MidiLoopbackManager::CreateTransientLoopback(loopback2::MidiLoopbackCreationConfig(definitionA, definitionB));

    VERIFY_IS_NOT_NULL(pair);
    VERIFY_IS_TRUE(pair.Success());

    auto const pairAssociationId = pair.CreatedLoopbackEntry().AssociationId();
    auto const endpointA = pair.CreatedLoopbackEntry().EndpointA().EndpointDeviceId();
    auto const endpointB = pair.CreatedLoopbackEntry().EndpointB().EndpointDeviceId();

    auto cleanupPair = wil::scope_exit([&]
        {
            try
            {
                loopback2::MidiLoopbackManager::RemoveTransientLoopback(loopback2::MidiLoopbackRemovalConfig(pairAssociationId));
            }
            catch (...)
            {
            }
        });

    // A and B are cross-wired: what goes to A comes out of B
    auto pairStillWorks = [&](uint32_t const word)
        {
            Receiver b{};
            Receiver a{};

            if (!b.Open(endpointB, L"Basic loopback stress MIDI 2.0 B") || !a.Open(endpointA, L"Basic loopback stress MIDI 2.0 A"))
            {
                return false;
            }

            b.Watch(word);

            return MidiEndpointConnection::SendMessageSucceeded(a.Connection.SendSingleMessageWords(MidiClock::TimestampConstantSendImmediately(), word)) &&
                b.WaitToHear(5000);
        };

    VERIFY_IS_TRUE(pairStillWorks(NoteOnWord(1)));

    LOG_OUTPUT(L"Asking the basic loopback transport to remove the MIDI 2.0 loopback's association id");

    auto const foreign = MidiBasicLoopbackManager::RemoveTransientLoopback(MidiBasicLoopbackRemovalConfig(pairAssociationId));

    VERIFY_IS_NOT_NULL(foreign);
    VERIFY_IS_FALSE(foreign.Success());
    VERIFY_ARE_EQUAL(static_cast<uint32_t>(foreign.ErrorCode()), static_cast<uint32_t>(MidiBasicLoopbackErrorCode::EndpointNotFound));

    VERIFY_IS_TRUE(pairStillWorks(NoteOnWord(2)));
    VERIFY_IS_TRUE(LoopbackExists(basicUniqueId));
    VERIFY_IS_TRUE(RoundTrip(basicEndpointDeviceId, NoteOnWord(3)));

    LOG_OUTPUT(L"Removing the basic loopback twice, and an association id nobody owns");

    RemoveLoopback(basicAssociationId);
    basicRemoved = true;

    auto const twice = MidiBasicLoopbackManager::RemoveTransientLoopback(MidiBasicLoopbackRemovalConfig(basicAssociationId));

    VERIFY_IS_NOT_NULL(twice);
    VERIFY_IS_FALSE(twice.Success());
    VERIFY_ARE_EQUAL(static_cast<uint32_t>(twice.ErrorCode()), static_cast<uint32_t>(MidiBasicLoopbackErrorCode::EndpointNotFound));

    auto const nobody = MidiBasicLoopbackManager::RemoveTransientLoopback(MidiBasicLoopbackRemovalConfig(foundation::GuidHelper::CreateNewGuid()));

    VERIFY_IS_NOT_NULL(nobody);
    VERIFY_IS_FALSE(nobody.Success());

    VERIFY_IS_TRUE(pairStillWorks(NoteOnWord(4)));
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
}


void MidiBasicLoopbackStressTests::TestRemovingLoopbackLeavesLoopbackWithLongerUniqueIdAlone()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    auto const shortUniqueId = MakeUniqueId(L"SPREFIX");
    auto const longUniqueId = shortUniqueId + L"2";

    auto const shorter = CreateLoopback(L"Stress Basic Loopback Prefix", shortUniqueId);
    auto const shortAssociationId = shorter.CreatedLoopbackEntry().AssociationId();

    bool shortRemoved{ false };
    auto cleanupShort = wil::scope_exit([&] { if (!shortRemoved) RemoveLoopbackQuietly(shortAssociationId); });

    auto const longer = TryCreateLoopback(L"Stress Basic Loopback Prefix Longer", longUniqueId);
    VERIFY_IS_NOT_NULL(longer);

    if (!longer.Success())
    {
        // refusing the second identifier keeps the first from ever taking it out
        LOG_OUTPUT(L"Refused: %s", longer.ErrorMessage().c_str());
        VERIFY_ARE_EQUAL(static_cast<uint32_t>(longer.ErrorCode()), static_cast<uint32_t>(MidiBasicLoopbackErrorCode::DuplicateUniqueId));
        VERIFY_IS_TRUE(RoundTrip(shorter.CreatedLoopbackEntry().EndpointDeviceId(), NoteOnWord(1)));

        LOG_OUTPUT(L"The other way around: with only the longer identifier in use, the shorter one is refused");
        RemoveLoopback(shortAssociationId);
        shortRemoved = true;

        auto const longOnly = CreateLoopback(L"Stress Basic Loopback Prefix Longer", longUniqueId);
        auto const longOnlyAssociationId = longOnly.CreatedLoopbackEntry().AssociationId();
        auto cleanupLongOnly = wil::scope_exit([&] { RemoveLoopbackQuietly(longOnlyAssociationId); });

        auto const shorterAgain = TryCreateLoopback(L"Stress Basic Loopback Prefix", shortUniqueId);
        VERIFY_IS_NOT_NULL(shorterAgain);

        auto cleanupShorterAgain = wil::scope_exit([&] { if (shorterAgain.Success()) RemoveLoopbackQuietly(shorterAgain.CreatedLoopbackEntry().AssociationId()); });

        VERIFY_IS_FALSE(shorterAgain.Success());
        VERIFY_ARE_EQUAL(static_cast<uint32_t>(shorterAgain.ErrorCode()), static_cast<uint32_t>(MidiBasicLoopbackErrorCode::DuplicateUniqueId));
        VERIFY_IS_TRUE(RoundTrip(longOnly.CreatedLoopbackEntry().EndpointDeviceId(), NoteOnWord(2)));

        return;
    }

    auto const longAssociationId = longer.CreatedLoopbackEntry().AssociationId();
    auto const longEndpointDeviceId = longer.CreatedLoopbackEntry().EndpointDeviceId();

    auto cleanupLong = wil::scope_exit([&] { RemoveLoopbackQuietly(longAssociationId); });

    Receiver longReceiver{};
    VERIFY_IS_TRUE(longReceiver.Open(longEndpointDeviceId, L"Basic loopback stress longer unique id"));
    VERIFY_IS_TRUE(longReceiver.SendAndHearBack(NoteOnWord(1), 5000));

    LOG_OUTPUT(L"Removing the loopback with the shorter unique identifier");
    RemoveLoopback(shortAssociationId);
    shortRemoved = true;

    VERIFY_IS_TRUE(WaitFor([&] { return !LoopbackExists(shortUniqueId); }, 5000));
    Sleep(1000);

    VERIFY_IS_TRUE(LoopbackExists(longUniqueId));
    VERIFY_IS_TRUE(longReceiver.SendAndHearBack(NoteOnWord(2), 5000));
    VERIFY_IS_TRUE(RoundTrip(longEndpointDeviceId, NoteOnWord(3)));

    longReceiver.Close();
}


void MidiBasicLoopbackStressTests::TestUniqueIdOfTransportParentIsRejected()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    if (!TransportRefusesOverlappingUniqueIds())
    {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"The installed basic loopback transport predates the reserved unique identifier check, so the reserved identifier is not sent to it.");
        return;
    }

    auto const control = CreateLoopback(L"Stress Basic Loopback Reserved Id Control", MakeUniqueId(L"SRESERVED"));
    auto const controlAssociationId = control.CreatedLoopbackEntry().AssociationId();

    auto cleanupControl = wil::scope_exit([&] { RemoveLoopbackQuietly(controlAssociationId); });

    for (auto const uniqueId : { L"TRANSPORT", L"transport", L"Transport" })
    {
        auto const response = TryCreateLoopback(L"Stress Basic Loopback Reserved Id", uniqueId);
        VERIFY_IS_NOT_NULL(response);

        auto cleanupUnexpected = wil::scope_exit([&] { if (response.Success()) RemoveLoopbackQuietly(response.CreatedLoopbackEntry().AssociationId()); });

        VERIFY_IS_FALSE(response.Success());
        VERIFY_ARE_EQUAL(static_cast<uint32_t>(response.ErrorCode()), static_cast<uint32_t>(MidiBasicLoopbackErrorCode::InvalidUniqueId));

        LOG_OUTPUT(L"%s refused: %s", uniqueId, response.ErrorMessage().c_str());
    }

    VERIFY_IS_TRUE(RoundTrip(control.CreatedLoopbackEntry().EndpointDeviceId(), NoteOnWord(1)));
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
}
