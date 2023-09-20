// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiStreamMessageBuilder.h"
#include "MidiStreamMessageBuilder.g.cpp"

// this is just to make intent clear
#define MIDI_RESERVED_WORD (uint32_t)0
#define MIDI_RESERVED_FIELD 0

#define MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_DISCOVERY 0x0
#define MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_INFO 0x1
#define MIDI_STREAM_MESSAGE_STATUS_DEVICE_IDENTITY 0x2

#define MIDI_STREAM_MESSAGE_STANDARD_FORM0 0x0

#define MIDI_STREAM_MESSAGE_MULTI_FORM_START 0x1
#define MIDI_STREAM_MESSAGE_MULTI_FORM_CONTINUE 0x2
#define MIDI_STREAM_MESSAGE_MULTI_FORM_END 0x3

#define MIDI_STREAM_MESSAGE_ENDPOINT_NAME_MAX_LENGTH 98


namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    midi2::MidiMessage128 MidiStreamMessageBuilder::BuildEndpointDiscoveryMessage(
         internal::MidiTimestamp const timestamp,
         uint8_t const umpVersionMajor,
         uint8_t const umpVersionMinor,
         midi2::MidiEndpointDiscoveryFilterFlags const requestFlags
        )
    {
        return MidiMessageBuilder::BuildStreamMessage(
            timestamp, 
            MIDI_STREAM_MESSAGE_STANDARD_FORM0, 
            MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_DISCOVERY, 
            (uint16_t)umpVersionMajor << 8 | umpVersionMinor, // ump major is msb, ump minor is lsb
            (uint32_t)0 | (uint32_t)requestFlags, // filter bitmap is least significant byte
            MIDI_RESERVED_WORD,      // reserved word2   
            MIDI_RESERVED_WORD       // reserved word3
            );     

    }

    _Use_decl_annotations_
    midi2::MidiMessage128 MidiStreamMessageBuilder::BuildEndpointInformationNotificationMessage(
         internal::MidiTimestamp const timestamp,
         uint8_t const umpVersionMajor,
         uint8_t const umpVersionMinor,
         bool const hasStaticFunctionBlocks,
         uint8_t const numberOfFunctionBlocks,
         bool const supportsMidi20Protocol,
         bool const supportsMidi10Protocol,
         bool const supportsReceivingJitterReductionTimestamps,
         bool const supportsSendingJitterReductionTimestamps
    )
    {
        uint32_t word1{ 0 };

        // high bit of the num function blocks field is a flag for static function blocks
        if (hasStaticFunctionBlocks) word1 = 0x80000000;

        // MSB is the static function blocks + 7 bit number of function blocks
        word1 |= (internal::CleanupByte7(numberOfFunctionBlocks) << 24);

        // bit 9 (reading from least sig to most) is midi 1, bit 10 is midi 2
        
        if (supportsMidi10Protocol) word1 |= 0x100;
        if (supportsMidi20Protocol) word1 |= 0x200;

        // first bit is tx jr, second bit 2 is rx jr

        if (supportsSendingJitterReductionTimestamps) word1 |= 0x1;
        if (supportsReceivingJitterReductionTimestamps) word1 |= 0x2;

        return MidiMessageBuilder::BuildStreamMessage(
            timestamp,
            MIDI_STREAM_MESSAGE_STANDARD_FORM0,     
            MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_INFO, 
            (uint16_t)umpVersionMajor << 8 | umpVersionMinor, // ump major is msb, ump minor is lsb
            word1, 
            MIDI_RESERVED_WORD,      // reserved word2   
            MIDI_RESERVED_WORD       // reserved word3
        );
    }

    _Use_decl_annotations_
    midi2::MidiMessage128 MidiStreamMessageBuilder::BuildDeviceIdentityNotificationMessage(
         internal::MidiTimestamp timestamp,
         uint8_t const deviceManufacturerSysExIdByte1,
         uint8_t const deviceManufacturerSysExIdByte2,
         uint8_t const deviceManufacturerSysExIdByte3,
         uint8_t const deviceFamilyLsb,
         uint8_t const deviceFamilyMsb,
         uint8_t const deviceFamilyModelNumberLsb,
         uint8_t const deviceFamilyModelNumberMsb,
         uint8_t const softwareRevisionLevelByte1,
         uint8_t const softwareRevisionLevelByte2,
         uint8_t const softwareRevisionLevelByte3,
         uint8_t const softwareRevisionLevelByte4
    )
    {
        uint32_t word1{ 0 };
        uint32_t word2{ 0 };
        uint32_t word3{ 0 };

        word1 |= internal::CleanupByte7(deviceManufacturerSysExIdByte1) << 16;
        word1 |= internal::CleanupByte7(deviceManufacturerSysExIdByte2) << 8;
        word1 |= internal::CleanupByte7(deviceManufacturerSysExIdByte3);

        word2 |= internal::CleanupByte7(deviceFamilyLsb) << 24;
        word2 |= internal::CleanupByte7(deviceFamilyMsb) << 16;
        word2 |= internal::CleanupByte7(deviceFamilyModelNumberLsb) << 8;
        word2 |= internal::CleanupByte7(deviceFamilyModelNumberMsb);

        word3 |= internal::CleanupByte7(softwareRevisionLevelByte1) << 24;
        word3 |= internal::CleanupByte7(softwareRevisionLevelByte2) << 16;
        word3 |= internal::CleanupByte7(softwareRevisionLevelByte3) << 8;
        word3 |= internal::CleanupByte7(softwareRevisionLevelByte4);


        return MidiMessageBuilder::BuildStreamMessage(
            timestamp,
            MIDI_STREAM_MESSAGE_STANDARD_FORM0,
            MIDI_STREAM_MESSAGE_STATUS_DEVICE_IDENTITY,
            MIDI_RESERVED_FIELD,
            word1,
            word2,
            word3
            );
    }

    _Use_decl_annotations_
    collections::IVector<midi2::MidiMessage128> MidiStreamMessageBuilder::BuildEndpointNameNotificationMessages(
         internal::MidiTimestamp const /*timestamp*/,
         winrt::hstring const& /*name*/
    )
    {

        throw winrt::hresult_not_implemented();

        //const int CharactersPerPacket = 14;

        //auto messages = winrt::single_threaded_vector<midi2::MidiUmp128>();

        //// endpoint name is one or more UMPs, max 98 bytes. Either 1 complete message (form 0x0)
        //// or start (form 0x1, continue messages 0x2, and end 0x3)

        //if (name.size() > MIDI_STREAM_MESSAGE_ENDPOINT_NAME_MAX_LENGTH)
        //{
        //    // TODO: Truncate to max length
        //}

        //uint16_t wordZeroRemainingBytes{ 0 };
        //uint32_t additionalWords[3]{ 0 };

        //uint8_t* bytePointer = (byte*) &additionalWords[0];

        //for (int i = 0; i < name.size(); i++)
        //{
        //    int characterNumberThisPacket = i % CharactersPerPacket;
        //    int packetNumber = i / CharactersPerPacket;
        //    int charactersRemaining = name.size() - i;

        //    // characters 1-2 go into the end two bytes of word 0
        //    // characters 3-6 go into word 1
        //    // characters 7-10 go into word 2
        //    // characters 11-13 go into word 3
        //    // max 14 characters per message, but could be fewer in any given message

        //    if (characterNumberThisPacket <= 2)
        //    {



        //    }
        //    else
        //    {
        //        // last two bytes of word 0




        //    }


        //    // build the message

        //}

    }

    _Use_decl_annotations_
    collections::IVector<midi2::MidiMessage128> MidiStreamMessageBuilder::BuildEndpointProductInstanceIdNotificationMessages(
        internal::MidiTimestamp const /*timestamp*/,
        winrt::hstring const& /*productInstanceId*/
    )
    {
        throw winrt::hresult_not_implemented();
    }

    _Use_decl_annotations_
    midi2::MidiMessage128 MidiStreamMessageBuilder::BuildStreamConfigurationRequestMessage(
        internal::MidiTimestamp const /*timestamp*/,
        uint8_t const /*protocol*/,
         bool const /*expectToReceiveJRTimestamps*/,
         bool const /*requestToSendJRTimestamps*/
    )
    {
        throw winrt::hresult_not_implemented();
    }

    _Use_decl_annotations_
    midi2::MidiMessage128 MidiStreamMessageBuilder::BuildStreamConfigurationNotificationMessage(
        internal::MidiTimestamp const /*timestamp*/,
        uint8_t const /*protocol*/,
         bool const /*confirmationWillReceiveJRTimestamps*/,
         bool const /*confirmationSendJRTimestamps*/
    )
    {
        throw winrt::hresult_not_implemented();
    }

    _Use_decl_annotations_
    midi2::MidiMessage128 MidiStreamMessageBuilder::BuildFunctionBlockDiscoveryMessage(
        internal::MidiTimestamp const /*timestamp*/,
        uint8_t const /*functionBlockNumber*/,
         bool const /*requestFunctionBlockInfoNotification*/,
         bool const /*requestFunctionBlockNameNotification*/
    )
    {
        throw winrt::hresult_not_implemented();
    }

    _Use_decl_annotations_
    midi2::MidiMessage128 MidiStreamMessageBuilder::BuildFunctionInfoNotificationMessage(
        internal::MidiTimestamp const /*timestamp*/,
        bool const /*active*/,
         uint8_t const /*functionBlockNumber*/,
         midi2::MidiFunctionBlockUIHint const& /*uiHint*/,
         midi2::MidiFunctionBlockMidi10 const& /*midi10*/,
         midi2::MidiFunctionBlockDirection const& /*direction*/,
         uint8_t const /*firstGroup*/,
         uint8_t const /*numberOfGroups*/,
         uint8_t const /*midiCIVersionFormat*/,
         uint8_t const /*maxNumberSysEx8Streams*/
    )
    {
        throw winrt::hresult_not_implemented();
    }

    _Use_decl_annotations_
    collections::IVector<midi2::MidiMessage128> MidiStreamMessageBuilder::BuildFunctionBlockNameNotificationMessages(
        internal::MidiTimestamp const /*timestamp*/,
        uint8_t const /*functionBlockNumber*/,
        winrt::hstring const& /*name*/
    ) 
    {
        throw winrt::hresult_not_implemented();
    }


}
