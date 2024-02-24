// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "stdafx.h"


using namespace winrt::Windows::Devices::Midi2;

void MidiMessagePacketTests::TestUmp32()
{
    auto mt = MidiMessageType::Midi1ChannelVoice32;

    MidiMessage32 ump;
    ump.MessageType(mt);

    VERIFY_ARE_EQUAL(ump.MessageType(), mt);
    VERIFY_ARE_EQUAL(ump.PacketType(), MidiPacketType::UniversalMidiPacket32);
}


void MidiMessagePacketTests::TestUmp64()
{
    auto mt = MidiMessageType::Midi2ChannelVoice64;

    MidiMessage64 ump;
    ump.MessageType(mt);

    VERIFY_ARE_EQUAL(ump.MessageType(), mt);
    VERIFY_ARE_EQUAL(ump.PacketType(), MidiPacketType::UniversalMidiPacket64);
}

void MidiMessagePacketTests::TestUmp96()
{
    auto mt = MidiMessageType::FutureReservedB96;

    MidiMessage96 ump;
    ump.MessageType(mt);

    VERIFY_ARE_EQUAL(ump.MessageType(), mt);
    VERIFY_ARE_EQUAL(ump.PacketType(), MidiPacketType::UniversalMidiPacket96);
}


void MidiMessagePacketTests::TestUmp128()
{
    auto mt = MidiMessageType::Stream128;

    MidiMessage128 ump;
    ump.MessageType(mt);

    VERIFY_ARE_EQUAL(ump.MessageType(), mt);
    VERIFY_ARE_EQUAL(ump.PacketType(), MidiPacketType::UniversalMidiPacket128);
}



void MidiMessagePacketTests::TestUmpInterfaceCasting()
{
    auto mt = MidiMessageType::Midi1ChannelVoice32;

    MidiMessage32 originalUmp;
    originalUmp.Word0(0x08675309);
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
