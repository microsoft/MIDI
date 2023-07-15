// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// ----------------------------------------------------------------------------
// This requires the version of MidiSrv with the hard-coded loopback endpoint
// ----------------------------------------------------------------------------


#include "pch.h"

#include "catch_amalgamated.hpp"

#include <iostream>
#include <algorithm>
#include <Windows.h>

//#include "..\api-core\ump_helpers.h"

using namespace winrt;
using namespace winrt::Windows::Devices::Midi2;

#define BIDI_ENDPOINT_DEVICE_ID L"foobarbaz"


TEST_CASE("Connected.Endpoint.CreateBidi Create bidirectional endpoint")
{
	auto settings = MidiSessionSettings::Default();
	auto session = MidiSession::CreateSession(L"Test Session Name", settings);

	REQUIRE((bool)(session.IsOpen()));

	REQUIRE((bool)(session.Connections().Size() == 0));

	std::cout << "Connecting to Endpoint" << std::endl;

	auto conn1 = session.ConnectBidirectionalEndpoint(BIDI_ENDPOINT_DEVICE_ID, nullptr);

	REQUIRE(conn1 != nullptr);
	REQUIRE(!conn1.Id().empty());
	REQUIRE(conn1.IsConnected());

	REQUIRE(session.Connections().Size() == 1);

	std::cout << "Endpoint Id: " << winrt::to_string(conn1.Id()) << std::endl;
	std::cout << "Device Id: " << winrt::to_string(conn1.DeviceId()) << std::endl;

	//std::for_each(
	//	begin(session.Connections()),
	//	end(session.Connections()),
	//	[](const winrt::Windows::Foundation::Collections::IKeyValuePair<hstring, winrt::Windows::Devices::Midi2::MidiEndpointConnection>& kvp)
	//	{
	//		hstring key = kvp.Key();

	//		std::cout << "Endpoint Key in Map: " << winrt::to_string(key) << std::endl;
	//	}
	//);

}


TEST_CASE("Connected.Endpoint.SingleUmp Send and receive single Ump32 message")
{
	auto settings = MidiSessionSettings::Default();
	auto session = MidiSession::CreateSession(L"Test Session Name", settings);

	REQUIRE((bool)(session.IsOpen()));
	REQUIRE((bool)(session.Connections().Size() == 0));

	std::cout << "Connecting to Endpoint" << std::endl;

	auto conn1 = session.ConnectBidirectionalEndpoint(L"foobarbaz", nullptr);

	REQUIRE((bool)(conn1 != nullptr));

	bool messageReceivedFlag = false;
	MidiUmp32 sentUmp;

	auto sentMessageType = MidiUmpMessageType::Midi1ChannelVoice32;
	auto sentTimestamp = MidiClock::GetMidiTimestamp();

	auto MessageReceivedHandler = [&messageReceivedFlag, &sentMessageType, &sentTimestamp](Windows::Foundation::IInspectable const& sender, MidiMessageReceivedEventArgs const& args)
		{
			REQUIRE((bool)(sender != nullptr));
			REQUIRE((bool)(args != nullptr));

			// TODO: making an assumption on type here.
			MidiUmp32 receivedUmp = args.Ump().as<MidiUmp32>();

			REQUIRE(receivedUmp != nullptr);
			REQUIRE(receivedUmp.MessageType() == sentMessageType);
			REQUIRE(receivedUmp.Timestamp() == sentTimestamp);

			messageReceivedFlag = true;
			//std::cout << "Received message in test" << std::endl;
			//std::cout << " - MidiUmpPacketType " << std::hex << (int)(receivedUmp.MidiUmpPacketType()) << std::endl;
			//std::cout << " - Timestamp " << std::hex << (receivedUmp.Timestamp()) << std::endl;
			//std::cout << " - MessageType " << std::hex << (int)(receivedUmp.MessageType()) << std::endl;
			//std::cout << " - First Word " << std::hex << (receivedUmp.Word0()) << std::endl;
		};

	auto eventRevokeToken = conn1.MessageReceived(MessageReceivedHandler);


	// send message

	sentUmp.MessageType(MidiUmpMessageType::Midi1ChannelVoice32);
	sentUmp.Timestamp(sentTimestamp);

	//std::cout << "Sending MidiUmpPacketType from test" << std::hex << (uint32_t)(sentUmp.MidiUmpPacketType()) << std::endl;
	//std::cout << " - Timestamp " << std::hex << (uint64_t)(sentUmp.Timestamp()) << std::endl;
	//std::cout << " - MessageType " << std::hex << (int)(sentUmp.MessageType()) << std::endl;
	//std::cout << " - First Word " << std::hex << (sentUmp.Word0()) << std::endl;

	conn1.SendUmp(sentUmp);


	// Wait for incoming message

	int timeoutCounter = 3000;
	while (!messageReceivedFlag && timeoutCounter > 0)
	{
		Sleep(1);
		timeoutCounter--;
	}

	REQUIRE(messageReceivedFlag);

	// unwire event
	conn1.MessageReceived(eventRevokeToken);

	// cleanup endpoint. Technically not required as session will do it
	session.DisconnectEndpointConnection(conn1.Id());
}




TEST_CASE("Connected.Endpoint.MultipleUmpWords Send and receive multiple words")
{
	uint64_t setupStartTimestamp = MidiClock::GetMidiTimestamp();


	auto settings = MidiSessionSettings::Default();
	auto session = MidiSession::CreateSession(L"Test Session Name", settings);

	REQUIRE((bool)(session.IsOpen()));
	REQUIRE((bool)(session.Connections().Size() == 0));

	std::cout << "Connecting to Endpoint" << std::endl;

	auto conn1 = session.ConnectBidirectionalEndpoint(BIDI_ENDPOINT_DEVICE_ID, nullptr);

	REQUIRE((bool)(conn1 != nullptr));

	uint32_t receivedMessageCount{};


	auto WordsReceivedHandler = [&receivedMessageCount](Windows::Foundation::IInspectable const& sender, MidiWordsReceivedEventArgs const& args)
		{
			REQUIRE((bool)(sender != nullptr));
			REQUIRE((bool)(args != nullptr));

			receivedMessageCount++;

		};

	auto eventRevokeToken = conn1.WordsReceived(WordsReceivedHandler);


	// send messages

	uint32_t numMessagesToSend = 500;

	//std::cout << "Sending messages. Count=" << std::dec << numMessagesToSend << std::endl;

	uint32_t numBytes = 0;


	uint32_t words[]{ 0,0,0,0 };
	uint32_t wordCount = 0;

	for (int i = 0; i < numMessagesToSend; i++)
	{
		auto timestamp = MidiClock::GetMidiTimestamp();

		switch (i % 4)
		{
		case 0:
		{
			words[0] = 0x20000000;
			wordCount = (uint32_t)(Windows::Devices::Midi2::MidiUmpPacketType::Ump32);

		}
		break;
		case 1:
		{
			words[0] = 0x40000000;
			wordCount = (uint32_t)(Windows::Devices::Midi2::MidiUmpPacketType::Ump64);
		}
		break;
		case 2:
		{
			words[0] = 0xB0000000;
			wordCount = (uint32_t)(Windows::Devices::Midi2::MidiUmpPacketType::Ump96);
		}
		break;
		case 3:
		{
			words[0] = 0xF0000000;
			wordCount = (uint32_t)(Windows::Devices::Midi2::MidiUmpPacketType::Ump128);
		}
		break;
		}

		numBytes += sizeof(uint32_t) * wordCount + sizeof(uint64_t);
		conn1.SendUmpWords(timestamp, words, wordCount);

	}
	// Wait for incoming message

	uint32_t timeoutCounter = 1000000;
	uint32_t sleepDuration = 0;

	while (receivedMessageCount < numMessagesToSend && timeoutCounter > 0)
	{
		Sleep(sleepDuration);

		timeoutCounter--;
	}

	REQUIRE(receivedMessageCount == numMessagesToSend);

	// unwire event
	conn1.WordsReceived(eventRevokeToken);

	// cleanup endpoint. Technically not required as session will do it
	session.DisconnectEndpointConnection(conn1.Id());
}
