// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Tells the customer when a loopback muted itself because MIDI was feeding back into it.
//
// Runs on the app's message thread, like the network notifier. The service's signal only says
// "ask again"; what is muted, and since when, comes from the service through the SDK.
class LoopbackFeedbackNotifier
{
public:
    void Evaluate() noexcept;

private:
    struct MutedLoopback
    {
        // association id and the time it tripped, so the same loopback tripping again after an
        // unmute is news, and the same trip seen twice is not
        std::wstring Identity;
        std::wstring DisplayName;   // already sanitized for a toast
    };

    std::vector<MutedLoopback> ReadMutedLoopbacks() const noexcept;

    void Notify(_In_ std::vector<MutedLoopback> const& muted) noexcept;

    ToastSender m_toast{ };

    std::set<std::wstring> m_reportedIdentities{ };

    std::chrono::steady_clock::time_point m_lastToast{ };

    static constexpr wchar_t ToastTag[]{ L"loopback-feedback" };

    // A customer who unmutes without fixing the loop trips it again within a second or two. One
    // banner already says what to do, so a burst of them would only be noise.
    static constexpr std::chrono::seconds MinimumIntervalBetweenToasts{ 20 };
};
