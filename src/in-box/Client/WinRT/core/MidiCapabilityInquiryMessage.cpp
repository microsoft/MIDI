// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiCapabilityInquiryMessage.h"

// Both are consumed here, so their implementation headers are needed as well as the projection.
#include "MidiUniqueId.h"
#include "MidiProfileId.h"
#include "MidiSystemExclusive7MessageHelper.h"

#include "MidiCiMessage.h"

#include "CapabilityInquiry.MidiCapabilityInquiryMessage.g.cpp"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    namespace
    {
        namespace native = ::WindowsMidiServicesCapabilityInquiry;

        std::vector<uint8_t> ToByteVector(
            _In_ foundation::Collections::IIterable<uint8_t> const& source) noexcept
        {
            std::vector<uint8_t> bytes{};

            try
            {
                if (source != nullptr)
                {
                    for (auto const value : source)
                    {
                        bytes.push_back(value);
                    }
                }
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
            }

            return bytes;
        }

        ci::MidiUniqueId MakeUniqueId(_In_ uint32_t const value) noexcept
        {
            try
            {
                return winrt::make<implementation::MidiUniqueId>(value);
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
                return nullptr;
            }
        }

        ci::MidiProfileId MakeProfileId(_In_reads_(5) uint8_t const* const bytes) noexcept
        {
            try
            {
                return winrt::make<implementation::MidiProfileId>(
                    bytes[0], bytes[1], bytes[2], bytes[3], bytes[4]);
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
                return nullptr;
            }
        }
    }

    _Use_decl_annotations_
    bool MidiCapabilityInquiryMessage::IsCapabilityInquiryData(
        foundation::Collections::IIterable<uint8_t> const& data) noexcept
    {
        auto const bytes = ToByteVector(data);

        // Universal system exclusive, any device id, then the capability inquiry sub-id.
        return bytes.size() > 2 &&
            bytes[0] == native::UniversalSystemExclusiveId &&
            bytes[2] == native::SubId1CapabilityInquiry;
    }

    _Use_decl_annotations_
    ci::MidiCapabilityInquiryMessage MidiCapabilityInquiryMessage::FromSystemExclusiveData(
        foundation::Collections::IIterable<uint8_t> const& data) noexcept
    {
        auto message = winrt::make_self<implementation::MidiCapabilityInquiryMessage>();

        try
        {
            auto const bytes = ToByteVector(data);

            for (auto const value : bytes)
            {
                message->m_data.Append(value);
            }

            native::ParsedMessage parsed{};

            if (native::Parse(bytes.data(), bytes.size(), parsed) != native::ParseStatus::Ok)
            {
                return *message;
            }

            message->m_isValid = true;
            message->m_messageType = static_cast<ci::MidiCapabilityInquiryMessageType>(parsed.Type);
            message->m_deviceId = parsed.DeviceId;
            message->m_sourceVersion = parsed.VersionFormat;
            message->m_sourceMuid = MakeUniqueId(parsed.SourceMuid);
            message->m_destinationMuid = MakeUniqueId(parsed.DestinationMuid);
            message->m_outputPathId = parsed.OutputPathId;

            if (parsed.Type == native::MessageType::InvalidateMuid)
            {
                message->m_targetMuid = MakeUniqueId(parsed.TargetMuid);
            }

            if (parsed.HasProfileFields)
            {
                message->m_hasProfileFields = true;

                auto const& profile = parsed.Profile;

                if (profile.HasProfileId)
                {
                    message->m_profileId = MakeProfileId(profile.ProfileId);
                }

                message->m_profileChannelCount = profile.ChannelCount;

                message->m_hasProfileInquiryTarget = profile.HasInquiryTarget;
                message->m_profileInquiryTarget = profile.InquiryTarget;

                // The parser reports where these sit in the buffer it was given rather than copying
                // them, and it has already checked every offset against the real length.
                for (uint32_t i = 0; i < profile.TargetDataByteCount; i++)
                {
                    message->m_profileData.Append(bytes[profile.TargetDataOffset + i]);
                }

                for (uint16_t i = 0; i < profile.EnabledProfileCount; i++)
                {
                    message->m_enabledProfiles.Append(
                        MakeProfileId(bytes.data() + profile.EnabledProfileOffset +
                            (size_t)i * native::ProfileIdByteCount));
                }

                for (uint16_t i = 0; i < profile.DisabledProfileCount; i++)
                {
                    message->m_disabledProfiles.Append(
                        MakeProfileId(bytes.data() + profile.DisabledProfileOffset +
                            (size_t)i * native::ProfileIdByteCount));
                }

                return *message;
            }

            if (parsed.HasAcknowledgmentFields)
            {
                message->m_hasAcknowledgmentFields = true;

                auto const& acknowledgment = parsed.Acknowledgment;

                message->m_originalMessageType =
                    static_cast<ci::MidiCapabilityInquiryMessageType>(acknowledgment.OriginalMessageType);

                message->m_statusCode = acknowledgment.StatusCode;
                message->m_statusData = acknowledgment.StatusData;

                for (auto const value : acknowledgment.Details)
                {
                    message->m_statusDetails.Append(value);
                }

                // Text in a capability inquiry message is seven bit ASCII, so widening each byte is
                // the whole conversion.
                std::wstring statusMessage{};
                statusMessage.reserve(acknowledgment.MessageTextByteCount);

                for (uint16_t i = 0; i < acknowledgment.MessageTextByteCount; i++)
                {
                    statusMessage.push_back(
                        static_cast<wchar_t>(bytes[acknowledgment.MessageTextOffset + i]));
                }

                message->m_statusMessage = winrt::hstring{ statusMessage };

                return *message;
            }

            if (!parsed.HasPropertyExchangeFields)
            {
                return *message;
            }

            message->m_hasPropertyExchangeFields = true;
            message->m_requestId = parsed.PropertyExchange.RequestId;
            message->m_chunkNumber = parsed.PropertyExchange.ChunkNumber;
            message->m_chunkCount = parsed.PropertyExchange.ChunkCount;

            // The parser reports where the header and body sit inside the buffer it was given
            // rather than copying them, so both are lifted out here against that same buffer.
            auto const headerOffset = parsed.PropertyExchange.HeaderOffset;
            auto const headerCount = parsed.PropertyExchange.HeaderByteCount;

            if (headerCount > 0 && headerOffset + headerCount <= bytes.size())
            {
                // Header bytes are seven bit ASCII, so widening each byte is the whole conversion.
                std::wstring headerText{};
                headerText.reserve(headerCount);

                for (uint16_t i = 0; i < headerCount; i++)
                {
                    headerText.push_back(static_cast<wchar_t>(bytes[headerOffset + i]));
                }

                message->m_headerText = winrt::hstring{ headerText };

                json::JsonObject headerObject{ nullptr };

                // A device is free to send a header this cannot parse. That is not a reason to
                // discard the message: HeaderText still shows what arrived.
                if (json::JsonObject::TryParse(message->m_headerText, headerObject))
                {
                    message->m_header = headerObject;
                }
            }

            auto const bodyOffset = parsed.PropertyExchange.DataOffset;
            auto const bodyCount = parsed.PropertyExchange.DataByteCount;

            if (bodyCount > 0 && bodyOffset + bodyCount <= bytes.size())
            {
                for (uint16_t i = 0; i < bodyCount; i++)
                {
                    message->m_body.Append(bytes[bodyOffset + i]);
                }
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        return *message;
    }

    _Use_decl_annotations_
    ci::MidiCapabilityInquiryMessage MidiCapabilityInquiryMessage::FromUmpMessages(
        foundation::Collections::IIterable<midi2::MidiMessage64> const& messages) noexcept
    {
        try
        {
            auto const payload =
                winrt::Windows::Devices::Midi2::Utilities::Messages::implementation::
                    MidiSystemExclusive7MessageHelper::GetDataBytesFromMultipleSystemExclusiveMessages(
                        messages);

            return FromSystemExclusiveData(payload);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        return winrt::make<implementation::MidiCapabilityInquiryMessage>();
    }
}
