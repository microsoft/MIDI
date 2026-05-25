// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

bool MidiMessage32Tests::ClassSetup()
{
    std::cout << "MidiEndpointIdHelperTests::ClassSetup" << std::endl;

    winrt::init_apartment();

    return m_initializer.InitializeSdkRuntime();
}

bool MidiMessage32Tests::ClassCleanup()
{
    std::cout << "MidiEndpointIdHelperTests::ClassCleanup" << std::endl;

    m_initializer.ShutdownSdkRuntime();

    winrt::uninit_apartment();

    return true;
}


void MidiMessage32Tests::TestCreateEmpty()
{
    MidiMessage32 ump;

    VERIFY_ARE_EQUAL(ump.MessageType(), static_cast<MidiMessageType>(0x0));
    VERIFY_ARE_EQUAL(ump.Word0(), 0x0u);
    VERIFY_ARE_EQUAL(ump.PeekFirstWord(), 0x0u);
}

void MidiMessage32Tests::TestMessageAndPacketType()
{
    auto mt = MidiMessageType::Midi1ChannelVoice32;

    MidiMessage32 ump;
    ump.MessageType(mt);

    VERIFY_ARE_EQUAL(ump.MessageType(), mt);
    VERIFY_ARE_EQUAL(ump.PacketType(), MidiPacketType::UniversalMidiPacket32);
}


void MidiMessage32Tests::TestCreateFromWords()
{
    uint32_t word0{ 0x28675309u };
    uint64_t ts{ MidiClock::Now() };

    MidiMessage32 originalUmp(ts, word0);
    
    VERIFY_ARE_EQUAL(originalUmp.Timestamp(), ts);
    VERIFY_ARE_EQUAL(originalUmp.Word0(), word0);

    VERIFY_ARE_EQUAL(originalUmp.MessageType(), static_cast<MidiMessageType>(0x2));

}

//void MidiMessage32Tests::TestCreateFromArray()
//{
//
//}

void MidiMessage32Tests::TestCreateFromStruct()
{
    MidiMessageStruct s{ };
    uint64_t ts{ MidiClock::Now() };

    s.Word0 = 0x21234567u;
    s.Word1 = 0xF0F0F0F0u;
    s.Word2 = 0x03263727u;
    s.Word3 = 0xDEADBEEFu;

    auto ump = MidiMessage32::CreateFromStruct(ts, s);

    VERIFY_ARE_EQUAL(ump.Timestamp(), ts);
    VERIFY_ARE_EQUAL(ump.PeekFirstWord(), s.Word0);
    VERIFY_ARE_EQUAL(ump.MessageType(), static_cast<MidiMessageType>(0x02));
    VERIFY_ARE_EQUAL(ump.Word0(), s.Word0);
}

void MidiMessage32Tests::TestInterfaceCasting()
{
    auto mt = MidiMessageType::Midi1ChannelVoice32;

    MidiMessage32 originalUmp;
    originalUmp.Word0(0x08675309u);
    originalUmp.MessageType(mt);        // set message type after the word because it changes Word0

    VERIFY_ARE_EQUAL(originalUmp.MessageType(), mt);
    VERIFY_ARE_EQUAL(originalUmp.PacketType(), MidiPacketType::UniversalMidiPacket32);

    // cast as the interface
    IMidiUniversalPacket iface = originalUmp.as<IMidiUniversalPacket>();
    VERIFY_ARE_EQUAL(iface.MessageType(), mt);
    VERIFY_ARE_EQUAL(iface.PacketType(), MidiPacketType::UniversalMidiPacket32);

    // recast from the interface back to the UMP type to validate data is still there
    MidiMessage32 recastUmp = iface.as<MidiMessage32>();
    VERIFY_ARE_EQUAL(recastUmp.MessageType(), mt);
    VERIFY_ARE_EQUAL(recastUmp.PacketType(), MidiPacketType::UniversalMidiPacket32);
    VERIFY_ARE_EQUAL(recastUmp.Word0(), originalUmp.Word0());
}

