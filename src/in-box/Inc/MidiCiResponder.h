// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiCiMessage.h"

namespace WindowsMidiServicesCapabilityInquiry
{
    struct ResponderConfig
    {
        uint32_t Muid{ 0 };

        uint8_t ManufacturerSysExId[3]{};

        uint16_t DeviceFamily{ 0 };
        uint16_t DeviceFamilyModelNumber{ 0 };

        uint8_t SoftwareRevisionLevel[4]{};

        uint8_t CapabilityCategories{ 0 };
        uint32_t ReceivableMaximumSysExSize{ 512 };

        uint8_t FunctionBlockNumber{ 0 };

        // Sets capability bit D3 in the Discovery reply. Without it no initiator will ever ask us
        // for a property.
        bool SupportsPropertyExchange{ false };
    };

    inline constexpr uint8_t CapabilityBitPropertyExchange{ 0x08 };

    enum class ResponderAction
    {
        Ignored = 0,
        Replied,
        MuidInvalidated,
        ReplyBufferTooSmall,

        // The caller owns the answer from here: it parses the header JSON with Windows.Data.Json,
        // which cannot be done at this layer, then drives a PropertyReplyChunker.
        PropertyDataRequested,
    };

    // Produces replies into a buffer the caller owns. It never sends, never allocates and holds no
    // transport, which is what lets the same responder serve the service, a virtual device app and
    // a message transform without being forked.
    class Responder
    {
    public:
        void Initialize(_In_ ResponderConfig const& config) noexcept
        {
            m_config = config;
            m_muidNeedsReplacement = false;
        }

        uint32_t Muid() const noexcept { return m_config.Muid; }

        void SetMuid(_In_ uint32_t const muid) noexcept
        {
            m_config.Muid = muid;
            m_muidNeedsReplacement = false;
        }

        // Set when another device tells us our MUID collided. The host owns replacing it, because
        // generating one needs a random source this layer deliberately does not have.
        bool MuidNeedsReplacement() const noexcept { return m_muidNeedsReplacement; }

        ResponderAction ProcessMessage(
            _In_ ParsedMessage const& message,
            _Out_writes_bytes_to_opt_(replyCapacity, *replyByteCount) uint8_t* const replyBuffer,
            _In_ size_t const replyCapacity,
            _Out_ size_t* const replyByteCount
        ) noexcept
        {
            if (replyByteCount != nullptr)
            {
                *replyByteCount = 0;
            }

            if (!AddressedToUs(message))
            {
                return ResponderAction::Ignored;
            }

            if (message.Type == MessageType::InvalidateMuid)
            {
                if (message.TargetMuid == m_config.Muid && m_config.Muid != 0)
                {
                    m_config.Muid = 0;
                    m_muidNeedsReplacement = true;

                    return ResponderAction::MuidInvalidated;
                }

                return ResponderAction::Ignored;
            }

            if (message.Type == MessageType::Discovery)
            {
                // Replying is required even though no categories are supported yet.
                if (m_config.Muid == 0)
                {
                    return ResponderAction::Ignored;
                }
                if (replyBuffer == nullptr || replyCapacity < DiscoveryReplyByteCount)
                {
                    return ResponderAction::ReplyBufferTooSmall;
                }

                DiscoveryReplyFields fields{};

                fields.SourceMuid = m_config.Muid;
                fields.DestinationMuid = message.SourceMuid;

                fields.ManufacturerSysExId[0] = m_config.ManufacturerSysExId[0];
                fields.ManufacturerSysExId[1] = m_config.ManufacturerSysExId[1];
                fields.ManufacturerSysExId[2] = m_config.ManufacturerSysExId[2];

                fields.DeviceFamily = m_config.DeviceFamily;
                fields.DeviceFamilyModelNumber = m_config.DeviceFamilyModelNumber;

                fields.SoftwareRevisionLevel[0] = m_config.SoftwareRevisionLevel[0];
                fields.SoftwareRevisionLevel[1] = m_config.SoftwareRevisionLevel[1];
                fields.SoftwareRevisionLevel[2] = m_config.SoftwareRevisionLevel[2];
                fields.SoftwareRevisionLevel[3] = m_config.SoftwareRevisionLevel[3];

                fields.CapabilityCategories = m_config.CapabilityCategories;

                if (m_config.SupportsPropertyExchange)
                {
                    fields.CapabilityCategories |= CapabilityBitPropertyExchange;
                }

                fields.ReceivableMaximumSysExSize = m_config.ReceivableMaximumSysExSize;

                fields.OutputPathId = message.OutputPathId;
                fields.FunctionBlockNumber = m_config.FunctionBlockNumber;

                const auto written = BuildDiscoveryReply(fields, replyBuffer, replyCapacity);

                if (written == 0)
                {
                    return ResponderAction::ReplyBufferTooSmall;
                }

                if (replyByteCount != nullptr)
                {
                    *replyByteCount = written;
                }

                return ResponderAction::Replied;
            }

            if (message.Type == MessageType::PropertyGetDataInquiry)
            {
                if (!m_config.SupportsPropertyExchange || m_config.Muid == 0)
                {
                    return ResponderAction::Ignored;
                }

                if (!message.HasPropertyExchangeFields)
                {
                    return ResponderAction::Ignored;
                }

                return ResponderAction::PropertyDataRequested;
            }

            return ResponderAction::Ignored;
        }

    private:
        bool AddressedToUs(_In_ ParsedMessage const& message) const noexcept
        {
            if (message.DestinationMuid == MuidBroadcast)
            {
                return true;
            }

            // Answering a conversation between two other devices would corrupt it.
            return m_config.Muid != 0 && message.DestinationMuid == m_config.Muid;
        }

        ResponderConfig m_config{};
        bool m_muidNeedsReplacement{ false };
    };
}
