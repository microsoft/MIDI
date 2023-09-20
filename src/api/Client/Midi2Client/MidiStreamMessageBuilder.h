// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiStreamMessageBuilder.g.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiStreamMessageBuilder
    {
        MidiStreamMessageBuilder() = default;

        static midi2::MidiMessage128 BuildEndpointDiscoveryMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const umpVersionMajor,
            _In_ uint8_t const umpVersionMinor,
            _In_ midi2::MidiEndpointDiscoveryFilterFlags const requestFlags
            );

        static midi2::MidiMessage128 BuildEndpointInformationNotificationMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const umpVersionMajor,
            _In_ uint8_t const umpVersionMinor,
            _In_ bool const hasStaticFunctionBlocks,
            _In_ uint8_t const numberOfFunctionBlocks,
            _In_ bool const supportsMidi20Protocol,
            _In_ bool const supportsMidi10Protocol,
            _In_ bool const supportsReceivingJitterReductionTimestamps,
            _In_ bool const supportsSendingJitterReductionTimestamps
            );

        static midi2::MidiMessage128 BuildDeviceIdentityNotificationMessage(
            _In_ internal::MidiTimestamp timestamp,
            _In_ uint8_t const deviceManufacturerSysExIdByte1,
            _In_ uint8_t const deviceManufacturerSysExIdByte2,
            _In_ uint8_t const deviceManufacturerSysExIdByte3,
            _In_ uint8_t const deviceFamilyLsb,
            _In_ uint8_t const deviceFamilyMsb,
            _In_ uint8_t const deviceFamilyModelNumberLsb,
            _In_ uint8_t const deviceFamilyModelNumberMsb,
            _In_ uint8_t const softwareRevisionLevelByte1,
            _In_ uint8_t const softwareRevisionLevelByte2,
            _In_ uint8_t const softwareRevisionLevelByte3,
            _In_ uint8_t const softwareRevisionLevelByte4
            );

        static collections::IVector<midi2::MidiMessage128> BuildEndpointNameNotificationMessages(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ winrt::hstring const& name
            );

        static collections::IVector<midi2::MidiMessage128> BuildEndpointProductInstanceIdNotificationMessages(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ winrt::hstring const& productInstanceId
            );

        static midi2::MidiMessage128 BuildStreamConfigurationRequestMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const protocol,
            _In_ bool const expectToReceiveJRTimestamps,
            _In_ bool const requestToSendJRTimestamps
            );

        static midi2::MidiMessage128 BuildStreamConfigurationNotificationMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const protocol,
            _In_ bool const confirmationWillReceiveJRTimestamps,
            _In_ bool const confirmationSendJRTimestamps
            );

        static midi2::MidiMessage128 BuildFunctionBlockDiscoveryMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const functionBlockNumber,
            _In_ bool const requestFunctionBlockInfoNotification,
            _In_ bool const requestFunctionBlockNameNotification
            );

        static midi2::MidiMessage128 BuildFunctionInfoNotificationMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ bool const active,
            _In_ uint8_t const functionBlockNumber,
            _In_ midi2::MidiFunctionBlockUIHint const& uiHint,
            _In_ midi2::MidiFunctionBlockMidi10 const& midi10,
            _In_ midi2::MidiFunctionBlockDirection const& direction,
            _In_ uint8_t const firstGroup,
            _In_ uint8_t const numberOfGroups,
            _In_ uint8_t const midiCIVersionFormat,
            _In_ uint8_t const maxNumberSysEx8Streams
            );

        static collections::IVector<midi2::MidiMessage128> BuildFunctionBlockNameNotificationMessages(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const functionBlockNumber,
            _In_ winrt::hstring const& name
            );
    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiStreamMessageBuilder : MidiStreamMessageBuilderT<MidiStreamMessageBuilder, implementation::MidiStreamMessageBuilder, winrt::static_lifetime>
    {
    };
}
