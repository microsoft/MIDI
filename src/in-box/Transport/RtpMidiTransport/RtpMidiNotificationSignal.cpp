// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

void
RtpMidiNotificationSignal::SignalPendingApprovalChanged() noexcept
{
    Queue(m_pendingApprovalWriteQueued, MIDI_RTP_NOTIFICATION_PENDING_APPROVAL_VALUE);
}

void
RtpMidiNotificationSignal::SignalHostNetworkAdapterChanged() noexcept
{
    Queue(m_hostNetworkAdapterWriteQueued, MIDI_RTP_NOTIFICATION_HOST_ADAPTER_VALUE);
}

_Use_decl_annotations_
void
RtpMidiNotificationSignal::Queue(std::atomic<uint32_t>& queued, PCWSTR const valueName) noexcept
{
    if (queued.exchange(1) != 0) return;

    try
    {
        m_work.Submit([&queued, valueName]()
            {
                queued.store(0);
                BumpCounter(valueName);
            });
    }
    catch (...)
    {
        // Queuing allocates, and this runs on the data path of the MIDI service for the whole
        // machine. Losing a notification is the right price.
        queued.store(0);
        LOG_CAUGHT_EXCEPTION();
    }
}

_Use_decl_annotations_
void
RtpMidiNotificationSignal::BumpCounter(PCWSTR const valueName) noexcept
{
    try
    {
        wil::unique_hkey rootKey{ };

        auto const rootResult = ::RegOpenKeyExW(
            HKEY_LOCAL_MACHINE,
            MIDI_RTP_NOTIFICATION_SIGNAL_ROOT_REG_KEY,
            0,
            KEY_CREATE_SUB_KEY,
            rootKey.put());

        if (rootResult != ERROR_SUCCESS)
        {
            TraceLoggingWrite(
                MidiRtpMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingWideString(L"The MIDI registry root is missing, so nothing can be signaled.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingHResult(HRESULT_FROM_WIN32(rootResult), MIDI_TRACE_EVENT_HRESULT_FIELD)
            );

            return;
        }

        wil::unique_hkey key{ };

        auto const openResult = ::RegCreateKeyExW(
            rootKey.get(),
            MIDI_RTP_NOTIFICATION_SIGNAL_SUBKEY_NAME,
            0,
            nullptr,
            REG_OPTION_VOLATILE,
            KEY_QUERY_VALUE | KEY_SET_VALUE,
            nullptr,
            key.put(),
            nullptr);

        if (openResult != ERROR_SUCCESS)
        {
            TraceLoggingWrite(
                MidiRtpMidiTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_WARNING,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                TraceLoggingWideString(L"Could not open the notification signal key.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingHResult(HRESULT_FROM_WIN32(openResult), MIDI_TRACE_EVENT_HRESULT_FIELD)
            );

            return;
        }

        DWORD currentValue{ 0 };
        DWORD valueSize{ sizeof(currentValue) };
        DWORD valueType{ 0 };

        if (::RegQueryValueExW(key.get(), valueName, nullptr, &valueType,
                reinterpret_cast<LPBYTE>(&currentValue), &valueSize) != ERROR_SUCCESS || valueType != REG_DWORD)
        {
            currentValue = 0;
        }

        // wrapping is fine: readers only look for a change
        DWORD const newValue{ currentValue + 1 };

        LOG_IF_WIN32_ERROR(::RegSetValueExW(key.get(), valueName, 0, REG_DWORD,
            reinterpret_cast<BYTE const*>(&newValue), sizeof(newValue)));
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
    }
}
