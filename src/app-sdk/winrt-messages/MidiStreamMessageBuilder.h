// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiStreamMessageBuilder.g.h"


namespace winrt::Microsoft::Devices::Midi2::Messages::implementation
{
    struct MidiStreamMessageBuilder
    {
        MidiStreamMessageBuilder() = default;

        _Success_(return != nullptr)
        static midi2::IMidiUniversalPacket BuildEndpointDiscoveryMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const umpVersionMajor,
            _In_ uint8_t const umpVersionMinor,
            _In_ midi2::MidiEndpointDiscoveryRequests const requestFlags
            ) noexcept;

        _Success_(return != nullptr)
        static midi2::IMidiUniversalPacket BuildEndpointInfoNotificationMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const umpVersionMajor,
            _In_ uint8_t const umpVersionMinor,
            _In_ bool const hasStaticFunctionBlocks,
            _In_ uint8_t const numberOfFunctionBlocks,
            _In_ bool const supportsMidi20Protocol,
            _In_ bool const supportsMidi10Protocol,
            _In_ bool const supportsReceivingJitterReductionTimestamps,
            _In_ bool const supportsSendingJitterReductionTimestamps
            ) noexcept;

        _Success_(return != nullptr)
        static midi2::IMidiUniversalPacket BuildDeviceIdentityNotificationMessage(
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
            ) noexcept;

        static collections::IVector<midi2::IMidiUniversalPacket> BuildEndpointNameNotificationMessages(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ winrt::hstring const& name
            ) noexcept;

        static collections::IVector<midi2::IMidiUniversalPacket> BuildProductInstanceIdNotificationMessages(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ winrt::hstring const& productInstanceId
            );

        _Success_(return != nullptr)
        static midi2::IMidiUniversalPacket BuildStreamConfigurationRequestMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const protocol,
            _In_ bool const expectToReceiveJRTimestamps,
            _In_ bool const requestToSendJRTimestamps
            );

        _Success_(return != nullptr)
        static midi2::IMidiUniversalPacket BuildStreamConfigurationNotificationMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const protocol,
            _In_ bool const confirmationWillReceiveJRTimestamps,
            _In_ bool const confirmationSendJRTimestamps
            );


        static winrt::hstring ParseEndpointNameNotificationMessages(
            _In_ collections::IIterable<midi2::IMidiUniversalPacket> const& messages
        );

        static winrt::hstring ParseProductInstanceIdNotificationMessages(
            _In_ collections::IIterable<midi2::IMidiUniversalPacket> const& messages
        );


        // function blocks

        _Success_(return != nullptr)
        static midi2::IMidiUniversalPacket BuildFunctionBlockDiscoveryMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const functionBlockNumber,
            _In_ midi2::MidiFunctionBlockDiscoveryRequests requestFlags
            );

        _Success_(return != nullptr)
        static midi2::IMidiUniversalPacket BuildFunctionBlockInfoNotificationMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ bool const active,
            _In_ uint8_t const functionBlockNumber,
            _In_ midi2::MidiFunctionBlockUIHint const& uiHint,
            _In_ midi2::MidiFunctionBlockRepresentsMidi10Connection const& midi10,
            _In_ midi2::MidiFunctionBlockDirection const& direction,
            _In_ uint8_t const firstGroup,
            _In_ uint8_t const numberOfGroups,
            _In_ uint8_t const midiCIVersionFormat,
            _In_ uint8_t const maxNumberSysEx8Streams
            );

        static collections::IVector<midi2::IMidiUniversalPacket> BuildFunctionBlockNameNotificationMessages(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const functionBlockNumber,
            _In_ winrt::hstring const& name
            );


        static winrt::hstring ParseFunctionBlockNameNotificationMessages(
            _In_ collections::IIterable<midi2::IMidiUniversalPacket> const& messages
            );




    private:
        static collections::IVector<midi2::IMidiUniversalPacket> BuildSplitTextMessages(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint8_t const status,
            _In_ uint16_t const word0Remainder,
            _In_ uint8_t const maxCharacters,
            _In_ uint8_t const maxCharactersPerPacket,
            _In_ winrt::hstring const& text);

        inline static void AppendCharToString(
            _In_ std::string& s, 
            _In_ uint8_t ch
            )
        {
            if (ch != 0)
                s += ch;
        }

    };
}

namespace winrt::Microsoft::Devices::Midi2::Messages::factory_implementation
{
    struct MidiStreamMessageBuilder : MidiStreamMessageBuilderT<MidiStreamMessageBuilder, implementation::MidiStreamMessageBuilder, winrt::static_lifetime>
    {
    };
}
