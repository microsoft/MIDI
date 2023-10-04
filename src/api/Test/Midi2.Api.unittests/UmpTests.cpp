// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"

#include "catch_amalgamated.hpp"

#include <iostream>
#include <algorithm>

using namespace winrt::Windows::Devices::Midi2;

TEST_CASE("Offline.Ump.MessageAndPacketTypes UMP Message and Packet Types")
{
    SECTION("Test UMP32")
    {
        auto mt = MidiMessageType::Midi1ChannelVoice32;

        MidiMessage32 ump;
        ump.MessageType(mt);

        REQUIRE(ump.MessageType() == mt);
        REQUIRE(ump.PacketType() == MidiPacketType::UniversalMidiPacket32);
    }


    SECTION("Test UMP64")
    {
        auto mt = MidiMessageType::Midi2ChannelVoice64;

        MidiMessage64 ump;
        ump.MessageType(mt);

        REQUIRE(ump.MessageType() == mt);
        REQUIRE(ump.PacketType() == MidiPacketType::UniversalMidiPacket64);
    }

    SECTION("Test UMP96")
    {
        auto mt = MidiMessageType::FutureReservedB96;

        MidiMessage96 ump;
        ump.MessageType(mt);

        REQUIRE(ump.MessageType() == mt);
        REQUIRE(ump.PacketType() == MidiPacketType::UniversalMidiPacket96);
    }

    SECTION("Test UMP128")
    {
        auto mt = MidiMessageType::Stream128;

        MidiMessage128 ump;
        ump.MessageType(mt);

        REQUIRE(ump.MessageType() == mt);
        REQUIRE(ump.PacketType() == MidiPacketType::UniversalMidiPacket128);
    }



    // TODO: Add message type checking into the UMP class and then test it here (message type must work for packet type)






}


TEST_CASE("Offline.Ump.Casting UMP Interface Casting")
{
    auto mt = MidiMessageType::Midi1ChannelVoice32;

    MidiMessage32 originalUmp;
    originalUmp.Word0(0x08675309);
    originalUmp.MessageType(mt);        // set message type after the word because it changes Word0

    REQUIRE(originalUmp.MessageType() == mt);
    REQUIRE(originalUmp.PacketType() == MidiPacketType::UniversalMidiPacket32);

    // cast as the interface
    IMidiUniversalPacket iface = originalUmp.as<IMidiUniversalPacket>();
    REQUIRE(iface.MessageType() == mt);
    REQUIRE(iface.PacketType() == MidiPacketType::UniversalMidiPacket32);

    // recast from the interface back to the UMP type to validate data is still there
    MidiMessage32 recastUmp = iface.as<MidiMessage32>();
    REQUIRE(recastUmp.MessageType() == mt);
    REQUIRE(recastUmp.PacketType() == MidiPacketType::UniversalMidiPacket32);
    REQUIRE(recastUmp.Word0() == originalUmp.Word0());

}


//TEST_CASE("UMP Buffer Access")
//{
//    SECTION("Test UMP32")
//    {
//        MidiUmpMessageType mt = MidiUmpMessageType::Midi1ChannelVoice32;
//
//        MidiUmp32 ump;
//        ump.MessageType(mt);
//
//
//
//        // TODO: Set properties and then check that the buffer data matches.
//
//
//
//    }
//}

