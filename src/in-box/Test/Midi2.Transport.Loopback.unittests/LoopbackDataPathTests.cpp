// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include "LoopbackDataPathTests.h"
#include "LoopbackTestSupport.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace LoopbackTest;

namespace
{
    // different notes, as a chord would have, and every one of them keeps going around
    constexpr uint32_t FeedbackLoopMessageCount{ 32 };

    // The detector needs about a second of sustained feedback before the mute is applied.
    constexpr DWORD FeedbackMuteWaitMilliseconds{ 15000 };

    // several times longer than protection needs to act
    constexpr DWORD FeedbackIgnoredWaitMilliseconds{ 5000 };


    bool FeedbackProtectionTestable()
    {
        if (!Feature_Servicing_MIDI2LoopbackFeedbackProtection::IsEnabled())
        {
            Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2LoopbackFeedbackProtection is disabled.");
            return false;
        }

        // protection acts by muting, and the list is how it is reported
        if (!Feature_Servicing_MIDI2LoopbackMuteAndList::IsEnabled())
        {
            Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2LoopbackMuteAndList is disabled.");
            return false;
        }

        if (!LoopbackAvailable())
        {
            Log::Result(TestResults::Skipped, L"Loopback transport is not available.");
            return false;
        }

        return true;
    }


    // The loop a customer makes by accident: whatever arrives at either end goes straight back out
    // of that same end, so it crosses to the other side and comes back again.
    void StartFeedbackLoop(_Inout_ OpenLoopback& loopback)
    {
        loopback.ReceivedA->EchoThrough(loopback.A.get());
        loopback.ReceivedB->EchoThrough(loopback.B.get());

        uint32_t failedSends{ 0 };

        for (uint32_t i = 0; i < FeedbackLoopMessageCount; i++)
        {
            if (FAILED(MidiTest::SendWords(loopback.A.get(), { 0x40900000 | ((i & 0x7F) << 8), 0x80000000 })))
            {
                failedSends++;
            }
        }

        VERIFY_ARE_EQUAL(failedSends, (uint32_t)0);
    }


    void StopFeedbackLoop(_Inout_ OpenLoopback& loopback) noexcept
    {
        loopback.ReceivedA->EchoThrough(nullptr);
        loopback.ReceivedB->EchoThrough(nullptr);
    }


    bool WaitForFeedbackMute(_In_ std::wstring const& associationId, _In_ DWORD const timeoutMilliseconds)
    {
        return MidiTest::WaitUntil([&] { return GetReportedState(associationId).MutedForFeedback; }, timeoutMilliseconds);
    }
}


bool LoopbackDataPathTests::TestSetup()
{
    m_deviceNodeTracker.Start();

    return true;
}

bool LoopbackDataPathTests::TestCleanup()
{
    m_deviceNodeTracker.RemoveDeviceNodesCreatedSinceStart();

    return true;
}


void LoopbackDataPathTests::TestMutedLoopbackDropsMessages()
{
    if (!Feature_Servicing_MIDI2LoopbackMuteAndList::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2LoopbackMuteAndList is disabled.");
        return;
    }

    if (!LoopbackAvailable())
    {
        Log::Result(TestResults::Skipped, L"Loopback transport is not available.");
        return;
    }

    MidiTest::MultithreadedApartment apartment{ };

    OpenLoopback loopback{ };
    auto cleanup = wil::scope_exit([&] { CloseAndRemove(loopback); });

    OpenBothSides(L"Mute Test", true, loopback);

    std::vector<uint32_t> const beforeMute{ 0x40903C00, 0x12340000 };
    VERIFY_SUCCEEDED(MidiTest::SendWords(loopback.A.get(), beforeMute));
    VERIFY_IS_TRUE(loopback.ReceivedB->WaitForMessage(beforeMute, MessageWaitMilliseconds));

    VERIFY_IS_TRUE(SendLoopbackCommand(L"mute", loopback.Loopback.AssociationId).IsSuccess());

    std::vector<uint32_t> const whileMuted{ 0x40903D00, 0x12340000 };
    VERIFY_SUCCEEDED(MidiTest::SendWords(loopback.A.get(), whileMuted));

    // long enough that it would have arrived had the loopback not been muted
    VERIFY_IS_FALSE(loopback.ReceivedB->WaitForMessage(whileMuted, 2000));

    VERIFY_IS_TRUE(SendLoopbackCommand(L"unmute", loopback.Loopback.AssociationId).IsSuccess());

    std::vector<uint32_t> const afterUnmute{ 0x40903E00, 0x12340000 };
    VERIFY_SUCCEEDED(MidiTest::SendWords(loopback.A.get(), afterUnmute));
    VERIFY_IS_TRUE(loopback.ReceivedB->WaitForMessage(afterUnmute, MessageWaitMilliseconds));
}


void LoopbackDataPathTests::TestFeedbackLoopMutesLoopback()
{
    if (!FeedbackProtectionTestable())
    {
        return;
    }

    MidiTest::MultithreadedApartment apartment{ };

    OpenLoopback loopback{ };
    auto cleanup = wil::scope_exit([&] { CloseAndRemove(loopback); });

    OpenBothSides(L"Feedback Loop Test", false, loopback);

    StartFeedbackLoop(loopback);

    auto const muted = WaitForFeedbackMute(loopback.Loopback.AssociationId, FeedbackMuteWaitMilliseconds);

    StopFeedbackLoop(loopback);

    Log::Comment(String().Format(L"Messages echoed: %llu", loopback.ReceivedA->EchoCount() + loopback.ReceivedB->EchoCount()));

    VERIFY_IS_TRUE(muted, L"the loopback should have muted itself");

    // the same mute a customer applies by hand, so every app sees it the same way
    auto const state = GetReportedState(loopback.Loopback.AssociationId);
    VERIFY_IS_TRUE(state.Found);
    VERIFY_IS_TRUE(state.Muted);
    VERIFY_IS_TRUE(state.MutedForFeedback);

    // unmuting is how the customer says the loop is fixed, and it clears the report
    VERIFY_IS_TRUE(SendLoopbackCommand(L"unmute", loopback.Loopback.AssociationId).IsSuccess());

    auto const unmuted = GetReportedState(loopback.Loopback.AssociationId);
    VERIFY_IS_TRUE(unmuted.Found);
    VERIFY_IS_FALSE(unmuted.Muted);
    VERIFY_IS_FALSE(unmuted.MutedForFeedback);
}


void LoopbackDataPathTests::TestFeedbackLoopWithProtectionOffIsNotMuted()
{
    if (!FeedbackProtectionTestable())
    {
        return;
    }

    MidiTest::MultithreadedApartment apartment{ };

    OpenLoopback loopback{ };
    auto cleanup = wil::scope_exit([&] { CloseAndRemove(loopback); });

    OpenBothSides(L"Feedback Loop Off Test", false, loopback);

    VERIFY_IS_TRUE(SendLoopbackCommand(
        L"setFeedbackProtection",
        loopback.Loopback.AssociationId,
        L",\"feedbackProtection\":\"off\"").IsSuccess());

    StartFeedbackLoop(loopback);

    auto const muted = WaitForFeedbackMute(loopback.Loopback.AssociationId, FeedbackIgnoredWaitMilliseconds);
    auto const echoed = loopback.ReceivedA->EchoCount() + loopback.ReceivedB->EchoCount();

    StopFeedbackLoop(loopback);

    Log::Comment(String().Format(L"Messages echoed: %llu", echoed));

    VERIFY_IS_FALSE(muted, L"with protection off, the loopback has to be left alone");

    // and the loop really was running, or not muting it proves nothing
    VERIFY_IS_GREATER_THAN(echoed, (uint64_t)10000);

    VERIFY_IS_FALSE(GetReportedState(loopback.Loopback.AssociationId).Muted);
}


// A sender which keeps sending, fast and repetitive, looks like a loop until the pause shows it is
// not. It must never be muted, and the check must not lose or reorder anything.
void LoopbackDataPathTests::TestSteadyTrafficIsNotMuted()
{
    if (!FeedbackProtectionTestable())
    {
        return;
    }

    MidiTest::MultithreadedApartment apartment{ };

    OpenLoopback loopback{ };
    auto cleanup = wil::scope_exit([&] { CloseAndRemove(loopback); });

    OpenBothSides(L"Steady Traffic Test", true, loopback);

    constexpr uint32_t messageCount{ 30000 };

    // Sixteen notes over and over, identical each time around. That is what makes it look like a
    // loop, and it also means the order can be checked: message n is always note n % 16.
    uint32_t failedSends{ 0 };

    for (uint32_t i = 0; i < messageCount; i++)
    {
        if (FAILED(MidiTest::SendWords(loopback.A.get(), { 0x40900000 | ((i & 0x0F) << 8), 0x80000000 })))
        {
            failedSends++;
        }

        // several thousand a second for a few seconds: well past the point where the checks start
        if ((i % 100) == 99)
        {
            Sleep(10);
        }
    }

    VERIFY_ARE_EQUAL(failedSends, (uint32_t)0);

    auto const gotEverything = MidiTest::WaitUntil([&] { return loopback.ReceivedB->Count() >= messageCount; }, 15000);

    VERIFY_IS_TRUE(gotEverything, String().Format(
        L"a pause to check for feedback must not lose anything: %u of %u arrived",
        static_cast<uint32_t>(loopback.ReceivedB->Count()),
        messageCount));

    auto const received = loopback.ReceivedB->Messages();

    uint32_t outOfOrder{ 0 };

    for (size_t i = 0; i < received.size(); i++)
    {
        if (((received[i][0] >> 8) & 0x7F) != (i & 0x0F))
        {
            outOfOrder++;
        }
    }

    VERIFY_ARE_EQUAL(outOfOrder, (uint32_t)0);

    auto const state = GetReportedState(loopback.Loopback.AssociationId);
    VERIFY_IS_TRUE(state.Found);
    VERIFY_IS_FALSE(state.Muted);
    VERIFY_IS_FALSE(state.MutedForFeedback);
}
