// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiRtpPendingRemoteClient.h"
#include "Transports.Rtp.MidiRtpPendingRemoteClient.g.cpp"

namespace winrt::Windows::Devices::Midi2::Transports::Rtp::implementation
{
    namespace
    {
        // The service writes ISO 8601 UTC with the full FILETIME precision, for example
        // 2026-09-27T22:14:05.1234567Z. Anything else is a zero DateTime.
        foundation::DateTime DateTimeFromIso8601(_In_ winrt::hstring const& value) noexcept
        {
            foundation::DateTime result{};

            if (value.empty()) return result;

            uint32_t year{}, month{}, day{}, hour{}, minute{}, second{}, fraction{};

            if (swscanf_s(value.c_str(), L"%4u-%2u-%2uT%2u:%2u:%2u.%7uZ", &year, &month, &day, &hour, &minute, &second, &fraction) != 7) return result;
            if (fraction >= 10000000u) return result;

            SYSTEMTIME st{};
            st.wYear = static_cast<WORD>(year);
            st.wMonth = static_cast<WORD>(month);
            st.wDay = static_cast<WORD>(day);
            st.wHour = static_cast<WORD>(hour);
            st.wMinute = static_cast<WORD>(minute);
            st.wSecond = static_cast<WORD>(second);

            FILETIME ft{};
            if (!SystemTimeToFileTime(&st, &ft)) return result;

            uint64_t const fileTime = ((static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime) + fraction;

            return winrt::clock::from_file_time(winrt::file_time{ fileTime });
        }
    }

    _Use_decl_annotations_
    bool MidiRtpPendingRemoteClient::InternalInitialize(json::JsonObject const& source) noexcept
    {
        try
        {
            if (!MidiRtpSdkJson::TryGuid(MidiRtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_IDENTIFIER_KEY), m_hostId)) return false;

            m_remoteClientName = MidiRtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_NAME_KEY);
            if (m_remoteClientName.empty()) return false;

            m_hostServiceInstanceName = MidiRtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_PENDING_HOST_SERVICE_INSTANCE_NAME_KEY);
            m_remoteAddress = MidiRtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_ADDRESS_KEY);
            m_requestTime = DateTimeFromIso8601(MidiRtpSdkJson::String(source, MIDI_CONFIG_JSON_RTP_MIDI_PENDING_REQUEST_TIME_KEY));
            m_isApproved = MidiRtpSdkJson::Boolean(source, MIDI_CONFIG_JSON_RTP_MIDI_PENDING_APPROVED_KEY);

            return true;
        }
        catch (...)
        {
            return false;
        }
    }
}
