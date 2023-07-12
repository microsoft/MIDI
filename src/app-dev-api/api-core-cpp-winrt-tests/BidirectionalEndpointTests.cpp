#include "pch.h"

#include "catch_amalgamated.hpp"

#include <iostream>
#include <algorithm>
#include <Windows.h>

using namespace winrt;
using namespace Windows::Devices::Midi2;

TEST_CASE("Create bidirectional endpoint")
{
	auto settings = MidiSessionSettings::Default();
	auto session = MidiSession::CreateSession(L"Test Session Name", settings);

	REQUIRE((bool)(session.IsOpen()));

	REQUIRE((bool)(session.Connections().Size() == 0));

	std::cout << "Connecting to Endpoint" << std::endl;

	auto conn1 = session.ConnectBidirectionalEndpoint(L"foobarbaz", L"", nullptr);

	REQUIRE((bool)(conn1 != nullptr));
	REQUIRE((bool)(conn1.IsConnected()));
	REQUIRE((bool)(session.Connections().Size() == 1));

	std::cout << "Endpoint Id: " << winrt::to_string(conn1.DeviceId()) << std::endl;

	std::for_each(
		begin(session.Connections()),
		end(session.Connections()),
		[](const winrt::Windows::Foundation::Collections::IKeyValuePair<hstring, winrt::Windows::Devices::Midi2::MidiEndpointConnection>& kvp)
		{
			hstring key = kvp.Key();

			std::cout << "Endpoint Key in Map: " << winrt::to_string(key) << std::endl;
		}
	);

}


TEST_CASE("Send and receive messages")
{
	auto settings = MidiSessionSettings::Default();
	auto session = MidiSession::CreateSession(L"Test Session Name", settings);

	REQUIRE((bool)(session.IsOpen()));
	REQUIRE((bool)(session.Connections().Size() == 0));

	std::cout << "Connecting to Endpoint" << std::endl;

	auto conn1 = session.ConnectBidirectionalEndpoint(L"foobarbaz", L"", nullptr);

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
			std::cout << "Received message in test" << std::endl;
			std::cout << " - MidiUmpPacketType " << std::hex << (int)(receivedUmp.MidiUmpPacketType()) << std::endl;
			std::cout << " - Timestamp " << std::hex << (receivedUmp.Timestamp()) << std::endl;
			std::cout << " - MessageType " << std::hex << (int)(receivedUmp.MessageType()) << std::endl;
			std::cout << " - First Word " << std::hex << (receivedUmp.Word0()) << std::endl;
		};

	auto eventRevokeToken = conn1.MessageReceived(MessageReceivedHandler);


	// send message

	sentUmp.MessageType(MidiUmpMessageType::Midi1ChannelVoice32);
	sentUmp.Timestamp(sentTimestamp);




	std::cout << "Sending MidiUmpPacketType from test" << std::hex << (uint32_t)(sentUmp.MidiUmpPacketType()) << std::endl;
	std::cout << " - Timestamp " << std::hex << (uint64_t)(sentUmp.Timestamp()) << std::endl;
	std::cout << " - MessageType " << std::hex << (int)(sentUmp.MessageType()) << std::endl;
	std::cout << " - First Word " << std::hex << (sentUmp.Word0()) << std::endl;

	conn1.SendUmp(sentUmp);


	// Wait for incoming message

	int timeoutCounter = 1000;
	while (!messageReceivedFlag && timeoutCounter > 0)
	{
		Sleep(10);

		timeoutCounter--;
	}

	REQUIRE(messageReceivedFlag);

	// unwire event
	conn1.MessageReceived(eventRevokeToken);

}
