#include "pch.h"

#include "catch_amalgamated.hpp"

#include <iostream>
#include <algorithm>

using namespace winrt;
using namespace Windows::Devices::Midi2;

TEST_CASE("UMP Message and Packet Types")
{
	SECTION("Test UMP32")
	{
		MidiUmpMessageType mt = MidiUmpMessageType::Midi1ChannelVoice32;

		MidiUmp32 ump;
		ump.MessageType(mt);

		REQUIRE(ump.MessageType() == mt);
		REQUIRE(ump.MidiUmpPacketType() == MidiUmpPacketType::Ump32);
	}


	SECTION("Test UMP64")
	{
		MidiUmpMessageType mt = MidiUmpMessageType::Midi2ChannelVoice64;

		MidiUmp64 ump;
		ump.MessageType(mt);

		REQUIRE(ump.MessageType() == mt);
		REQUIRE(ump.MidiUmpPacketType() == MidiUmpPacketType::Ump64);
	}

	SECTION("Test UMP96")
	{
		MidiUmpMessageType mt = MidiUmpMessageType::FutureReservedB96;

		MidiUmp96 ump;
		ump.MessageType(mt);

		REQUIRE(ump.MessageType() == mt);
		REQUIRE(ump.MidiUmpPacketType() == MidiUmpPacketType::Ump96);
	}

	SECTION("Test UMP128")
	{
		MidiUmpMessageType mt = MidiUmpMessageType::UmpStream128;

		MidiUmp128 ump;
		ump.MessageType(mt);

		REQUIRE(ump.MessageType() == mt);
		REQUIRE(ump.MidiUmpPacketType() == MidiUmpPacketType::Ump128);
	}



	// TODO: Add message type checking into the UMP class and then test it here (message type must work for packet type)






}


TEST_CASE("UMP Interface Casting")
{
	MidiUmpMessageType mt = MidiUmpMessageType::Midi1ChannelVoice32;

	MidiUmp32 originalUmp;
	originalUmp.Word0(0x08675309);
	originalUmp.MessageType(mt);		// set message type after the word because it changes Word0

	REQUIRE(originalUmp.MessageType() == mt);
	REQUIRE(originalUmp.MidiUmpPacketType() == MidiUmpPacketType::Ump32);

	// cast as the interface
	IMidiUmp iface = originalUmp.as<IMidiUmp>();
	REQUIRE(iface.MessageType() == mt);
	REQUIRE(iface.MidiUmpPacketType() == MidiUmpPacketType::Ump32);

	// recast from the interface back to the UMP type to validate data is still there
	MidiUmp32 recastUmp = iface.as<MidiUmp32>();
	REQUIRE(recastUmp.MessageType() == mt);
	REQUIRE(recastUmp.MidiUmpPacketType() == MidiUmpPacketType::Ump32);
	REQUIRE(recastUmp.Word0() == originalUmp.Word0());

}


//TEST_CASE("UMP Buffer Access")
//{
//	SECTION("Test UMP32")
//	{
//		MidiUmpMessageType mt = MidiUmpMessageType::Midi1ChannelVoice32;
//
//		MidiUmp32 ump;
//		ump.MessageType(mt);
//
//
//
//		// TODO: Set properties and then check that the buffer data matches.
//
//
//
//	}
//}

