// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "CapabilityInquiry.MidiPropertySubscriptionUpdatedEventArgs.g.h"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    struct MidiPropertySubscriptionUpdatedEventArgs : MidiPropertySubscriptionUpdatedEventArgsT<MidiPropertySubscriptionUpdatedEventArgs>
    {
        MidiPropertySubscriptionUpdatedEventArgs() = default;

        ci::MidiPropertySubscription Subscription() const noexcept { return m_subscription; }
        winrt::hstring Command() const noexcept { return m_command; }
        bool IsSubscriptionEnded() const noexcept { return m_isSubscriptionEnded; }
        ci::MidiPropertyExchangeResponse Update() const noexcept { return m_update; }

        // Not projected. The session fills these in before raising the event.
        void InternalSetSubscription(_In_ ci::MidiPropertySubscription const& value) noexcept { m_subscription = value; }
        void InternalSetCommand(_In_ winrt::hstring const& value) noexcept { m_command = value; }
        void InternalSetIsSubscriptionEnded(_In_ bool const value) noexcept { m_isSubscriptionEnded = value; }
        void InternalSetUpdate(_In_ ci::MidiPropertyExchangeResponse const& value) noexcept { m_update = value; }

    private:
        ci::MidiPropertySubscription m_subscription{ nullptr };
        winrt::hstring m_command{};
        bool m_isSubscriptionEnded{ false };
        ci::MidiPropertyExchangeResponse m_update{ nullptr };
    };
}
