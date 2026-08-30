// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiSystemExclusiveReceiver.h"
#include "MidiSystemExclusiveReceivedEventArgs.h"
#include "Utilities.SysExTransfer.MidiSystemExclusiveReceiver.g.cpp"

#define MINIMUM_MAXIMUM_BYTES_PER_EVENT 8

namespace winrt::Windows::Devices::Midi2::Utilities::SysExTransfer::implementation
{
    _Use_decl_annotations_
    MidiSystemExclusiveReceiver::MidiSystemExclusiveReceiver(
        midi2::MidiEndpointConnection const& sourceConnection,
        midi2::MidiGroup const& sourceGroup,
        uint32_t maximumBytesPerEvent)
    {
        if (sourceConnection == nullptr)
        {
            throw winrt::hresult_error(E_INVALIDARG, L"sourceConnection is required");
        }

        if (sourceGroup == nullptr)
        {
            throw winrt::hresult_error(E_INVALIDARG, L"sourceGroup is required");
        }

        m_connection = sourceConnection;
        m_group = sourceGroup;
        m_maximumBytesPerEvent = std::max<uint32_t>(maximumBytesPerEvent, MINIMUM_MAXIMUM_BYTES_PER_EVENT);

        m_pending.reserve(m_maximumBytesPerEvent);
    }

    bool MidiSystemExclusiveReceiver::Start()
    {
        try
        {
            std::lock_guard<std::mutex> guard{ m_lock };

            if (m_isReceiving || m_connection == nullptr)
            {
                return false;
            }

            m_messageReceivedToken = m_connection.MessageReceived(
                { this, &MidiSystemExclusiveReceiver::OnMessageReceived });

            m_isReceiving = true;

            return true;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return false;
        }
    }

    void MidiSystemExclusiveReceiver::Stop()
    {
        try
        {
            {
                std::lock_guard<std::mutex> guard{ m_lock };

                if (!m_isReceiving)
                {
                    return;
                }

                if (m_connection != nullptr)
                {
                    m_connection.MessageReceived(m_messageReceivedToken);
                }

                m_isReceiving = false;
            }

            // documented behavior: whatever is still buffered is handed out now, so stopping
            // may raise one or more further events
            RaisePending(true);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    void MidiSystemExclusiveReceiver::Close() noexcept
    {
        try
        {
            Stop();

            m_connection = nullptr;
            m_group = nullptr;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    _Use_decl_annotations_
    void MidiSystemExclusiveReceiver::OnMessageReceived(
        foundation::IInspectable const&,
        midi2::MidiMessageReceivedEventArgs const& args) noexcept
    {
        try
        {
            if (args == nullptr)
            {
                return;
            }

            uint32_t word0{ 0 };
            uint32_t word1{ 0 };
            uint32_t word2{ 0 };
            uint32_t word3{ 0 };

            auto const wordCount = args.FillWords(word0, word1, word2, word3);

            // SysEx7 only, and only for the group this receiver was created for
            if (wordCount < 2)
            {
                return;
            }

            if (((word0 >> 28) & 0x0F) != static_cast<uint32_t>(midi2::MidiMessageType::DataMessage64))
            {
                return;
            }

            if (((word0 >> 24) & 0x0F) != m_group.Index())
            {
                return;
            }

            auto const status = (word0 >> 20) & 0x0F;
            auto const count = std::min<uint32_t>((word0 >> 16) & 0x0F, 6);

            uint8_t const payload[6]
            {
                static_cast<uint8_t>((word0 >> 8) & 0x7F),
                static_cast<uint8_t>(word0 & 0x7F),
                static_cast<uint8_t>((word1 >> 24) & 0x7F),
                static_cast<uint8_t>((word1 >> 16) & 0x7F),
                static_cast<uint8_t>((word1 >> 8) & 0x7F),
                static_cast<uint8_t>(word1 & 0x7F)
            };

            {
                std::lock_guard<std::mutex> guard{ m_lock };

                // the UMP form carries no F0/F7, so the framing is restored here and callers can
                // write the bytes straight to a .syx file
                if (status == 0 || status == 1)
                {
                    AppendPayloadByte(0xF0);
                    m_insideMessage = true;
                    m_pendingSawStart = true;
                }

                for (uint32_t i = 0; i < count; i++)
                {
                    AppendPayloadByte(payload[i]);
                }

                if (status == 0 || status == 3)
                {
                    AppendPayloadByte(0xF7);
                    m_insideMessage = false;
                    m_pendingSawEnd = true;
                    m_countMessagesReceived++;
                }
            }

            // a completed message, or enough bytes to be worth handing over
            if (m_pendingSawEnd || m_pending.size() >= m_maximumBytesPerEvent)
            {
                RaisePending(false);
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    _Use_decl_annotations_
    void MidiSystemExclusiveReceiver::AppendPayloadByte(uint8_t value) noexcept
    {
        m_pending.push_back(value);
        m_countBytesReceived++;
    }

    _Use_decl_annotations_
    void MidiSystemExclusiveReceiver::RaisePending(bool flushing) noexcept
    {
        try
        {
            std::vector<uint8_t> block{};
            bool isPartial{ false };
            midi2::MidiGroup group{ nullptr };

            {
                std::lock_guard<std::mutex> guard{ m_lock };

                if (m_pending.empty())
                {
                    return;
                }

                block.swap(m_pending);
                m_pending.reserve(m_maximumBytesPerEvent);

                // partial when this block does not hold a matched open and close pair
                isPartial = !(m_pendingSawStart && m_pendingSawEnd);

                m_pendingSawStart = false;
                m_pendingSawEnd = false;

                group = m_group;
            }

            UNREFERENCED_PARAMETER(flushing);

            auto args = winrt::make_self<implementation::MidiSystemExclusiveReceivedEventArgs>();

            args->InternalInitialize(
                group,
                winrt::single_threaded_vector<uint8_t>(std::move(block)).GetView(),
                isPartial);

            m_bytesReceivedEvent(*this, *args);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }
}
