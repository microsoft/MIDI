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
