// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

bool MidiMessage128Tests::ClassSetup()
{
    std::cout << "MidiEndpointIdHelperTests::ClassSetup" << std::endl;

    winrt::init_apartment();

    return m_initializer.InitializeSdkRuntime();
}

bool MidiMessage128Tests::ClassCleanup()
{
    std::cout << "MidiEndpointIdHelperTests::ClassCleanup" << std::endl;

    m_initializer.ShutdownSdkRuntime();

    winrt::uninit_apartment();

    return true;
}


void MidiMessage128Tests::TestCreateEmpty()
{
    MidiMessage128 ump;

    VERIFY_ARE_EQUAL(ump.MessageType(), MidiMessageType::UtilityMessage32);
    VERIFY_ARE_EQUAL(ump.Word0(), 0x0u);
    VERIFY_ARE_EQUAL(ump.PeekFirstWord(), 0x0u);
}

void MidiMessage128Tests::TestMessageAndPacketType()
{
    auto mt = MidiMessageType::Stream128;

    MidiMessage128 ump;
    ump.MessageType(mt);

    VERIFY_ARE_EQUAL(ump.MessageType(), mt);
    VERIFY_ARE_EQUAL(ump.PacketType(), MidiPacketType::UniversalMidiPacket128);
}

void MidiMessage128Tests::TestCreateFromWords()
{
    uint32_t word0{ 0xF8675309 };
    uint32_t word1{ 0x03263827 };
    uint32_t word2{ 0x01010101 };
    uint32_t word3{ 0x21212121 };
    uint64_t ts{ MidiClock::Now() };

    MidiMessage128 originalUmp(ts, word0, word1, word2, word3);

    VERIFY_ARE_EQUAL(originalUmp.Timestamp(), ts);
    VERIFY_ARE_EQUAL(originalUmp.Word0(), word0);
    VERIFY_ARE_EQUAL(originalUmp.Word1(), word1);
    VERIFY_ARE_EQUAL(originalUmp.Word2(), word2);
    VERIFY_ARE_EQUAL(originalUmp.Word3(), word3);

    VERIFY_ARE_EQUAL(originalUmp.MessageType(), MidiMessageType::Stream128);

}

void MidiMessage128Tests::TestCreateFromArray()
{
    uint32_t word0{ 0xF8675309 };
    uint32_t word1{ 0x03263827 };
    uint32_t word2{ 0x01010101 };
    uint32_t word3{ 0x21212121 };
    uint64_t ts{ MidiClock::Now() };

    uint32_t arr[]{ word0, word1, word2, word3 };

    MidiMessage128 originalUmp(ts, arr);

    VERIFY_ARE_EQUAL(originalUmp.Timestamp(), ts);
    VERIFY_ARE_EQUAL(originalUmp.Word0(), word0);
    VERIFY_ARE_EQUAL(originalUmp.Word1(), word1);
    VERIFY_ARE_EQUAL(originalUmp.Word2(), word2);
    VERIFY_ARE_EQUAL(originalUmp.Word3(), word3);
}

void MidiMessage128Tests::TestCreateFromStruct()
{
    MidiMessageStruct s{ };
    uint64_t ts{ MidiClock::Now() };

    s.Word0 = 0xF1234567;
    s.Word1 = 0xF0F0F0F0;
    s.Word2 = 0x03263727;
    s.Word3 = 0xDEADBEEF;

    auto ump = MidiMessage128::CreateFromStruct(ts, s);

    VERIFY_ARE_EQUAL(ump.Timestamp(), ts);
    VERIFY_ARE_EQUAL(ump.PeekFirstWord(), s.Word0);
    VERIFY_ARE_EQUAL(ump.MessageType(), static_cast<MidiMessageType>(0x0F));
    VERIFY_ARE_EQUAL(ump.Word0(), s.Word0);
    VERIFY_ARE_EQUAL(ump.Word1(), s.Word1);
    VERIFY_ARE_EQUAL(ump.Word2(), s.Word2);
    VERIFY_ARE_EQUAL(ump.Word3(), s.Word3);
}

void MidiMessage128Tests::TestInterfaceCasting()
{
    auto mt = MidiMessageType::FlexData128;

    MidiMessage128 originalUmp;
    originalUmp.Word0(0x08675309);
    originalUmp.Word1(0x12345678);
    originalUmp.Word2(0xDEADBEEF);
    originalUmp.Word2(0x03263827);
    originalUmp.MessageType(mt);        // set message type after the word because it changes Word0

    VERIFY_ARE_EQUAL(originalUmp.MessageType(), mt);
    VERIFY_ARE_EQUAL(originalUmp.PacketType(), MidiPacketType::UniversalMidiPacket128);

    // cast as the interface
    IMidiUniversalPacket iface = originalUmp.as<IMidiUniversalPacket>();
    VERIFY_ARE_EQUAL(iface.MessageType(), mt);
    VERIFY_ARE_EQUAL(iface.PacketType(), MidiPacketType::UniversalMidiPacket128);

    // recast from the interface back to the UMP type to validate data is still there
    MidiMessage128 recastUmp = iface.as<MidiMessage128>();
    VERIFY_ARE_EQUAL(recastUmp.MessageType(), mt);
    VERIFY_ARE_EQUAL(recastUmp.PacketType(), MidiPacketType::UniversalMidiPacket128);
    VERIFY_ARE_EQUAL(recastUmp.Word0(), originalUmp.Word0());
}
