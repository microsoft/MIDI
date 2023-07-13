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

	auto conn1 = session.ConnectBidirectionalEndpoint(BIDI_ENDPOINT_DEVICE_ID, L"", nullptr);

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

TEST_CASE("Connected.Endpoint.MultipleUmps Send and receive mixed multiple messages")
{
	uint64_t setupStartTimestamp = MidiClock::GetMidiTimestamp();


	auto settings = MidiSessionSettings::Default();
	auto session = MidiSession::CreateSession(L"Test Session Name", settings);

	REQUIRE((bool)(session.IsOpen()));
	REQUIRE((bool)(session.Connections().Size() == 0));

	std::cout << "Connecting to Endpoint" << std::endl;

	auto conn1 = session.ConnectBidirectionalEndpoint(BIDI_ENDPOINT_DEVICE_ID, L"", nullptr);

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

	//std::cout << "Sending messages. Count=" << std::dec << numMessagesToSend << std::endl;

	uint32_t numBytes = 0;

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
			numBytes += sizeof(uint32_t) + sizeof(uint64_t);
		}
		break;
		case 1:
		{
			MidiUmp64 ump64{};
			ump64.MessageType(ump64mt);
			ump = ump64.as<IMidiUmp>();
			numBytes += sizeof(uint32_t) * 2 + sizeof(uint64_t);
		}
		break;
		case 2:
		{
			MidiUmp96 ump96{};
			ump96.MessageType(ump96mt);
			ump = ump96.as<IMidiUmp>();
			numBytes += sizeof(uint32_t) * 3 + sizeof(uint64_t);
		}
		break;
		case 3:
		{
			MidiUmp128 ump128{};
			ump128.MessageType(ump128mt);
			ump = ump128.as<IMidiUmp>();
			numBytes += sizeof(uint32_t) * 4 + sizeof(uint64_t);
		}
		break;
		}

		ump.Timestamp(MidiClock::GetMidiTimestamp());
		conn1.SendUmp(ump);
	}

	uint64_t sendingFinishTimestamp = MidiClock::GetMidiTimestamp();


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


	uint64_t sendOnlyDurationDelta = sendingFinishTimestamp - sendingStartTimestamp;
	uint64_t sendReceiveDurationDelta = endingTimestamp - sendingStartTimestamp;
	uint64_t setupDurationDelta = sendingStartTimestamp - setupStartTimestamp;

	uint64_t freq = MidiClock::GetMidiTimestampFrequency();

	//	std::cout << " - timeoutCounter " << std::dec << timeoutCounter << std::endl;

	std::cout << "Num Messages:                " << std::dec << numMessagesToSend << std::endl;
	std::cout << "Num Bytes (inc timestamp):   " << std::dec << numBytes << std::endl;
	std::cout << "Timestamp Frequency:         " << std::dec << freq << " hz (ticks/second)" << std::endl;
	std::cout << "-----------------------------" << std::endl;
	std::cout << "Setup Start Timestamp:       " << std::dec << setupStartTimestamp << std::endl;
	std::cout << "Setup/Connection Delta:      " << std::dec << setupDurationDelta << " ticks" << std::endl;
	std::cout << "Sending Start timestamp:     " << std::dec << sendingStartTimestamp << std::endl;
	std::cout << "Sending Stop timestamp:      " << std::dec << sendingFinishTimestamp << std::endl;
	std::cout << "Send/Rec End timestamp:      " << std::dec << endingTimestamp << std::endl;
	std::cout << "Sending/Receiving Delta:     " << std::dec << sendReceiveDurationDelta << " ticks" << std::endl;
	std::cout << "Num Wait loop Sleep Calls:   " << std::dec << numSleepCalls << std::endl;


	double sendOnlySeconds = sendOnlyDurationDelta / (double)freq;
	double sendOnlyMilliseconds = sendOnlySeconds * 1000.0;
	double sendOnlyMicroseconds = sendOnlyMilliseconds * 1000;
	double sendOnlyAverageMilliseconds = sendOnlyMilliseconds / (double)numMessagesToSend;
	double sendOnlyAverageMicroseconds = sendOnlyAverageMilliseconds * 1000;


	double sendReceiveSeconds = sendReceiveDurationDelta / (double)freq;
	double sendReceiveMilliseconds = sendReceiveSeconds * 1000.0;
	double sendReceiveMicroseconds = sendReceiveMilliseconds * 1000;
	double sendReceiveAverageMilliseconds = sendReceiveMilliseconds / (double)numMessagesToSend;
	double sendReceiveAverageMicroseconds = sendReceiveAverageMilliseconds * 1000;

	double setupSeconds = setupDurationDelta / (double)freq;
	double setupMilliseconds = setupSeconds * 1000.0;
	double setupMicroseconds = setupMilliseconds * 1000;


	std::cout << std::endl;
	std::cout << "Setup and connect:           " << std::dec << std::fixed << setupMilliseconds << "ms (" << setupSeconds << " seconds, " << setupMicroseconds << " microseconds)." << std::endl;
	std::cout << std::endl;
	std::cout << "Send only:                   " << std::dec << std::fixed << sendOnlyMilliseconds << "ms (" << sendOnlySeconds << " seconds, " << sendOnlyMicroseconds << " microseconds)." << std::endl;
	std::cout << "Average single send          " << std::dec << std::fixed << sendOnlyAverageMilliseconds << "ms (" << sendOnlyAverageMicroseconds << " microseconds)." << std::endl;
	std::cout << std::endl;
	std::cout << "Send/receive total:          " << std::dec << std::fixed << sendReceiveMilliseconds << "ms (" << sendReceiveSeconds << " seconds, " << sendReceiveMicroseconds << " microseconds)." << std::endl;
	std::cout << "Average single send/receive: " << std::dec << std::fixed << sendReceiveAverageMilliseconds << "ms (" << sendReceiveAverageMicroseconds << " microseconds)." << std::endl;


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

	auto conn1 = session.ConnectBidirectionalEndpoint(BIDI_ENDPOINT_DEVICE_ID, L"", nullptr);

	REQUIRE((bool)(conn1 != nullptr));

	uint32_t receivedMessageCount{};


	auto MessageReceivedHandler = [&receivedMessageCount](Windows::Foundation::IInspectable const& sender, MidiMessageReceivedEventArgs const& args)
		{
			//REQUIRE((bool)(sender != nullptr));
			//REQUIRE((bool)(args != nullptr));

			receivedMessageCount++;

			//		std::cout << " - Received MessageType " << std::hex << (int)(args.Ump().MessageType()) << std::endl;

					// TODO: Verify we have the correct actual packet type




		};

	auto eventRevokeToken = conn1.MessageReceived(MessageReceivedHandler);


	// send messages

	uint32_t numMessagesToSend = 1000;

	//std::cout << "Sending messages. Count=" << std::dec << numMessagesToSend << std::endl;

	uint32_t numBytes = 0;

	uint64_t sendingStartTimestamp = MidiClock::GetMidiTimestamp();



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
		conn1.SendWords(timestamp, words, wordCount);

	}


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

	std::cout << "Num Messages:              " << std::dec << numMessagesToSend << std::endl;
	std::cout << "Num Bytes (inc timestamp): " << std::dec << numBytes << std::endl;
	std::cout << "Timestamp Frequency:       " << std::dec << freq << " hz (ticks/second)" << std::endl;
	std::cout << "---------------------------" << std::endl;
	std::cout << "Setup Start Timestamp:     " << std::dec << setupStartTimestamp << std::endl;
	std::cout << "Setup/Connection Delta:    " << std::dec << setupDurationDelta << " ticks" << std::endl;
	std::cout << "Sending Start timestamp:   " << std::dec << sendingStartTimestamp << std::endl;
	std::cout << "Sending End timestamp:     " << std::dec << endingTimestamp << std::endl;
	std::cout << "Sending/Receiving Delta:   " << std::dec << sendDurationDelta << " ticks" << std::endl;
	std::cout << "Num Wait loop Sleep Calls: " << std::dec << numSleepCalls << std::endl;


	double sendSeconds = sendDurationDelta / (double)freq;
	double sendMilliseconds = sendSeconds * 1000.0;
	double sendMicroseconds = sendMilliseconds * 1000;

	double setupSeconds = setupDurationDelta / (double)freq;
	double setupMilliseconds = setupSeconds * 1000.0;
	double setupMicroseconds = setupMilliseconds * 1000;


	std::cout << "Setup and connect:           " << std::dec << std::fixed << setupMilliseconds << "ms (" << setupSeconds << " seconds, " << setupMicroseconds << " microseconds)." << std::endl;
	std::cout << "Send/receive, inc wait loop: " << std::dec << std::fixed << sendMilliseconds << "ms (" << sendSeconds << " seconds, " << sendMicroseconds << " microseconds)." << std::endl;
	std::cout << "Average single send/receive: " << std::dec << std::fixed << sendMilliseconds / (double)numMessagesToSend << "ms (" << sendMicroseconds / (double)numMessagesToSend << " microseconds)." << std::endl;


	// unwire event
	conn1.MessageReceived(eventRevokeToken);

	// cleanup endpoint. Technically not required as session will do it
	session.DisconnectEndpointConnection(conn1.Id());
}
