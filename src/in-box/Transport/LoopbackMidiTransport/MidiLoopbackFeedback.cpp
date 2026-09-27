// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

namespace
{
    PCWSTR FeedbackTestName(_In_ internal::MidiFeedbackTest const test) noexcept
    {
        switch (test)
        {
        case internal::MidiFeedbackTest::Repeat:
            return MIDI_CONFIG_JSON_ENDPOINT_COMMON_FEEDBACK_TEST_VALUE_REPEAT;

        case internal::MidiFeedbackTest::Runaway:
            return MIDI_CONFIG_JSON_ENDPOINT_COMMON_FEEDBACK_TEST_VALUE_RUNAWAY;

        default:
            return L"";
        }
    }

    PCWSTR DirectionName(_In_ bool const directionAToB) noexcept
    {
        return directionAToB ?
            MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_FEEDBACK_DIRECTION_VALUE_A_TO_B :
            MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_FEEDBACK_DIRECTION_VALUE_B_TO_A;
    }
}

_Use_decl_annotations_
std::shared_ptr<MidiLoopbackFeedback> MidiLoopbackFeedback::Create(
    std::wstring const& associationId,
    bool const enabled) noexcept
{
    try
    {
        std::shared_ptr<MidiLoopbackFeedback> feedback{ new MidiLoopbackFeedback(associationId, enabled) };

        auto const raw = feedback.get();

        // The guards are owned by this object and gone before it is, so the raw pointer in each
        // handler never outlives what it points at.
        feedback->m_aToB = std::make_shared<internal::MidiFeedbackGuard>(
            [raw](internal::MidiFeedbackEvent feedbackEvent, internal::MidiFeedbackTripInfo const& info)
            {
                raw->OnGuardEvent(true, feedbackEvent, info);
            });

        feedback->m_bToA = std::make_shared<internal::MidiFeedbackGuard>(
            [raw](internal::MidiFeedbackEvent feedbackEvent, internal::MidiFeedbackTripInfo const& info)
            {
                raw->OnGuardEvent(false, feedbackEvent, info);
            });

        return feedback;
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
        return nullptr;
    }
}

_Use_decl_annotations_
MidiLoopbackFeedback::MidiLoopbackFeedback(
    std::wstring const& associationId,
    bool const enabled) noexcept :
    m_enabled{ enabled }
{
    try
    {
        m_associationId = associationId;
    }
    catch (...)
    {
    }
}

_Use_decl_annotations_
void MidiLoopbackFeedback::SetEnabled(bool const enabled) noexcept
{
    auto const wasEnabled = m_enabled.exchange(enabled, std::memory_order_acq_rel);

    if (wasEnabled == enabled)
    {
        return;
    }

    for (auto const& guard : { m_aToB, m_bToA })
    {
        if (guard != nullptr)
        {
            // Turning it off must not lose what a test happens to be holding.
            guard->Reset(!enabled);
        }
    }

    TraceLoggingWrite(
        MidiLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Loopback feedback protection changed", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(m_associationId.c_str(), "association id"),
        TraceLoggingBool(enabled, "feedback protection on")
    );
}

void MidiLoopbackFeedback::OnMutedStateChanging() noexcept
{
    bool wasMutedForFeedback{ false };

    {
        auto lock = m_statusLock.lock_exclusive();

        wasMutedForFeedback = m_status.MutedForFeedback;
        m_status = {};
    }

    for (auto const& guard : { m_aToB, m_bToA })
    {
        if (guard != nullptr)
        {
            // Muted or unmuted, anything held is either unwanted or stale. A fresh start also
            // clears the backoff, so a loop the customer recreates is caught again at once.
            guard->Reset(false);
        }
    }

    if (wasMutedForFeedback)
    {
        TraceLoggingWrite(
            MidiLoopbackMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Feedback mute cleared by a change to the muted state", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_associationId.c_str(), "association id")
        );

        // so a banner still saying this loopback is muted can be taken down
        LOG_IF_FAILED(internal::BumpNotificationSignalCounter(MIDI_NOTIFICATION_LOOPBACK_FEEDBACK_VALUE));
    }
}

MidiLoopbackFeedback::Status MidiLoopbackFeedback::GetStatus() const noexcept
{
    auto lock = m_statusLock.lock_shared();

    return m_status;
}

_Use_decl_annotations_
bool MidiLoopbackFeedback::MuteIfStillTripped(bool& isMuted) noexcept
{
    auto lock = m_statusLock.lock_exclusive();

    if (!m_status.MutedForFeedback)
    {
        return false;
    }

    isMuted = true;

    return true;
}

_Use_decl_annotations_
void MidiLoopbackFeedback::OnGuardEvent(
    bool const directionAToB,
    internal::MidiFeedbackEvent const feedbackEvent,
    internal::MidiFeedbackTripInfo const& info) noexcept
{
    if (feedbackEvent == internal::MidiFeedbackEvent::NotALoop)
    {
        TraceLoggingWrite(
            MidiLoopbackMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Loopback traffic was held briefly to test for feedback. It was not feedback, and nothing was lost.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_associationId.c_str(), "association id"),
            TraceLoggingWideString(DirectionName(directionAToB), "direction"),
            TraceLoggingWideString(FeedbackTestName(info.Test), "test"),
            TraceLoggingUInt32(info.MessagesPerSecond, "messages per second"),
            TraceLoggingUInt32(info.RepeatPercent, "repeat percent"),
            TraceLoggingUInt32(info.PauseMicroseconds, "pause microseconds")
        );

        return;
    }

    {
        auto lock = m_statusLock.lock_exclusive();

        m_status.MutedForFeedback = true;
        ::GetSystemTimeAsFileTime(&m_status.DetectedTime);
        m_status.DirectionAToB = directionAToB;
        m_status.Info = info;
    }

    // The muted flag, the endpoint properties and the notification all belong on another thread:
    // this may be a send.
    std::wstring associationId{};

    try
    {
        associationId = m_associationId;
    }
    catch (...)
    {
        return;
    }

    if (!internal::MidiFeedbackDetachedWork::Submit(
        [associationId, directionAToB, info]()
        {
            MidiLoopbackFeedback::MutePairAfterTrip(associationId, directionAToB, info);
        }))
    {
        // Without the work item the tripped direction is still blocked by its own guard. Only the
        // pair-wide mute and the notification are lost.
        LOG_HR(E_OUTOFMEMORY);
    }
}

_Use_decl_annotations_
void MidiLoopbackFeedback::MutePairAfterTrip(
    std::wstring const& associationId,
    bool const directionAToB,
    internal::MidiFeedbackTripInfo const& info) noexcept
{
    try
    {
        auto table = TransportState::Current().GetEndpointTable();

        if (table == nullptr)
        {
            return;
        }

        // Looked up again rather than held, because the loopback may have been removed meanwhile.
        auto device = table->GetDevice(associationId);

        if (device == nullptr || device->Feedback == nullptr)
        {
            return;
        }

        if (!device->Feedback->MuteIfStillTripped(device->IsMuted))
        {
            // The customer changed the muted state after the trip. Their choice stands.
            return;
        }

        auto manager = TransportState::Current().GetEndpointManager();

        if (manager != nullptr)
        {
            LOG_IF_FAILED(manager->UpdateSingleEndpointMutedStateProperty(device->DefinitionA, true));
            LOG_IF_FAILED(manager->UpdateSingleEndpointMutedStateProperty(device->DefinitionB, true));

            if (!device->IsMuted)
            {
                // unmuted while the properties were being written
                LOG_IF_FAILED(manager->UpdateSingleEndpointMutedStateProperty(device->DefinitionA, false));
                LOG_IF_FAILED(manager->UpdateSingleEndpointMutedStateProperty(device->DefinitionB, false));
            }
        }

        TraceLoggingWrite(
            MidiLoopbackMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingWideString(L"MIDI was feeding back into a loopback, so the loopback has been muted. Unmute it after fixing the loop.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(associationId.c_str(), "association id"),
            TraceLoggingWideString(device->DefinitionA.CreatedEndpointInterfaceId.c_str(), "endpoint a"),
            TraceLoggingWideString(device->DefinitionB.CreatedEndpointInterfaceId.c_str(), "endpoint b"),
            TraceLoggingWideString(DirectionName(directionAToB), "direction"),
            TraceLoggingWideString(FeedbackTestName(info.Test), "test"),
            TraceLoggingUInt32(info.MessagesPerSecond, "messages per second"),
            TraceLoggingUInt32(info.RepeatPercent, "repeat percent"),
            TraceLoggingUInt32(info.LapMicroseconds, "lap microseconds"),
            TraceLoggingUInt32(info.PauseMicroseconds, "pause microseconds"),
            TraceLoggingUInt32(info.ExpectedArrivals, "expected arrivals"),
            TraceLoggingUInt32(info.ObservedArrivals, "observed arrivals")
        );

        LOG_IF_FAILED(internal::BumpNotificationSignalCounter(MIDI_NOTIFICATION_LOOPBACK_FEEDBACK_VALUE));
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
    }
}
