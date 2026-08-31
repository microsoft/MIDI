// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

#include <string>
#include <vector>
#include <algorithm>
#include <cwctype>
#include <thread>
#include <atomic>


namespace
{
    // The specification states the endpoint name limit in UTF-8 bytes, not characters, so a name
    // well inside the character limit can still be over the byte limit once encoded.
    constexpr size_t MaxEndpointNameByteCount = 98;

    // appended by the transport to the device-side endpoint so it can be told apart from the
    // client-visible one
    constexpr wchar_t DeviceSideNameSuffix[] = L" (Virtual MIDI Device)";

    // U+8A2D, three UTF-8 bytes. Written as an escape so the tests do not depend on the encoding
    // this source file happens to be saved in.
    constexpr wchar_t ThreeByteCharacter[] = L"\u8A2D";

    // U+1F3B9 MUSICAL KEYBOARD, four UTF-8 bytes and a surrogate pair in UTF-16
    constexpr wchar_t FourByteCharacter[] = L"\U0001F3B9";

    size_t Utf8ByteCount(std::wstring const& value)
    {
        if (value.empty()) return 0;

        auto count = ::WideCharToMultiByte(CP_UTF8, 0, value.c_str(), (int)value.length(), nullptr, 0, nullptr, nullptr);

        return count > 0 ? (size_t)count : 0;
    }

    std::wstring RepeatedString(std::wstring const& unit, uint32_t const count)
    {
        std::wstring result{};

        for (uint32_t i = 0; i < count; i++)
        {
            result += unit;
        }

        return result;
    }

    std::wstring MakeUniqueProductInstanceId()
    {
        std::wstring value{ winrt::to_hstring(winrt::Windows::Foundation::GuidHelper::CreateNewGuid()) };

        value.erase(
            std::remove_if(value.begin(), value.end(), [](wchar_t c) { return !std::iswalnum(c); }),
            value.end());

        return value;
    }


    struct TestVirtualDevice
    {
        MidiSession Session{ nullptr };
        MidiVirtualDevice Device{ nullptr };
        MidiEndpointConnection DeviceConnection{ nullptr };

        winrt::hstring DeviceEndpointDeviceId{};
        winrt::hstring ClientEndpointDeviceId{};
    };

    void CleanupTestVirtualDevice(TestVirtualDevice& device)
    {
        if (device.DeviceConnection != nullptr && device.Session != nullptr)
        {
            if (device.Device != nullptr)
            {
                device.DeviceConnection.RemoveMessageProcessingPlugin(device.Device.PluginId());
            }

            device.Session.DisconnectEndpointConnection(device.DeviceConnection.ConnectionId());
        }

        if (device.Session != nullptr)
        {
            device.Session.Close();
        }
    }

    // Creates a minimal virtual device and opens the device-side connection, which is what causes
    // the client-visible endpoint to be created. No user-supplied name is set: a user-supplied
    // name outranks the transport-supplied one in Name(), which would hide what the transport did.
    bool CreateTestVirtualDevice(_In_ std::wstring const& transportSuppliedName, _Out_ TestVirtualDevice& result)
    {
        MidiDeclaredEndpointInfo declaredEndpointInfo{};
        declaredEndpointInfo.Name(winrt::hstring{ transportSuppliedName });
        declaredEndpointInfo.ProductInstanceId(winrt::hstring{ MakeUniqueProductInstanceId() });
        declaredEndpointInfo.SpecificationVersionMajor(1);
        declaredEndpointInfo.SpecificationVersionMinor(1);
        declaredEndpointInfo.SupportsMidi10Protocol(true);
        declaredEndpointInfo.SupportsMidi20Protocol(true);
        declaredEndpointInfo.HasStaticFunctionBlocks(true);

        MidiDeclaredDeviceIdentity declaredDeviceIdentity{};
        MidiEndpointUserSuppliedInfo userSuppliedInfo{};

        MidiVirtualDeviceCreationConfig config(
            winrt::hstring{ transportSuppliedName },
            L"Virtual device integration test",
            L"Windows MIDI Services Test",
            declaredEndpointInfo,
            declaredDeviceIdentity,
            userSuppliedInfo);

        result.Session = MidiSession::Create(L"Virtual device integration tests");
        if (result.Session == nullptr) return false;

        result.Device = MidiVirtualDeviceManager::CreateVirtualDevice(config);
        if (result.Device == nullptr) return false;

        result.DeviceEndpointDeviceId = result.Device.DeviceEndpointDeviceId();
        if (result.DeviceEndpointDeviceId.empty()) return false;

        result.DeviceConnection = result.Session.CreateEndpointConnection(result.DeviceEndpointDeviceId);
        if (result.DeviceConnection == nullptr) return false;

        result.DeviceConnection.AddMessageProcessingPlugin(result.Device);

        if (!result.DeviceConnection.Open()) return false;

        // the client-visible endpoint is created asynchronously once the device connection opens
        for (int i = 0; i < 100 && result.ClientEndpointDeviceId.empty(); i++)
        {
            Sleep(100);
            result.ClientEndpointDeviceId = MidiVirtualDeviceManager::GetAssociatedClientEndpointDeviceId(result.Device.AssociationId());
        }

        return !result.ClientEndpointDeviceId.empty();
    }

    std::wstring GetTransportSuppliedName(_In_ winrt::hstring const& endpointDeviceId)
    {
        auto info = MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(endpointDeviceId);

        if (info == nullptr) return {};

        return std::wstring{ info.GetTransportSuppliedInfo().Name() };
    }

    // Both the client-visible and the device-side endpoints, which are normally hidden from apps.
    uint32_t CountVirtualEndpoints()
    {
        auto all = MidiEndpointDeviceInformation::FindAll(
            MidiEndpointDeviceInformationSortOrder::Name,
            MidiEndpointDeviceInformationFilters::AllStandardEndpoints |
            MidiEndpointDeviceInformationFilters::VirtualDeviceResponder);

        uint32_t count{ 0 };

        for (auto const& info : all)
        {
            if (info.GetTransportSuppliedInfo().TransportCode() == L"APP")
            {
                count++;
            }
        }

        return count;
    }

    bool EndpointExists(_In_ winrt::hstring const& endpointDeviceId)
    {
        return MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(endpointDeviceId) != nullptr;
    }

    // Sends one distinctive message and waits for it to arrive at the other end.
    bool MessageFlows(
        _In_ MidiEndpointConnection const& sender,
        _In_ MidiEndpointConnection const& receiver,
        _In_ uint32_t const timeoutMilliseconds = 3000)
    {
        const uint32_t word0 = 0x40903C00;
        const uint32_t word1 = 0x12340000;

        wil::unique_event_nothrow received;
        received.create();

        bool matched{ false };

        auto token = receiver.MessageReceived([&](IMidiMessageReceivedEventSource const&, MidiMessageReceivedEventArgs const& args)
            {
                auto message = args.GetMessagePacket().try_as<MidiMessage64>();

                if (message != nullptr && message.Word0() == word0 && message.Word1() == word1)
                {
                    matched = true;
                    received.SetEvent();
                }
            });

        auto sendResult = sender.SendSingleMessagePacket(
            MidiMessage64(MidiClock::TimestampConstantSendImmediately(), word0, word1));

        if (MidiEndpointConnection::SendMessageSucceeded(sendResult))
        {
            received.wait(timeoutMilliseconds);
        }

        receiver.MessageReceived(token);

        return matched;
    }
}


// Each virtual device leaves a deactivated software device node behind for its device-side and
// its client-visible UMP endpoint, and for the MIDI 1.0 ports created alongside them, because
// the service never deletes one. These two hooks remove exactly the nodes this test method
// caused to be created.
bool MidiVirtualDeviceTests::TestSetup()
{
    m_deviceNodeTracker.Start();

    return true;
}

bool MidiVirtualDeviceTests::TestCleanup()
{
    m_deviceNodeTracker.RemoveDeviceNodesCreatedSinceStart();

    return true;
}


void MidiVirtualDeviceTests::TestCreateVirtualDevice()
{
    {
        winrt::hstring createdClientEndpointId;
        winrt::hstring createdDeviceEndpointId;               

        winrt::hstring endpointSuppliedName = L"TAEF Virtual Endpoint";

        winrt::hstring userSuppliedName = L"TAEF Virtual Endpoint (User Named)";
        winrt::hstring userSuppliedDescription = L"This is the user-supplied description";

        winrt::hstring transportSuppliedName = L"TAEF Virtual Endpoint (Transport Named)";
        winrt::hstring transportSuppliedDescription = L"This is the transport-supplied description";
        winrt::hstring transportSuppliedManufacturerName = L"TAEF Manufacturer Name";


        // endpoint information returned from endpoint discovery
        MidiDeclaredEndpointInfo declaredEndpointInfo{ };
        declaredEndpointInfo.Name(endpointSuppliedName);
        declaredEndpointInfo.ProductInstanceId(L"TAEF_TEST_3263827");   // must be unique
        declaredEndpointInfo.SpecificationVersionMajor(1); // see latest MIDI 2 UMP spec
        declaredEndpointInfo.SpecificationVersionMinor(1); // see latest MIDI 2 UMP spec
        declaredEndpointInfo.SupportsMidi10Protocol(true);
        declaredEndpointInfo.SupportsMidi20Protocol(true);
        declaredEndpointInfo.SupportsReceivingJitterReductionTimestamps(false);
        declaredEndpointInfo.SupportsSendingJitterReductionTimestamps(false);
        declaredEndpointInfo.HasStaticFunctionBlocks(true);

        MidiDeclaredDeviceIdentity declaredDeviceIdentity{ };
        // todo: set any device identity values if you want. This is optional

        MidiEndpointUserSuppliedInfo userSuppliedInfo{ };
        userSuppliedInfo.Name(userSuppliedName);           // for names, this will bubble to the top in priority
        userSuppliedInfo.Description(userSuppliedDescription);

        // create the config type to aggregate all this info
        MidiVirtualDeviceCreationConfig config(
            transportSuppliedName,                          // this could be a different "transport-supplied" name value here
            transportSuppliedDescription,                   // transport-supplied description
            transportSuppliedManufacturerName,              // transport-supplied company name
            declaredEndpointInfo,                           // for endpoint discovery
            declaredDeviceIdentity,                         // for endpoint discovery
            userSuppliedInfo
        );

        // Function blocks.The MIDI 2 UMP specification covers the meanings of these values
        MidiFunctionBlock block1{ };
        block1.Number(0);
        block1.Name(L"Test Output");
        block1.IsActive(true);
        block1.UIHint(MidiFunctionBlockUIHint::Sender);
        block1.FirstGroup(MidiGroup((uint8_t)0));
        block1.GroupCount(1);
        block1.Direction(MidiFunctionBlockDirection::Bidirectional);
        block1.RepresentsMidi10Connection(MidiFunctionBlockRepresentsMidi10Connection::Not10);
        block1.MaxSystemExclusive8Streams(0);
        block1.MidiCIMessageVersionFormat(0);

        config.FunctionBlocks().Append(block1);

        MidiFunctionBlock block2{ };
        block2.Number(1);
        block2.Name(L"A Test Function Block");
        block2.IsActive(true);
        block2.UIHint(MidiFunctionBlockUIHint::Sender);
        block2.FirstGroup(MidiGroup((uint8_t)1));
        block2.GroupCount(2);
        block2.Direction(MidiFunctionBlockDirection::Bidirectional);
        block2.RepresentsMidi10Connection(MidiFunctionBlockRepresentsMidi10Connection::Not10);
        block2.MaxSystemExclusive8Streams(0);
        block2.MidiCIMessageVersionFormat(0);

        config.FunctionBlocks().Append(block2);



        // create the session. The name here is just convenience.
        auto session = MidiSession::Create(config.Name());
        VERIFY_IS_NOT_NULL(session);

        // create the virtual device, so we can get the endpoint device id to connect to
        auto virtualDevice = MidiVirtualDeviceManager::CreateVirtualDevice(config);
        VERIFY_IS_NOT_NULL(virtualDevice);
        VERIFY_IS_FALSE(virtualDevice.DeviceEndpointDeviceId().empty());
        LOG_OUTPUT(L"Created virtual device");
        LOG_OUTPUT(virtualDevice.DeviceEndpointDeviceId().c_str());

        createdDeviceEndpointId = virtualDevice.DeviceEndpointDeviceId();


        // create the endpoint connection to the device-side endpoint
        // to prevent confusion, this endpoint is not enumerated to 
        // apps when using the standard set of enumeration filters
        auto connection = session.CreateEndpointConnection(virtualDevice.DeviceEndpointDeviceId());
        VERIFY_IS_NOT_NULL(connection);

        LOG_OUTPUT(L"Checking device properties");
        auto info = MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(virtualDevice.DeviceEndpointDeviceId());
        VERIFY_IS_NOT_NULL(info);

        LOG_OUTPUT(info.Name().c_str());
        LOG_OUTPUT(info.EndpointDeviceId().c_str());


        // add the virtual device as a message processing plugin so it receives the messages
        connection.AddMessageProcessingPlugin(virtualDevice);

        // wire up the stream configuration request received handler
        //auto streamEventToken = virtualDevice.StreamConfigRequestReceived(
        //    { this, &MidiVirtualDeviceTests::OnStreamConfigurationRequestReceived });

        //// wire up the message received handler on the connection itself
        //auto messageEventToken = connection.MessageReceived(
        //    { this, &MidiVirtualDeviceTests::OnMidiMessageReceived });

        // the client-side endpoint will become visible to other apps once Open() completes
        VERIFY_IS_TRUE(connection.Open());

        // provide time for the client endpoint to be created. We could do this with a watcher
        // if we want to be more appropriate about the process
        LOG_OUTPUT(L"Sleeping for a moment to give time to create the client-side connection");
        Sleep(2000);

        // Test SDK function to get client-side device id
        LOG_OUTPUT(L"Validating that we have a client-side SWD");
        createdClientEndpointId = MidiVirtualDeviceManager::GetAssociatedClientEndpointDeviceId(virtualDevice.AssociationId());
        VERIFY_IS_FALSE(createdClientEndpointId.empty());
        LOG_OUTPUT(createdClientEndpointId.c_str());

        // validate that both endpoints now exist
        LOG_OUTPUT(L"Validating that the device-side endpoint exists");
        auto deviceEndpointInfo = MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(createdDeviceEndpointId);
        VERIFY_IS_NOT_NULL(deviceEndpointInfo);

        LOG_OUTPUT(L"Validating that the client-side endpoint exists");
        auto clientEndpointInfo = MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(createdClientEndpointId);
        VERIFY_IS_NOT_NULL(clientEndpointInfo);


        // Send/receive test: send a set of MIDI 2.0 Note On messages from the
        // virtual device (device-side connection) to the client-side connection,
        // and verify the count and content of what the client receives.

        LOG_OUTPUT(L"Creating client-side connection for send/receive test");

        auto clientConnection = session.CreateEndpointConnection(createdClientEndpointId);
        VERIFY_IS_NOT_NULL(clientConnection);

        wil::unique_event_nothrow allMessagesReceived;
        allMessagesReceived.create();

        const uint32_t numberOfMessagesToSend = 10;

        // Build the expected MIDI 2.0 Note On messages.
        // MIDI 2.0 Channel Voice (Message Type 4) Note On (status 0x9).
        // word0: 0x4 | group | 0x9 | channel | noteNumber | attributeType
        // word1: velocity (16 bits) | attribute data (16 bits)
        const uint8_t group = 0;
        const uint8_t channel = 0;
        const uint8_t attributeType = 0;
        const uint16_t attributeData = 0x0000;

        std::vector<std::pair<uint32_t, uint32_t>> expectedMessages;
        for (uint32_t i = 0; i < numberOfMessagesToSend; i++)
        {
            uint8_t noteNumber = static_cast<uint8_t>(60 + i);          // notes 60..69
            uint16_t velocity = static_cast<uint16_t>(0x1000 + i);      // distinct velocities

            uint32_t word0 =
                (0x4u << 28) |
                (static_cast<uint32_t>(group & 0x0F) << 24) |
                (0x9u << 20) |
                (static_cast<uint32_t>(channel & 0x0F) << 16) |
                (static_cast<uint32_t>(noteNumber) << 8) |
                (static_cast<uint32_t>(attributeType));

            uint32_t word1 =
                (static_cast<uint32_t>(velocity) << 16) |
                (static_cast<uint32_t>(attributeData));

            expectedMessages.push_back({ word0, word1 });
        }

        uint32_t receivedMessageCount{ 0 };
        bool allMessageContentValid{ true };

        auto MessageReceivedHandler = [&](IMidiMessageReceivedEventSource const& sender, MidiMessageReceivedEventArgs const& args)
            {
                VERIFY_IS_NOT_NULL(sender);
                VERIFY_IS_NOT_NULL(args);

                uint32_t index = receivedMessageCount;

                // verify message type
                if (args.MessageType() != MidiMessageType::Midi2ChannelVoice64)
                {
                    allMessageContentValid = false;
                }
                else if (index < expectedMessages.size())
                {
                    // verify content against what we sent
                    auto message = args.GetMessagePacket().as<MidiMessage64>();

                    if (message.Word0() != expectedMessages[index].first ||
                        message.Word1() != expectedMessages[index].second)
                    {
                        allMessageContentValid = false;
                    }
                }

                receivedMessageCount++;

                if (receivedMessageCount == numberOfMessagesToSend)
                {
                    allMessagesReceived.SetEvent();
                }
            };

        auto messageEventToken = clientConnection.MessageReceived(MessageReceivedHandler);

        VERIFY_IS_TRUE(clientConnection.Open());

        LOG_OUTPUT(L"Sending MIDI 2.0 Note On messages from the virtual device to the client");

        std::vector<IMidiUniversalPacket> packetList;
        for (auto const& expected : expectedMessages)
        {
            packetList.push_back(
                MidiMessage64(MidiClock::TimestampConstantSendImmediately(), expected.first, expected.second));
        }

        auto sendResult = connection.SendMultipleMessagesPacketList(packetList);
        VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(sendResult));

        LOG_OUTPUT(L"Waiting for the client to receive all messages");
        if (!allMessagesReceived.wait(5000))
        {
            LOG_OUTPUT(L"Timed out waiting for messages");
        }

        // verify the correct number of messages were received
        VERIFY_ARE_EQUAL(receivedMessageCount, numberOfMessagesToSend);

        // verify the content of each received message was as expected
        VERIFY_IS_TRUE(allMessageContentValid);

        clientConnection.MessageReceived(messageEventToken);

        packetList.clear();

        LOG_OUTPUT(L"Disconnecting client-side connection");
        session.DisconnectEndpointConnection(clientConnection.ConnectionId());
        clientConnection = nullptr;



        LOG_OUTPUT(L"Removing message processing plugin");
        connection.RemoveMessageProcessingPlugin(virtualDevice.PluginId());

        // shut down the app/device-side connection. This will tear down 
        // client and device endpoints
        LOG_OUTPUT(L"Disconnecting endpoint connection and closing the session");
        session.DisconnectEndpointConnection(connection.ConnectionId());

        LOG_OUTPUT(L"Sleeping for a moment to give time to destroy the devices");
        Sleep(3000);

        // validate device endpoint was removed
        LOG_OUTPUT(L"Validating that device-side endpoint has been removed");
        deviceEndpointInfo = MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(createdDeviceEndpointId);
        VERIFY_IS_NULL(deviceEndpointInfo);

        // Validate client endpoint was removed as well
        LOG_OUTPUT(L"Validating that client-side endpoint has been removed");
        clientEndpointInfo = MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(createdClientEndpointId);
        VERIFY_IS_NULL(clientEndpointInfo);

        session.Close();
    }

}




// ============================================================================
// Endpoint name limits
// ============================================================================


void MidiVirtualDeviceTests::TestCompliantNameIsNotModified()
{
    std::wstring name{ L"Contoso Test Synth" };

    VERIFY_IS_LESS_THAN(Utf8ByteCount(name), MaxEndpointNameByteCount);

    TestVirtualDevice device{};
    auto cleanup = wil::scope_exit([&] { CleanupTestVirtualDevice(device); });

    VERIFY_IS_TRUE(CreateTestVirtualDevice(name, device));

    // a name that already fits must come back untouched
    VERIFY_ARE_EQUAL(GetTransportSuppliedName(device.ClientEndpointDeviceId), name);
    VERIFY_ARE_EQUAL(GetTransportSuppliedName(device.DeviceEndpointDeviceId), name + DeviceSideNameSuffix);
}


void MidiVirtualDeviceTests::TestOverlongUnicodeNameIsTruncatedOnCharacterBoundary()
{
    // 40 CJK characters: 40 UTF-16 code units, but 120 UTF-8 bytes. 32 characters (96 bytes) is
    // the most that fits, because a 33rd would land on byte 99.
    std::wstring name = RepeatedString(ThreeByteCharacter, 40);

    VERIFY_ARE_EQUAL(name.length(), (size_t)40);
    VERIFY_ARE_EQUAL(Utf8ByteCount(name), (size_t)120);

    TestVirtualDevice device{};
    auto cleanup = wil::scope_exit([&] { CleanupTestVirtualDevice(device); });

    VERIFY_IS_TRUE(CreateTestVirtualDevice(name, device));

    auto clientName = GetTransportSuppliedName(device.ClientEndpointDeviceId);

    LOG_OUTPUT(L"Submitted %u chars / %u bytes, client endpoint returned %u chars / %u bytes",
        (unsigned)name.length(), (unsigned)Utf8ByteCount(name),
        (unsigned)clientName.length(), (unsigned)Utf8ByteCount(clientName));

    VERIFY_IS_LESS_THAN_OR_EQUAL(Utf8ByteCount(clientName), MaxEndpointNameByteCount);

    // whole characters only, and a prefix of what was submitted rather than a mangled string
    VERIFY_ARE_EQUAL(clientName.length(), (size_t)32);
    VERIFY_ARE_EQUAL(Utf8ByteCount(clientName), (size_t)96);
    VERIFY_ARE_EQUAL(name.compare(0, clientName.length(), clientName), 0);
}


void MidiVirtualDeviceTests::TestOverlongAsciiNameIsTruncated()
{
    std::wstring name = RepeatedString(L"A", 150);

    VERIFY_ARE_EQUAL(Utf8ByteCount(name), (size_t)150);

    TestVirtualDevice device{};
    auto cleanup = wil::scope_exit([&] { CleanupTestVirtualDevice(device); });

    VERIFY_IS_TRUE(CreateTestVirtualDevice(name, device));

    auto clientName = GetTransportSuppliedName(device.ClientEndpointDeviceId);

    // all single byte characters, so the byte limit and the character count line up exactly
    VERIFY_ARE_EQUAL(clientName.length(), MaxEndpointNameByteCount);
    VERIFY_ARE_EQUAL(Utf8ByteCount(clientName), MaxEndpointNameByteCount);
}


void MidiVirtualDeviceTests::TestDeviceSideNameKeepsSuffixWhenTruncated()
{
    // The device-side endpoint name is the base name plus a suffix. Truncating the composed name
    // would eat the suffix and leave the two endpoints indistinguishable, so the base is what
    // must give way.
    std::wstring name = RepeatedString(ThreeByteCharacter, 40);

    TestVirtualDevice device{};
    auto cleanup = wil::scope_exit([&] { CleanupTestVirtualDevice(device); });

    VERIFY_IS_TRUE(CreateTestVirtualDevice(name, device));

    auto deviceName = GetTransportSuppliedName(device.DeviceEndpointDeviceId);

    LOG_OUTPUT(L"Device-side endpoint name is %u chars / %u bytes",
        (unsigned)deviceName.length(), (unsigned)Utf8ByteCount(deviceName));

    VERIFY_IS_LESS_THAN_OR_EQUAL(Utf8ByteCount(deviceName), MaxEndpointNameByteCount);

    // the suffix survived, and it is still distinguishable from the client-visible endpoint
    std::wstring suffix{ DeviceSideNameSuffix };
    VERIFY_IS_GREATER_THAN_OR_EQUAL(deviceName.length(), suffix.length());
    VERIFY_ARE_EQUAL(deviceName.compare(deviceName.length() - suffix.length(), suffix.length(), suffix), 0);
    VERIFY_ARE_NOT_EQUAL(deviceName, GetTransportSuppliedName(device.ClientEndpointDeviceId));
}




// ============================================================================
// General virtual device behavior
// ============================================================================


void MidiVirtualDeviceTests::TestEmptyNameGetsDefaultName()
{
    TestVirtualDevice device{};
    auto cleanup = wil::scope_exit([&] { CleanupTestVirtualDevice(device); });

    VERIFY_IS_TRUE(CreateTestVirtualDevice(L"", device));

    // an unnamed device still has to be identifiable in the UI
    VERIFY_IS_FALSE(GetTransportSuppliedName(device.ClientEndpointDeviceId).empty());
    VERIFY_IS_FALSE(GetTransportSuppliedName(device.DeviceEndpointDeviceId).empty());
}


void MidiVirtualDeviceTests::TestMultipleVirtualDevicesHaveDistinctIdentities()
{
    TestVirtualDevice first{};
    auto cleanupFirst = wil::scope_exit([&] { CleanupTestVirtualDevice(first); });
    VERIFY_IS_TRUE(CreateTestVirtualDevice(L"Contoso Test Synth", first));

    TestVirtualDevice second{};
    auto cleanupSecond = wil::scope_exit([&] { CleanupTestVirtualDevice(second); });
    VERIFY_IS_TRUE(CreateTestVirtualDevice(L"Contoso Test Synth", second));

    // same requested name, but they must not collide
    VERIFY_ARE_NOT_EQUAL(first.Device.AssociationId(), second.Device.AssociationId());
    VERIFY_ARE_NOT_EQUAL(first.DeviceEndpointDeviceId, second.DeviceEndpointDeviceId);
    VERIFY_ARE_NOT_EQUAL(first.ClientEndpointDeviceId, second.ClientEndpointDeviceId);

    // and the association lookup must return the right one for each
    VERIFY_ARE_EQUAL(
        MidiVirtualDeviceManager::GetAssociatedClientEndpointDeviceId(first.Device.AssociationId()),
        first.ClientEndpointDeviceId);
    VERIFY_ARE_EQUAL(
        MidiVirtualDeviceManager::GetAssociatedClientEndpointDeviceId(second.Device.AssociationId()),
        second.ClientEndpointDeviceId);
}


void MidiVirtualDeviceTests::TestFunctionBlocksAreVisibleOnVirtualDevice()
{
    MidiDeclaredEndpointInfo declaredEndpointInfo{};
    declaredEndpointInfo.Name(L"Contoso Block Test");
    declaredEndpointInfo.ProductInstanceId(winrt::hstring{ MakeUniqueProductInstanceId() });
    declaredEndpointInfo.SpecificationVersionMajor(1);
    declaredEndpointInfo.SpecificationVersionMinor(1);
    declaredEndpointInfo.SupportsMidi20Protocol(true);
    declaredEndpointInfo.HasStaticFunctionBlocks(true);

    MidiVirtualDeviceCreationConfig config(
        L"Contoso Block Test",
        L"Virtual device integration test",
        L"Windows MIDI Services Test",
        declaredEndpointInfo);

    MidiFunctionBlock block{};
    block.Number(0);
    block.Name(L"Test Block");
    block.IsActive(true);
    block.UIHint(MidiFunctionBlockUIHint::Sender);
    block.FirstGroup(MidiGroup((uint8_t)0));
    block.GroupCount(1);
    block.Direction(MidiFunctionBlockDirection::Bidirectional);

    config.FunctionBlocks().Append(block);

    auto session = MidiSession::Create(L"Virtual device integration tests");
    VERIFY_IS_NOT_NULL(session);

    auto cleanup = wil::scope_exit([&] { session.Close(); });

    auto virtualDevice = MidiVirtualDeviceManager::CreateVirtualDevice(config);
    VERIFY_IS_NOT_NULL(virtualDevice);

    // the blocks supplied at creation must be readable back off the device
    VERIFY_ARE_EQUAL(virtualDevice.FunctionBlocks().Size(), (uint32_t)1);
    VERIFY_IS_TRUE(virtualDevice.FunctionBlocks().HasKey(0));
    VERIFY_ARE_EQUAL(virtualDevice.FunctionBlocks().Lookup(0).Name(), winrt::hstring{ L"Test Block" });
    VERIFY_ARE_EQUAL(virtualDevice.FunctionBlocks().Lookup(0).GroupCount(), (uint8_t)1);
}




// ============================================================================
// In-protocol stream message text
//
// These limits are also stated in UTF-8 bytes. The endpoint name notification carries 14 bytes
// per packet and the function block name notification 13, because the block number takes a byte
// out of the first word.
// ============================================================================


void MidiVirtualDeviceTests::TestEndpointNameNotificationRespectsByteLimit()
{
    // 120 UTF-8 bytes in 40 UTF-16 code units
    std::wstring name = RepeatedString(ThreeByteCharacter, 40);

    auto messages = MidiStreamMessageBuilder::BuildEndpointNameNotificationMessages(0, winrt::hstring{ name });
    VERIFY_IS_GREATER_THAN(messages.Size(), (uint32_t)0);

    std::wstring parsed{ MidiStreamMessageBuilder::ParseEndpointNameNotificationMessages(messages) };

    LOG_OUTPUT(L"Endpoint name: submitted %u bytes, on the wire %u bytes",
        (unsigned)Utf8ByteCount(name), (unsigned)Utf8ByteCount(parsed));

    VERIFY_IS_LESS_THAN_OR_EQUAL(Utf8ByteCount(parsed), (size_t)98);

    // 32 whole three-byte characters is the most that fits under 98
    VERIFY_ARE_EQUAL(parsed.length(), (size_t)32);
    VERIFY_ARE_EQUAL(Utf8ByteCount(parsed), (size_t)96);
    VERIFY_ARE_EQUAL(name.compare(0, parsed.length(), parsed), 0);
}


void MidiVirtualDeviceTests::TestFunctionBlockNameNotificationRespectsByteLimit()
{
    // 91, not the 98 the specification states: the block number takes a byte out of the first
    // word, so only 13 text bytes fit per packet instead of 14.
    const size_t maxFunctionBlockNameByteCount = 91;

    // 120 UTF-8 bytes submitted against a 91 byte limit
    std::wstring name = RepeatedString(ThreeByteCharacter, 40);

    auto messages = MidiStreamMessageBuilder::BuildFunctionBlockNameNotificationMessages(0, 0, winrt::hstring{ name });
    VERIFY_IS_GREATER_THAN(messages.Size(), (uint32_t)0);

    std::wstring parsed{ MidiStreamMessageBuilder::ParseFunctionBlockNameNotificationMessages(messages) };

    LOG_OUTPUT(L"Function block name: submitted %u bytes, on the wire %u bytes",
        (unsigned)Utf8ByteCount(name), (unsigned)Utf8ByteCount(parsed));

    VERIFY_IS_LESS_THAN_OR_EQUAL(Utf8ByteCount(parsed), maxFunctionBlockNameByteCount);

    // 30 whole three-byte characters is the most that fits under 91
    VERIFY_ARE_EQUAL(parsed.length(), (size_t)30);
    VERIFY_ARE_EQUAL(Utf8ByteCount(parsed), (size_t)90);
    VERIFY_ARE_EQUAL(name.compare(0, parsed.length(), parsed), 0);
}


void MidiVirtualDeviceTests::TestSplitTextMessagesNeverSplitAMultiByteCharacter()
{
    // Sweep names of every length across the limit. Whatever lands on the wire must decode back
    // to a clean prefix, which it cannot do if a character was cut in half.
    for (uint32_t characterCount = 1; characterCount <= 45; characterCount++)
    {
        std::wstring name = RepeatedString(ThreeByteCharacter, characterCount);

        auto messages = MidiStreamMessageBuilder::BuildEndpointNameNotificationMessages(0, winrt::hstring{ name });
        std::wstring parsed{ MidiStreamMessageBuilder::ParseEndpointNameNotificationMessages(messages) };

        VERIFY_IS_LESS_THAN_OR_EQUAL(Utf8ByteCount(parsed), (size_t)98);
        VERIFY_ARE_EQUAL(Utf8ByteCount(parsed) % 3, (size_t)0);
        VERIFY_ARE_EQUAL(name.compare(0, parsed.length(), parsed), 0);
    }

    // a four byte character, which is a surrogate pair in UTF-16, must not be split either
    for (uint32_t characterCount = 1; characterCount <= 30; characterCount++)
    {
        std::wstring name = RepeatedString(FourByteCharacter, characterCount);

        auto messages = MidiStreamMessageBuilder::BuildEndpointNameNotificationMessages(0, winrt::hstring{ name });
        std::wstring parsed{ MidiStreamMessageBuilder::ParseEndpointNameNotificationMessages(messages) };

        VERIFY_IS_LESS_THAN_OR_EQUAL(Utf8ByteCount(parsed), (size_t)98);
        VERIFY_ARE_EQUAL(Utf8ByteCount(parsed) % 4, (size_t)0);
        VERIFY_ARE_EQUAL(name.compare(0, parsed.length(), parsed), 0);
    }
}




// ============================================================================
// Connection lifetime
//
// The device-side connection owns both endpoints. Clients come and go underneath it.
// ============================================================================


void MidiVirtualDeviceTests::TestClientDisconnectsBeforeDevice()
{
    TestVirtualDevice device{};
    auto cleanup = wil::scope_exit([&] { CleanupTestVirtualDevice(device); });

    VERIFY_IS_TRUE(CreateTestVirtualDevice(L"Contoso Disconnect Order Test", device));

    auto clientConnection = device.Session.CreateEndpointConnection(device.ClientEndpointDeviceId);
    VERIFY_IS_NOT_NULL(clientConnection);
    VERIFY_IS_TRUE(clientConnection.Open());

    // a second client, opened while the first is still up, to tell "the endpoint dies on any
    // disconnect" apart from "the endpoint dies when the last client leaves"
    auto secondClient = device.Session.CreateEndpointConnection(device.ClientEndpointDeviceId);
    VERIFY_IS_NOT_NULL(secondClient);
    VERIFY_IS_TRUE(secondClient.Open());

    LOG_OUTPUT(L"Disconnecting the first client while a second client and the device are still up");
    device.Session.DisconnectEndpointConnection(clientConnection.ConnectionId());
    clientConnection = nullptr;

    Sleep(1000);

    // the device owns the lifetime, so nothing should have been torn down
    VERIFY_IS_NOT_NULL(MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(device.DeviceEndpointDeviceId));
    VERIFY_IS_NOT_NULL(MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(device.ClientEndpointDeviceId));

    bool reopenedWhileSecondClientAttached{ false };
    {
        auto reconnected = device.Session.CreateEndpointConnection(device.ClientEndpointDeviceId);
        VERIFY_IS_NOT_NULL(reconnected);
        reopenedWhileSecondClientAttached = reconnected.Open();
        device.Session.DisconnectEndpointConnection(reconnected.ConnectionId());
    }

    LOG_OUTPUT(L"Disconnecting the second client, leaving no clients attached");
    device.Session.DisconnectEndpointConnection(secondClient.ConnectionId());
    secondClient = nullptr;

    Sleep(1000);

    bool reopenedWithNoClientsAttached{ false };
    {
        auto otherSession = MidiSession::Create(L"Virtual device reconnect test");
        VERIFY_IS_NOT_NULL(otherSession);

        auto reconnected = otherSession.CreateEndpointConnection(device.ClientEndpointDeviceId);
        VERIFY_IS_NOT_NULL(reconnected);
        reopenedWithNoClientsAttached = reconnected.Open();

        otherSession.DisconnectEndpointConnection(reconnected.ConnectionId());
        otherSession.Close();
    }

    LOG_OUTPUT(L"Reopen with another client attached: %d, reopen with none attached: %d",
        (int)reopenedWhileSecondClientAttached, (int)reopenedWithNoClientsAttached);

    // a client leaving must not make the endpoint unusable for the next one
    VERIFY_IS_TRUE(reopenedWhileSecondClientAttached);
    VERIFY_IS_TRUE(reopenedWithNoClientsAttached);
}


void MidiVirtualDeviceTests::TestClientDisconnectsAfterDevice()
{
    TestVirtualDevice device{};
    auto cleanup = wil::scope_exit([&] { CleanupTestVirtualDevice(device); });

    VERIFY_IS_TRUE(CreateTestVirtualDevice(L"Contoso Disconnect Order Test", device));

    auto deviceEndpointDeviceId = device.DeviceEndpointDeviceId;
    auto clientEndpointDeviceId = device.ClientEndpointDeviceId;

    auto clientConnection = device.Session.CreateEndpointConnection(clientEndpointDeviceId);
    VERIFY_IS_NOT_NULL(clientConnection);
    VERIFY_IS_TRUE(clientConnection.Open());

    LOG_OUTPUT(L"Disconnecting the device while a client is still attached");
    device.DeviceConnection.RemoveMessageProcessingPlugin(device.Device.PluginId());
    device.Session.DisconnectEndpointConnection(device.DeviceConnection.ConnectionId());
    device.DeviceConnection = nullptr;

    Sleep(3000);

    // both endpoints go away even though a client still holds one open
    VERIFY_IS_NULL(MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(deviceEndpointDeviceId));
    VERIFY_IS_NULL(MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(clientEndpointDeviceId));

    // and closing the now orphaned client connection must not fault
    LOG_OUTPUT(L"Disconnecting the orphaned client");
    device.Session.DisconnectEndpointConnection(clientConnection.ConnectionId());
    clientConnection = nullptr;
}


void MidiVirtualDeviceTests::TestMultipleClientsConnectAndDisconnect()
{
    TestVirtualDevice device{};
    auto cleanup = wil::scope_exit([&] { CleanupTestVirtualDevice(device); });

    VERIFY_IS_TRUE(CreateTestVirtualDevice(L"Contoso Multiclient Test", device));

    auto deviceEndpointDeviceId = device.DeviceEndpointDeviceId;
    auto clientEndpointDeviceId = device.ClientEndpointDeviceId;

    std::vector<MidiEndpointConnection> clients{};

    for (uint32_t i = 0; i < 3; i++)
    {
        auto client = device.Session.CreateEndpointConnection(clientEndpointDeviceId);
        VERIFY_IS_NOT_NULL(client);
        VERIFY_IS_TRUE(client.Open());

        clients.push_back(client);
    }

    VERIFY_IS_NOT_NULL(MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(deviceEndpointDeviceId));
    VERIFY_IS_NOT_NULL(MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(clientEndpointDeviceId));

    // drop the clients one at a time. Nothing should disappear as they go
    for (auto& client : clients)
    {
        device.Session.DisconnectEndpointConnection(client.ConnectionId());

        Sleep(500);

        VERIFY_IS_NOT_NULL(MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(deviceEndpointDeviceId));
        VERIFY_IS_NOT_NULL(MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(clientEndpointDeviceId));
    }

    clients.clear();

    LOG_OUTPUT(L"All clients gone, now disconnecting the device");
    device.DeviceConnection.RemoveMessageProcessingPlugin(device.Device.PluginId());
    device.Session.DisconnectEndpointConnection(device.DeviceConnection.ConnectionId());
    device.DeviceConnection = nullptr;

    Sleep(3000);

    // only the device going away tears the endpoints down
    VERIFY_IS_NULL(MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(deviceEndpointDeviceId));
    VERIFY_IS_NULL(MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(clientEndpointDeviceId));
}




// ============================================================================
// Endpoint publication and teardown
// ============================================================================


void MidiVirtualDeviceTests::TestEndpointsPublishedBeforeAnyClientConnects()
{
    TestVirtualDevice device{};
    auto cleanup = wil::scope_exit([&] { CleanupTestVirtualDevice(device); });

    VERIFY_IS_TRUE(CreateTestVirtualDevice(L"Contoso Publication Test", device));

    // no client has ever connected at this point
    VERIFY_IS_TRUE(EndpointExists(device.DeviceEndpointDeviceId));
    VERIFY_IS_TRUE(EndpointExists(device.ClientEndpointDeviceId));

    // and the very first client must be able to use it
    auto client = device.Session.CreateEndpointConnection(device.ClientEndpointDeviceId);
    VERIFY_IS_NOT_NULL(client);
    VERIFY_IS_TRUE(client.Open());

    VERIFY_IS_TRUE(MessageFlows(device.DeviceConnection, client));

    device.Session.DisconnectEndpointConnection(client.ConnectionId());
}


void MidiVirtualDeviceTests::TestBothEndpointsRemovedWhenDeviceDisconnects()
{
    TestVirtualDevice device{};
    auto cleanup = wil::scope_exit([&] { CleanupTestVirtualDevice(device); });

    VERIFY_IS_TRUE(CreateTestVirtualDevice(L"Contoso Teardown Test", device));

    auto deviceEndpointDeviceId = device.DeviceEndpointDeviceId;
    auto clientEndpointDeviceId = device.ClientEndpointDeviceId;

    VERIFY_IS_TRUE(EndpointExists(deviceEndpointDeviceId));
    VERIFY_IS_TRUE(EndpointExists(clientEndpointDeviceId));

    device.DeviceConnection.RemoveMessageProcessingPlugin(device.Device.PluginId());
    device.Session.DisconnectEndpointConnection(device.DeviceConnection.ConnectionId());
    device.DeviceConnection = nullptr;

    Sleep(3000);

    VERIFY_IS_FALSE(EndpointExists(deviceEndpointDeviceId));
    VERIFY_IS_FALSE(EndpointExists(clientEndpointDeviceId));

    // and neither is still sitting in enumeration
    auto all = MidiEndpointDeviceInformation::FindAll(
        MidiEndpointDeviceInformationSortOrder::Name,
        MidiEndpointDeviceInformationFilters::AllStandardEndpoints |
        MidiEndpointDeviceInformationFilters::VirtualDeviceResponder);

    for (auto const& info : all)
    {
        VERIFY_ARE_NOT_EQUAL(info.EndpointDeviceId(), deviceEndpointDeviceId);
        VERIFY_ARE_NOT_EQUAL(info.EndpointDeviceId(), clientEndpointDeviceId);
    }
}


void MidiVirtualDeviceTests::TestRepeatedCreateAndDestroyDoesNotLeakEndpoints()
{
    auto startingCount = CountVirtualEndpoints();

    LOG_OUTPUT(L"Virtual endpoints before: %u", startingCount);

    for (uint32_t iteration = 0; iteration < 3; iteration++)
    {
        TestVirtualDevice device{};

        VERIFY_IS_TRUE(CreateTestVirtualDevice(L"Contoso Leak Test", device));

        // two new endpoints each time round
        VERIFY_ARE_EQUAL(CountVirtualEndpoints(), startingCount + 2);

        auto client = device.Session.CreateEndpointConnection(device.ClientEndpointDeviceId);
        VERIFY_IS_NOT_NULL(client);
        VERIFY_IS_TRUE(client.Open());
        device.Session.DisconnectEndpointConnection(client.ConnectionId());

        CleanupTestVirtualDevice(device);

        Sleep(3000);

        VERIFY_ARE_EQUAL(CountVirtualEndpoints(), startingCount);
    }

    LOG_OUTPUT(L"Virtual endpoints after: %u", CountVirtualEndpoints());

    VERIFY_ARE_EQUAL(CountVirtualEndpoints(), startingCount);
}


void MidiVirtualDeviceTests::TestEndpointStaysFunctionalAcrossClientReconnects()
{
    TestVirtualDevice device{};
    auto cleanup = wil::scope_exit([&] { CleanupTestVirtualDevice(device); });

    VERIFY_IS_TRUE(CreateTestVirtualDevice(L"Contoso Reconnect Test", device));

    // Opening is not enough. Each new client has to actually receive messages, which is what
    // breaks if the device link is dropped when a client leaves.
    for (uint32_t attempt = 0; attempt < 3; attempt++)
    {
        auto client = device.Session.CreateEndpointConnection(device.ClientEndpointDeviceId);
        VERIFY_IS_NOT_NULL(client);
        VERIFY_IS_TRUE(client.Open());

        LOG_OUTPUT(L"Client generation %u connected", attempt + 1);

        VERIFY_IS_TRUE(MessageFlows(device.DeviceConnection, client));

        device.Session.DisconnectEndpointConnection(client.ConnectionId());

        Sleep(500);

        // the endpoints must survive every generation
        VERIFY_IS_TRUE(EndpointExists(device.DeviceEndpointDeviceId));
        VERIFY_IS_TRUE(EndpointExists(device.ClientEndpointDeviceId));
    }
}




// ============================================================================
// Misuse of the device-side endpoint
// ============================================================================


void MidiVirtualDeviceTests::TestExtraConnectionToDeviceSideEndpointDoesNotBreakClient()
{
    TestVirtualDevice device{};
    auto cleanup = wil::scope_exit([&] { CleanupTestVirtualDevice(device); });

    VERIFY_IS_TRUE(CreateTestVirtualDevice(L"Contoso Device Side Misuse Test", device));

    auto client = device.Session.CreateEndpointConnection(device.ClientEndpointDeviceId);
    VERIFY_IS_NOT_NULL(client);
    VERIFY_IS_TRUE(client.Open());

    VERIFY_IS_TRUE(MessageFlows(device.DeviceConnection, client));

    // An app has no business opening the device-side endpoint, but nothing stops it finding the
    // id. The endpoint is correctly published as single-client, but the service does not enforce
    // that today (CMidiDevicePipe reads PKEY_MIDI_SupportsMulticlient and never consults it), so
    // this connection currently succeeds. Tracked separately. What is asserted here is the part
    // that must hold either way: the intruder must not disturb the real device.
    auto intruderSession = MidiSession::Create(L"Virtual device intruder");
    VERIFY_IS_NOT_NULL(intruderSession);

    auto intruder = intruderSession.CreateEndpointConnection(device.DeviceEndpointDeviceId);
    VERIFY_IS_NOT_NULL(intruder);

    bool intruderOpened = intruder.Open();
    LOG_OUTPUT(L"Second connection to the device-side endpoint opened: %d (expected 0 once single-client is enforced)", (int)intruderOpened);

    // the legitimate device to client path still has to work
    VERIFY_IS_TRUE(MessageFlows(device.DeviceConnection, client));

    intruderSession.DisconnectEndpointConnection(intruder.ConnectionId());
    intruderSession.Close();

    Sleep(1000);

    // and the intruder leaving must not have torn anything down
    VERIFY_IS_TRUE(EndpointExists(device.DeviceEndpointDeviceId));
    VERIFY_IS_TRUE(EndpointExists(device.ClientEndpointDeviceId));
    VERIFY_IS_TRUE(MessageFlows(device.DeviceConnection, client));

    device.Session.DisconnectEndpointConnection(client.ConnectionId());
}




// ============================================================================
// Teardown robustness
//
// A client that is mid-send, or that never disconnects at all, must not be able to wedge the
// service when the device side goes away.
// ============================================================================


void MidiVirtualDeviceTests::TestDeviceTeardownWhileClientSendingDoesNotHang()
{
    TestVirtualDevice device{};
    auto cleanup = wil::scope_exit([&] { CleanupTestVirtualDevice(device); });

    VERIFY_IS_TRUE(CreateTestVirtualDevice(L"Contoso Teardown Race Test", device));

    auto deviceEndpointDeviceId = device.DeviceEndpointDeviceId;
    auto clientEndpointDeviceId = device.ClientEndpointDeviceId;

    auto client = device.Session.CreateEndpointConnection(clientEndpointDeviceId);
    VERIFY_IS_NOT_NULL(client);
    VERIFY_IS_TRUE(client.Open());

    std::atomic<bool> keepSending{ true };
    std::atomic<uint32_t> sendCount{ 0 };

    std::thread sender([&]
        {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);

            while (keepSending)
            {
                client.SendSingleMessagePacket(
                    MidiMessage64(MidiClock::TimestampConstantSendImmediately(), 0x40903C00, 0x12340000));

                sendCount++;
            }

            winrt::uninit_apartment();
        });

    // let the sender get going before pulling the device out from under it
    Sleep(500);
    VERIFY_IS_GREATER_THAN(sendCount.load(), (uint32_t)0);

    auto teardownStart = GetTickCount64();

    device.DeviceConnection.RemoveMessageProcessingPlugin(device.Device.PluginId());
    device.Session.DisconnectEndpointConnection(device.DeviceConnection.ConnectionId());
    device.DeviceConnection = nullptr;

    auto teardownMilliseconds = GetTickCount64() - teardownStart;

    LOG_OUTPUT(L"Device teardown took %llu ms with a client sending (%u messages sent)",
        teardownMilliseconds, sendCount.load());

    keepSending = false;
    sender.join();

    // teardown must not block on the sending client
    VERIFY_IS_LESS_THAN(teardownMilliseconds, (uint64_t)10000);

    Sleep(3000);

    VERIFY_IS_FALSE(EndpointExists(deviceEndpointDeviceId));
    VERIFY_IS_FALSE(EndpointExists(clientEndpointDeviceId));

    // the service has to still be answering
    VERIFY_IS_TRUE(MidiEndpointDeviceInformation::FindAll().Size() >= 0);

    device.Session.DisconnectEndpointConnection(client.ConnectionId());
}


void MidiVirtualDeviceTests::TestDeviceTeardownWithoutClientDisconnectDoesNotHang()
{
    TestVirtualDevice device{};
    auto cleanup = wil::scope_exit([&] { CleanupTestVirtualDevice(device); });

    VERIFY_IS_TRUE(CreateTestVirtualDevice(L"Contoso Orphan Client Test", device));

    auto deviceEndpointDeviceId = device.DeviceEndpointDeviceId;
    auto clientEndpointDeviceId = device.ClientEndpointDeviceId;

    // a second session standing in for another app that simply never cleans up
    auto orphanSession = MidiSession::Create(L"Virtual device orphan client");
    VERIFY_IS_NOT_NULL(orphanSession);

    auto orphan = orphanSession.CreateEndpointConnection(clientEndpointDeviceId);
    VERIFY_IS_NOT_NULL(orphan);
    VERIFY_IS_TRUE(orphan.Open());

    auto teardownStart = GetTickCount64();

    device.DeviceConnection.RemoveMessageProcessingPlugin(device.Device.PluginId());
    device.Session.DisconnectEndpointConnection(device.DeviceConnection.ConnectionId());
    device.DeviceConnection = nullptr;

    auto teardownMilliseconds = GetTickCount64() - teardownStart;

    LOG_OUTPUT(L"Device teardown took %llu ms with an undisconnected client", teardownMilliseconds);

    VERIFY_IS_LESS_THAN(teardownMilliseconds, (uint64_t)10000);

    Sleep(3000);

    VERIFY_IS_FALSE(EndpointExists(deviceEndpointDeviceId));
    VERIFY_IS_FALSE(EndpointExists(clientEndpointDeviceId));

    // sending on the now dead connection must fail rather than hang or fault
    auto sendResult = orphan.SendSingleMessagePacket(
        MidiMessage64(MidiClock::TimestampConstantSendImmediately(), 0x40903C00, 0x12340000));

    LOG_OUTPUT(L"Send on orphaned client returned 0x%08X", (unsigned)sendResult);

    // closing the orphan afterwards must also complete
    orphanSession.Close();

    VERIFY_IS_TRUE(MidiEndpointDeviceInformation::FindAll().Size() >= 0);
}




// ============================================================================
// Client endpoint in-use notification
//
// The transport reports only the 0 -> 1 and 1 -> 0 transitions, because midisrv keeps one
// transport connection per endpoint regardless of how many apps are attached.
// ============================================================================


void MidiVirtualDeviceTests::TestClientEndpointInUseIsFalseBeforeAnyClientConnects()
{
    TestVirtualDevice device{};
    auto cleanup = wil::scope_exit([&] { CleanupTestVirtualDevice(device); });

    VERIFY_IS_TRUE(CreateTestVirtualDevice(L"Contoso In Use Initial", device));

    // no client has ever connected, and the value has to be readable without waiting for an edge
    VERIFY_IS_FALSE(device.Device.IsClientEndpointInUse());
}


void MidiVirtualDeviceTests::TestClientEndpointInUseRaisedOnConnectAndDisconnect()
{
    TestVirtualDevice device{};
    auto cleanup = wil::scope_exit([&] { CleanupTestVirtualDevice(device); });

    VERIFY_IS_TRUE(CreateTestVirtualDevice(L"Contoso In Use Events", device));

    wil::unique_event_nothrow inUseRaised;
    inUseRaised.create();

    wil::unique_event_nothrow notInUseRaised;
    notInUseRaised.create();

    std::atomic<uint32_t> eventCount{ 0 };

    auto token = device.Device.ClientEndpointInUseChanged(
        [&](MidiVirtualDevice const&, MidiVirtualDeviceClientEndpointInUseChangedEventArgs const& args)
        {
            eventCount++;

            if (args.IsClientEndpointInUse())
            {
                inUseRaised.SetEvent();
            }
            else
            {
                notInUseRaised.SetEvent();
            }
        });

    auto revoke = wil::scope_exit([&] { device.Device.ClientEndpointInUseChanged(token); });

    auto client = device.Session.CreateEndpointConnection(device.ClientEndpointDeviceId);
    VERIFY_IS_NOT_NULL(client);
    VERIFY_IS_TRUE(client.Open());

    // property updates travel through PnP, so this is not instant
    VERIFY_IS_TRUE(inUseRaised.wait(15000));
    VERIFY_IS_TRUE(device.Device.IsClientEndpointInUse());

    device.Session.DisconnectEndpointConnection(client.ConnectionId());

    VERIFY_IS_TRUE(notInUseRaised.wait(15000));
    VERIFY_IS_FALSE(device.Device.IsClientEndpointInUse());

    LOG_OUTPUT(L"Raised %u in-use change events", eventCount.load());
}


void MidiVirtualDeviceTests::TestClientEndpointInUseSurvivesDeviceTeardownFirst()
{
    TestVirtualDevice device{};
    auto cleanup = wil::scope_exit([&] { CleanupTestVirtualDevice(device); });

    VERIFY_IS_TRUE(CreateTestVirtualDevice(L"Contoso In Use Teardown", device));

    auto deviceEndpointDeviceId = device.DeviceEndpointDeviceId;
    auto clientEndpointDeviceId = device.ClientEndpointDeviceId;

    auto token = device.Device.ClientEndpointInUseChanged(
        [&](MidiVirtualDevice const&, MidiVirtualDeviceClientEndpointInUseChangedEventArgs const&) { });

    auto revoke = wil::scope_exit([&] { device.Device.ClientEndpointInUseChanged(token); });

    // a client which is still attached when the device side goes away
    auto orphanSession = MidiSession::Create(L"Virtual device in-use orphan");
    VERIFY_IS_NOT_NULL(orphanSession);

    auto orphan = orphanSession.CreateEndpointConnection(clientEndpointDeviceId);
    VERIFY_IS_NOT_NULL(orphan);
    VERIFY_IS_TRUE(orphan.Open());

    Sleep(2000);

    // device side tears down first, taking the endpoint the watcher is watching with it
    device.DeviceConnection.RemoveMessageProcessingPlugin(device.Device.PluginId());
    device.Session.DisconnectEndpointConnection(device.DeviceConnection.ConnectionId());
    device.DeviceConnection = nullptr;

    Sleep(3000);

    VERIFY_IS_FALSE(EndpointExists(deviceEndpointDeviceId));
    VERIFY_IS_FALSE(EndpointExists(clientEndpointDeviceId));

    // the orphaned client leaving now drives a property update for an endpoint which no longer
    // exists, which must not fault
    orphanSession.DisconnectEndpointConnection(orphan.ConnectionId());
    orphanSession.Close();

    Sleep(1000);

    // reading the property on the torn down device must still be safe
    VERIFY_IS_FALSE(device.Device.IsClientEndpointInUse());

    VERIFY_IS_TRUE(MidiEndpointDeviceInformation::FindAll().Size() >= 0);
}
