// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiStreamMessageBuilder.h"
#include "Messages.MidiStreamMessageBuilder.g.cpp"





namespace winrt::Microsoft::Windows::Devices::Midi2::Messages::implementation
{
    _Use_decl_annotations_
    midi2::IMidiUniversalPacket MidiStreamMessageBuilder::BuildEndpointDiscoveryMessage(
         internal::MidiTimestamp const timestamp,
         uint8_t const umpVersionMajor,
         uint8_t const umpVersionMinor,
         msgs::MidiEndpointDiscoveryRequests const requestFlags
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
    midi2::IMidiUniversalPacket MidiStreamMessageBuilder::BuildEndpointInfoNotificationMessage(
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
        word1 |= (static_cast<uint32_t>(internal::CleanupByte7(numberOfFunctionBlocks)) << 24);

        // bit 9 (reading from least sig to most) is midi 1, bit 10 is midi 2
        
        if (supportsMidi10Protocol) word1 |= (uint32_t)0x0100;
        if (supportsMidi20Protocol) word1 |= (uint32_t)0x0200;

        // first bit is tx jr, second bit 2 is rx jr

        if (supportsSendingJitterReductionTimestamps) word1 |= (uint32_t)0x01;
        if (supportsReceivingJitterReductionTimestamps) word1 |= (uint32_t)0x02;

        return MidiMessageBuilder::BuildStreamMessage(
            timestamp,
            MIDI_STREAM_MESSAGE_STANDARD_FORM0,     
            MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_INFO_NOTIFICATION, 
            static_cast<uint16_t>(umpVersionMajor) << 8 | static_cast<uint16_t>(umpVersionMinor), // ump major is msb, ump minor is lsb
            word1, 
            MIDI_RESERVED_WORD,      // reserved word2   
            MIDI_RESERVED_WORD       // reserved word3
        );
    }

    _Use_decl_annotations_
    midi2::IMidiUniversalPacket MidiStreamMessageBuilder::BuildDeviceIdentityNotificationMessage(
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

        word1 |= static_cast<uint32_t>(internal::CleanupByte7(deviceManufacturerSysExIdByte1)) << 16;
        word1 |= static_cast<uint32_t>(internal::CleanupByte7(deviceManufacturerSysExIdByte2)) << 8;
        word1 |= static_cast<uint32_t>(internal::CleanupByte7(deviceManufacturerSysExIdByte3));

        word2 |= static_cast<uint32_t>(internal::CleanupByte7(deviceFamilyLsb)) << 24;
        word2 |= static_cast<uint32_t>(internal::CleanupByte7(deviceFamilyMsb)) << 16;
        word2 |= static_cast<uint32_t>(internal::CleanupByte7(deviceFamilyModelNumberLsb)) << 8;
        word2 |= static_cast<uint32_t>(internal::CleanupByte7(deviceFamilyModelNumberMsb));

        word3 |= static_cast<uint32_t>(internal::CleanupByte7(softwareRevisionLevelByte1)) << 24;
        word3 |= static_cast<uint32_t>(internal::CleanupByte7(softwareRevisionLevelByte2)) << 16;
        word3 |= static_cast<uint32_t>(internal::CleanupByte7(softwareRevisionLevelByte3)) << 8;
        word3 |= static_cast<uint32_t>(internal::CleanupByte7(softwareRevisionLevelByte4));


        return MidiMessageBuilder::BuildStreamMessage(
            timestamp,
            MIDI_STREAM_MESSAGE_STANDARD_FORM0,
            MIDI_STREAM_MESSAGE_STATUS_DEVICE_IDENTITY_NOTIFICATION,
            MIDI_RESERVED_FIELD,
            word1,
            word2,
            word3
            );
    }


    // TODO: Add ASCII/UTF-8 encoding option

    _Use_decl_annotations_
    collections::IVector<midi2::IMidiUniversalPacket> MidiStreamMessageBuilder::BuildSplitTextMessages(
        internal::MidiTimestamp const timestamp,
        uint8_t const status, 
        uint16_t const word0Remainder, 
        uint8_t const maxCharacters,
        uint8_t const maxCharactersPerPacket,
        winrt::hstring const& text)
    {
        auto messages = winrt::single_threaded_vector<midi2::IMidiUniversalPacket>();

        // endpoint name is one or more UMPs, max 98 bytes. Either 1 complete message (form 0x0)
        // or start (form 0x1, continue messages 0x2, and end 0x3)

        // don't process past the last allowed character
        size_t totalCharacters = (text.size() > maxCharacters) ? maxCharacters : text.size();
        size_t remainingCharacters = totalCharacters;

        // TODO: The Product Instance Id is ASCII, not UTF-8, so need to provide some encoding
        // information to this function
        // 
        // convert to 8 bit UTF-8 characters, and get the pointer to the first character
        auto utf8Version = winrt::to_string(text);
        char* utf8StringPointer = utf8Version.data();

        size_t umpCount = totalCharacters / maxCharactersPerPacket;
        if (totalCharacters % maxCharactersPerPacket != 0) umpCount++;

        for (size_t currentUmp = 0; currentUmp < umpCount; currentUmp++)
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
                msgs::MidiMessageBuilder::BuildStreamMessage(
                    timestamp,
                    form,
                    status,
                    word0Remainder,
                    (uint32_t)0,
                    (uint32_t)0,
                    (uint32_t)0
                );

            //auto ump = winrt::get_self<implementation::MidiMessage128>(umpProjected);
            
            // allocate for the most we ever see
            uint8_t packetBytes[14]{ 0 };

            // endianness dependence means we can't just cast a pointer to the struct as byte* and run through it.
            for (uint8_t i = 0; i < maxCharactersPerPacket && remainingCharacters > 0; i++)
            {
                packetBytes[i] = *utf8StringPointer;
                utf8StringPointer++;    // move along the source string

                remainingCharacters--;
            }

            // convert network to native endianness when copying the bytes over. This is far uglier
            // than just walking a pointer, but it's required here.

            // figure out where in the first word we start. This differs by status. We use the
            // max characters per packet to figure this out
            uint8_t characterCountFirstWord = maxCharactersPerPacket % 4;


            uint8_t currentIndex = 0;

            uint32_t word0 = umpProjected.Word0();

            if (characterCountFirstWord == 2)
            {
                word0 |= static_cast<uint32_t>(packetBytes[currentIndex++]) << 8;
            }

            if (characterCountFirstWord >= 1)
            {
                word0 |= static_cast<uint32_t>(packetBytes[currentIndex++]);
            }

            umpProjected.Word0(word0);

            // remaining packets are filled with the data we have

            umpProjected.Word1(
                static_cast<uint32_t>(packetBytes[currentIndex++]) << 24 |
                static_cast<uint32_t>(packetBytes[currentIndex++]) << 16 |
                static_cast<uint32_t>(packetBytes[currentIndex++]) << 8 |
                static_cast<uint32_t>(packetBytes[currentIndex++]));

            umpProjected.Word2(
                static_cast<uint32_t>(packetBytes[currentIndex++]) << 24 |
                static_cast<uint32_t>(packetBytes[currentIndex++]) << 16 |
                static_cast<uint32_t>(packetBytes[currentIndex++]) << 8 |
                static_cast<uint32_t>(packetBytes[currentIndex++]));

            umpProjected.Word3(
                static_cast<uint32_t>(packetBytes[currentIndex++]) << 24 |
                static_cast<uint32_t>(packetBytes[currentIndex++]) << 16 |
                static_cast<uint32_t>(packetBytes[currentIndex++]) << 8 |
                static_cast<uint32_t>(packetBytes[currentIndex++]));

            messages.Append(umpProjected);

        }

        utf8StringPointer = nullptr;

        return messages;

    }



    // TODO: This is UTF-8
    _Use_decl_annotations_
    collections::IVector<midi2::IMidiUniversalPacket> MidiStreamMessageBuilder::BuildEndpointNameNotificationMessages(
         internal::MidiTimestamp const timestamp,
         winrt::hstring const& name
    ) noexcept
    {
        return BuildSplitTextMessages(
            timestamp,
            MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION,
            (uint16_t)0,
            MIDI_STREAM_MESSAGE_ENDPOINT_NAME_MAX_LENGTH,
            MIDI_STREAM_MESSAGE_ENDPOINT_NAME_CHARACTERS_PER_PACKET,
            name
        );
    }

    // TODO: This is ASCII
    _Use_decl_annotations_
    collections::IVector<midi2::IMidiUniversalPacket> MidiStreamMessageBuilder::BuildProductInstanceIdNotificationMessages(
        internal::MidiTimestamp const timestamp,
        winrt::hstring const& productInstanceId
    )
    {
        return BuildSplitTextMessages(
            timestamp,
            MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_PRODUCT_INSTANCE_ID_NOTIFICATION,
            (uint16_t)0,
            MIDI_STREAM_MESSAGE_PRODUCT_INSTANCE_ID_MAX_LENGTH,
            MIDI_STREAM_MESSAGE_PRODUCT_INSTANCE_ID_CHARACTERS_PER_PACKET,
            productInstanceId
        );

    }

    _Use_decl_annotations_
    midi2::IMidiUniversalPacket MidiStreamMessageBuilder::BuildStreamConfigurationRequestMessage(
        internal::MidiTimestamp const timestamp,
        uint8_t const protocol,
        bool const expectToReceiveJRTimestamps,
        bool const requestToSendJRTimestamps
    )
    {
        uint16_t word0Remaining{ 0 };

        word0Remaining |= (static_cast<uint16_t>(protocol) << 8);

        if (expectToReceiveJRTimestamps)
            word0Remaining |= 0x02;     // second bit

        if (requestToSendJRTimestamps)
            word0Remaining |= 0x01;     // first bit

        return MidiMessageBuilder::BuildStreamMessage(
            timestamp,
            MIDI_STREAM_MESSAGE_STANDARD_FORM0,
            MIDI_STREAM_MESSAGE_STATUS_STREAM_CONFIGURATION_REQUEST,
            word0Remaining,
            MIDI_RESERVED_WORD,
            MIDI_RESERVED_WORD,
            MIDI_RESERVED_WORD
        );

    }

    _Use_decl_annotations_
    midi2::IMidiUniversalPacket MidiStreamMessageBuilder::BuildStreamConfigurationNotificationMessage(
        internal::MidiTimestamp const timestamp,
        uint8_t const protocol,
        bool const confirmationWillReceiveJRTimestamps,
        bool const confirmationSendJRTimestamps
    )
    {
        uint16_t word0Remaining{ 0 };

        word0Remaining |= (static_cast<uint16_t>(protocol) << 8);

        if (confirmationWillReceiveJRTimestamps)
            word0Remaining |= 0x02;     // second bit

        if (confirmationSendJRTimestamps)
            word0Remaining |= 0x01;     // first bit

        return MidiMessageBuilder::BuildStreamMessage(
            timestamp,
            MIDI_STREAM_MESSAGE_STANDARD_FORM0,
            MIDI_STREAM_MESSAGE_STATUS_STREAM_CONFIGURATION_NOTIFICATION,
            word0Remaining,
            MIDI_RESERVED_WORD,
            MIDI_RESERVED_WORD,
            MIDI_RESERVED_WORD
        );
    }

    _Use_decl_annotations_
    midi2::IMidiUniversalPacket MidiStreamMessageBuilder::BuildFunctionBlockDiscoveryMessage(
        internal::MidiTimestamp const timestamp,
        uint8_t const functionBlockNumber,
        msgs::MidiFunctionBlockDiscoveryRequests const requestFlags
        )
    {
        uint16_t word0Remaining{ 0 };

        word0Remaining |= (static_cast<uint16_t>(functionBlockNumber) << 8);

        word0Remaining |= (uint8_t)requestFlags;

        return MidiMessageBuilder::BuildStreamMessage(
            timestamp,
            MIDI_STREAM_MESSAGE_STANDARD_FORM0,
            MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_DISCOVERY,
            word0Remaining,
            MIDI_RESERVED_WORD,
            MIDI_RESERVED_WORD,
            MIDI_RESERVED_WORD
        );
    }

    _Use_decl_annotations_
    midi2::IMidiUniversalPacket MidiStreamMessageBuilder::BuildFunctionBlockInfoNotificationMessage(
        internal::MidiTimestamp const timestamp,
        bool const active,
        uint8_t const functionBlockNumber,
        midi2::MidiFunctionBlockUIHint const& uiHint,
        midi2::MidiFunctionBlockRepresentsMidi10Connection const& midi10,
        midi2::MidiFunctionBlockDirection const& direction,
        uint8_t const firstGroup,
        uint8_t const numberOfGroups,
        uint8_t const midiCIVersionFormat,
        uint8_t const maxNumberSysEx8Streams
    )
    {
        uint16_t word0Remaining{ 0 };
        uint32_t word1{ 0 };


        word0Remaining |= static_cast<uint16_t>(internal::CleanupByte7(functionBlockNumber)) << 8;
        word0Remaining |= static_cast<uint16_t>(uiHint) << 4;
        word0Remaining |= static_cast<uint16_t>(midi10) << 2;
        word0Remaining |= static_cast<uint16_t>(direction);

        if (active)
        {
            word0Remaining |= 0x8000;
        }

        word1 =
            static_cast<uint32_t>(firstGroup) << 24 |
            static_cast<uint32_t>(numberOfGroups) << 16 |
            static_cast<uint32_t>(midiCIVersionFormat) << 8 |
            static_cast<uint32_t>(maxNumberSysEx8Streams);


        return MidiMessageBuilder::BuildStreamMessage(
            timestamp,
            MIDI_STREAM_MESSAGE_STANDARD_FORM0,
            MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_INFO_NOTIFICATION,
            word0Remaining,
            word1,
            MIDI_RESERVED_WORD,
            MIDI_RESERVED_WORD
            );

    }

    // TODO: This is UTF-8
    _Use_decl_annotations_
    collections::IVector<midi2::IMidiUniversalPacket> MidiStreamMessageBuilder::BuildFunctionBlockNameNotificationMessages(
        internal::MidiTimestamp const timestamp,
        uint8_t const functionBlockNumber,
        winrt::hstring const& name
    ) 
    {
        // fb notifications include the function block number as the second
        // to last byte
        uint16_t word0Remainder = static_cast<uint16_t>(functionBlockNumber) << 8;

        return BuildSplitTextMessages(
            timestamp,
            MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_NAME_NOTIFICATION,
            word0Remainder,
            MIDI_STREAM_MESSAGE_FUNCTION_BLOCK_NAME_MAX_LENGTH,
            MIDI_STREAM_MESSAGE_FUNCTION_BLOCK_NAME_CHARACTERS_PER_PACKET,
            name
        );
    }


    _Use_decl_annotations_
    winrt::hstring MidiStreamMessageBuilder::ParseFunctionBlockNameNotificationMessages(
        collections::IIterable<midi2::IMidiUniversalPacket> const& messages
        )
    {
        std::string s{};
        s.reserve(13);

        for (auto ump : messages)
        {
            midi2::MidiMessage128 message{ nullptr };

            if (ump.MessageType() == midi2::MidiMessageType::Stream128)
            {
                message = ump.as<midi2::MidiMessage128>();

                // verify that the message form is correct (begin/[continue]/end or just complete)

                // verify the status is correct

                AppendCharToString(s, MIDIWORDBYTE4(message.Word0()));

                AppendCharToString(s, MIDIWORDBYTE1(message.Word1()));
                AppendCharToString(s, MIDIWORDBYTE2(message.Word1()));
                AppendCharToString(s, MIDIWORDBYTE3(message.Word1()));
                AppendCharToString(s, MIDIWORDBYTE4(message.Word1()));

                AppendCharToString(s, MIDIWORDBYTE1(message.Word2()));
                AppendCharToString(s, MIDIWORDBYTE2(message.Word2()));
                AppendCharToString(s, MIDIWORDBYTE3(message.Word2()));
                AppendCharToString(s, MIDIWORDBYTE4(message.Word2()));

                AppendCharToString(s, MIDIWORDBYTE1(message.Word3()));
                AppendCharToString(s, MIDIWORDBYTE2(message.Word3()));
                AppendCharToString(s, MIDIWORDBYTE3(message.Word3()));
                AppendCharToString(s, MIDIWORDBYTE4(message.Word3()));
            }

        }

        return winrt::to_hstring(s);

    }

    _Use_decl_annotations_
    winrt::hstring MidiStreamMessageBuilder::ParseEndpointNameNotificationMessages(
        collections::IIterable<midi2::IMidiUniversalPacket> const& messages
        )
    {
        std::string s{};
        s.reserve(14);

        for (auto ump : messages)
        {
            midi2::MidiMessage128 message{ nullptr };

            if (ump.MessageType() == midi2::MidiMessageType::Stream128)
            {
                message = ump.as<midi2::MidiMessage128>();

                // verify that the message form is correct (begin/[continue]/end or just complete)

                // verify the status is correct

                AppendCharToString(s, MIDIWORDBYTE3(message.Word0()));
                AppendCharToString(s, MIDIWORDBYTE4(message.Word0()));

                AppendCharToString(s, MIDIWORDBYTE1(message.Word1()));
                AppendCharToString(s, MIDIWORDBYTE2(message.Word1()));
                AppendCharToString(s, MIDIWORDBYTE3(message.Word1()));
                AppendCharToString(s, MIDIWORDBYTE4(message.Word1()));

                AppendCharToString(s, MIDIWORDBYTE1(message.Word2()));
                AppendCharToString(s, MIDIWORDBYTE2(message.Word2()));
                AppendCharToString(s, MIDIWORDBYTE3(message.Word2()));
                AppendCharToString(s, MIDIWORDBYTE4(message.Word2()));

                AppendCharToString(s, MIDIWORDBYTE1(message.Word3()));
                AppendCharToString(s, MIDIWORDBYTE2(message.Word3()));
                AppendCharToString(s, MIDIWORDBYTE3(message.Word3()));
                AppendCharToString(s, MIDIWORDBYTE4(message.Word3()));
            }
        }

        return winrt::to_hstring(s);
    }

    _Use_decl_annotations_
    winrt::hstring MidiStreamMessageBuilder::ParseProductInstanceIdNotificationMessages(
        collections::IIterable<midi2::IMidiUniversalPacket> const& messages
        )
    {
        std::string s{};
        s.reserve(14);

        for (auto ump : messages)
        {
            midi2::MidiMessage128 message{ nullptr };

            if (ump.MessageType() == midi2::MidiMessageType::Stream128)
            {
                message = ump.as<midi2::MidiMessage128>();
                // verify that the message form is correct (begin/[continue]/end or just complete)

            // verify the status is correct

                AppendCharToString(s, MIDIWORDBYTE3(message.Word0()));
                AppendCharToString(s, MIDIWORDBYTE4(message.Word0()));

                AppendCharToString(s, MIDIWORDBYTE1(message.Word1()));
                AppendCharToString(s, MIDIWORDBYTE2(message.Word1()));
                AppendCharToString(s, MIDIWORDBYTE3(message.Word1()));
                AppendCharToString(s, MIDIWORDBYTE4(message.Word1()));

                AppendCharToString(s, MIDIWORDBYTE1(message.Word2()));
                AppendCharToString(s, MIDIWORDBYTE2(message.Word2()));
                AppendCharToString(s, MIDIWORDBYTE3(message.Word2()));
                AppendCharToString(s, MIDIWORDBYTE4(message.Word2()));

                AppendCharToString(s, MIDIWORDBYTE1(message.Word3()));
                AppendCharToString(s, MIDIWORDBYTE2(message.Word3()));
                AppendCharToString(s, MIDIWORDBYTE3(message.Word3()));
                AppendCharToString(s, MIDIWORDBYTE4(message.Word3()));
            }
        }

        return winrt::to_hstring(s);
    }


}
