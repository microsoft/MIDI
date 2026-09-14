// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "MidiMessageForwarder.h"

#include <ump_helpers.h>

namespace internal = ::WindowsMidiServicesInternal;

namespace midiapp
{
    namespace
    {
        constexpr uint32_t MaximumWordsPerUmp = 4;
    }

    struct MidiMessageForwarder::Callback : winrt::implements<Callback, IMidiEndpointConnectionMessagesReceivedCallback>
    {
        explicit Callback(_In_ MidiMessageForwarder* owner) noexcept : m_owner{ owner } {}

        STDMETHOD(MessagesReceived)(
            GUID sessionId,
            GUID connectionId,
            UINT64 timestamp,
            UINT32 wordCount,
            UINT32 const* messages) override
        {
            UNREFERENCED_PARAMETER(sessionId);
            UNREFERENCED_PARAMETER(connectionId);

            if (messages != nullptr && wordCount > 0)
            {
                m_owner->OnMessagesReceived(timestamp, wordCount, messages);
            }

            return S_OK;
        }

    private:
        MidiMessageForwarder* m_owner;
    };

    MidiMessageForwarder::~MidiMessageForwarder()
    {
        Stop();
    }

    _Use_decl_annotations_
    HRESULT MidiMessageForwarder::Start(
        winrt::Windows::Devices::Midi2::MidiEndpointConnection const& source,
        winrt::Windows::Devices::Midi2::MidiEndpointConnection const& destination,
        MidiForwardRoute const& route)
    {
        if (source == nullptr || destination == nullptr)
        {
            RETURN_HR(E_INVALIDARG);
        }

        if (route.SourceGroupIndex > 15 || route.DestinationGroupIndex > 15)
        {
            RETURN_HR(E_INVALIDARG);
        }

        if (m_running.load())
        {
            RETURN_HR(E_ILLEGAL_METHOD_CALL);
        }

        auto sourceRaw = source.try_as<IMidiEndpointConnectionRaw>();
        auto destinationRaw = destination.try_as<IMidiEndpointConnectionRaw>();

        if (sourceRaw == nullptr || destinationRaw == nullptr)
        {
            RETURN_HR(E_NOINTERFACE);
        }

        m_route = route;

        // One batch never exceeds what the destination will take in a single call, so the send
        // path below can split on that boundary without ever reallocating. The floor matters:
        // a buffer shorter than the longest UMP could not hold even one message, and the flush
        // in the receive path would not be able to make room for it.
        auto const maxWords = destinationRaw->GetSupportedMaxMidiWordsPerTransmission();

        if (maxWords < MaximumWordsPerUmp)
        {
            RETURN_HR(E_UNEXPECTED);
        }

        m_sendBuffer.assign(maxWords, 0);
        m_sendBufferUsed = 0;

        auto callback = winrt::make_self<Callback>(this);

        RETURN_IF_FAILED(sourceRaw->SetMessagesReceivedCallback(callback.get()));

        m_callback = callback.as<::IUnknown>();
        m_sourceRaw = sourceRaw;
        m_destinationRaw = destinationRaw;

        m_running.store(true);

        return S_OK;
    }

    void MidiMessageForwarder::Stop() noexcept
    {
        if (!m_running.exchange(false))
        {
            return;
        }

        if (m_sourceRaw != nullptr)
        {
            LOG_IF_FAILED(m_sourceRaw->RemoveMessagesReceivedCallback());
        }

        m_sourceRaw = nullptr;
        m_destinationRaw = nullptr;
        m_callback = nullptr;
    }

    _Use_decl_annotations_
    void MidiMessageForwarder::OnMessagesReceived(uint64_t timestamp, uint32_t wordCount, uint32_t const* messages) noexcept
    {
        if (!m_running.load(std::memory_order_relaxed))
        {
            return;
        }

        m_sendBufferUsed = 0;

        uint32_t position{ 0 };

        while (position < wordCount)
        {
            auto const word0 = messages[position];
            auto const messageWordCount = internal::GetUmpLengthInMidiWordsFromFirstWord(word0);

            if (messageWordCount == 0 || messageWordCount > MaximumWordsPerUmp ||
                position + messageWordCount > wordCount)
            {
                // The SDK only hands over complete UMPs, so a partial tail means something
                // upstream is malformed. Stop rather than read past the end of the buffer.
                break;
            }

            m_messagesReceived.fetch_add(1, std::memory_order_relaxed);

            // A groupless message says nothing about which group it belongs to, so there is no
            // way to tell whether it was meant for the group being forwarded. Passing it on would
            // duplicate it onto the destination for every route the customer sets up.
            if (internal::MessageHasGroupField(word0) &&
                internal::GetGroupIndexFromFirstWord(word0) == m_route.SourceGroupIndex)
            {
                if (m_sendBufferUsed + messageWordCount > m_sendBuffer.size())
                {
                    SendBuffered(timestamp);
                }

                m_sendBuffer[m_sendBufferUsed] =
                    internal::GetFirstWordWithNewGroup(word0, m_route.DestinationGroupIndex);

                for (uint8_t i = 1; i < messageWordCount; i++)
                {
                    m_sendBuffer[m_sendBufferUsed + i] = messages[position + i];
                }

                m_sendBufferUsed += messageWordCount;

                m_messagesForwarded.fetch_add(1, std::memory_order_relaxed);
            }

            position += messageWordCount;
        }

        SendBuffered(timestamp);
    }

    _Use_decl_annotations_
    void MidiMessageForwarder::SendBuffered(uint64_t timestamp) noexcept
    {
        if (m_sendBufferUsed == 0)
        {
            return;
        }

        // The timestamp the service delivered is sent back out unchanged, so a message scheduled
        // for the future stays scheduled rather than being flattened to "now" by the hop.
        if (FAILED(m_destinationRaw->SendMidiMessagesRaw(
            timestamp, static_cast<UINT32>(m_sendBufferUsed), m_sendBuffer.data())))
        {
            m_sendFailureCount.fetch_add(1, std::memory_order_relaxed);
        }

        m_sendBufferUsed = 0;
    }
}
