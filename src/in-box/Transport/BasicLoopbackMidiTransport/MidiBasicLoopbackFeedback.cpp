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
}

_Use_decl_annotations_
std::shared_ptr<MidiBasicLoopbackFeedback> MidiBasicLoopbackFeedback::Create(
    GUID const& associationId,
    bool const enabled) noexcept
{
    try
    {
        std::shared_ptr<MidiBasicLoopbackFeedback> feedback{ new MidiBasicLoopbackFeedback(associationId, enabled) };

        auto const raw = feedback.get();

        // The guard is owned by this object and gone before it is, so the raw pointer in the
        // handler never outlives what it points at.
        feedback->m_guard = std::make_shared<internal::MidiFeedbackGuard>(
            [raw](internal::MidiFeedbackEvent feedbackEvent, internal::MidiFeedbackTripInfo const& info)
            {
                raw->OnGuardEvent(feedbackEvent, info);
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
MidiBasicLoopbackFeedback::MidiBasicLoopbackFeedback(
    GUID const& associationId,
    bool const enabled) noexcept :
    m_associationId{ associationId },
    m_enabled{ enabled }
{
}

_Use_decl_annotations_
void MidiBasicLoopbackFeedback::SetEnabled(bool const enabled) noexcept
{
    auto const wasEnabled = m_enabled.exchange(enabled, std::memory_order_acq_rel);

    if (wasEnabled == enabled)
    {
        return;
    }

    if (m_guard != nullptr)
    {
        // Turning it off must not lose what a test happens to be holding.
        m_guard->Reset(!enabled);
    }

    TraceLoggingWrite(
        MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Basic loopback feedback protection changed", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingGuid(m_associationId, "association id"),
        TraceLoggingBool(enabled, "feedback protection on")
    );
}

void MidiBasicLoopbackFeedback::OnMutedStateChanging() noexcept
{
    bool wasMutedForFeedback{ false };

    {
        auto lock = m_statusLock.lock_exclusive();

        wasMutedForFeedback = m_status.MutedForFeedback;
        m_status = {};
    }

    if (m_guard != nullptr)
    {
        // Muted or unmuted, anything held is unwanted or stale, and a fresh start also clears the
        // backoff so a loop the customer recreates is caught again at once.
        m_guard->Reset(false);
    }

    if (wasMutedForFeedback)
    {
        TraceLoggingWrite(
            MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Feedback mute cleared by a change to the muted state", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingGuid(m_associationId, "association id")
        );

        // so a banner still saying this loopback is muted can be taken down
        LOG_IF_FAILED(internal::BumpNotificationSignalCounter(MIDI_NOTIFICATION_LOOPBACK_FEEDBACK_VALUE));
    }
}

MidiBasicLoopbackFeedback::Status MidiBasicLoopbackFeedback::GetStatus() const noexcept
{
    auto lock = m_statusLock.lock_shared();

    return m_status;
}

_Use_decl_annotations_
bool MidiBasicLoopbackFeedback::MuteIfStillTripped(bool& isMuted) noexcept
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
void MidiBasicLoopbackFeedback::OnGuardEvent(
    internal::MidiFeedbackEvent const feedbackEvent,
    internal::MidiFeedbackTripInfo const& info) noexcept
{
    if (feedbackEvent == internal::MidiFeedbackEvent::NotALoop)
    {
        TraceLoggingWrite(
            MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Basic loopback traffic was held briefly to test for feedback. It was not feedback, and nothing was lost.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingGuid(m_associationId, "association id"),
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
        m_status.Info = info;
    }

    // The muted flag, the endpoint property and the notification belong on another thread: this
    // may be a send.
    auto const associationId = m_associationId;

    if (!internal::MidiFeedbackDetachedWork::Submit(
        [associationId, info]()
        {
            MidiBasicLoopbackFeedback::MuteAfterTrip(associationId, info);
        }))
    {
        // The guard still blocks the loopback by itself. Only the mute flag and the notification
        // are lost.
        LOG_HR(E_OUTOFMEMORY);
    }
}

_Use_decl_annotations_
void MidiBasicLoopbackFeedback::MuteAfterTrip(
    GUID const associationId,
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

        auto definition = device->Definition;

        if (definition == nullptr)
        {
            return;
        }

        if (!device->Feedback->MuteIfStillTripped(definition->IsMuted))
        {
            // The customer changed the muted state after the trip. Their choice stands.
            return;
        }

        auto manager = TransportState::Current().GetEndpointManager();

        if (manager != nullptr)
        {
            LOG_IF_FAILED(manager->UpdateEndpointMutedStateProperty(definition));
        }

        TraceLoggingWrite(
            MidiBasicLoopbackMidiTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_WARNING,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
            TraceLoggingWideString(L"MIDI was feeding back into a basic loopback, so the loopback has been muted. Unmute it after fixing the loop.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingGuid(associationId, "association id"),
            TraceLoggingWideString(definition->CreatedEndpointInterfaceId.c_str(), "endpoint"),
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
