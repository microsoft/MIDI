// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

_Use_decl_annotations_
void MidiNetworkNotificationSignal::SignalPendingApprovalChanged() noexcept
{
    // Already queued, and the write which is coming will cover this change too.
    if (m_pendingApprovalWriteQueued.exchange(1) != 0)
    {
        return;
    }

    try
    {
        m_work.Submit([this]()
            {
                // Cleared first. A change arriving during the write queues a fresh one, which is
                // what keeps the last change from being lost.
                m_pendingApprovalWriteQueued.store(0);

                BumpCounter(MIDI_NOTIFICATION_NETWORK_PENDING_APPROVAL_VALUE);
            });
    }
    catch (...)
    {
        // Queuing allocates. Letting that escape a noexcept function on the socket receive path
        // would end the process, and the process is the MIDI service for the whole machine. A
        // customer losing a notification is the correct price.
        m_pendingApprovalWriteQueued.store(0);

        LOG_CAUGHT_EXCEPTION();
    }
}

_Use_decl_annotations_
void MidiNetworkNotificationSignal::BumpCounter(PCWSTR const valueName) noexcept
{
    try
    {
        wil::unique_hkey rootKey{ };

        // Opened, not created. See network_notification_defs.h for why the parent is never made.
        auto const rootResult = ::RegOpenKeyExW(
            HKEY_LOCAL_MACHINE,
            MIDI_NOTIFICATION_SIGNAL_ROOT_REG_KEY,
            0,
            KEY_CREATE_SUB_KEY,
            rootKey.put());

        if (rootResult != ERROR_SUCCESS)
        {
            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
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
            MIDI_NOTIFICATION_SIGNAL_SUBKEY_NAME,
            0,
            nullptr,
            REG_OPTION_VOLATILE,
            KEY_QUERY_VALUE | KEY_SET_VALUE,
            nullptr,
            key.put(),
            nullptr);

        if (openResult != ERROR_SUCCESS)
        {
            // Not fatal. The customer loses a notification, not a MIDI connection, so this is
            // logged and dropped rather than failing anything upstream.
            TraceLoggingWrite(
                MidiNetworkMidiTransportTelemetryProvider::Provider(),
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

        if (::RegQueryValueExW(
                key.get(),
                valueName,
                nullptr,
                &valueType,
                reinterpret_cast<LPBYTE>(&currentValue),
                &valueSize) != ERROR_SUCCESS ||
            valueType != REG_DWORD)
        {
            currentValue = 0;
        }

        // Wrapping is fine. Readers compare against the last value they saw, so only a change
        // matters, never the magnitude.
        DWORD const newValue{ currentValue + 1 };

        LOG_IF_WIN32_ERROR(::RegSetValueExW(
            key.get(),
            valueName,
            0,
            REG_DWORD,
            reinterpret_cast<BYTE const*>(&newValue),
            sizeof(newValue)));
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
    }
}
