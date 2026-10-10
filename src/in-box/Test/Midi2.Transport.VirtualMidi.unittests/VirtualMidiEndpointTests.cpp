// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include <array>

#include "VirtualMidiEndpointTests.h"
#include "VirtualDeviceTestSupport.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace VirtualDeviceTest;

namespace
{
    // the service opens its own connection to the client side shortly after the device side opens
    constexpr DWORD DiscoveryWaitMilliseconds{ 10000 };

    constexpr DWORD PropertyWaitMilliseconds{ 10000 };
    constexpr DWORD MessageWaitMilliseconds{ 5000 };


    bool CanRun(_In_ bool const gateEnabled, _In_z_ wchar_t const* gateName)
    {
        if (!gateEnabled)
        {
            Log::Result(TestResults::Skipped, String().Format(L"%s is disabled.", gateName));
            return false;
        }

        if (!Feature_Servicing_MIDI2VirtualDeviceRemovalDeadlock::IsEnabled())
        {
            Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2VirtualDeviceRemovalDeadlock is disabled, so closing this test's virtual device would hang the service. VirtualMidiTeardownTests reports that.");
            return false;
        }

        if (!VirtualAvailable())
        {
            Log::Result(TestResults::Skipped, L"Virtual MIDI transport is not available.");
            return false;
        }

        return true;
    }


    // A virtual device with its device side open, as the app which publishes it would have it.
    struct OpenDevice
    {
        CreatedDevice Device{ };
        MidiTest::ServiceSession Session{ };
        wil::com_ptr_nothrow<MidiTest::ReceivedMessages> Received{ };
        wil::com_ptr_nothrow<IMidiBidirectional> DeviceSide{ };
    };

    void Open(_In_ std::wstring const& name, _Inout_ OpenDevice& device)
    {
        device.Device = CreateDevice(name);
        VERIFY_IS_FALSE(device.Device.DeviceEndpointId.empty(), device.Device.Detail.c_str());
        VERIFY_IS_FALSE(device.Device.ClientEndpointId.empty());

        VERIFY_SUCCEEDED(device.Session.Open(L"Virtual MIDI endpoint tests, device app"));

        device.Received = MidiTest::ReceivedMessages::Create();
        VERIFY_IS_NOT_NULL(device.Received.get());

        VERIFY_SUCCEEDED(device.Session.Connect(device.Device.DeviceEndpointId, device.Received.get(), device.DeviceSide));
    }

    // Closing the device side is what removes the device, and it is the call a hung service never
    // returns from. After a hang, nothing here calls into the service again.
    bool Close(_Inout_ OpenDevice& device) noexcept
    {
        if (!MidiTest::CloseConnectionWithHangGuard(device.DeviceSide))
        {
            device.Session.Abandon();
            return false;
        }

        device.Session.Close();

        return true;
    }

    void VerifyClosed(_Inout_ OpenDevice& device)
    {
        VERIFY_IS_TRUE(Close(device), String().Format(
            L"The service did not return within %u seconds while closing the device side, so it is hung. MIDI will not work on this PC until the MIDI service is restarted.",
            MidiTest::HangTimeoutMilliseconds / 1000));
    }


    bool IsEndpointDiscovery(_In_ uint32_t const firstWord)
    {
        return
            WindowsMidiServicesInternal::GetUmpMessageTypeFromFirstWord(firstWord) == MIDI_STREAM_MESSAGE_UMP_MESSAGE_TYPE &&
            ((firstWord >> 16) & 0x3FF) == MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_DISCOVERY;
    }


    bool WaitForInUse(_In_ OpenDevice const& device, _In_ bool const expected)
    {
        return MidiTest::WaitUntil([&]
            {
                auto const inUse = MidiTest::ReadEndpointBoolProperty(
                    device.Device.DeviceEndpointId,
                    STRING_PKEY_MIDI_VirtualMidiClientEndpointInUse);

                return inUse.has_value() && inUse.value() == expected;
            },
            PropertyWaitMilliseconds);
    }


    // Endpoint Name Notification carries 14 bytes of UTF-8 per message: 2 in the first word and 4
    // in each of the other three.
    std::vector<std::array<uint32_t, 4>> BuildEndpointNameNotification(_In_ std::wstring const& name)
    {
        std::string utf8(MidiTest::Utf8ByteCount(name), '\0');

        if (!utf8.empty())
        {
            WideCharToMultiByte(CP_UTF8, 0, name.c_str(), static_cast<int>(name.length()), utf8.data(), static_cast<int>(utf8.size()), nullptr, nullptr);
        }

        constexpr size_t bytesPerMessage{ 14 };

        auto messageCount = (utf8.size() + bytesPerMessage - 1) / bytesPerMessage;

        if (messageCount == 0)
        {
            messageCount = 1;
        }

        std::vector<std::array<uint32_t, 4>> messages{ };

        for (size_t i = 0; i < messageCount; i++)
        {
            uint32_t form{ MIDI_STREAM_MESSAGE_MULTI_FORM_CONTINUE };

            if (messageCount == 1)
            {
                form = MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE;
            }
            else if (i == 0)
            {
                form = MIDI_STREAM_MESSAGE_MULTI_FORM_START;
            }
            else if (i == messageCount - 1)
            {
                form = MIDI_STREAM_MESSAGE_MULTI_FORM_END;
            }

            uint8_t text[bytesPerMessage]{ };

            for (size_t b = 0; b < bytesPerMessage && i * bytesPerMessage + b < utf8.size(); b++)
            {
                text[b] = static_cast<uint8_t>(utf8[i * bytesPerMessage + b]);
            }

            std::array<uint32_t, 4> words{ };

            words[0] =
                static_cast<uint32_t>(MIDI_STREAM_MESSAGE_UMP_MESSAGE_TYPE) << 28 |
                form << 26 |
                static_cast<uint32_t>(MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION) << 16 |
                static_cast<uint32_t>(text[0]) << 8 |
                static_cast<uint32_t>(text[1]);

            for (size_t w = 1; w < 4; w++)
            {
                auto const first = 2 + (w - 1) * 4;

                words[w] =
                    static_cast<uint32_t>(text[first]) << 24 |
                    static_cast<uint32_t>(text[first + 1]) << 16 |
                    static_cast<uint32_t>(text[first + 2]) << 8 |
                    static_cast<uint32_t>(text[first + 3]);
            }

            messages.push_back(words);
        }

        return messages;
    }
}


bool VirtualMidiEndpointTests::TestSetup()
{
    m_deviceNodeTracker.Start();

    return true;
}

bool VirtualMidiEndpointTests::TestCleanup()
{
    m_deviceNodeTracker.RemoveDeviceNodesCreatedSinceStart();

    return true;
}


void VirtualMidiEndpointTests::TestClientCanReconnectAfterTheLastClientCloses()
{
    if (!CanRun(Feature_Servicing_MIDI2VirtualDeviceClientReconnect::IsEnabled(), L"Feature_Servicing_MIDI2VirtualDeviceClientReconnect"))
    {
        return;
    }

    MidiTest::MultithreadedApartment apartment{ };

    OpenDevice device{ };
    auto closeDevice = wil::scope_exit([&] { Close(device); });

    Open(L"Reconnect Test Virtual", device);

    MidiTest::ServiceSession app{ };
    VERIFY_SUCCEEDED(app.Open(L"Virtual MIDI endpoint tests, app"));

    // the only app connected, and then gone
    {
        auto const firstReceived = MidiTest::ReceivedMessages::Create(false);
        wil::com_ptr_nothrow<IMidiBidirectional> firstClient{ };

        VERIFY_SUCCEEDED(app.Connect(device.Device.ClientEndpointId, firstReceived.get(), firstClient));
        VERIFY_IS_TRUE(MidiTest::CloseConnectionWithHangGuard(firstClient));
    }

    auto const received = MidiTest::ReceivedMessages::Create();
    wil::com_ptr_nothrow<IMidiBidirectional> client{ };
    auto closeClient = wil::scope_exit([&] { MidiTest::CloseConnectionWithHangGuard(client); });

    // a rolled back service refuses this, because the last app leaving also unlinked the device side
    VERIFY_SUCCEEDED(app.Connect(device.Device.ClientEndpointId, received.get(), client));

    std::vector<uint32_t> const message{ 0x40903C00, 0x12340000 };

    VERIFY_SUCCEEDED(MidiTest::SendWords(device.DeviceSide.get(), message));
    VERIFY_IS_TRUE(received->WaitForMessage(message, MessageWaitMilliseconds));

    closeClient.reset();
    app.Close();

    closeDevice.release();
    VerifyClosed(device);
}


void VirtualMidiEndpointTests::TestTransportDeclaresClientEndpointInUseCapability()
{
    if (!Feature_Servicing_MIDI2VirtualDeviceClientEndpointInUse::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2VirtualDeviceClientEndpointInUse is disabled.");
        return;
    }

    if (!VirtualAvailable())
    {
        Log::Result(TestResults::Skipped, L"Virtual MIDI transport is not available.");
        return;
    }

    auto const result = SendVirtualConfig(LR"({"transportCommand":{"commandName":"queryCapabilities"}})");

    VERIFY_IS_TRUE(result.IsSuccess(), result.ResponseJson.c_str());

    // the device app only waits for the report when this says one will come
    VERIFY_IS_TRUE(
        result.ResponseJson.find(L"\"clientEndpointInUseNotification\":true") != std::wstring::npos,
        result.ResponseJson.c_str());
}


void VirtualMidiEndpointTests::TestClientEndpointInUseFollowsAppConnections()
{
    if (!CanRun(Feature_Servicing_MIDI2VirtualDeviceClientEndpointInUse::IsEnabled(), L"Feature_Servicing_MIDI2VirtualDeviceClientEndpointInUse"))
    {
        return;
    }

    MidiTest::MultithreadedApartment apartment{ };

    OpenDevice device{ };
    auto closeDevice = wil::scope_exit([&] { Close(device); });

    Open(L"In Use Test Virtual", device);

    // The service keeps its own connection to the client side for endpoint discovery, and that is
    // not an app. The discovery request arriving shows that connection is open.
    VERIFY_IS_TRUE(device.Received->WaitForFirstWord(IsEndpointDiscovery, DiscoveryWaitMilliseconds));
    VERIFY_IS_TRUE(WaitForInUse(device, false));

    MidiTest::ServiceSession app{ };
    VERIFY_SUCCEEDED(app.Open(L"Virtual MIDI endpoint tests, app"));

    auto const received = MidiTest::ReceivedMessages::Create(false);
    wil::com_ptr_nothrow<IMidiBidirectional> client{ };
    auto closeClient = wil::scope_exit([&] { MidiTest::CloseConnectionWithHangGuard(client); });

    VERIFY_SUCCEEDED(app.Connect(device.Device.ClientEndpointId, received.get(), client));
    VERIFY_IS_TRUE(WaitForInUse(device, true));

    closeClient.reset();
    VERIFY_IS_TRUE(WaitForInUse(device, false));

    app.Close();

    closeDevice.release();
    VerifyClosed(device);
}


void VirtualMidiEndpointTests::TestOverlongUnicodeNameIsCutOnCharacterBoundary()
{
    if (!CanRun(Feature_Servicing_MIDI2EndpointNameUtf8ByteLimit::IsEnabled(), L"Feature_Servicing_MIDI2EndpointNameUtf8ByteLimit"))
    {
        return;
    }

    MidiTest::MultithreadedApartment apartment{ };

    // 40 three-byte characters is 120 UTF-8 bytes. 32 of them (96 bytes) is the most which fits,
    // because a 33rd would end on byte 99.
    std::wstring const longName(40, L'\u8A2D');

    OpenDevice device{ };
    auto closeDevice = wil::scope_exit([&] { Close(device); });

    Open(longName, device);

    // the client side is the one apps see
    std::wstring clientName{ };

    VERIFY_IS_TRUE(MidiTest::WaitUntil([&]
        {
            clientName = MidiTest::ReadEndpointStringProperty(device.Device.ClientEndpointId, STRING_PKEY_MIDI_EndpointName);
            return !clientName.empty();
        },
        PropertyWaitMilliseconds));

    VERIFY_ARE_EQUAL(longName.substr(0, 32), clientName);

    // the device side's name has a suffix after the cut, and still has to fit as a whole
    auto const deviceName = MidiTest::ReadEndpointStringProperty(device.Device.DeviceEndpointId, STRING_PKEY_MIDI_EndpointName);

    VERIFY_IS_FALSE(deviceName.empty());
    VERIFY_IS_LESS_THAN_OR_EQUAL(MidiTest::Utf8ByteCount(deviceName), (size_t)MIDI_STREAM_MESSAGE_ENDPOINT_NAME_MAX_LENGTH);

    closeDevice.release();
    VerifyClosed(device);
}


void VirtualMidiEndpointTests::TestEndpointNameFromDeviceIsDecodedAsUtf8()
{
    if (!CanRun(Feature_Servicing_MIDI2StreamTextUtf8::IsEnabled(), L"Feature_Servicing_MIDI2StreamTextUtf8"))
    {
        return;
    }

    MidiTest::MultithreadedApartment apartment{ };

    OpenDevice device{ };
    auto closeDevice = wil::scope_exit([&] { Close(device); });

    Open(L"Stream Text Test Virtual", device);

    // the service has to be listening on the client side before the device can answer it
    VERIFY_IS_TRUE(device.Received->WaitForFirstWord(IsEndpointDiscovery, DiscoveryWaitMilliseconds));

    // Thirteen ASCII characters first, so the three UTF-8 bytes of U+2019 are split across the
    // first two messages. Decoding each message on its own breaks that character.
    std::wstring const name{ L"Straddle Test\u2019s Synth" };

    for (auto const& message : BuildEndpointNameNotification(name))
    {
        VERIFY_SUCCEEDED(MidiTest::SendWords(device.DeviceSide.get(), std::vector<uint32_t>(message.begin(), message.end())));
    }

    std::wstring reported{ };

    auto const matched = MidiTest::WaitUntil([&]
        {
            reported = MidiTest::ReadEndpointStringProperty(device.Device.ClientEndpointId, STRING_PKEY_MIDI_EndpointProvidedName);
            return reported == name;
        },
        PropertyWaitMilliseconds);

    VERIFY_IS_TRUE(matched, String().Format(L"The service reported the name as: %s", reported.c_str()));

    closeDevice.release();
    VerifyClosed(device);
}
