// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "CapabilityInquiry.MidiCapabilityInquirySession.g.h"

#include <condition_variable>
#include <map>
#include <mutex>
#include <set>
#include <vector>

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    struct MidiCapabilityInquirySession : MidiCapabilityInquirySessionT<MidiCapabilityInquirySession>
    {
        MidiCapabilityInquirySession() = default;
        ~MidiCapabilityInquirySession() { Close(); }

        static ci::MidiCapabilityInquirySession Create(
            _In_ midi2::MidiEndpointConnection const& connection) noexcept;

        static ci::MidiCapabilityInquirySession Create(
            _In_ midi2::MidiEndpointConnection const& connection,
            _In_ midi2enum::MidiDeclaredDeviceIdentity const& identity) noexcept;

        ci::MidiUniqueId SourceMuid() const noexcept { return m_sourceMuid; }

        midi2::MidiGroup Group() const noexcept;
        void Group(_In_ midi2::MidiGroup const& value) noexcept;

        uint32_t ResponseTimeoutMilliseconds() const noexcept { return m_responseTimeoutMilliseconds; }
        void ResponseTimeoutMilliseconds(_In_ uint32_t const value) noexcept;

        midi2enum::MidiDeclaredDeviceIdentity Identity() const noexcept;
        void Identity(_In_ midi2enum::MidiDeclaredDeviceIdentity const& value) noexcept;

        bool IsOpen() const noexcept { return m_isOpen; }

        foundation::Collections::IVectorView<ci::MidiCapabilityInquiryResponder> GetResponders();
        ci::MidiCapabilityInquiryResponder GetResponder(_In_ ci::MidiUniqueId const& muid) noexcept;

        foundation::IAsyncOperation<foundation::Collections::IVectorView<ci::MidiCapabilityInquiryResponder>>
            DiscoverAsync();

        bool SendInvalidateMuid() noexcept;

        foundation::IAsyncOperation<ci::MidiCapabilityInquiryStatus>
            RequestPropertyExchangeCapabilitiesAsync(_In_ ci::MidiUniqueId destinationMuid);

        foundation::IAsyncOperation<ci::MidiPropertyExchangeResponse> GetPropertyDataAsync(
            _In_ ci::MidiUniqueId destinationMuid,
            _In_ json::JsonObject header);

        foundation::IAsyncOperation<ci::MidiPropertyExchangeResponse> SetPropertyDataAsync(
            _In_ ci::MidiUniqueId destinationMuid,
            _In_ json::JsonObject header,
            _In_ foundation::Collections::IIterable<uint8_t> body);

        foundation::IAsyncOperation<ci::MidiResourceList> GetResourceListAsync(
            _In_ ci::MidiUniqueId destinationMuid);

        foundation::IAsyncOperation<ci::MidiDeviceInfo> GetDeviceInfoAsync(
            _In_ ci::MidiUniqueId destinationMuid);

        foundation::IAsyncOperation<ci::MidiChannelList> GetChannelListAsync(
            _In_ ci::MidiUniqueId destinationMuid);

        foundation::IAsyncOperation<ci::MidiProgramList> GetProgramListAsync(
            _In_ ci::MidiUniqueId destinationMuid,
            _In_ winrt::hstring resourceId);

        foundation::IAsyncOperation<ci::MidiProgramList> GetProgramListPageAsync(
            _In_ ci::MidiUniqueId destinationMuid,
            _In_ winrt::hstring resourceId,
            _In_ uint32_t offset,
            _In_ uint32_t limit);

        foundation::IAsyncOperation<ci::MidiPropertySubscription> SubscribeAsync(
            _In_ ci::MidiUniqueId destinationMuid,
            _In_ winrt::hstring resource,
            _In_ winrt::hstring resourceId);

        foundation::IAsyncOperation<bool> UnsubscribeAsync(
            _In_ ci::MidiPropertySubscription subscription);

        foundation::Collections::IVectorView<ci::MidiPropertySubscription> GetSubscriptions();

        foundation::IAsyncOperation<ci::MidiProfileInquiryResponse> GetProfilesAsync(
            _In_ ci::MidiUniqueId destinationMuid,
            _In_ uint8_t deviceId);

        bool SendSetProfileOn(
            _In_ ci::MidiUniqueId const& destinationMuid,
            _In_ uint8_t const deviceId,
            _In_ ci::MidiProfileId const& profileId,
            _In_ uint16_t const channelCount) noexcept;

        bool SendSetProfileOff(
            _In_ ci::MidiUniqueId const& destinationMuid,
            _In_ uint8_t const deviceId,
            _In_ ci::MidiProfileId const& profileId) noexcept;

        winrt::event_token ProfileStateChanged(
            _In_ foundation::TypedEventHandler<ci::MidiCapabilityInquirySession, ci::MidiCapabilityInquiryMessageReceivedEventArgs> const& handler);
        void ProfileStateChanged(_In_ winrt::event_token const& token) noexcept;

        winrt::event_token ResponderFound(
            _In_ foundation::TypedEventHandler<ci::MidiCapabilityInquirySession, ci::MidiCapabilityInquiryResponder> const& handler);
        void ResponderFound(_In_ winrt::event_token const& token) noexcept;

        winrt::event_token PropertySubscriptionUpdated(
            _In_ foundation::TypedEventHandler<ci::MidiCapabilityInquirySession, ci::MidiPropertySubscriptionUpdatedEventArgs> const& handler);
        void PropertySubscriptionUpdated(_In_ winrt::event_token const& token) noexcept;

        winrt::event_token MessageReceived(
            _In_ foundation::TypedEventHandler<ci::MidiCapabilityInquirySession, ci::MidiCapabilityInquiryMessageReceivedEventArgs> const& handler);
        void MessageReceived(_In_ winrt::event_token const& token) noexcept;

        void Close() noexcept;

        // Not projected. Called by the static Create methods once the object exists, because
        // subscribing to the connection needs a reference to this session.
        bool InternalInitialize(
            _In_ midi2::MidiEndpointConnection const& connection,
            _In_ midi2enum::MidiDeclaredDeviceIdentity const& identity) noexcept;

    private:
        // One outstanding request. The waiting coroutine owns it; the receive handler fills it in
        // and signals it.
        struct PendingRequest
        {
            ci::MidiCapabilityInquiryMessageType ExpectedReply{ ci::MidiCapabilityInquiryMessageType::PropertyGetDataInquiryReply };

            uint32_t DestinationMuid{ 0 };
            uint8_t RequestId{ 0 };
            uint8_t DeviceId{ 0x7F };

            bool IsComplete{ false };

            // Property exchange replies arrive in chunks, and the header comes only on the first.
            uint16_t ChunkCount{ 0 };
            uint16_t ChunksReceived{ 0 };

            ci::MidiCapabilityInquiryMessage FirstMessage{ nullptr };
            std::vector<uint8_t> Body{};
        };

        void OnMessageReceived(
            _In_ foundation::IInspectable const& sender,
            _In_ midi2::MidiMessageReceivedEventArgs const& args) noexcept;

        void HandleCompleteTransfer(
            _In_ std::vector<uint8_t> const& payload,
            _In_ midi2::MidiGroup const& group,
            _In_ internal::MidiTimestamp const timestamp) noexcept;

        // Returns true when the message answered something this session asked for.
        bool TryCompleteRequest(_In_ ci::MidiCapabilityInquiryMessage const& message) noexcept;

        void RecordResponder(_In_ ci::MidiCapabilityInquiryMessage const& message) noexcept;

        bool Send(_In_ foundation::Collections::IVector<midi2::MidiMessage64> const& messages) noexcept;

        uint8_t NextRequestId() noexcept;

        // Waits for the pending request at this key, or for the timeout. Returns false on timeout
        // or when the session closed while waiting.
        bool WaitForRequest(_In_ uint64_t const key) noexcept;

        void RemoveRequest(_In_ uint64_t const key) noexcept;

        uint32_t MaximumSystemExclusiveSizeFor(_In_ uint32_t const muid) noexcept;

        // The capabilities transaction, blocking. The projected method and the automatic one below
        // are both this.
        ci::MidiCapabilityInquiryStatus RequestPropertyExchangeCapabilities(
            _In_ ci::MidiUniqueId const& destinationMuid) noexcept;

        // Runs the capabilities transaction the first time this session asks a responder for a
        // property, because the specification puts it ahead of everything else in property
        // exchange and it is how the responder declares the limits the session has to honor.
        void EnsurePropertyExchangeCapabilities(_In_ ci::MidiUniqueId const& destinationMuid) noexcept;

        // Fills in a response object from a completed request, including the negative
        // acknowledgment case.
        ci::MidiPropertyExchangeResponse BuildPropertyResponse(
            _In_ PendingRequest const& request,
            _In_ bool const timedOut) noexcept;

        ci::MidiPropertyExchangeResponse RequestProperty(
            _In_ ci::MidiUniqueId const& destinationMuid,
            _In_ json::JsonObject const& header,
            _In_ foundation::Collections::IIterable<uint8_t> const& body,
            _In_ bool const isSet) noexcept;

        // A subscription start or end. Blocking, like every other request here.
        ci::MidiPropertyExchangeResponse SendSubscriptionCommand(
            _In_ ci::MidiUniqueId const& destinationMuid,
            _In_ json::JsonObject const& header) noexcept;

        // An update a device sent us, reassembled. Raises the event and sends the reply the
        // specification requires.
        void CompleteIncomingUpdate(
            _In_ uint64_t const key,
            _In_ PendingRequest const& update) noexcept;

        // Collects the chunks of an unsolicited subscription message. Returns true when it was
        // consumed here, whether or not it finished the transfer.
        bool TryCollectSubscriptionUpdate(_In_ ci::MidiCapabilityInquiryMessage const& message) noexcept;

        void EndAllSubscriptions() noexcept;

        std::atomic<bool> m_isOpen{ false };

        midi2::MidiEndpointConnection m_connection{ nullptr };
        winrt::event_token m_messageReceivedToken{};

        ci::MidiUniqueId m_sourceMuid{ nullptr };
        uint32_t m_sourceMuidValue{ 0 };

        midi2::MidiGroup m_group{ nullptr };
        midi2enum::MidiDeclaredDeviceIdentity m_identity{ nullptr };

        std::atomic<uint32_t> m_responseTimeoutMilliseconds{ 2000 };
        std::atomic<uint8_t> m_nextRequestId{ 1 };

        // Guards everything below, and is not held while an event is raised.
        mutable std::mutex m_lock;
        std::condition_variable m_requestCompleted;

        std::map<uint64_t, PendingRequest> m_pendingRequests{};
        std::map<uint32_t, ci::MidiCapabilityInquiryResponder> m_responders{};

        // Live subscriptions, by the identifier the device assigned. A device is free to hand out
        // the same identifier as another device, so the responder identifier is part of the key.
        std::map<std::wstring, ci::MidiPropertySubscription> m_subscriptions{};

        // Subscription updates arriving now, keyed the same way as a pending request. A device
        // owns the request identifier on an update, so these cannot share that map.
        std::map<uint64_t, PendingRequest> m_incomingUpdates{};

        // Responders this session has already run the capabilities transaction with, whatever the
        // outcome was, so a device which does not answer it is asked only once.
        std::set<uint32_t> m_propertyExchangeCapabilitiesAsked{};

        // Reassembly of the system exclusive transfer currently arriving.
        std::vector<uint8_t> m_incoming{};
        bool m_incomingIsOpen{ false };

        winrt::event<foundation::TypedEventHandler<ci::MidiCapabilityInquirySession, ci::MidiCapabilityInquiryMessageReceivedEventArgs>> m_profileStateChangedEvent;
        winrt::event<foundation::TypedEventHandler<ci::MidiCapabilityInquirySession, ci::MidiCapabilityInquiryResponder>> m_responderFoundEvent;
        winrt::event<foundation::TypedEventHandler<ci::MidiCapabilityInquirySession, ci::MidiPropertySubscriptionUpdatedEventArgs>> m_propertySubscriptionUpdatedEvent;
        winrt::event<foundation::TypedEventHandler<ci::MidiCapabilityInquirySession, ci::MidiCapabilityInquiryMessageReceivedEventArgs>> m_messageReceivedEvent;
    };
}

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::factory_implementation
{
    struct MidiCapabilityInquirySession : MidiCapabilityInquirySessionT<MidiCapabilityInquirySession, implementation::MidiCapabilityInquirySession>
    {
    };
}
