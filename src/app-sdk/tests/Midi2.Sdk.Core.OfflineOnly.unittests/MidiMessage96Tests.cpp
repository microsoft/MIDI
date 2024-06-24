// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

void MidiMessage96Tests::TestCreateEmpty()
{
    MidiMessage96 ump;

    VERIFY_ARE_EQUAL(ump.MessageType(), MidiMessageType::UtilityMessage32);
    VERIFY_ARE_EQUAL(ump.Word0(), 0x0u);
    VERIFY_ARE_EQUAL(ump.PeekFirstWord(), 0x0u);
}

void MidiMessage96Tests::TestMessageAndPacketType()
{
    auto mt = MidiMessageType::FutureReservedB96;

    MidiMessage96 ump;
    ump.MessageType(mt);

    VERIFY_ARE_EQUAL(ump.MessageType(), mt);
    VERIFY_ARE_EQUAL(ump.PacketType(), MidiPacketType::UniversalMidiPacket96);
}

void MidiMessage96Tests::TestCreateFromWords()
{
    uint32_t word0{ 0xB8675309 };
    uint32_t word1{ 0x03263827 };
    uint32_t word2{ 0x01010101 };
    uint64_t ts{ MidiClock::Now() };

    MidiMessage96 originalUmp(ts, word0, word1, word2);

    VERIFY_ARE_EQUAL(originalUmp.Timestamp(), ts);
    VERIFY_ARE_EQUAL(originalUmp.Word0(), word0);
    VERIFY_ARE_EQUAL(originalUmp.Word1(), word1);
    VERIFY_ARE_EQUAL(originalUmp.Word2(), word2);

    VERIFY_ARE_EQUAL(originalUmp.MessageType(), static_cast<MidiMessageType>(0xB));
}

void MidiMessage96Tests::TestCreateFromArray()
{
    uint32_t word0{ 0xB8675309 };
    uint32_t word1{ 0x03263827 };
    uint32_t word2{ 0x01010101 };
    uint64_t ts{ MidiClock::Now() };

    uint32_t arr[]{ word0, word1, word2 };

    MidiMessage96 originalUmp(ts, arr);

    VERIFY_ARE_EQUAL(originalUmp.Timestamp(), ts);
    VERIFY_ARE_EQUAL(originalUmp.Word0(), word0);
    VERIFY_ARE_EQUAL(originalUmp.Word1(), word1);
    VERIFY_ARE_EQUAL(originalUmp.Word2(), word2);

}

void MidiMessage96Tests::TestCreateFromStruct()
{
    MidiMessageStruct s{ };
    uint64_t ts{ MidiClock::Now() };

    s.Word0 = 0xB1234567;
    s.Word1 = 0xF0F0F0F0;
    s.Word2 = 0x03263727;
    s.Word3 = 0xDEADBEEF;

    auto ump = MidiMessage96::CreateFromStruct(ts, s);

    VERIFY_ARE_EQUAL(ump.Timestamp(), ts);
    VERIFY_ARE_EQUAL(ump.PeekFirstWord(), s.Word0);
    VERIFY_ARE_EQUAL(ump.MessageType(), static_cast<MidiMessageType>(0x0B));
    VERIFY_ARE_EQUAL(ump.Word0(), s.Word0);
    VERIFY_ARE_EQUAL(ump.Word1(), s.Word1);
    VERIFY_ARE_EQUAL(ump.Word2(), s.Word2);
}

void MidiMessage96Tests::TestInterfaceCasting()
{
    auto mt = MidiMessageType::FutureReservedC96;

    MidiMessage96 originalUmp;
    originalUmp.Word0(0x08675309);
    originalUmp.Word1(0x12345678);
    originalUmp.Word2(0xDEADBEEF);
    originalUmp.MessageType(mt);        // set message type after the word because it changes Word0

    VERIFY_ARE_EQUAL(originalUmp.MessageType(), mt);
    VERIFY_ARE_EQUAL(originalUmp.PacketType(), MidiPacketType::UniversalMidiPacket96);

    // cast as the interface
    IMidiUniversalPacket iface = originalUmp.as<IMidiUniversalPacket>();
    VERIFY_ARE_EQUAL(iface.MessageType(), mt);
    VERIFY_ARE_EQUAL(iface.PacketType(), MidiPacketType::UniversalMidiPacket96);

    // recast from the interface back to the UMP type to validate data is still there
    MidiMessage96 recastUmp = iface.as<MidiMessage96>();
    VERIFY_ARE_EQUAL(recastUmp.MessageType(), mt);
    VERIFY_ARE_EQUAL(recastUmp.PacketType(), MidiPacketType::UniversalMidiPacket96);
    VERIFY_ARE_EQUAL(recastUmp.Word0(), originalUmp.Word0());
}
