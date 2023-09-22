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
#define MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME 0x3
#define MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_PRODUCT_INSTANCE_ID 0x4

#define MIDI_STREAM_MESSAGE_STANDARD_FORM0 0x0

#define MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE 0x0
#define MIDI_STREAM_MESSAGE_MULTI_FORM_START 0x1
#define MIDI_STREAM_MESSAGE_MULTI_FORM_CONTINUE 0x2
#define MIDI_STREAM_MESSAGE_MULTI_FORM_END 0x3

#define MIDI_STREAM_MESSAGE_ENDPOINT_NAME_MAX_LENGTH 98
#define MIDI_STREAM_MESSAGE_ENDPOINT_NAME_CHARACTERS_PER_PACKET 14

namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    midi2::MidiMessage128 MidiStreamMessageBuilder::BuildEndpointDiscoveryMessage(
         internal::MidiTimestamp const timestamp,
         uint8_t const umpVersionMajor,
         uint8_t const umpVersionMinor,
         midi2::MidiEndpointDiscoveryFilterFlags const requestFlags
        ) noexcept
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
    ) noexcept
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
    ) noexcept
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

    // todo: refactor this and share with the product instance ID notification
    _Use_decl_annotations_
    collections::IVector<midi2::MidiMessage128> MidiStreamMessageBuilder::BuildEndpointNameNotificationMessages(
         internal::MidiTimestamp const timestamp,
         winrt::hstring const& name
    ) noexcept
    {
        auto messages = winrt::single_threaded_vector<midi2::MidiMessage128>();

        // endpoint name is one or more UMPs, max 98 bytes. Either 1 complete message (form 0x0)
        // or start (form 0x1, continue messages 0x2, and end 0x3)

        // don't process past the last allowed character
        size_t totalCharacters = (name.size() > MIDI_STREAM_MESSAGE_ENDPOINT_NAME_MAX_LENGTH) ? MIDI_STREAM_MESSAGE_ENDPOINT_NAME_MAX_LENGTH : name.size();
        size_t remainingCharacters = totalCharacters;

        // convert to 8 bit UTF-8 characters, and get the pointer to the first character
        auto utf8Version = winrt::to_string(name);
        char* utf8StringPointer = utf8Version.data();

        size_t umpCount = totalCharacters / MIDI_STREAM_MESSAGE_ENDPOINT_NAME_CHARACTERS_PER_PACKET;
        if (totalCharacters % MIDI_STREAM_MESSAGE_ENDPOINT_NAME_CHARACTERS_PER_PACKET != 0) umpCount++;

        //auto umpCount = std::ceil(totalCharacters / (double)MIDI_STREAM_MESSAGE_ENDPOINT_NAME_CHARACTERS_PER_PACKET);

        for (int currentUmp = 0; currentUmp < umpCount; currentUmp++)
        {
            uint8_t form;

            // if entire string fits in a single UMP, form = 0. Otherwise
            // the packets must have a start and end packet, and optionally, up to 5 continue packets in the middle

            if (currentUmp == 0 && umpCount == 1)
            {
                // everything fits in one UMP
                form = MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE;
            }
            else if (currentUmp == 0 && umpCount > 1)
            {
                // first of > 1 UMPs
                form = MIDI_STREAM_MESSAGE_MULTI_FORM_START;
            }
            else if (currentUmp == umpCount - 1)
            {
                // end UMP
                form = MIDI_STREAM_MESSAGE_MULTI_FORM_END;
            }
            else
            {
                // interim UMP
                form = MIDI_STREAM_MESSAGE_MULTI_FORM_CONTINUE;
            }

            midi2::MidiMessage128 umpProjected =             
                midi2::MidiMessageBuilder::BuildStreamMessage(
                    timestamp,
                    form,
                    MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME, 
                    (uint16_t)0, 
                    (uint32_t)0, 
                    (uint32_t)0,
                    (uint32_t)0
                );

            auto ump = winrt::get_self<implementation::MidiMessage128>(umpProjected);

            byte packetBytes[MIDI_STREAM_MESSAGE_ENDPOINT_NAME_CHARACTERS_PER_PACKET]{ 0 };

            // endianness dependence means we can't just cast a pointer to the struct as byte* and run through it.
            for (int i = 0; i < MIDI_STREAM_MESSAGE_ENDPOINT_NAME_CHARACTERS_PER_PACKET && remainingCharacters > 0; i++)
            { 
                packetBytes[i] = *utf8StringPointer;
                utf8StringPointer++;    // move along the source string

                remainingCharacters--;
            }

            // convert network to native endianness when copying the bytes over. This is far uglier
            // than just walking a pointer, but it's required here.

            uint32_t word0 = ump->Word0();
            word0 |= packetBytes[0] << 8 | packetBytes[1];  // first two characters are end of first word
            ump->Word0(word0);

            ump->Word1(
                packetBytes[2] << 24 | 
                packetBytes[3] << 16 | 
                packetBytes[4] << 8 | 
                packetBytes[5]);

            ump->Word2(
                packetBytes[6] << 24 |
                packetBytes[7] << 16 |
                packetBytes[8] << 8 |
                packetBytes[9]);

            ump->Word3(
                packetBytes[10] << 24 |
                packetBytes[11] << 16 |
                packetBytes[12] << 8 |
                packetBytes[13]);


            messages.Append(umpProjected);

        }

        utf8StringPointer = nullptr;

        return messages;
    }

    // this code should really be refactored and shared with the name notification builder
    _Use_decl_annotations_
    collections::IVector<midi2::MidiMessage128> MidiStreamMessageBuilder::BuildEndpointProductInstanceIdNotificationMessages(
        internal::MidiTimestamp const timestamp,
        winrt::hstring const& productInstanceId
    )
    {
        auto messages = winrt::single_threaded_vector<midi2::MidiMessage128>();

        // endpoint name is one or more UMPs, max 98 bytes. Either 1 complete message (form 0x0)
        // or start (form 0x1, continue messages 0x2, and end 0x3)

        // don't process past the last allowed character
        size_t totalCharacters = (productInstanceId.size() > MIDI_STREAM_MESSAGE_ENDPOINT_NAME_MAX_LENGTH) ? MIDI_STREAM_MESSAGE_ENDPOINT_NAME_MAX_LENGTH : productInstanceId.size();
        size_t remainingCharacters = totalCharacters;

        // convert to 8 bit UTF-8 characters, and get the pointer to the first character
        auto utf8Version = winrt::to_string(productInstanceId);
        char* utf8StringPointer = utf8Version.data();

        size_t umpCount = totalCharacters / MIDI_STREAM_MESSAGE_ENDPOINT_NAME_CHARACTERS_PER_PACKET;
        if (totalCharacters % MIDI_STREAM_MESSAGE_ENDPOINT_NAME_CHARACTERS_PER_PACKET != 0) umpCount++;

        //auto umpCount = std::ceil(totalCharacters / (double)MIDI_STREAM_MESSAGE_ENDPOINT_NAME_CHARACTERS_PER_PACKET);

        for (int currentUmp = 0; currentUmp < umpCount; currentUmp++)
        {
            uint8_t form;

            // if entire string fits in a single UMP, form = 0. Otherwise
            // the packets must have a start and end packet, and optionally, up to 5 continue packets in the middle

            if (currentUmp == 0 && umpCount == 1)
            {
                // everything fits in one UMP
                form = MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE;
            }
            else if (currentUmp == 0 && umpCount > 1)
            {
                // first of > 1 UMPs
                form = MIDI_STREAM_MESSAGE_MULTI_FORM_START;
            }
            else if (currentUmp == umpCount - 1)
            {
                // end UMP
                form = MIDI_STREAM_MESSAGE_MULTI_FORM_END;
            }
            else
            {
                // interim UMP
                form = MIDI_STREAM_MESSAGE_MULTI_FORM_CONTINUE;
            }

            midi2::MidiMessage128 umpProjected =
                midi2::MidiMessageBuilder::BuildStreamMessage(
                    timestamp,
                    form,
                    MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_PRODUCT_INSTANCE_ID,
                    (uint16_t)0,
                    (uint32_t)0,
                    (uint32_t)0,
                    (uint32_t)0
                );

            auto ump = winrt::get_self<implementation::MidiMessage128>(umpProjected);

            byte packetBytes[MIDI_STREAM_MESSAGE_ENDPOINT_NAME_CHARACTERS_PER_PACKET]{ 0 };

            // endianness dependence means we can't just cast a pointer to the struct as byte* and run through it.
            for (int i = 0; i < MIDI_STREAM_MESSAGE_ENDPOINT_NAME_CHARACTERS_PER_PACKET && remainingCharacters > 0; i++)
            {
                packetBytes[i] = *utf8StringPointer;
                utf8StringPointer++;    // move along the source string

                remainingCharacters--;
            }

            // convert network to native endianness when copying the bytes over. This is far uglier
            // than just walking a pointer, but it's required here.

            uint32_t word0 = ump->Word0();
            word0 |= packetBytes[0] << 8 | packetBytes[1];  // first two characters are end of first word
            ump->Word0(word0);

            ump->Word1(
                packetBytes[2] << 24 |
                packetBytes[3] << 16 |
                packetBytes[4] << 8 |
                packetBytes[5]);

            ump->Word2(
                packetBytes[6] << 24 |
                packetBytes[7] << 16 |
                packetBytes[8] << 8 |
                packetBytes[9]);

            ump->Word3(
                packetBytes[10] << 24 |
                packetBytes[11] << 16 |
                packetBytes[12] << 8 |
                packetBytes[13]);


            messages.Append(umpProjected);

        }

        utf8StringPointer = nullptr;

        return messages;
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
