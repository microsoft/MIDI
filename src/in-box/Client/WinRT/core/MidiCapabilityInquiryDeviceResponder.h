// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "CapabilityInquiry.MidiCapabilityInquiryDeviceResponder.g.h"

#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    struct MidiCapabilityInquiryDeviceResponder : MidiCapabilityInquiryDeviceResponderT<MidiCapabilityInquiryDeviceResponder>
    {
        MidiCapabilityInquiryDeviceResponder() = default;

        bool IsEnabled() const noexcept { return m_isEnabled; }
        void IsEnabled(_In_ bool const value) noexcept { m_isEnabled = value; }

        ci::MidiUniqueId GetMuid(_In_ uint8_t const functionBlockNumber) noexcept;
        bool RegenerateMuid(_In_ uint8_t const functionBlockNumber) noexcept;

        ci::MidiCapabilityInquiryCategories SupportedCategories() noexcept;

        uint32_t ReceivableMaximumSystemExclusiveSize() const noexcept { return m_maximumSystemExclusiveSize; }
        void ReceivableMaximumSystemExclusiveSize(_In_ uint32_t const value) noexcept;

        ci::MidiDeviceInfo DeviceInfo() noexcept;
        void DeviceInfo(_In_ ci::MidiDeviceInfo const& value) noexcept;

        ci::MidiChannelList ChannelList() noexcept;
        void ChannelList(_In_ ci::MidiChannelList const& value) noexcept;

        ci::MidiResourceList ResourceList() noexcept;
        void ResourceList(_In_ ci::MidiResourceList const& value) noexcept;

        void SetProgramList(_In_ winrt::hstring const& resourceId, _In_ ci::MidiProgramList const& programList) noexcept;
        ci::MidiProgramList GetProgramList(_In_ winrt::hstring const& resourceId) noexcept;

        void SetResource(
            _In_ winrt::hstring const& resource,
            _In_ winrt::hstring const& resourceId,
            _In_ winrt::hstring const& jsonData) noexcept;

        bool RemoveResource(_In_ winrt::hstring const& resource, _In_ winrt::hstring const& resourceId) noexcept;

        void SetProfiles(
            _In_ uint8_t const deviceId,
            _In_ foundation::Collections::IIterable<ci::MidiProfileId> const& enabledProfiles,
            _In_ foundation::Collections::IIterable<ci::MidiProfileId> const& disabledProfiles) noexcept;

        bool SendProfileEnabledReport(_In_ uint8_t const functionBlockNumber, _In_ uint8_t const deviceId, _In_ ci::MidiProfileId const& profileId, _In_ uint16_t const channelCount) noexcept;
        bool SendProfileDisabledReport(_In_ uint8_t const functionBlockNumber, _In_ uint8_t const deviceId, _In_ ci::MidiProfileId const& profileId, _In_ uint16_t const channelCount) noexcept;
        bool SendProfileAddedReport(_In_ uint8_t const functionBlockNumber, _In_ uint8_t const deviceId, _In_ ci::MidiProfileId const& profileId) noexcept;
        bool SendProfileRemovedReport(_In_ uint8_t const functionBlockNumber, _In_ uint8_t const deviceId, _In_ ci::MidiProfileId const& profileId) noexcept;

        winrt::event_token MessageReceived(
            _In_ foundation::TypedEventHandler<ci::MidiCapabilityInquiryDeviceResponder, ci::MidiCapabilityInquiryMessageReceivedEventArgs> const& handler)
        {
            return m_messageReceivedEvent.add(handler);
        }

        void MessageReceived(_In_ winrt::event_token const& token) noexcept
        {
            m_messageReceivedEvent.remove(token);
        }

        // Not projected. The virtual device owns this responder and drives it.
        void InternalAttach(
            _In_ midi2::MidiEndpointConnection const& connection,
            _In_ midi2enum::MidiDeclaredDeviceIdentity const& identity) noexcept;

        void InternalDetach() noexcept;

        // Feeds one seven bit system exclusive packet in. Returns true when the packet belonged to
        // a capability inquiry message, so the caller knows whether it handled it.
        bool InternalProcessSystemExclusivePacket(
            _In_ uint32_t const word0,
            _In_ uint32_t const word1,
            _In_ internal::MidiTimestamp const timestamp) noexcept;

    private:
        void HandleMessage(
            _In_ ci::MidiCapabilityInquiryMessage const& message,
            _In_ midi2::MidiGroup const& group,
            _In_ internal::MidiTimestamp const timestamp) noexcept;

        void HandleDiscovery(_In_ ci::MidiCapabilityInquiryMessage const& message, _In_ midi2::MidiGroup const& group) noexcept;
        void HandlePropertyGet(_In_ ci::MidiCapabilityInquiryMessage const& message, _In_ midi2::MidiGroup const& group) noexcept;
        void HandleProfileInquiry(_In_ ci::MidiCapabilityInquiryMessage const& message, _In_ midi2::MidiGroup const& group) noexcept;

        bool Send(_In_ foundation::Collections::IVector<midi2::MidiMessage64> const& messages) noexcept;

        // The identifier a reply carries. A message addressed to a function block this responder
        // does not have still gets answered from the first one, because a device with a single
        // function block is by far the common case and refusing would look like a dead device.
        ci::MidiUniqueId MuidForReply() noexcept;

        // Serializes whichever resource a header named, or an empty string when there is none.
        std::string ResourceDataFor(
            _In_ winrt::hstring const& resource,
            _In_ winrt::hstring const& resourceId,
            _In_ int32_t const offset,
            _In_ int32_t const limit,
            _Out_ int32_t& totalCount) noexcept;

        std::string BuildResourceListJson() noexcept;

        static std::string ResourceKey(_In_ winrt::hstring const& resource, _In_ winrt::hstring const& resourceId) noexcept;

        std::atomic<bool> m_isEnabled{ false };
        std::atomic<uint32_t> m_maximumSystemExclusiveSize{ 512 };

        midi2::MidiEndpointConnection m_connection{ nullptr };
        midi2enum::MidiDeclaredDeviceIdentity m_identity{ nullptr };

        mutable std::mutex m_lock;

        std::map<uint8_t, ci::MidiUniqueId> m_muids{};

        // What each initiator said it can receive, so a reply to it is chunked to fit.
        std::map<uint32_t, uint32_t> m_initiatorMaximumSystemExclusiveSizes{};

        ci::MidiDeviceInfo m_deviceInfo{ nullptr };
        ci::MidiChannelList m_channelList{ nullptr };
        ci::MidiResourceList m_resourceList{ nullptr };

        std::map<std::wstring, ci::MidiProgramList> m_programLists{};
        std::map<std::string, std::string> m_resources{};

        struct ProfileSet
        {
            std::vector<ci::MidiProfileId> Enabled{};
            std::vector<ci::MidiProfileId> Disabled{};
        };

        std::map<uint8_t, ProfileSet> m_profiles{};

        std::vector<uint8_t> m_incoming{};
        bool m_incomingIsOpen{ false };

        winrt::event<foundation::TypedEventHandler<ci::MidiCapabilityInquiryDeviceResponder, ci::MidiCapabilityInquiryMessageReceivedEventArgs>> m_messageReceivedEvent;
    };
}
