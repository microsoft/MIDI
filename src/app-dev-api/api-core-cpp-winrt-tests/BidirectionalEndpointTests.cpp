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


TEST_CASE("Send and receive single Ump32 message")
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


// TODO
TEST_CASE("Send and receive mixed multiple messages")
{
	uint64_t setupStartTimestamp = MidiClock::GetMidiTimestamp();


	auto settings = MidiSessionSettings::Default();
	auto session = MidiSession::CreateSession(L"Test Session Name", settings);

	REQUIRE((bool)(session.IsOpen()));
	REQUIRE((bool)(session.Connections().Size() == 0));

	std::cout << "Connecting to Endpoint" << std::endl;

	auto conn1 = session.ConnectBidirectionalEndpoint(L"foobarbaz", L"", nullptr);

	REQUIRE((bool)(conn1 != nullptr));

	uint32_t receivedMessageCount{};

	auto ump32mt = MidiUmpMessageType::UtilityMessage32;
	auto ump64mt = MidiUmpMessageType::DataMessage64;
	auto ump96mt = MidiUmpMessageType::FutureReservedB96;
	auto ump128mt = MidiUmpMessageType::FlexData128;


	auto MessageReceivedHandler = [&receivedMessageCount](Windows::Foundation::IInspectable const& sender, MidiMessageReceivedEventArgs const& args)
		{
			REQUIRE((bool)(sender != nullptr));
			REQUIRE((bool)(args != nullptr));

			receivedMessageCount++;

	//		std::cout << " - Received MessageType " << std::hex << (int)(args.Ump().MessageType()) << std::endl;

			// TODO: Verify we have the correct actual packet type




		};

	auto eventRevokeToken = conn1.MessageReceived(MessageReceivedHandler);


	// send messages

	uint32_t numMessagesToSend = 1000;
	std::cout << "Sending messages. Count=" << std::dec << numMessagesToSend << std::endl;

	uint64_t sendingStartTimestamp = MidiClock::GetMidiTimestamp();

	for (int i = 0; i < numMessagesToSend; i++)
	{
		IMidiUmp ump;

		switch (i % 4)
		{
		case 0:
			{
				MidiUmp32 ump32{};
				ump32.MessageType(ump32mt);
				ump = ump32.as<IMidiUmp>();
			}
			break;
		case 1:
			{
				MidiUmp64 ump64{};
				ump64.MessageType(ump64mt);
				ump = ump64.as<IMidiUmp>();
			}
			break;
		case 2:
			{
				MidiUmp96 ump96{};
				ump96.MessageType(ump96mt);
				ump = ump96.as<IMidiUmp>();
			}
			break;
		case 3:
			{
				MidiUmp128 ump128{};
				ump128.MessageType(ump128mt);
				ump = ump128.as<IMidiUmp>();
			}
			break;
		}

		ump.Timestamp(MidiClock::GetMidiTimestamp());
		conn1.SendUmp(ump);
	}
	uint64_t sendingEndTimestamp = MidiClock::GetMidiTimestamp();



	// Wait for incoming message

	uint32_t timeoutCounter = 1000000;
	uint32_t numSleepCalls = 0;
	uint32_t sleepDuration = 0;

	while (receivedMessageCount < numMessagesToSend && timeoutCounter > 0)
	{
		Sleep(sleepDuration);

		timeoutCounter--;
		numSleepCalls++;
	}

	uint64_t endingTimestamp = MidiClock::GetMidiTimestamp();

	REQUIRE(receivedMessageCount == numMessagesToSend);


	uint64_t sendDurationDelta = endingTimestamp - sendingStartTimestamp;
	uint64_t setupDurationDelta = sendingStartTimestamp - setupStartTimestamp;

	uint64_t freq = MidiClock::GetMidiTimestampFrequency();

//	std::cout << " - timeoutCounter " << std::dec << timeoutCounter << std::endl;

	std::cout << "Timestamp Frequency:       " << std::dec << freq << " hz (ticks/second)" << std::endl;
	std::cout << "Setup Start Timestamp:     " << std::dec << setupStartTimestamp << std::endl;
	std::cout << "Setup/Connection Delta:    " << std::dec << setupDurationDelta << " ticks" << std::endl;
	std::cout << "Sending Start timestamp:   " << std::dec << sendingStartTimestamp << std::endl;
	std::cout << "Sending End timestamp:     " << std::dec << endingTimestamp << std::endl;
	std::cout << "Sending/Receiving Delta:   " << std::dec << sendDurationDelta << " ticks" << std::endl;
	std::cout << "Num Wait loop Sleep Calls: " << std::dec << numSleepCalls << std::endl;


	double sendMilliseconds = sendDurationDelta / (double)freq * 1000.0;
	double sendMicroseconds = sendMilliseconds * 1000;

	double setupMilliseconds = setupDurationDelta / (double)freq * 1000.0;
	double setupMicroseconds = setupMilliseconds * 1000;


	std::cout << "Time setup and connect: " << std::dec << std::fixed << setupMilliseconds << " milliseconds (" << setupMicroseconds << " microseconds)." << std::endl;
	std::cout << "Time to send/receive, including wait loop: " << std::dec << numMessagesToSend << " messages : " << std::dec << std::fixed << sendMilliseconds << " milliseconds (" << sendMicroseconds << " microseconds)." << std::endl;
	std::cout << "Average single send/receive: " << std::dec << std::fixed << sendMilliseconds / (double)numMessagesToSend << " milliseconds (" << sendMicroseconds / (double)numMessagesToSend << " microseconds)." << std::endl;


	// unwire event
	conn1.MessageReceived(eventRevokeToken);

	// cleanup endpoint. Technically not required as session will do it
	session.DisconnectEndpointConnection(conn1.Id());
}

