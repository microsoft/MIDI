// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiServicePingResponse.g.h"

namespace winrt::Microsoft::Devices::Midi2::Diagnostics::implementation
{
    struct MidiServicePingResponse : MidiServicePingResponseT<MidiServicePingResponse>
    {
        MidiServicePingResponse() = default;

        uint32_t SourceId() const noexcept { return m_idHigh; }
        uint32_t Index() const noexcept { return m_idLow; }
        internal::MidiTimestamp ClientSendMidiTimestamp() const noexcept { return m_clientSendTimestamp; }
        internal::MidiTimestamp ServiceReportedMidiTimestamp() const noexcept { return m_serviceTimestamp; }
        internal::MidiTimestamp ClientReceiveMidiTimestamp() const noexcept { return m_clientReceiveTimestamp; }
        internal::MidiTimestamp ClientDeltaTimestamp() const noexcept { return m_clientDeltaTimestamp; }


        void InternalSetSendInfo(
            _In_ uint32_t const pingSourceId,
            _In_ uint32_t const pingIndex,
            _In_ internal::MidiTimestamp const sendTimestamp)
        {
            m_idHigh = pingSourceId;
            m_idLow = pingIndex;
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
                m_clientDeltaTimestamp = m_clientReceiveTimestamp - m_clientSendTimestamp;
            }
            else
            {
                // if we hit a wrap situation. We could do some better math here if we wanted to.
                m_clientDeltaTimestamp = 0;
            }
        }

    private:
        uint32_t m_idHigh{};
        uint32_t m_idLow{};
        internal::MidiTimestamp m_clientSendTimestamp{};
        internal::MidiTimestamp m_serviceTimestamp{};
        internal::MidiTimestamp m_clientReceiveTimestamp{};
        internal::MidiTimestamp m_clientDeltaTimestamp{};
    };
}
