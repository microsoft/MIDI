// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Net-new for Feature_Servicing_MIDI2LoopbackFeedbackProtection. One per loopback pair, with a
// guard for each direction. Every copy of the device shares it, so feedback in either direction
// mutes the pair, exactly as the customer's own mute does.
class MidiLoopbackFeedback
{
public:
    struct Status
    {
        bool MutedForFeedback{ false };
        FILETIME DetectedTime{};
        bool DirectionAToB{ true };
        internal::MidiFeedbackTripInfo Info{};
    };

    // Null only when out of memory, and then the pair simply has no protection.
    static std::shared_ptr<MidiLoopbackFeedback> Create(
        _In_ std::wstring const& associationId,
        _In_ bool const enabled) noexcept;

    MidiLoopbackFeedback(_In_ MidiLoopbackFeedback const&) = delete;
    MidiLoopbackFeedback& operator=(_In_ MidiLoopbackFeedback const&) = delete;

    bool IsEnabled() const noexcept { return m_enabled.load(std::memory_order_acquire); }

    // Off releases anything a test is holding, in order. On starts clean.
    void SetEnabled(_In_ bool const enabled) noexcept;

    // Called before the muted flag changes, for either a mute or an unmute, so a trip that is still
    // being handled cannot mute the pair again behind the customer.
    void OnMutedStateChanging() noexcept;

    std::shared_ptr<internal::MidiFeedbackGuard> const& AToB() const noexcept { return m_aToB; }
    std::shared_ptr<internal::MidiFeedbackGuard> const& BToA() const noexcept { return m_bToA; }

    Status GetStatus() const noexcept;

    // Sets the pair's muted flag only if the trip is still standing. Done under the same lock
    // OnMutedStateChanging takes, so a customer's unmute can never be undone by a late trip.
    bool MuteIfStillTripped(_Inout_ bool& isMuted) noexcept;

    // Runs on a detached threadpool thread, never on a send.
    static void MutePairAfterTrip(
        _In_ std::wstring const& associationId,
        _In_ bool const directionAToB,
        _In_ internal::MidiFeedbackTripInfo const& info) noexcept;

private:
    MidiLoopbackFeedback(_In_ std::wstring const& associationId, _In_ bool const enabled) noexcept;

    void OnGuardEvent(
        _In_ bool const directionAToB,
        _In_ internal::MidiFeedbackEvent const feedbackEvent,
        _In_ internal::MidiFeedbackTripInfo const& info) noexcept;

    std::wstring m_associationId{};
    std::atomic<bool> m_enabled{ true };

    mutable wil::srwlock m_statusLock{};
    Status m_status{};

    // last, so the guards and their timers are gone before anything their handlers touch
    std::shared_ptr<internal::MidiFeedbackGuard> m_aToB{};
    std::shared_ptr<internal::MidiFeedbackGuard> m_bToA{};
};
