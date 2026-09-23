// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "CapabilityInquiry.MidiCapabilityInquiryMessageBuilder.g.h"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    struct MidiCapabilityInquiryMessageBuilder
    {
        MidiCapabilityInquiryMessageBuilder() = default;

        static uint32_t MinimumReceivableSystemExclusiveSize() noexcept { return 512; }

        static foundation::Collections::IVector<midi2::MidiMessage64> BuildDiscovery(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ ci::MidiUniqueId const& sourceMuid,
            _In_ midi2enum::MidiDeclaredDeviceIdentity const& identity,
            _In_ ci::MidiCapabilityInquiryCategories const supportedCategories,
            _In_ uint32_t const receivableMaximumSystemExclusiveSize,
            _In_ uint8_t const outputPathId) noexcept;

        static foundation::Collections::IVector<midi2::MidiMessage64> BuildDiscoveryReply(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ ci::MidiUniqueId const& sourceMuid,
            _In_ ci::MidiUniqueId const& destinationMuid,
            _In_ midi2enum::MidiDeclaredDeviceIdentity const& identity,
            _In_ ci::MidiCapabilityInquiryCategories const supportedCategories,
            _In_ uint32_t const receivableMaximumSystemExclusiveSize,
            _In_ uint8_t const outputPathId,
            _In_ uint8_t const functionBlockNumber) noexcept;

        static foundation::Collections::IVector<midi2::MidiMessage64> BuildInvalidateMuid(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ ci::MidiUniqueId const& sourceMuid,
            _In_ ci::MidiUniqueId const& muidToInvalidate) noexcept;

        static foundation::Collections::IVector<midi2::MidiMessage64> BuildAck(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ uint8_t const deviceId,
            _In_ ci::MidiUniqueId const& sourceMuid,
            _In_ ci::MidiUniqueId const& destinationMuid,
            _In_ ci::MidiCapabilityInquiryMessageType const originalMessageType,
            _In_ uint8_t const statusCode,
            _In_ uint8_t const statusData,
            _In_ foundation::Collections::IIterable<uint8_t> const& statusDetails,
            _In_ winrt::hstring const& statusMessage) noexcept;

        static foundation::Collections::IVector<midi2::MidiMessage64> BuildNak(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ uint8_t const deviceId,
            _In_ ci::MidiUniqueId const& sourceMuid,
            _In_ ci::MidiUniqueId const& destinationMuid,
            _In_ ci::MidiCapabilityInquiryMessageType const originalMessageType,
            _In_ uint8_t const statusCode,
            _In_ uint8_t const statusData,
            _In_ foundation::Collections::IIterable<uint8_t> const& statusDetails,
            _In_ winrt::hstring const& statusMessage) noexcept;

        static foundation::Collections::IVector<midi2::MidiMessage64> BuildPropertyExchangeCapabilitiesInquiry(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ ci::MidiUniqueId const& sourceMuid,
            _In_ ci::MidiUniqueId const& destinationMuid,
            _In_ uint8_t const maximumSimultaneousRequests,
            _In_ uint8_t const messageVersion) noexcept;

        static foundation::Collections::IVector<midi2::MidiMessage64> BuildPropertyExchangeCapabilitiesReply(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ ci::MidiUniqueId const& sourceMuid,
            _In_ ci::MidiUniqueId const& destinationMuid,
            _In_ uint8_t const maximumSimultaneousRequests,
            _In_ uint8_t const messageVersion) noexcept;

        static foundation::Collections::IVector<midi2::MidiMessage64> BuildPropertyGetDataInquiry(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ ci::MidiUniqueId const& sourceMuid,
            _In_ ci::MidiUniqueId const& destinationMuid,
            _In_ uint8_t const requestId,
            _In_ json::JsonObject const& header) noexcept;

        static foundation::Collections::IVector<midi2::MidiMessage64> BuildPropertyMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ ci::MidiCapabilityInquiryMessageType const messageType,
            _In_ ci::MidiUniqueId const& sourceMuid,
            _In_ ci::MidiUniqueId const& destinationMuid,
            _In_ uint8_t const requestId,
            _In_ json::JsonObject const& header,
            _In_ foundation::Collections::IIterable<uint8_t> const& body,
            _In_ uint32_t const destinationMaximumSystemExclusiveSize) noexcept;

        static uint16_t GetPropertyChunkCount(
            _In_ json::JsonObject const& header,
            _In_ uint32_t const bodyByteCount,
            _In_ uint32_t const destinationMaximumSystemExclusiveSize) noexcept;

        static foundation::Collections::IVector<midi2::MidiMessage64> BuildProfileInquiry(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ uint8_t const deviceId,
            _In_ ci::MidiUniqueId const& sourceMuid,
            _In_ ci::MidiUniqueId const& destinationMuid) noexcept;

        static foundation::Collections::IVector<midi2::MidiMessage64> BuildProfileInquiryReply(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ uint8_t const deviceId,
            _In_ ci::MidiUniqueId const& sourceMuid,
            _In_ ci::MidiUniqueId const& destinationMuid,
            _In_ foundation::Collections::IIterable<ci::MidiProfileId> const& enabledProfiles,
            _In_ foundation::Collections::IIterable<ci::MidiProfileId> const& disabledProfiles) noexcept;

        static foundation::Collections::IVector<midi2::MidiMessage64> BuildSetProfileOn(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ uint8_t const deviceId,
            _In_ ci::MidiUniqueId const& sourceMuid,
            _In_ ci::MidiUniqueId const& destinationMuid,
            _In_ ci::MidiProfileId const& profileId,
            _In_ uint16_t const channelCount) noexcept;

        static foundation::Collections::IVector<midi2::MidiMessage64> BuildSetProfileOff(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ uint8_t const deviceId,
            _In_ ci::MidiUniqueId const& sourceMuid,
            _In_ ci::MidiUniqueId const& destinationMuid,
            _In_ ci::MidiProfileId const& profileId) noexcept;

        static foundation::Collections::IVector<midi2::MidiMessage64> BuildProfileEnabledReport(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ uint8_t const deviceId,
            _In_ ci::MidiUniqueId const& sourceMuid,
            _In_ ci::MidiProfileId const& profileId,
            _In_ uint16_t const channelCount) noexcept;

        static foundation::Collections::IVector<midi2::MidiMessage64> BuildProfileDisabledReport(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ uint8_t const deviceId,
            _In_ ci::MidiUniqueId const& sourceMuid,
            _In_ ci::MidiProfileId const& profileId,
            _In_ uint16_t const channelCount) noexcept;

        static foundation::Collections::IVector<midi2::MidiMessage64> BuildProfileAddedReport(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ uint8_t const deviceId,
            _In_ ci::MidiUniqueId const& sourceMuid,
            _In_ ci::MidiProfileId const& profileId) noexcept;

        static foundation::Collections::IVector<midi2::MidiMessage64> BuildProfileRemovedReport(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ uint8_t const deviceId,
            _In_ ci::MidiUniqueId const& sourceMuid,
            _In_ ci::MidiProfileId const& profileId) noexcept;

        static foundation::Collections::IVector<midi2::MidiMessage64> BuildProfileDetailsInquiry(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ uint8_t const deviceId,
            _In_ ci::MidiUniqueId const& sourceMuid,
            _In_ ci::MidiUniqueId const& destinationMuid,
            _In_ ci::MidiProfileId const& profileId,
            _In_ uint8_t const inquiryTarget) noexcept;

        static foundation::Collections::IVector<midi2::MidiMessage64> BuildProfileDetailsReply(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ uint8_t const deviceId,
            _In_ ci::MidiUniqueId const& sourceMuid,
            _In_ ci::MidiUniqueId const& destinationMuid,
            _In_ ci::MidiProfileId const& profileId,
            _In_ uint8_t const inquiryTarget,
            _In_ foundation::Collections::IIterable<uint8_t> const& inquiryTargetData) noexcept;

        static foundation::Collections::IVector<midi2::MidiMessage64> BuildProfileSpecificData(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ uint8_t const deviceId,
            _In_ ci::MidiUniqueId const& sourceMuid,
            _In_ ci::MidiUniqueId const& destinationMuid,
            _In_ ci::MidiProfileId const& profileId,
            _In_ foundation::Collections::IIterable<uint8_t> const& data) noexcept;

        static foundation::Collections::IVector<midi2::MidiMessage64> BuildFromSystemExclusiveData(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ foundation::Collections::IIterable<uint8_t> const& data) noexcept;
    };
}

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::factory_implementation
{
    struct MidiCapabilityInquiryMessageBuilder : MidiCapabilityInquiryMessageBuilderT<MidiCapabilityInquiryMessageBuilder, implementation::MidiCapabilityInquiryMessageBuilder, winrt::static_lifetime>
    {
    };
}
