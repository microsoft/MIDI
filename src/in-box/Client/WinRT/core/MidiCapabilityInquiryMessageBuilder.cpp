// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiCapabilityInquiryMessageBuilder.h"

// Everything below is consumed here, so the implementation headers are needed as well as the
// projections.
#include "MidiSystemExclusive7MessageBuilder.h"

#include "MidiCiMessage.h"

#include "CapabilityInquiry.MidiCapabilityInquiryMessageBuilder.g.cpp"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    namespace
    {
        namespace native = ::WindowsMidiServicesCapabilityInquiry;

        // Big enough for the largest message any builder here produces in one piece. Property
        // exchange chunks are sized against what the destination declared it can receive, which is
        // checked against this before anything is written.
        constexpr size_t MaximumMessageByteCount = 0x4100;

        foundation::Collections::IVector<midi2::MidiMessage64> EmptyMessageList() noexcept
        {
            try
            {
                return winrt::single_threaded_vector<midi2::MidiMessage64>();
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
                return nullptr;
            }
        }

        uint32_t MuidValue(_In_ ci::MidiUniqueId const& id) noexcept
        {
            return id == nullptr ? 0 : id.AsCombined28BitValue();
        }

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

        // A header travels as seven bit text. JSON's own structural characters are all ASCII, so
        // anything above that can only be inside a string, where a six character escape is both
        // valid JSON and within the range a system exclusive message can carry. Escaping each UTF-16
        // unit separately is correct for a surrogate pair too.
        std::vector<uint8_t> ToHeaderBytes(_In_ json::JsonObject const& header) noexcept
        {
            std::vector<uint8_t> bytes{};

            try
            {
                if (header == nullptr)
                {
                    return bytes;
                }

                auto const text = header.Stringify();

                for (auto const character : std::wstring_view{ text })
                {
                    if (character < 0x80)
                    {
                        bytes.push_back(static_cast<uint8_t>(character));
                    }
                    else
                    {
                        auto const escape = std::format("\\u{:04x}", static_cast<uint16_t>(character));

                        for (auto const escapeCharacter : escape)
                        {
                            bytes.push_back(static_cast<uint8_t>(escapeCharacter));
                        }
                    }
                }
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
                return {};
            }

            return bytes;
        }

        foundation::Collections::IVector<midi2::MidiMessage64> ToUmpMessages(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_reads_(byteCount) uint8_t const* const payload,
            _In_ size_t const byteCount) noexcept
        {
            try
            {
                if (payload == nullptr || byteCount == 0)
                {
                    return EmptyMessageList();
                }

                auto data = winrt::single_threaded_vector<uint8_t>();

                for (size_t i = 0; i < byteCount; i++)
                {
                    data.Append(payload[i]);
                }

                return msgs::implementation::MidiSystemExclusive7MessageBuilder::
                    BuildSystemExclusive7Messages(timestamp, group, data);
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
                return EmptyMessageList();
            }
        }

        // Fills in the parts of a discovery message that a discovery and its reply share.
        void FillDiscoveryFields(
            _Inout_ native::DiscoveryReplyFields& fields,
            _In_ midi2enum::MidiDeclaredDeviceIdentity const& identity,
            _In_ ci::MidiCapabilityInquiryCategories const supportedCategories,
            _In_ uint32_t const receivableMaximumSystemExclusiveSize,
            _In_ uint8_t const outputPathId) noexcept
        {
            if (identity != nullptr)
            {
                auto const systemExclusiveId = identity.SystemExclusiveId();

                if (systemExclusiveId.size() >= 3)
                {
                    fields.ManufacturerSysExId[0] = systemExclusiveId[0];
                    fields.ManufacturerSysExId[1] = systemExclusiveId[1];
                    fields.ManufacturerSysExId[2] = systemExclusiveId[2];
                }

                fields.DeviceFamily = static_cast<uint16_t>(
                    identity.DeviceFamilyLsb() | (identity.DeviceFamilyMsb() << 7));

                fields.DeviceFamilyModelNumber = static_cast<uint16_t>(
                    identity.DeviceFamilyModelNumberLsb() | (identity.DeviceFamilyModelNumberMsb() << 7));

                auto const revision = identity.SoftwareRevisionLevel();

                if (revision.size() >= 4)
                {
                    fields.SoftwareRevisionLevel[0] = revision[0];
                    fields.SoftwareRevisionLevel[1] = revision[1];
                    fields.SoftwareRevisionLevel[2] = revision[2];
                    fields.SoftwareRevisionLevel[3] = revision[3];
                }
            }

            fields.CapabilityCategories = static_cast<uint8_t>(supportedCategories) & 0x7F;
            fields.ReceivableMaximumSysExSize = receivableMaximumSystemExclusiveSize;
            fields.OutputPathId = outputPathId;
        }

        void FillProfileId(
            _Out_writes_(5) uint8_t* const destination,
            _In_ ci::MidiProfileId const& profileId) noexcept
        {
            if (profileId == nullptr)
            {
                destination[0] = 0;
                destination[1] = 0;
                destination[2] = 0;
                destination[3] = 0;
                destination[4] = 0;

                return;
            }

            destination[0] = profileId.IdByte1();
            destination[1] = profileId.IdByte2();
            destination[2] = profileId.IdByte3();
            destination[3] = profileId.IdByte4();
            destination[4] = profileId.IdByte5();
        }

        // The profile messages that differ only in which fields they carry all go through here.
        foundation::Collections::IVector<midi2::MidiMessage64> BuildProfileMessageInternal(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ native::MessageType const messageType,
            _In_ uint8_t const deviceId,
            _In_ ci::MidiUniqueId const& sourceMuid,
            _In_ uint32_t const destinationMuid,
            _In_ ci::MidiProfileId const& profileId,
            _In_ uint16_t const channelCount,
            _In_ uint8_t const inquiryTarget,
            _In_ std::vector<uint8_t> const& data) noexcept
        {
            try
            {
                native::ProfileMessageFields fields{};

                fields.Type = messageType;
                fields.DeviceId = deviceId;
                fields.SourceMuid = MuidValue(sourceMuid);
                fields.DestinationMuid = destinationMuid;
                fields.ChannelCount = channelCount;
                fields.InquiryTarget = inquiryTarget;
                fields.Data = data.empty() ? nullptr : data.data();
                fields.DataByteCount = static_cast<uint32_t>(data.size());

                FillProfileId(fields.ProfileId, profileId);

                std::vector<uint8_t> buffer(
                    native::CommonHeaderByteCount + native::ProfileIdByteCount + 8 + data.size());

                auto const written = native::BuildProfileMessage(fields, buffer.data(), buffer.size());

                return ToUmpMessages(timestamp, group, buffer.data(), written);
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
                return EmptyMessageList();
            }
        }

        foundation::Collections::IVector<midi2::MidiMessage64> BuildAcknowledgmentInternal(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ native::MessageType const messageType,
            _In_ uint8_t const deviceId,
            _In_ ci::MidiUniqueId const& sourceMuid,
            _In_ ci::MidiUniqueId const& destinationMuid,
            _In_ ci::MidiCapabilityInquiryMessageType const originalMessageType,
            _In_ uint8_t const statusCode,
            _In_ uint8_t const statusData,
            _In_ foundation::Collections::IIterable<uint8_t> const& statusDetails,
            _In_ winrt::hstring const& statusMessage) noexcept
        {
            try
            {
                native::AcknowledgmentFields fields{};

                fields.OriginalMessageType = static_cast<uint8_t>(originalMessageType);
                fields.StatusCode = statusCode;
                fields.StatusData = statusData;

                auto const details = ToByteVector(statusDetails);

                for (size_t i = 0; i < details.size() && i < 5; i++)
                {
                    fields.Details[i] = details[i];
                }

                // Text goes on the wire as seven bit bytes. A character outside that range is
                // refused rather than truncated, because a garbled explanation of a failure is
                // worse than none.
                std::vector<uint8_t> text{};

                for (auto const character : std::wstring_view{ statusMessage })
                {
                    if (character > 0x7F)
                    {
                        return EmptyMessageList();
                    }

                    text.push_back(static_cast<uint8_t>(character));
                }

                if (text.size() > 0x3FFF)
                {
                    return EmptyMessageList();
                }

                std::vector<uint8_t> buffer(native::AcknowledgmentFixedByteCount + text.size());

                auto const written = native::BuildAcknowledgment(
                    messageType,
                    deviceId,
                    MuidValue(sourceMuid),
                    MuidValue(destinationMuid),
                    fields,
                    text.empty() ? nullptr : text.data(),
                    static_cast<uint16_t>(text.size()),
                    buffer.data(),
                    buffer.size());

                return ToUmpMessages(timestamp, group, buffer.data(), written);
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
                return EmptyMessageList();
            }
        }

        // Sets up the chunker both the builder and the chunk count use, so the two can never
        // disagree about how many chunks a transfer takes.
        bool PlanPropertyChunks(
            _In_ native::PropertyReplyChunker& chunker,
            _In_ std::vector<uint8_t> const& header,
            _In_ std::vector<uint8_t> const& body,
            _In_ uint32_t const destinationMaximumSystemExclusiveSize) noexcept
        {
            auto maximumSize = static_cast<size_t>(destinationMaximumSystemExclusiveSize);

            // A device which said nothing still has to accept the minimum the specification
            // requires of anything doing property exchange.
            if (maximumSize == 0)
            {
                maximumSize = MidiCapabilityInquiryMessageBuilder::MinimumReceivableSystemExclusiveSize();
            }

            if (maximumSize > MaximumMessageByteCount)
            {
                maximumSize = MaximumMessageByteCount;
            }

            if (header.size() > 0x3FFF)
            {
                return false;
            }

            chunker.Resource = body.empty() ? nullptr : body.data();
            chunker.ResourceByteCount = body.size();
            chunker.Header = header.empty() ? nullptr : header.data();
            chunker.HeaderByteCount = static_cast<uint16_t>(header.size());

            return chunker.Plan(maximumSize);
        }
    }


    _Use_decl_annotations_
    foundation::Collections::IVector<midi2::MidiMessage64>
    MidiCapabilityInquiryMessageBuilder::BuildDiscovery(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        ci::MidiUniqueId const& sourceMuid,
        midi2enum::MidiDeclaredDeviceIdentity const& identity,
        ci::MidiCapabilityInquiryCategories const supportedCategories,
        uint32_t const receivableMaximumSystemExclusiveSize,
        uint8_t const outputPathId) noexcept
    {
        try
        {
            native::DiscoveryReplyFields fields{};

            fields.SourceMuid = MuidValue(sourceMuid);

            FillDiscoveryFields(
                fields, identity, supportedCategories,
                receivableMaximumSystemExclusiveSize, outputPathId);

            uint8_t buffer[native::DiscoveryByteCount]{};

            auto const written = native::BuildDiscovery(fields, buffer, sizeof(buffer));

            return ToUmpMessages(timestamp, group, buffer, written);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return EmptyMessageList();
        }
    }

    _Use_decl_annotations_
    foundation::Collections::IVector<midi2::MidiMessage64>
    MidiCapabilityInquiryMessageBuilder::BuildDiscoveryReply(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        ci::MidiUniqueId const& sourceMuid,
        ci::MidiUniqueId const& destinationMuid,
        midi2enum::MidiDeclaredDeviceIdentity const& identity,
        ci::MidiCapabilityInquiryCategories const supportedCategories,
        uint32_t const receivableMaximumSystemExclusiveSize,
        uint8_t const outputPathId,
        uint8_t const functionBlockNumber) noexcept
    {
        try
        {
            native::DiscoveryReplyFields fields{};

            fields.SourceMuid = MuidValue(sourceMuid);
            fields.DestinationMuid = MuidValue(destinationMuid);
            fields.FunctionBlockNumber = functionBlockNumber;

            FillDiscoveryFields(
                fields, identity, supportedCategories,
                receivableMaximumSystemExclusiveSize, outputPathId);

            uint8_t buffer[native::DiscoveryReplyByteCount]{};

            auto const written = native::BuildDiscoveryReply(fields, buffer, sizeof(buffer));

            return ToUmpMessages(timestamp, group, buffer, written);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return EmptyMessageList();
        }
    }

    _Use_decl_annotations_
    foundation::Collections::IVector<midi2::MidiMessage64>
    MidiCapabilityInquiryMessageBuilder::BuildInvalidateMuid(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        ci::MidiUniqueId const& sourceMuid,
        ci::MidiUniqueId const& muidToInvalidate) noexcept
    {
        try
        {
            uint8_t buffer[native::InvalidateMuidByteCount]{};

            auto const written = native::BuildInvalidateMuid(
                MuidValue(sourceMuid), MuidValue(muidToInvalidate), buffer, sizeof(buffer));

            return ToUmpMessages(timestamp, group, buffer, written);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return EmptyMessageList();
        }
    }

    _Use_decl_annotations_
    foundation::Collections::IVector<midi2::MidiMessage64>
    MidiCapabilityInquiryMessageBuilder::BuildAck(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        uint8_t const deviceId,
        ci::MidiUniqueId const& sourceMuid,
        ci::MidiUniqueId const& destinationMuid,
        ci::MidiCapabilityInquiryMessageType const originalMessageType,
        uint8_t const statusCode,
        uint8_t const statusData,
        foundation::Collections::IIterable<uint8_t> const& statusDetails,
        winrt::hstring const& statusMessage) noexcept
    {
        return BuildAcknowledgmentInternal(
            timestamp, group, native::MessageType::Ack, deviceId, sourceMuid, destinationMuid,
            originalMessageType, statusCode, statusData, statusDetails, statusMessage);
    }

    _Use_decl_annotations_
    foundation::Collections::IVector<midi2::MidiMessage64>
    MidiCapabilityInquiryMessageBuilder::BuildNak(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        uint8_t const deviceId,
        ci::MidiUniqueId const& sourceMuid,
        ci::MidiUniqueId const& destinationMuid,
        ci::MidiCapabilityInquiryMessageType const originalMessageType,
        uint8_t const statusCode,
        uint8_t const statusData,
        foundation::Collections::IIterable<uint8_t> const& statusDetails,
        winrt::hstring const& statusMessage) noexcept
    {
        return BuildAcknowledgmentInternal(
            timestamp, group, native::MessageType::Nak, deviceId, sourceMuid, destinationMuid,
            originalMessageType, statusCode, statusData, statusDetails, statusMessage);
    }

    _Use_decl_annotations_
    foundation::Collections::IVector<midi2::MidiMessage64>
    MidiCapabilityInquiryMessageBuilder::BuildPropertyExchangeCapabilitiesInquiry(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        ci::MidiUniqueId const& sourceMuid,
        ci::MidiUniqueId const& destinationMuid,
        uint8_t const maximumSimultaneousRequests) noexcept
    {
        try
        {
            // The inquiry and its reply are the same shape, so the builder for the reply writes
            // both and only the sub id differs.
            uint8_t buffer[native::PropertyExchangeCapabilitiesByteCount]{};

            auto const written = native::BuildPropertyExchangeCapabilitiesReply(
                MuidValue(sourceMuid), MuidValue(destinationMuid),
                maximumSimultaneousRequests, buffer, sizeof(buffer));

            if (written == 0)
            {
                return EmptyMessageList();
            }

            buffer[native::OffsetSubId2] =
                static_cast<uint8_t>(native::MessageType::PropertyExchangeCapabilitiesInquiry);

            return ToUmpMessages(timestamp, group, buffer, written);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return EmptyMessageList();
        }
    }

    _Use_decl_annotations_
    foundation::Collections::IVector<midi2::MidiMessage64>
    MidiCapabilityInquiryMessageBuilder::BuildPropertyExchangeCapabilitiesReply(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        ci::MidiUniqueId const& sourceMuid,
        ci::MidiUniqueId const& destinationMuid,
        uint8_t const maximumSimultaneousRequests) noexcept
    {
        try
        {
            uint8_t buffer[native::PropertyExchangeCapabilitiesByteCount]{};

            auto const written = native::BuildPropertyExchangeCapabilitiesReply(
                MuidValue(sourceMuid), MuidValue(destinationMuid),
                maximumSimultaneousRequests, buffer, sizeof(buffer));

            return ToUmpMessages(timestamp, group, buffer, written);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return EmptyMessageList();
        }
    }

    _Use_decl_annotations_
    foundation::Collections::IVector<midi2::MidiMessage64>
    MidiCapabilityInquiryMessageBuilder::BuildPropertyGetDataInquiry(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        ci::MidiUniqueId const& sourceMuid,
        ci::MidiUniqueId const& destinationMuid,
        uint8_t const requestId,
        json::JsonObject const& header) noexcept
    {
        // A get carries no body, so it is always one chunk and the destination's size never
        // matters.
        return BuildPropertyMessage(
            timestamp, group, ci::MidiCapabilityInquiryMessageType::PropertyGetDataInquiry,
            sourceMuid, destinationMuid, requestId, header, nullptr, 0);
    }

    _Use_decl_annotations_
    foundation::Collections::IVector<midi2::MidiMessage64>
    MidiCapabilityInquiryMessageBuilder::BuildPropertyMessage(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        ci::MidiCapabilityInquiryMessageType const messageType,
        ci::MidiUniqueId const& sourceMuid,
        ci::MidiUniqueId const& destinationMuid,
        uint8_t const requestId,
        json::JsonObject const& header,
        foundation::Collections::IIterable<uint8_t> const& body,
        uint32_t const destinationMaximumSystemExclusiveSize) noexcept
    {
        try
        {
            auto const nativeType = static_cast<native::MessageType>(messageType);

            if (!native::MessageTypeCarriesPropertyExchangeData(nativeType))
            {
                return EmptyMessageList();
            }

            auto const headerBytes = ToHeaderBytes(header);
            auto const bodyBytes = ToByteVector(body);

            native::PropertyReplyChunker chunker{};

            chunker.Type = nativeType;

            if (!PlanPropertyChunks(chunker, headerBytes, bodyBytes, destinationMaximumSystemExclusiveSize))
            {
                return EmptyMessageList();
            }

            auto messages = winrt::single_threaded_vector<midi2::MidiMessage64>();

            std::vector<uint8_t> buffer(MaximumMessageByteCount);

            for (uint16_t chunkNumber = 1; chunkNumber <= chunker.ChunkCount; chunkNumber++)
            {
                auto const written = chunker.BuildChunk(
                    chunkNumber,
                    MuidValue(sourceMuid),
                    MuidValue(destinationMuid),
                    requestId,
                    buffer.data(),
                    buffer.size());

                if (written == 0)
                {
                    return EmptyMessageList();
                }

                // Each chunk is its own system exclusive transfer, so the packets of one chunk
                // start and end within it and the chunks can simply follow one another.
                auto const chunkMessages = ToUmpMessages(timestamp, group, buffer.data(), written);

                if (chunkMessages == nullptr || chunkMessages.Size() == 0)
                {
                    return EmptyMessageList();
                }

                for (auto const& message : chunkMessages)
                {
                    messages.Append(message);
                }
            }

            return messages;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return EmptyMessageList();
        }
    }

    _Use_decl_annotations_
    uint16_t MidiCapabilityInquiryMessageBuilder::GetPropertyChunkCount(
        json::JsonObject const& header,
        uint32_t const bodyByteCount,
        uint32_t const destinationMaximumSystemExclusiveSize) noexcept
    {
        try
        {
            auto const headerBytes = ToHeaderBytes(header);

            // Only the size of the body matters here, so it is not worth asking the caller for the
            // bytes themselves just to count them.
            std::vector<uint8_t> body(bodyByteCount);

            native::PropertyReplyChunker chunker{};

            if (!PlanPropertyChunks(chunker, headerBytes, body, destinationMaximumSystemExclusiveSize))
            {
                return 0;
            }

            return chunker.ChunkCount;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return 0;
        }
    }

    _Use_decl_annotations_
    foundation::Collections::IVector<midi2::MidiMessage64>
    MidiCapabilityInquiryMessageBuilder::BuildProfileInquiry(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        uint8_t const deviceId,
        ci::MidiUniqueId const& sourceMuid,
        ci::MidiUniqueId const& destinationMuid) noexcept
    {
        return BuildProfileMessageInternal(
            timestamp, group, native::MessageType::ProfileInquiry, deviceId,
            sourceMuid, MuidValue(destinationMuid), nullptr, 0, 0, {});
    }

    _Use_decl_annotations_
    foundation::Collections::IVector<midi2::MidiMessage64>
    MidiCapabilityInquiryMessageBuilder::BuildProfileInquiryReply(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        uint8_t const deviceId,
        ci::MidiUniqueId const& sourceMuid,
        ci::MidiUniqueId const& destinationMuid,
        foundation::Collections::IIterable<ci::MidiProfileId> const& enabledProfiles,
        foundation::Collections::IIterable<ci::MidiProfileId> const& disabledProfiles) noexcept
    {
        try
        {
            auto flatten = [](foundation::Collections::IIterable<ci::MidiProfileId> const& source)
            {
                std::vector<uint8_t> bytes{};

                if (source != nullptr)
                {
                    for (auto const& profileId : source)
                    {
                        if (profileId == nullptr)
                        {
                            continue;
                        }

                        uint8_t id[native::ProfileIdByteCount]{};

                        FillProfileId(id, profileId);

                        bytes.insert(bytes.end(), std::begin(id), std::end(id));
                    }
                }

                return bytes;
            };

            auto const enabled = flatten(enabledProfiles);
            auto const disabled = flatten(disabledProfiles);

            auto const enabledCount = enabled.size() / native::ProfileIdByteCount;
            auto const disabledCount = disabled.size() / native::ProfileIdByteCount;

            if (enabledCount > 0x3FFF || disabledCount > 0x3FFF)
            {
                return EmptyMessageList();
            }

            std::vector<uint8_t> buffer(
                native::CommonHeaderByteCount + 4 + enabled.size() + disabled.size());

            auto const written = native::BuildProfileInquiryReply(
                deviceId,
                MuidValue(sourceMuid),
                MuidValue(destinationMuid),
                enabled.empty() ? nullptr : enabled.data(),
                static_cast<uint16_t>(enabledCount),
                disabled.empty() ? nullptr : disabled.data(),
                static_cast<uint16_t>(disabledCount),
                buffer.data(),
                buffer.size());

            return ToUmpMessages(timestamp, group, buffer.data(), written);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return EmptyMessageList();
        }
    }

    _Use_decl_annotations_
    foundation::Collections::IVector<midi2::MidiMessage64>
    MidiCapabilityInquiryMessageBuilder::BuildSetProfileOn(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        uint8_t const deviceId,
        ci::MidiUniqueId const& sourceMuid,
        ci::MidiUniqueId const& destinationMuid,
        ci::MidiProfileId const& profileId,
        uint16_t const channelCount) noexcept
    {
        return BuildProfileMessageInternal(
            timestamp, group, native::MessageType::SetProfileOn, deviceId,
            sourceMuid, MuidValue(destinationMuid), profileId, channelCount, 0, {});
    }

    _Use_decl_annotations_
    foundation::Collections::IVector<midi2::MidiMessage64>
    MidiCapabilityInquiryMessageBuilder::BuildSetProfileOff(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        uint8_t const deviceId,
        ci::MidiUniqueId const& sourceMuid,
        ci::MidiUniqueId const& destinationMuid,
        ci::MidiProfileId const& profileId) noexcept
    {
        // The two bytes where Set Profile On asks for channels are reserved here, and zero.
        return BuildProfileMessageInternal(
            timestamp, group, native::MessageType::SetProfileOff, deviceId,
            sourceMuid, MuidValue(destinationMuid), profileId, 0, 0, {});
    }

    _Use_decl_annotations_
    foundation::Collections::IVector<midi2::MidiMessage64>
    MidiCapabilityInquiryMessageBuilder::BuildProfileEnabledReport(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        uint8_t const deviceId,
        ci::MidiUniqueId const& sourceMuid,
        ci::MidiProfileId const& profileId,
        uint16_t const channelCount) noexcept
    {
        return BuildProfileMessageInternal(
            timestamp, group, native::MessageType::ProfileEnabledReport, deviceId,
            sourceMuid, native::MuidBroadcast, profileId, channelCount, 0, {});
    }

    _Use_decl_annotations_
    foundation::Collections::IVector<midi2::MidiMessage64>
    MidiCapabilityInquiryMessageBuilder::BuildProfileDisabledReport(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        uint8_t const deviceId,
        ci::MidiUniqueId const& sourceMuid,
        ci::MidiProfileId const& profileId,
        uint16_t const channelCount) noexcept
    {
        return BuildProfileMessageInternal(
            timestamp, group, native::MessageType::ProfileDisabledReport, deviceId,
            sourceMuid, native::MuidBroadcast, profileId, channelCount, 0, {});
    }

    _Use_decl_annotations_
    foundation::Collections::IVector<midi2::MidiMessage64>
    MidiCapabilityInquiryMessageBuilder::BuildProfileAddedReport(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        uint8_t const deviceId,
        ci::MidiUniqueId const& sourceMuid,
        ci::MidiProfileId const& profileId) noexcept
    {
        return BuildProfileMessageInternal(
            timestamp, group, native::MessageType::ProfileAddedReport, deviceId,
            sourceMuid, native::MuidBroadcast, profileId, 0, 0, {});
    }

    _Use_decl_annotations_
    foundation::Collections::IVector<midi2::MidiMessage64>
    MidiCapabilityInquiryMessageBuilder::BuildProfileRemovedReport(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        uint8_t const deviceId,
        ci::MidiUniqueId const& sourceMuid,
        ci::MidiProfileId const& profileId) noexcept
    {
        return BuildProfileMessageInternal(
            timestamp, group, native::MessageType::ProfileRemovedReport, deviceId,
            sourceMuid, native::MuidBroadcast, profileId, 0, 0, {});
    }

    _Use_decl_annotations_
    foundation::Collections::IVector<midi2::MidiMessage64>
    MidiCapabilityInquiryMessageBuilder::BuildProfileDetailsInquiry(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        uint8_t const deviceId,
        ci::MidiUniqueId const& sourceMuid,
        ci::MidiUniqueId const& destinationMuid,
        ci::MidiProfileId const& profileId,
        uint8_t const inquiryTarget) noexcept
    {
        return BuildProfileMessageInternal(
            timestamp, group, native::MessageType::ProfileDetailsInquiry, deviceId,
            sourceMuid, MuidValue(destinationMuid), profileId, 0, inquiryTarget, {});
    }

    _Use_decl_annotations_
    foundation::Collections::IVector<midi2::MidiMessage64>
    MidiCapabilityInquiryMessageBuilder::BuildProfileDetailsReply(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        uint8_t const deviceId,
        ci::MidiUniqueId const& sourceMuid,
        ci::MidiUniqueId const& destinationMuid,
        ci::MidiProfileId const& profileId,
        uint8_t const inquiryTarget,
        foundation::Collections::IIterable<uint8_t> const& inquiryTargetData) noexcept
    {
        auto const data = ToByteVector(inquiryTargetData);

        if (data.size() > 0x3FFF)
        {
            return EmptyMessageList();
        }

        return BuildProfileMessageInternal(
            timestamp, group, native::MessageType::ProfileDetailsInquiryReply, deviceId,
            sourceMuid, MuidValue(destinationMuid), profileId, 0, inquiryTarget, data);
    }

    _Use_decl_annotations_
    foundation::Collections::IVector<midi2::MidiMessage64>
    MidiCapabilityInquiryMessageBuilder::BuildProfileSpecificData(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        uint8_t const deviceId,
        ci::MidiUniqueId const& sourceMuid,
        ci::MidiUniqueId const& destinationMuid,
        ci::MidiProfileId const& profileId,
        foundation::Collections::IIterable<uint8_t> const& data) noexcept
    {
        return BuildProfileMessageInternal(
            timestamp, group, native::MessageType::ProfileSpecificData, deviceId,
            sourceMuid, MuidValue(destinationMuid), profileId, 0, 0, ToByteVector(data));
    }

    _Use_decl_annotations_
    foundation::Collections::IVector<midi2::MidiMessage64>
    MidiCapabilityInquiryMessageBuilder::BuildFromSystemExclusiveData(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        foundation::Collections::IIterable<uint8_t> const& data) noexcept
    {
        try
        {
            return msgs::implementation::MidiSystemExclusive7MessageBuilder::
                BuildSystemExclusive7Messages(timestamp, group, data);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return EmptyMessageList();
        }
    }
}
