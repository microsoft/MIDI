// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

#include <winrt/Windows.Devices.Midi2.Transports.BasicLoopback.h>

#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "..\LoopbackStressHelpers.h"
#include "MidiLoopbackStressTests.h"

#include "Feature_Servicing_MIDI2LoopbackAddRemoveReliability.h"

#pragma comment(lib, "winmm.lib")

namespace basic = winrt::Windows::Devices::Midi2::Transports::BasicLoopback;

using namespace LoopbackStress;

namespace
{
    struct Pair
    {
        winrt::guid AssociationId{};
        winrt::hstring EndpointA{};
        winrt::hstring EndpointB{};
    };

    MidiLoopbackCreationResponse TryCreatePair(
        _In_ std::wstring const& name,
        _In_ std::wstring const& uniqueIdA,
        _In_ std::wstring const& uniqueIdB)
    {
        MidiLoopbackEndpointDefinition definitionA(
            winrt::hstring{ name + L" A" },
            L"Created by the MIDI 2.0 loopback stress tests",
            winrt::hstring{ uniqueIdA });

        MidiLoopbackEndpointDefinition definitionB(
            winrt::hstring{ name + L" B" },
            L"Created by the MIDI 2.0 loopback stress tests",
            winrt::hstring{ uniqueIdB });

        return MidiLoopbackManager::CreateTransientLoopback(MidiLoopbackCreationConfig(definitionA, definitionB));
    }

    Pair CreatePair(_In_ std::wstring const& name, _In_ std::wstring const& uniqueIdA, _In_ std::wstring const& uniqueIdB)
    {
        auto const response = TryCreatePair(name, uniqueIdA, uniqueIdB);

        if (!response.Success())
        {
            LOG_OUTPUT(L"Create failed. Error code 0x%08x: %s", static_cast<uint32_t>(response.ErrorCode()), response.ErrorMessage().c_str());
        }

        VERIFY_IS_TRUE(response.Success());

        return Pair{
            response.CreatedLoopbackEntry().AssociationId(),
            response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId(),
            response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId() };
    }

    void RemovePair(_In_ winrt::guid const& associationId)
    {
        auto const response = MidiLoopbackManager::RemoveTransientLoopback(MidiLoopbackRemovalConfig(associationId));

        if (!response.Success())
        {
            LOG_OUTPUT(L"Remove failed. Error code 0x%08x: %s", static_cast<uint32_t>(response.ErrorCode()), response.ErrorMessage().c_str());
        }

        VERIFY_IS_TRUE(response.Success());
    }

    // For cleanup, which must never throw. Removing one which is already gone just fails.
    void RemovePairQuietly(_In_ winrt::guid const& associationId) noexcept
    {
        try
        {
            MidiLoopbackManager::RemoveTransientLoopback(MidiLoopbackRemovalConfig(associationId));
        }
        catch (...)
        {
        }
    }

    bool PairExists(_In_ std::wstring const& uniqueIdA, _In_ std::wstring const& uniqueIdB)
    {
        return MidiLoopbackManager::DoesLoopbackAExist(winrt::hstring{ uniqueIdA }) && MidiLoopbackManager::DoesLoopbackBExist(winrt::hstring{ uniqueIdB });
    }

    bool PairGone(_In_ std::wstring const& uniqueIdA, _In_ std::wstring const& uniqueIdB)
    {
        return !MidiLoopbackManager::DoesLoopbackAExist(winrt::hstring{ uniqueIdA }) && !MidiLoopbackManager::DoesLoopbackBExist(winrt::hstring{ uniqueIdB });
    }

    bool Send(_In_ Receiver& from, _In_ uint32_t const word)
    {
        return MidiEndpointConnection::SendMessageSucceeded(
            from.Connection.SendSingleMessageWords(MidiClock::TimestampConstantSendImmediately(), word));
    }

    // Fresh connections on both sides. What one side sends, only the other side hears.
    bool CrossTrip(_In_ winrt::hstring const& fromEndpoint, _In_ winrt::hstring const& toEndpoint, _In_ uint32_t const word)
    {
        Receiver to{};
        Receiver from{};

        if (!to.Open(toEndpoint, L"MIDI 2.0 loopback stress listener") ||
            !from.Open(fromEndpoint, L"MIDI 2.0 loopback stress sender"))
        {
            return false;
        }

        to.Watch(word);

        return Send(from, word) && to.WaitToHear(5000);
    }

    // Proves MIDI still works for everyone else, on a loopback which has nothing to do with the test
    void VerifyServiceStillPassesMessages()
    {
        VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());

        auto const uniqueId = MakeUniqueId(L"SHEALTH");
        auto const pair = CreatePair(L"Stress Loopback Health Check", uniqueId, uniqueId);

        auto cleanup = wil::scope_exit([&] { RemovePairQuietly(pair.AssociationId); });

        VERIFY_IS_TRUE(CrossTrip(pair.EndpointA, pair.EndpointB, NoteOnWord(98)));
        VERIFY_IS_TRUE(CrossTrip(pair.EndpointB, pair.EndpointA, NoteOnWord(99)));

        RemovePair(pair.AssociationId);
        cleanup.release();
    }

    void VerifyServiceDidNotRestart(_In_ DWORD const processIdAtStart)
    {
        auto const processIdNow = MidiServiceProcessId();

        if (processIdNow != processIdAtStart)
        {
            LOG_OUTPUT(L"The MIDI service stopped during the test. Its process id was %u and is now %u.", processIdAtStart, processIdNow);
        }

        VERIFY_ARE_EQUAL(processIdNow, processIdAtStart);
    }

    void VerifyRemovalRefusedAsNotFound(_In_ MidiLoopbackRemovalResponse const& response)
    {
        VERIFY_IS_FALSE(response.Success());

        if (Feature_Servicing_MIDI2LoopbackAddRemoveReliability::IsEnabled())
        {
            VERIFY_ARE_EQUAL(static_cast<uint32_t>(response.ErrorCode()), static_cast<uint32_t>(MidiLoopbackErrorCode::EndpointNotFound));
            VERIFY_IS_FALSE(response.ErrorMessage().empty());
        }
    }
}


bool MidiLoopbackStressTests::TestSetup()
{
    m_deviceNodeTracker.Start();

    return true;
}

bool MidiLoopbackStressTests::TestCleanup()
{
    m_deviceNodeTracker.RemoveDeviceNodesCreatedSinceStart();

    return true;
}


void MidiLoopbackStressTests::TestCreateAndRemoveSamePairRepeatedly()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    auto const uniqueIdA = MakeUniqueId(L"SREPEATA");
    auto const uniqueIdB = MakeUniqueId(L"SREPEATB");

    constexpr uint32_t iterations{ 20 };

    winrt::hstring firstEndpointA{};
    winrt::hstring firstEndpointB{};

    for (uint32_t i = 0; i < iterations; i++)
    {
        auto const pair = CreatePair(L"Stress Loopback Repeat", uniqueIdA, uniqueIdB);

        auto cleanup = wil::scope_exit([&] { RemovePairQuietly(pair.AssociationId); });

        if (i == 0)
        {
            firstEndpointA = pair.EndpointA;
            firstEndpointB = pair.EndpointB;
        }
        else
        {
            // same unique identifiers, so the same endpoints come back
            VERIFY_IS_TRUE(HStringsAreCaseInsensitiveEqual(pair.EndpointA, firstEndpointA));
            VERIFY_IS_TRUE(HStringsAreCaseInsensitiveEqual(pair.EndpointB, firstEndpointB));
        }

        VERIFY_IS_TRUE(WaitFor([&] { return PairExists(uniqueIdA, uniqueIdB); }, 5000));

        VERIFY_IS_TRUE(CrossTrip(pair.EndpointA, pair.EndpointB, NoteOnWord(i)));
        VERIFY_IS_TRUE(CrossTrip(pair.EndpointB, pair.EndpointA, NoteOnWord(i + 1)));

        RemovePair(pair.AssociationId);
        cleanup.release();

        VERIFY_IS_TRUE(WaitFor([&] { return PairGone(uniqueIdA, uniqueIdB); }, 5000));
    }

    LOG_OUTPUT(L"Created and removed the same loopback %u times", iterations);

    VerifyServiceStillPassesMessages();
}


void MidiLoopbackStressTests::TestRemoveWhileReceiversConnectedAndSending()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    auto const processIdAtStart = MidiServiceProcessId();

    auto const uniqueIdA = MakeUniqueId(L"SLOADA");
    auto const uniqueIdB = MakeUniqueId(L"SLOADB");
    auto const pair = CreatePair(L"Stress Loopback Load", uniqueIdA, uniqueIdB);

    bool removed{ false };
    auto cleanup = wil::scope_exit([&] { if (!removed) RemovePairQuietly(pair.AssociationId); });

    // three applications listening on each side, and one sending into each side
    constexpr uint32_t receiversPerSide{ 3 };
    std::vector<std::unique_ptr<Receiver>> receiversA{};
    std::vector<std::unique_ptr<Receiver>> receiversB{};

    for (uint32_t i = 0; i < receiversPerSide; i++)
    {
        receiversA.push_back(std::make_unique<Receiver>());
        VERIFY_IS_TRUE(receiversA.back()->Open(pair.EndpointA, L"MIDI 2.0 loopback stress receiver A"));

        receiversB.push_back(std::make_unique<Receiver>());
        VERIFY_IS_TRUE(receiversB.back()->Open(pair.EndpointB, L"MIDI 2.0 loopback stress receiver B"));
    }

    Receiver senderA{};
    VERIFY_IS_TRUE(senderA.Open(pair.EndpointA, L"MIDI 2.0 loopback stress sender A"));

    Receiver senderB{};
    VERIFY_IS_TRUE(senderB.Open(pair.EndpointB, L"MIDI 2.0 loopback stress sender B"));

    // every receiver hears the first message before the floods start
    receiversB.front()->Watch(NoteOnWord(1));
    VERIFY_IS_TRUE(Send(senderA, NoteOnWord(1)));
    VERIFY_IS_TRUE(receiversB.front()->WaitToHear(5000));

    receiversA.front()->Watch(NoteOnWord(2));
    VERIFY_IS_TRUE(Send(senderB, NoteOnWord(2)));
    VERIFY_IS_TRUE(receiversA.front()->WaitToHear(5000));

    for (auto const& receiver : receiversA)
    {
        VERIFY_IS_TRUE(WaitFor([&] { return receiver->Count.load() > 0; }, 5000));
    }

    for (auto const& receiver : receiversB)
    {
        VERIFY_IS_TRUE(WaitFor([&] { return receiver->Count.load() > 0; }, 5000));
    }

    ConnectionFlood floodAToB{};
    floodAToB.Start(senderA.Connection);

    ConnectionFlood floodBToA{};
    floodBToA.Start(senderB.Connection);

    // long enough, at this rate, for feedback protection to start holding messages to test them
    Sleep(1500);

    auto const countsABeforeRemove = SnapshotCounts(receiversA);
    auto const countsBBeforeRemove = SnapshotCounts(receiversB);

    for (auto const count : countsABeforeRemove)
    {
        VERIFY_IS_GREATER_THAN(count, static_cast<uint64_t>(1));
    }

    for (auto const count : countsBBeforeRemove)
    {
        VERIFY_IS_GREATER_THAN(count, static_cast<uint64_t>(1));
    }

    LOG_OUTPUT(L"Flooded A to B with %llu messages and B to A with %llu in 1.5 s", floodAToB.MessagesSent(), floodBToA.MessagesSent());
    LOG_OUTPUT(L"The first receiver on A heard %llu, the first on B heard %llu", countsABeforeRemove.front(), countsBBeforeRemove.front());

    auto const removeStart = StressClock::now();
    RemovePair(pair.AssociationId);
    removed = true;
    auto const removeMs = MillisecondsSince(removeStart);

    LOG_OUTPUT(L"Removed the loopback in %lld ms with %u receivers connected and both senders still sending", removeMs, receiversPerSide * 2);
    VERIFY_IS_LESS_THAN(removeMs, 10'000LL);

    VERIFY_IS_TRUE(WaitFor([&] { return PairGone(uniqueIdA, uniqueIdB); }, 5000));

    // messages already on their way may still land, so the comparison starts once they have
    Sleep(500);
    auto const countsAAfterRemove = SnapshotCounts(receiversA);
    auto const countsBAfterRemove = SnapshotCounts(receiversB);
    Sleep(1000);

    VERIFY_IS_TRUE(SnapshotCounts(receiversA) == countsAAfterRemove);
    VERIFY_IS_TRUE(SnapshotCounts(receiversB) == countsBAfterRemove);

    floodAToB.Stop();
    floodBToA.Stop();

    LOG_OUTPUT(L"Sender A: %llu messages accepted, %llu sends refused", floodAToB.MessagesSent(), floodAToB.FailedSends());
    LOG_OUTPUT(L"Sender B: %llu messages accepted, %llu sends refused", floodBToA.MessagesSent(), floodBToA.FailedSends());
    VERIFY_IS_FALSE(floodAToB.Threw());
    VERIFY_IS_FALSE(floodBToA.Threw());

    VerifyServiceDidNotRestart(processIdAtStart);
    VerifyServiceStillPassesMessages();

    auto const closeStart = StressClock::now();

    senderA.Close();
    senderB.Close();

    for (auto& receiver : receiversA)
    {
        receiver->Close();
    }

    for (auto& receiver : receiversB)
    {
        receiver->Close();
    }

    auto const closeMs = MillisecondsSince(closeStart);
    LOG_OUTPUT(L"Closed %u connections to the removed loopback in %lld ms", (receiversPerSide * 2) + 2, closeMs);
    VERIFY_IS_LESS_THAN(closeMs, 10'000LL);
}


void MidiLoopbackStressTests::TestRecreateWhileOldConnectionsStillOpen()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    auto const uniqueIdA = MakeUniqueId(L"SRECREATEA");
    auto const uniqueIdB = MakeUniqueId(L"SRECREATEB");

    auto const first = CreatePair(L"Stress Loopback Recreate", uniqueIdA, uniqueIdB);

    bool firstRemoved{ false };
    auto cleanupFirst = wil::scope_exit([&] { if (!firstRemoved) RemovePairQuietly(first.AssociationId); });

    Receiver oldA{};
    VERIFY_IS_TRUE(oldA.Open(first.EndpointA, L"MIDI 2.0 loopback stress old connection A"));

    Receiver oldB{};
    VERIFY_IS_TRUE(oldB.Open(first.EndpointB, L"MIDI 2.0 loopback stress old connection B"));

    oldB.Watch(NoteOnWord(1));
    VERIFY_IS_TRUE(Send(oldA, NoteOnWord(1)));
    VERIFY_IS_TRUE(oldB.WaitToHear(5000));

    LOG_OUTPUT(L"Removing the loopback with a connection still open on each side");
    RemovePair(first.AssociationId);
    firstRemoved = true;

    VERIFY_IS_TRUE(WaitFor([&] { return PairGone(uniqueIdA, uniqueIdB); }, 5000));

    LOG_OUTPUT(L"Creating it again, same unique identifiers");
    auto const second = CreatePair(L"Stress Loopback Recreate", uniqueIdA, uniqueIdB);

    auto cleanupSecond = wil::scope_exit([&] { RemovePairQuietly(second.AssociationId); });

    VERIFY_IS_TRUE(HStringsAreCaseInsensitiveEqual(second.EndpointA, first.EndpointA));
    VERIFY_IS_TRUE(HStringsAreCaseInsensitiveEqual(second.EndpointB, first.EndpointB));
    VERIFY_IS_TRUE(WaitFor([&] { return PairExists(uniqueIdA, uniqueIdB); }, 5000));

    Receiver newA{};
    VERIFY_IS_TRUE(newA.Open(second.EndpointA, L"MIDI 2.0 loopback stress new connection A"));

    Receiver newB{};
    VERIFY_IS_TRUE(newB.Open(second.EndpointB, L"MIDI 2.0 loopback stress new connection B"));

    auto const oldACount = oldA.Count.load();
    auto const oldBCount = oldB.Count.load();

    newB.Watch(NoteOnWord(2));
    VERIFY_IS_TRUE(Send(newA, NoteOnWord(2)));
    VERIFY_IS_TRUE(newB.WaitToHear(5000));

    newA.Watch(NoteOnWord(3));
    VERIFY_IS_TRUE(Send(newB, NoteOnWord(3)));
    VERIFY_IS_TRUE(newA.WaitToHear(5000));

    // the old connections belong to the loopback which was removed, so they hear nothing from the new one
    Sleep(500);
    VERIFY_ARE_EQUAL(oldA.Count.load(), oldACount);
    VERIFY_ARE_EQUAL(oldB.Count.load(), oldBCount);

    // and nothing sent on an old connection reaches the new loopback
    auto const newACount = newA.Count.load();
    auto const newBCount = newB.Count.load();

    Send(oldA, NoteOnWord(4));
    Send(oldB, NoteOnWord(5));
    Sleep(1000);

    VERIFY_ARE_EQUAL(newA.Count.load(), newACount);
    VERIFY_ARE_EQUAL(newB.Count.load(), newBCount);

    LOG_OUTPUT(L"Closing the old connections");
    oldA.Close();
    oldB.Close();

    // closing what was left of the old loopback must not silence the new one
    Sleep(500);

    newB.Watch(NoteOnWord(6));
    VERIFY_IS_TRUE(Send(newA, NoteOnWord(6)));
    VERIFY_IS_TRUE(newB.WaitToHear(5000));

    newA.Watch(NoteOnWord(7));
    VERIFY_IS_TRUE(Send(newB, NoteOnWord(7)));
    VERIFY_IS_TRUE(newA.WaitToHear(5000));

    VERIFY_IS_TRUE(CrossTrip(second.EndpointA, second.EndpointB, NoteOnWord(8)));

    newA.Close();
    newB.Close();
}


void MidiLoopbackStressTests::TestRemoveWhileWinMMPortsOpenAndSending()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    auto const processIdAtStart = MidiServiceProcessId();

    auto const uniqueIdA = MakeUniqueId(L"SWINMMA");
    auto const uniqueIdB = MakeUniqueId(L"SWINMMB");
    auto const pair = CreatePair(L"Stress Loopback WinMM", uniqueIdA, uniqueIdB);

    bool removed{ false };
    auto cleanup = wil::scope_exit([&] { if (!removed) RemovePairQuietly(pair.AssociationId); });

    // sending into A comes out of B
    uint32_t outputPortNumber{ 0 };
    uint32_t inputPortNumber{ 0 };

    VERIFY_IS_TRUE(TryResolveWinMMPortNumber(pair.EndpointA, Midi1PortFlow::MidiMessageDestination, outputPortNumber));
    VERIFY_IS_TRUE(TryResolveWinMMPortNumber(pair.EndpointB, Midi1PortFlow::MidiMessageSource, inputPortNumber));

    LOG_OUTPUT(L"WinMM output %u (side A), input %u (side B)", outputPortNumber, inputPortNumber);

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
    RemovePair(pair.AssociationId);
    removed = true;
    auto const removeMs = MillisecondsSince(removeStart);

    LOG_OUTPUT(L"Removed the loopback in %lld ms with two WinMM inputs and one output open and sending", removeMs);
    VERIFY_IS_LESS_THAN(removeMs, 10'000LL);

    VERIFY_IS_TRUE(WaitFor([&] { return PairGone(uniqueIdA, uniqueIdB); }, 5000));

    Sleep(500);
    auto const firstAfter = received[0].load();
    auto const secondAfter = received[1].load();
    Sleep(1000);

    VERIFY_ARE_EQUAL(received[0].load(), firstAfter);
    VERIFY_ARE_EQUAL(received[1].load(), secondAfter);

    flood.Stop();

    LOG_OUTPUT(L"midiOutShortMsg: %llu succeeded, %llu MMSYSERR_NODRIVER, %llu other failures", flood.Succeeded(), flood.NoDriver(), flood.OtherFailures());

    VerifyServiceDidNotRestart(processIdAtStart);
    VerifyServiceStillPassesMessages();

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


void MidiLoopbackStressTests::TestSideReopenedWhileOtherSideSends()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    auto const processIdAtStart = MidiServiceProcessId();

    auto const uniqueIdA = MakeUniqueId(L"SREOPENA");
    auto const uniqueIdB = MakeUniqueId(L"SREOPENB");
    auto const pair = CreatePair(L"Stress Loopback Reopen", uniqueIdA, uniqueIdB);

    auto cleanup = wil::scope_exit([&] { RemovePairQuietly(pair.AssociationId); });

    constexpr uint32_t quietReopensPerSide{ 10 };
    constexpr uint32_t reopensPerSide{ 100 };

    // The listening side has no other connection, so each open and close starts and stops the
    // service's whole connection to that side while the other side is sending into it.
    auto const reopenWhileFlooding = [&](_In_ winrt::hstring const& sendingEndpoint, _In_ winrt::hstring const& listeningEndpoint, _In_ wchar_t const* sideName)
        {
            Receiver sender{};
            VERIFY_IS_TRUE(sender.Open(sendingEndpoint, L"MIDI 2.0 loopback stress reopen sender"));

            // the same with nothing being sent, to compare against
            PhaseTiming quietOpen{};
            PhaseTiming quietClose{};

            for (uint32_t i = 0; i < quietReopensPerSide; i++)
            {
                Receiver listener{};

                auto const openStart = StressClock::now();
                VERIFY_IS_TRUE(listener.Open(listeningEndpoint, L"MIDI 2.0 loopback stress reopen listener"));
                quietOpen.Add(MillisecondsSince(openStart));

                auto const closeStart = StressClock::now();
                listener.Close();
                quietClose.Add(MillisecondsSince(closeStart));
            }

            LOG_OUTPUT(L"Side %s with nothing being sent: open %lld ms on average (longest %lld), close %lld ms on average (longest %lld)",
                sideName, quietOpen.TotalMs / quietReopensPerSide, quietOpen.LongestMs, quietClose.TotalMs / quietReopensPerSide, quietClose.LongestMs);

            ConnectionFlood flood{};
            flood.Start(sender.Connection);

            uint32_t silentOpens{ 0 };
            PhaseTiming open{};
            PhaseTiming firstMessage{};
            PhaseTiming close{};

            auto const start = StressClock::now();

            for (uint32_t i = 0; i < reopensPerSide; i++)
            {
                Receiver listener{};

                auto const openStart = StressClock::now();
                VERIFY_IS_TRUE(listener.Open(listeningEndpoint, L"MIDI 2.0 loopback stress reopen listener"));
                open.Add(MillisecondsSince(openStart));

                auto const listenStart = StressClock::now();

                if (!WaitFor([&] { return listener.Count.load() > 0; }, 2000))
                {
                    silentOpens++;
                }

                firstMessage.Add(MillisecondsSince(listenStart));

                auto const closeStart = StressClock::now();
                listener.Close();
                close.Add(MillisecondsSince(closeStart));
            }

            auto const elapsedMs = MillisecondsSince(start);

            flood.Stop();

            LOG_OUTPUT(L"Side %s opened and closed %u times in %lld ms while the other side sent %llu messages (%llu sends refused). %u opens heard nothing.",
                sideName, reopensPerSide, elapsedMs, flood.MessagesSent(), flood.FailedSends(), silentOpens);

            LOG_OUTPUT(L"Side %s while the other side sent: open %lld ms on average (longest %lld), first message %lld ms (longest %lld), close %lld ms (longest %lld)",
                sideName,
                open.TotalMs / reopensPerSide, open.LongestMs,
                firstMessage.TotalMs / reopensPerSide, firstMessage.LongestMs,
                close.TotalMs / reopensPerSide, close.LongestMs);

            VERIFY_IS_FALSE(flood.Threw());
            VERIFY_ARE_EQUAL(silentOpens, static_cast<uint32_t>(0));

            sender.Close();
        };

    reopenWhileFlooding(pair.EndpointA, pair.EndpointB, L"B");
    VerifyServiceDidNotRestart(processIdAtStart);

    reopenWhileFlooding(pair.EndpointB, pair.EndpointA, L"A");
    VerifyServiceDidNotRestart(processIdAtStart);

    // and the pair still works both ways afterward
    VERIFY_IS_TRUE(CrossTrip(pair.EndpointA, pair.EndpointB, NoteOnWord(1)));
    VERIFY_IS_TRUE(CrossTrip(pair.EndpointB, pair.EndpointA, NoteOnWord(2)));

    RemovePair(pair.AssociationId);
    cleanup.release();

    VerifyServiceStillPassesMessages();
}


void MidiLoopbackStressTests::TestRemoveWithAssociationIdItDoesNotOwnIsRejected()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    auto const processIdAtStart = MidiServiceProcessId();

    auto const uniqueIdA = MakeUniqueId(L"SFOREIGNA");
    auto const uniqueIdB = MakeUniqueId(L"SFOREIGNB");
    auto const pair = CreatePair(L"Stress Loopback Foreign Id", uniqueIdA, uniqueIdB);

    bool removed{ false };
    auto cleanup = wil::scope_exit([&] { if (!removed) RemovePairQuietly(pair.AssociationId); });

    // a basic loopback, so there is a real association id which belongs to another transport
    if (basic::MidiBasicLoopbackManager::IsTransportAvailable())
    {
        auto const basicUniqueId = MakeUniqueId(L"SFOREIGNBASIC");

        auto const basicResponse = basic::MidiBasicLoopbackManager::CreateTransientLoopback(
            basic::MidiBasicLoopbackCreationConfig(
                basic::MidiBasicLoopbackEndpointDefinition(
                    L"Stress Loopback Foreign Id Basic",
                    L"Created by the MIDI 2.0 loopback stress tests",
                    winrt::hstring{ basicUniqueId })));

        VERIFY_IS_TRUE(basicResponse.Success());

        auto const basicAssociationId = basicResponse.CreatedLoopbackEntry().AssociationId();
        auto const basicEndpoint = basicResponse.CreatedLoopbackEntry().EndpointDeviceId();

        auto cleanupBasic = wil::scope_exit([&]
            {
                try
                {
                    basic::MidiBasicLoopbackManager::RemoveTransientLoopback(basic::MidiBasicLoopbackRemovalConfig(basicAssociationId));
                }
                catch (...)
                {
                }
            });

        LOG_OUTPUT(L"Asking the MIDI 2.0 loopback transport to remove a basic loopback");

        auto const response = MidiLoopbackManager::RemoveTransientLoopback(MidiLoopbackRemovalConfig(basicAssociationId));

        LOG_OUTPUT(L"Success %d, error code 0x%08x: %s", response.Success(), static_cast<uint32_t>(response.ErrorCode()), response.ErrorMessage().c_str());
        VerifyRemovalRefusedAsNotFound(response);

        // the basic loopback is untouched
        VERIFY_IS_TRUE(basic::MidiBasicLoopbackManager::DoesLoopbackExist(winrt::hstring{ basicUniqueId }));

        Receiver basicClient{};
        VERIFY_IS_TRUE(basicClient.Open(basicEndpoint, L"MIDI 2.0 loopback stress basic loopback client"));
        VERIFY_IS_TRUE(basicClient.SendAndHearBack(NoteOnWord(1), 5000));
        basicClient.Close();
    }
    else
    {
        LOG_OUTPUT(L"The basic loopback transport isn't available, so only the ids which belong to nothing are tried");
    }

    // an id which belongs to nothing at all
    {
        auto const response = MidiLoopbackManager::RemoveTransientLoopback(MidiLoopbackRemovalConfig(foundation::GuidHelper::CreateNewGuid()));

        LOG_OUTPUT(L"Random id: success %d, error code 0x%08x: %s", response.Success(), static_cast<uint32_t>(response.ErrorCode()), response.ErrorMessage().c_str());
        VerifyRemovalRefusedAsNotFound(response);
    }

    // the MIDI 2.0 loopback is untouched
    VERIFY_IS_TRUE(PairExists(uniqueIdA, uniqueIdB));
    VERIFY_IS_TRUE(CrossTrip(pair.EndpointA, pair.EndpointB, NoteOnWord(2)));
    VERIFY_IS_TRUE(CrossTrip(pair.EndpointB, pair.EndpointA, NoteOnWord(3)));

    RemovePair(pair.AssociationId);
    removed = true;

    VERIFY_IS_TRUE(WaitFor([&] { return PairGone(uniqueIdA, uniqueIdB); }, 5000));

    // removing it a second time
    {
        auto const response = MidiLoopbackManager::RemoveTransientLoopback(MidiLoopbackRemovalConfig(pair.AssociationId));

        LOG_OUTPUT(L"Second removal: success %d, error code 0x%08x: %s", response.Success(), static_cast<uint32_t>(response.ErrorCode()), response.ErrorMessage().c_str());
        VerifyRemovalRefusedAsNotFound(response);
    }

    VerifyServiceDidNotRestart(processIdAtStart);
    VerifyServiceStillPassesMessages();
}


void MidiLoopbackStressTests::TestOverlappingUniqueIdsAreRejected()
{
    if (!Feature_Servicing_MIDI2LoopbackAddRemoveReliability::IsEnabled())
    {
        LOG_OUTPUT(L"KIR disabled, skipping.");
        return;
    }

    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    auto const processIdAtStart = MidiServiceProcessId();

    // the same identifier on both sides, as the midi console does
    auto const shortId = MakeUniqueId(L"SPREFIX");
    auto const longId = shortId + L"2";
    auto const shorterId = shortId.substr(0, shortId.size() - 1);

    auto const firstPair = CreatePair(L"Stress Loopback Prefix", shortId, shortId);

    bool firstRemoved{ false };
    auto cleanupFirst = wil::scope_exit([&] { if (!firstRemoved) RemovePairQuietly(firstPair.AssociationId); });

    struct
    {
        wchar_t const* Name;
        std::wstring UniqueIdA;
        std::wstring UniqueIdB;
        MidiLoopbackErrorCode ExpectedErrorCode;
    } const attempts[]
    {
        { L"Stress Loopback Prefix Longer", longId, longId, MidiLoopbackErrorCode::DuplicateUniqueIdA },
        { L"Stress Loopback Prefix Shorter", shorterId, shorterId, MidiLoopbackErrorCode::DuplicateUniqueIdA },
        { L"Stress Loopback Prefix Side B", MakeUniqueId(L"SOTHER"), longId, MidiLoopbackErrorCode::DuplicateUniqueIdB },
    };

    for (auto const& attempt : attempts)
    {
        auto const response = TryCreatePair(attempt.Name, attempt.UniqueIdA, attempt.UniqueIdB);

        auto cleanupUnexpected = wil::scope_exit([&] { if (response.Success()) RemovePairQuietly(response.CreatedLoopbackEntry().AssociationId()); });

        LOG_OUTPUT(L"%s: success %d, error code 0x%08x: %s", attempt.Name, response.Success(), static_cast<uint32_t>(response.ErrorCode()), response.ErrorMessage().c_str());

        VERIFY_IS_FALSE(response.Success());
        VERIFY_ARE_EQUAL(static_cast<uint32_t>(response.ErrorCode()), static_cast<uint32_t>(attempt.ExpectedErrorCode));
        VERIFY_IS_FALSE(response.ErrorMessage().empty());
    }

    // the loopback already there is untouched
    VERIFY_IS_TRUE(PairExists(shortId, shortId));
    VERIFY_IS_TRUE(CrossTrip(firstPair.EndpointA, firstPair.EndpointB, NoteOnWord(1)));
    VERIFY_IS_TRUE(CrossTrip(firstPair.EndpointB, firstPair.EndpointA, NoteOnWord(2)));

    RemovePair(firstPair.AssociationId);
    firstRemoved = true;

    VERIFY_IS_TRUE(WaitFor([&] { return PairGone(shortId, shortId); }, 5000));

    // identifiers which share a start, without either one starting with the other, are fine
    auto const siblingOne = CreatePair(L"Stress Loopback Prefix Sibling One", shortId + L"A", shortId + L"A");
    auto cleanupSiblingOne = wil::scope_exit([&] { RemovePairQuietly(siblingOne.AssociationId); });

    auto const siblingTwo = CreatePair(L"Stress Loopback Prefix Sibling Two", shortId + L"B", shortId + L"B");
    auto cleanupSiblingTwo = wil::scope_exit([&] { RemovePairQuietly(siblingTwo.AssociationId); });

    RemovePair(siblingOne.AssociationId);
    cleanupSiblingOne.release();

    VERIFY_IS_TRUE(WaitFor([&] { return PairGone(shortId + L"A", shortId + L"A"); }, 5000));

    Sleep(1000);
    VERIFY_IS_TRUE(PairExists(shortId + L"B", shortId + L"B"));
    VERIFY_IS_TRUE(CrossTrip(siblingTwo.EndpointA, siblingTwo.EndpointB, NoteOnWord(3)));

    VerifyServiceDidNotRestart(processIdAtStart);
}
