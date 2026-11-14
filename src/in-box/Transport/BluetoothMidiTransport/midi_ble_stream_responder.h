// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#ifndef MIDI_BLE_STREAM_RESPONDER_H
#define MIDI_BLE_STREAM_RESPONDER_H

#include <cstdint>
#include <string>
#include <vector>

// Answers the UMP Endpoint questions a remote Central asks when this PC is the BLE MIDI 2.0
// Peripheral.
//
// BLE MIDI 2.0 has the Central run UMP Endpoint Discovery and then a Stream Configuration Request
// to agree the protocol and Jitter Reduction (JR) Timestamps. In the Central role, the service
// asks and the device answers. In the Peripheral role this PC is the device being asked, and the
// endpoint apps see represents the remote Central, so nothing above the transport is in a position
// to answer. The transport answers for itself, the way a device's firmware would, and the requests
// never reach apps.
//
// The answers describe this PC's BLE MIDI 2.0 link, not any app: no Function Blocks, both
// protocols, and JR in whichever directions the transport can do it.
//
// No Bluetooth, service or COM dependency, so it is unit tested on its own.
namespace MidiBleStreamResponder
{
    inline constexpr uint8_t StreamMessageType = 0xF;
    inline constexpr uint16_t StatusEndpointDiscovery = 0x000;
    inline constexpr uint16_t StatusEndpointInfoNotification = 0x001;
    inline constexpr uint16_t StatusDeviceIdentityNotification = 0x002;
    inline constexpr uint16_t StatusEndpointNameNotification = 0x003;
    inline constexpr uint16_t StatusStreamConfigurationRequest = 0x005;
    inline constexpr uint16_t StatusStreamConfigurationNotification = 0x006;
    inline constexpr uint16_t StatusFunctionBlockDiscovery = 0x010;

    // UMP 1.1, the version which has JR negotiation
    inline constexpr uint8_t UmpVersionMajor = 1;
    inline constexpr uint8_t UmpVersionMinor = 1;

    inline constexpr uint8_t ProtocolMidi1 = 0x01;
    inline constexpr uint8_t ProtocolMidi2 = 0x02;

    // Endpoint Discovery filter bits
    inline constexpr uint8_t FilterEndpointInfo = 0x01;
    inline constexpr uint8_t FilterDeviceIdentity = 0x02;
    inline constexpr uint8_t FilterEndpointName = 0x04;
    inline constexpr uint8_t FilterStreamConfiguration = 0x10;

    inline constexpr size_t EndpointNameMaxBytes = 98;
    inline constexpr size_t EndpointNameBytesPerMessage = 14;

    struct Identity
    {
        // UTF-8, cut to EndpointNameMaxBytes on a character boundary when sent
        std::string EndpointName{ };

        uint8_t ManufacturerSysExId[3]{ 0, 0, 0 };
        uint16_t DeviceFamily{ 0 };
        uint16_t DeviceFamilyModelNumber{ 0 };
        uint8_t SoftwareRevision[4]{ 0, 0, 0, 0 };
    };

    inline uint8_t GetMessageType(_In_ uint32_t const word) noexcept { return static_cast<uint8_t>(word >> 28); }
    inline uint8_t GetForm(_In_ uint32_t const word) noexcept { return static_cast<uint8_t>((word >> 26) & 0x3); }
    inline uint16_t GetStatus(_In_ uint32_t const word) noexcept { return static_cast<uint16_t>((word >> 16) & 0x3FF); }

    inline uint32_t BuildStreamFirstWord(_In_ uint8_t const form, _In_ uint16_t const status, _In_ uint16_t const low) noexcept
    {
        return (static_cast<uint32_t>(StreamMessageType) << 28) |
            (static_cast<uint32_t>(form & 0x3) << 26) |
            (static_cast<uint32_t>(status & 0x3FF) << 16) |
            low;
    }

    inline bool IsRequestThisPeripheralAnswers(_In_ uint32_t const firstWord) noexcept
    {
        if (GetMessageType(firstWord) != StreamMessageType || GetForm(firstWord) != 0)
        {
            return false;
        }

        auto const status = GetStatus(firstWord);

        return status == StatusEndpointDiscovery ||
            status == StatusStreamConfigurationRequest ||
            status == StatusFunctionBlockDiscovery;
    }

    // Cut to at most maxBytes without splitting a multi-byte character
    inline size_t Utf8PrefixLength(_In_ std::string const& text, _In_ size_t const maxBytes) noexcept
    {
        if (text.size() <= maxBytes)
        {
            return text.size();
        }

        size_t length = maxBytes;

        // continuation bytes are 10xxxxxx
        while (length > 0 && (static_cast<uint8_t>(text[length]) & 0xC0) == 0x80)
        {
            length--;
        }

        return length;
    }

    inline void AppendEndpointName(_In_ std::string const& name, _Inout_ std::vector<uint32_t>& words)
    {
        size_t const length = Utf8PrefixLength(name, EndpointNameMaxBytes);

        if (length == 0)
        {
            return;
        }

        size_t const messageCount = (length + EndpointNameBytesPerMessage - 1) / EndpointNameBytesPerMessage;

        for (size_t message = 0; message < messageCount; message++)
        {
            // 0 complete, 1 start, 2 continue, 3 end
            uint8_t const form = messageCount == 1 ? 0 :
                message == 0 ? 1 :
                message + 1 == messageCount ? 3 : 2;

            uint8_t bytes[EndpointNameBytesPerMessage]{ };

            for (size_t i = 0; i < EndpointNameBytesPerMessage; i++)
            {
                size_t const index = message * EndpointNameBytesPerMessage + i;

                bytes[i] = index < length ? static_cast<uint8_t>(name[index]) : 0;
            }

            words.push_back(BuildStreamFirstWord(form, StatusEndpointNameNotification,
                static_cast<uint16_t>((bytes[0] << 8) | bytes[1])));

            for (size_t word = 0; word < 3; word++)
            {
                size_t const base = 2 + word * 4;

                words.push_back(
                    (static_cast<uint32_t>(bytes[base]) << 24) |
                    (static_cast<uint32_t>(bytes[base + 1]) << 16) |
                    (static_cast<uint32_t>(bytes[base + 2]) << 8) |
                    static_cast<uint32_t>(bytes[base + 3]));
            }
        }
    }

    class Responder
    {
    public:
        // The link dropped. Stream state isn't kept across a disconnection.
        void Reset() noexcept
        {
            m_protocol = ProtocolMidi2;
            m_receiveJr = false;
            m_sendJr = false;
        }

        void SetIdentity(_In_ Identity const& identity)
        {
            m_identity = identity;
        }

        // What this PC can do with JR right now. Sending needs the JR Clock schedule running.
        void SetJrCapabilities(_In_ bool const canReceive, _In_ bool const canSend) noexcept
        {
            m_canReceiveJr = canReceive;
            m_canSendJr = canSend;
        }

        uint8_t Protocol() const noexcept { return m_protocol; }

        // As agreed with the Central through its Stream Configuration Request
        bool ReceivesJr() const noexcept { return m_receiveJr; }
        bool SendsJr() const noexcept { return m_sendJr; }

        // Takes the complete UMPs of one payload. Requests addressed to this PC as the UMP
        // Endpoint are answered into replies and left out of forward. Everything else goes to
        // forward, in order. Returns true when the stream configuration changed.
        bool ProcessPayload(
            _In_reads_(wordCount) uint32_t const* const words,
            _In_ size_t const wordCount,
            _Inout_ std::vector<uint32_t>& forward,
            _Inout_ std::vector<uint32_t>& replies)
        {
            forward.clear();
            replies.clear();

            bool configurationChanged{ false };

            if (words == nullptr)
            {
                return false;
            }

            for (size_t i = 0; i < wordCount; )
            {
                size_t const messageWordCount = UmpWordCount(words[i]);

                if (i + messageWordCount > wordCount)
                {
                    break;
                }

                if (IsRequestThisPeripheralAnswers(words[i]) && messageWordCount == 4)
                {
                    switch (GetStatus(words[i]))
                    {
                    case StatusEndpointDiscovery:
                        AnswerEndpointDiscovery(static_cast<uint8_t>(words[i + 1] & 0xFF), replies);
                        break;

                    case StatusStreamConfigurationRequest:
                        configurationChanged = AnswerStreamConfigurationRequest(words[i], replies) || configurationChanged;
                        break;

                    default:
                        // Function Block Discovery. This PC declares no Function Blocks, so there
                        // is nothing to report.
                        break;
                    }
                }
                else
                {
                    forward.insert(forward.end(), words + i, words + i + messageWordCount);
                }

                i += messageWordCount;
            }

            return configurationChanged;
        }

    private:
        static size_t UmpWordCount(_In_ uint32_t const firstWord) noexcept
        {
            switch (GetMessageType(firstWord))
            {
            case 0x0: case 0x1: case 0x2: case 0x6: case 0x7:
                return 1;
            case 0x3: case 0x4: case 0x8: case 0x9: case 0xA:
                return 2;
            case 0xB: case 0xC:
                return 3;
            default:
                return 4;
            }
        }

        uint32_t StreamConfigurationNotification() const noexcept
        {
            uint16_t const low = static_cast<uint16_t>(
                (static_cast<uint16_t>(m_protocol) << 8) |
                (m_receiveJr ? 0x2 : 0) |
                (m_sendJr ? 0x1 : 0));

            return BuildStreamFirstWord(0, StatusStreamConfigurationNotification, low);
        }

        void AnswerEndpointDiscovery(_In_ uint8_t const filter, _Inout_ std::vector<uint32_t>& replies) const
        {
            if ((filter & FilterEndpointInfo) != 0)
            {
                replies.push_back(BuildStreamFirstWord(0, StatusEndpointInfoNotification,
                    static_cast<uint16_t>((UmpVersionMajor << 8) | UmpVersionMinor)));

                // No Function Blocks, both protocols, and JR as far as it can be done
                replies.push_back(
                    0x00000200 | 0x00000100 |
                    (m_canReceiveJr ? 0x00000002 : 0) |
                    (m_canSendJr ? 0x00000001 : 0));

                replies.push_back(0);
                replies.push_back(0);
            }

            if ((filter & FilterDeviceIdentity) != 0)
            {
                replies.push_back(BuildStreamFirstWord(0, StatusDeviceIdentityNotification, 0));

                replies.push_back(
                    (static_cast<uint32_t>(m_identity.ManufacturerSysExId[0] & 0x7F) << 16) |
                    (static_cast<uint32_t>(m_identity.ManufacturerSysExId[1] & 0x7F) << 8) |
                    static_cast<uint32_t>(m_identity.ManufacturerSysExId[2] & 0x7F));

                // family and model are 14 bits each, sent LSB first
                replies.push_back(
                    (static_cast<uint32_t>(m_identity.DeviceFamily & 0x7F) << 24) |
                    (static_cast<uint32_t>((m_identity.DeviceFamily >> 7) & 0x7F) << 16) |
                    (static_cast<uint32_t>(m_identity.DeviceFamilyModelNumber & 0x7F) << 8) |
                    static_cast<uint32_t>((m_identity.DeviceFamilyModelNumber >> 7) & 0x7F));

                replies.push_back(
                    (static_cast<uint32_t>(m_identity.SoftwareRevision[0] & 0x7F) << 24) |
                    (static_cast<uint32_t>(m_identity.SoftwareRevision[1] & 0x7F) << 16) |
                    (static_cast<uint32_t>(m_identity.SoftwareRevision[2] & 0x7F) << 8) |
                    static_cast<uint32_t>(m_identity.SoftwareRevision[3] & 0x7F));
            }

            if ((filter & FilterEndpointName) != 0)
            {
                AppendEndpointName(m_identity.EndpointName, replies);
            }

            // No Product Instance Id: nothing about this PC is both stable and fine to share with
            // any Central in range.

            if ((filter & FilterStreamConfiguration) != 0)
            {
                replies.push_back(StreamConfigurationNotification());
                replies.push_back(0);
                replies.push_back(0);
                replies.push_back(0);
            }
        }

        // UMP section 7.1.6: whatever part of the request can't be met, the reply carries the
        // current setting instead, and the reply is final.
        bool AnswerStreamConfigurationRequest(_In_ uint32_t const firstWord, _Inout_ std::vector<uint32_t>& replies)
        {
            uint8_t const requestedProtocol = static_cast<uint8_t>((firstWord >> 8) & 0xFF);
            bool const requestedReceive = (firstWord & 0x2) != 0;
            bool const requestedSend = (firstWord & 0x1) != 0;

            auto const previousProtocol = m_protocol;
            auto const previousReceive = m_receiveJr;
            auto const previousSend = m_sendJr;

            if (requestedProtocol == ProtocolMidi1 || requestedProtocol == ProtocolMidi2)
            {
                m_protocol = requestedProtocol;
            }

            m_receiveJr = requestedReceive && m_canReceiveJr;
            m_sendJr = requestedSend && m_canSendJr;

            replies.push_back(StreamConfigurationNotification());
            replies.push_back(0);
            replies.push_back(0);
            replies.push_back(0);

            return previousProtocol != m_protocol || previousReceive != m_receiveJr || previousSend != m_sendJr;
        }

        Identity m_identity{ };
        bool m_canReceiveJr{ true };
        bool m_canSendJr{ false };

        uint8_t m_protocol{ ProtocolMidi2 };
        bool m_receiveJr{ false };
        bool m_sendJr{ false };
    };
}

#endif
