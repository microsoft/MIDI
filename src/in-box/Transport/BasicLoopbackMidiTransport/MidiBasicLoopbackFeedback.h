// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Feedback protection for one basic loopback. It has one endpoint and one direction, so one guard.
class MidiBasicLoopbackFeedback
{
public:
    struct Status
    {
        bool MutedForFeedback{ false };
        FILETIME DetectedTime{};
        internal::MidiFeedbackTripInfo Info{};
    };

    // Null only when out of memory, and then the loopback simply has no protection.
    static std::shared_ptr<MidiBasicLoopbackFeedback> Create(
        _In_ GUID const& associationId,
        _In_ bool const enabled) noexcept;

    MidiBasicLoopbackFeedback(_In_ MidiBasicLoopbackFeedback const&) = delete;
    MidiBasicLoopbackFeedback& operator=(_In_ MidiBasicLoopbackFeedback const&) = delete;

    bool IsEnabled() const noexcept { return m_enabled.load(std::memory_order_acquire); }

    // Off releases anything a test is holding, in order. On starts clean.
    void SetEnabled(_In_ bool const enabled) noexcept;

    // Called before the muted flag changes, for either a mute or an unmute, so a trip that is still
    // being handled cannot mute the loopback again behind the customer.
    void OnMutedStateChanging() noexcept;

    std::shared_ptr<internal::MidiFeedbackGuard> const& Guard() const noexcept { return m_guard; }

    Status GetStatus() const noexcept;

    // Sets the muted flag only if the trip is still standing, under the lock OnMutedStateChanging
    // takes, so a customer's unmute can never be undone by a late trip.
    bool MuteIfStillTripped(_Inout_ bool& isMuted) noexcept;

    // Runs on a detached threadpool thread, never on a send.
    static void MuteAfterTrip(
        _In_ GUID const associationId,
        _In_ internal::MidiFeedbackTripInfo const& info) noexcept;

private:
    MidiBasicLoopbackFeedback(_In_ GUID const& associationId, _In_ bool const enabled) noexcept;

    void OnGuardEvent(
        _In_ internal::MidiFeedbackEvent const feedbackEvent,
        _In_ internal::MidiFeedbackTripInfo const& info) noexcept;

    GUID m_associationId{};
    std::atomic<bool> m_enabled{ true };

    mutable wil::srwlock m_statusLock{};
    Status m_status{};

    // last, so the guard and its timer are gone before anything its handler touches
    std::shared_ptr<internal::MidiFeedbackGuard> m_guard{};
};
