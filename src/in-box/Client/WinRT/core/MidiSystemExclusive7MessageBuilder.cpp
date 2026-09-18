// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiSystemExclusive7MessageBuilder.h"

// Building each packet reuses the single message builder, which has to be included as an
// implementation header to be callable from here.
#include "MidiMessageBuilder.h"

#include "Utilities.Messages.MidiSystemExclusive7MessageBuilder.g.cpp"

namespace winrt::Windows::Devices::Midi2::Utilities::Messages::implementation
{
    namespace
    {
        // Status nibbles for a system exclusive packet, from the UMP specification.
        constexpr uint8_t StatusCompleteInOnePacket = 0x0;
        constexpr uint8_t StatusStart = 0x1;
        constexpr uint8_t StatusContinue = 0x2;
        constexpr uint8_t StatusEnd = 0x3;

        constexpr uint32_t DataBytesPerMessage = 6;
    }

    _Use_decl_annotations_
    uint32_t MidiSystemExclusive7MessageBuilder::GetMessageCountForDataByteCount(
        uint32_t const dataByteCount) noexcept
    {
        // An empty payload is still one packet. It is a complete, if empty, system exclusive
        // message, and returning zero would say nothing needs sending at all.
        if (dataByteCount == 0)
        {
            return 1;
        }

        return (dataByteCount + DataBytesPerMessage - 1) / DataBytesPerMessage;
    }

    _Use_decl_annotations_
    foundation::Collections::IVector<midi2::MidiMessage64>
    MidiSystemExclusive7MessageBuilder::BuildSystemExclusive7Messages(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        foundation::Collections::IIterable<uint8_t> const& dataBytes) noexcept
    {
        auto messages = winrt::single_threaded_vector<midi2::MidiMessage64>();

        try
        {
            std::vector<uint8_t> payload{};

            if (dataBytes != nullptr)
            {
                for (auto const value : dataBytes)
                {
                    // Refuse rather than mask. A high bit here means the caller handed us something
                    // that is not seven bit data, and quietly changing it would put corrupt bytes
                    // on the wire under the caller's name.
                    if ((value & 0x80) != 0)
                    {
                        return winrt::single_threaded_vector<midi2::MidiMessage64>();
                    }

                    payload.push_back(value);
                }
            }

            auto const totalMessages = GetMessageCountForDataByteCount(
                static_cast<uint32_t>(payload.size()));

            for (uint32_t messageIndex = 0; messageIndex < totalMessages; messageIndex++)
            {
                auto const offset = messageIndex * DataBytesPerMessage;
                auto const remaining = static_cast<uint32_t>(payload.size()) - offset;
                auto const countThisMessage = (remaining < DataBytesPerMessage)
                    ? remaining : DataBytesPerMessage;

                uint8_t status{};

                if (totalMessages == 1)
                {
                    status = StatusCompleteInOnePacket;
                }
                else if (messageIndex == 0)
                {
                    status = StatusStart;
                }
                else if (messageIndex == totalMessages - 1)
                {
                    status = StatusEnd;
                }
                else
                {
                    status = StatusContinue;
                }

                uint8_t bytes[DataBytesPerMessage]{};

                for (uint32_t i = 0; i < countThisMessage; i++)
                {
                    bytes[i] = payload[offset + i];
                }

                messages.Append(
                    implementation::MidiMessageBuilder::BuildSystemExclusive7Message(
                        timestamp,
                        group,
                        status,
                        static_cast<uint8_t>(countThisMessage),
                        bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]));
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return winrt::single_threaded_vector<midi2::MidiMessage64>();
        }

        return messages;
    }
}
