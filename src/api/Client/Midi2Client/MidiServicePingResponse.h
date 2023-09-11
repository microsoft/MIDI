// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiServicePingResponse.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiServicePingResponse : MidiServicePingResponseT<MidiServicePingResponse>
    {
        MidiServicePingResponse() = default;

        uint64_t Id() const { return m_id; }
        internal::MidiTimestamp ClientSendMidiTimestamp() const { return m_clientSendTimestamp; }
        internal::MidiTimestamp ServiceReportedMidiTimestamp() const { return m_serviceTimestamp; }
        internal::MidiTimestamp ClientReceiveMidiTimestamp() const { return m_clientReceiveTimestamp; }
        internal::MidiTimestamp ClientDeltaTimestamp() const { return m_clientDeltaTimestamp; }


        void InternalSetSendInfo(
            _In_ uint64_t const pingId,
            _In_ internal::MidiTimestamp const sendTimestamp)
        {
            m_id = pingId;
            m_clientSendTimestamp = sendTimestamp;
        }

        void InternalSetReceiveInfo(
            _In_ internal::MidiTimestamp const serviceTimestamp,
            _In_ internal::MidiTimestamp const receiveTimestamp)
        {
            m_serviceTimestamp = serviceTimestamp;
            m_clientReceiveTimestamp = receiveTimestamp;

            if (m_clientReceiveTimestamp > m_clientSendTimestamp)
            {
                m_clientDeltaTimestamp = m_clientSendTimestamp - m_clientReceiveTimestamp;
            }
            else
            {
                m_clientDeltaTimestamp = 0;
            }
        }

    private:
        uint64_t m_id{};
        internal::MidiTimestamp m_clientSendTimestamp{};
        internal::MidiTimestamp m_serviceTimestamp{};
        internal::MidiTimestamp m_clientReceiveTimestamp{};
        internal::MidiTimestamp m_clientDeltaTimestamp{};
    };
}
